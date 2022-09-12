#define DOCTEST_CONFIG_IMPLEMENT

#include <functional>
#include <iostream>
#include <random>
#include <span>
#include <sstream>

#include "benchmark/benchmark.h"
#include "doctest/doctest.h"

namespace {

struct Node {
  Node* left = nullptr;
  Node* right = nullptr;
  long key = 0;
};

#ifndef __GNUC__
#error "gcc or clang required"
#endif
#ifdef __clang__
#define ATTRIBUTE_NOIPA __attribute__((noinline))
#else
#define ATTRIBUTE_NOIPA __attribute__((noipa))
#endif

ATTRIBUTE_NOIPA Node* LowerBound(Node* x, long i) {
  Node* lower = nullptr;
  while (x != nullptr) {
    if (x->key < i) {
      lower = x;
      x = x->left;
    } else {
      x = x->right;
    }
  }
  return lower;
}

struct TreeProperties {
  int height = 0;
  int size = 0;
};

std::ostream& operator<<(std::ostream& os, TreeProperties& properties) {
  return os << "height:" << properties.height << " size:" << properties.size;
}

// Return the TreeProperties (height and size) of the given tree.
TreeProperties ComputeTreeProperties(Node* node, const long* minimum = nullptr,
                                     const long* maximum = nullptr) {
  if (node == nullptr) {
    return TreeProperties();
  }

  bool error = false;
  if (minimum != nullptr && node->key <= *minimum) {
    std::cerr
        << "Node " << node->key
        << " is less than the minimum allowable key implied by its parents\n";
    error = true;
  } else if (maximum != nullptr && node->key >= *maximum) {
    std::cerr << "Node " << node->key
              << " is greater than the maximum allowable key implied by its "
                 "parents\n";
    error = true;
  }
  if (error) {
    std::cerr << "aborting...\n";
    std::abort();
  }

  TreeProperties left = ComputeTreeProperties(node->left, minimum, &node->key);
  TreeProperties right =
      ComputeTreeProperties(node->right, &node->key, maximum);

  return TreeProperties{.height = 1 + std::max(left.height, right.height),
                        .size = 1 + left.size + right.size};
}

void PrintTree(std::ostream& os, Node* node) {
  os << '(';
  if (node != nullptr) {
    os << node->key;
    if (!(node->left == nullptr && node->right == nullptr)) {
      os << ' ';
      PrintTree(os, node->left);
      os << ' ';
      PrintTree(os, node->right);
    }
  }
  os << ')';
}

enum class MemoryLayout { kLevelOrdered, kRandom };
enum class AccessPattern { kAscending, kRandom };

std::ostream& operator<<(std::ostream& os, MemoryLayout layout) {
  switch (layout) {
    case MemoryLayout::kLevelOrdered:
      return os << "Level";
      break;
    case MemoryLayout::kRandom:
      return os << "Random";
      break;
  }
  return os << "MemoryLayout(" << static_cast<int>(layout) << ')';
}

std::ostream& operator<<(std::ostream& os, AccessPattern pattern) {
  switch (pattern) {
    case AccessPattern::kAscending:
      return os << "Ascending";
      break;
    case AccessPattern::kRandom:
      return os << "Random";
      break;
  }
  return os << "AccessPattern(" << static_cast<int>(pattern) << ')';
}

constexpr bool kDebugLog = false;

struct Fixture {
  std::vector<Node> nodes;
  std::vector<int> keys;
  Node* root;

  ATTRIBUTE_NOIPA Fixture(int key_count, MemoryLayout layout,
                          AccessPattern access_pattern) {
    nodes.resize(key_count);
    keys = AscendingIntegers(key_count);

    std::vector<int> layout_map = AscendingIntegers(key_count);
    if constexpr (kDebugLog) {
      std::cout << "MemoryLayout is " << layout << '\n';
    }
    if (layout == MemoryLayout::kRandom) {
      std::shuffle(layout_map.begin(), layout_map.end(),
                   std::minstd_rand0(17));
    }
    if constexpr (kDebugLog) {
      std::cout << "layout_map:";
      for (int index : layout_map) {
        std::cout << ' ' << index;
      }
      std::cout << '\n';
    }

    int key = 0;
    root = InsertLevelOrder(layout_map, nodes, /* index= */ 0, /* key= */ key);

    constexpr int kUnroll = 32;
    while (keys.size() < kUnroll) {
      keys.reserve(keys.size() * 2);
      keys.insert(keys.end(), keys.begin(), keys.end());
    }
    if (access_pattern == AccessPattern::kRandom) {
      std::shuffle(keys.begin(), keys.end(), std::minstd_rand0(42));
    }
  }
  ATTRIBUTE_NOIPA ~Fixture() {}

  size_t WorkingSetBytes() {
    return nodes.size() * sizeof(nodes[0]) + keys.size() * sizeof(keys[0]);
  }

 private:
  static std::vector<int> AscendingIntegers(int count) {
    std::vector<int> numbers;
    numbers.reserve(count);
    for (int i = 0; i < count; ++i) {
      numbers.push_back(i);
    }
    return numbers;
  }

  static Node* InsertLevelOrder(const std::vector<int>& layout_map,
                                std::span<Node> nodes, int index, int& key,
                                int depth = 0) {
    if constexpr (kDebugLog) {
      auto prefix = [depth]() -> std::ostream& {
        for (int i = 0; i < depth; ++i) {
          std::cout << "   ";
        }
        return std::cout;
      };
      prefix() << "InsertLevelOrder(key=" << index << ")";
    }
    // Base case for recursion
    if (index >= layout_map.size()) {
      if constexpr (kDebugLog) {
        std::cout << "-> null\n";
      }
      return {};
    }
    if constexpr (kDebugLog) {
      std::cout << "-> layout_map[" << index << "] -> " << layout_map[index]
                << '\n';
    }
    Node* root = &nodes[layout_map[index]];
    root->left =
        InsertLevelOrder(layout_map, nodes, 2 * index + 1, key, depth + 1);
    root->key = key++;
    root->right =
        InsertLevelOrder(layout_map, nodes, 2 * index + 2, key, depth + 1);
    return root;
  }
};

void BM_LowerBound(benchmark::State& state, MemoryLayout layout,
                   AccessPattern access_pattern) {
  TreeProperties expected;
  expected.height = state.range(0);
  expected.size = (1U << expected.height) - 1;
  Fixture fixture(expected.size, layout, access_pattern);
  TreeProperties actual = ComputeTreeProperties(fixture.root);
  if (expected.height != actual.height || expected.size != actual.size) {
    std::ostringstream os;
    os << "tree height or size mismatch; expected " << expected
       << " != actual " << actual << "; ";
    PrintTree(os, fixture.root);
    state.SkipWithError(os.str().c_str());
    return;
  }

  Node* root = fixture.root;
  while (state.KeepRunningBatch(fixture.keys.size())) {
    for (int key : fixture.keys) {
      benchmark::DoNotOptimize(LowerBound(root, key));
    }
  }

  state.counters["tree_height"] =
      benchmark::Counter(static_cast<double>(expected.height));
  state.counters["node_count"] =
      benchmark::Counter(static_cast<double>(expected.size));
  state.counters["time_per_node_traversed"] =
      benchmark::Counter(static_cast<double>(expected.height),
                         benchmark::Counter::kIsIterationInvariantRate |
                             benchmark::Counter::kInvert);
  state.counters["working_set_bytes"] = benchmark::Counter(
      static_cast<double>(fixture.WorkingSetBytes()),
      benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

void RegisterAll() {
  for (MemoryLayout layout :
       {MemoryLayout::kLevelOrdered, MemoryLayout::kRandom}) {
    for (AccessPattern access :
         {AccessPattern::kAscending, AccessPattern::kRandom}) {
      std::ostringstream os;
      os << "BM_LowerBound/" << layout << '/' << access;
      auto* benchmark = benchmark::RegisterBenchmark(
          os.str().c_str(), [layout, access](benchmark::State& state) {
            BM_LowerBound(state, layout, access);
          });
      for (int height = 1; height <= 24; ++height) {
        benchmark->Arg(height);
      }
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  {
    // Run all doctest tests before Google Benchmark code runs.  This
    // always occurs; no command line configuration is possible.
    doctest::Context context;
    int res = context.run();
    if (res != 0 || context.shouldExit()) {
      return res;
    }
  }

  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
  RegisterAll();
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
