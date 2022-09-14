// Benchmark for a "lower bound" search on a binary tree.
#include <functional>
#include <iostream>
#include <random>
#include <span>
#include <sstream>

#include "benchmark/benchmark.h"
#include "lower_bound.h"
#include "lower_bound_test.h"

namespace lower_bound {
namespace {

// OutputStreamable<T> is a concept satisfied when T can be streamed to a
// std::ostream.
template <typename T>
concept OutputStreamable = requires(const T& obj, std::ostream& os) {
  { os << obj } -> std::convertible_to<std::ostream&>;
};

// ContainerDebugString returns a string representation of a container.
template <typename T>
std::string ContainerDebugString(const T& container) {
  std::ostringstream os;
  os << "[";
  bool first = true;
  for (auto& val : container) {
    if (!first) {
      os << ", ";
    }
    first = false;
    os << val;
  }
  os << "]";
  return os.str();
}

// MemoryLayout names the layout of binary tree nodes in memory.  In all
// cases this benchmark allocates nodes in an array.  This enum controls
// the arangement of nodes within the array.
//
// This benchmark always uses complete binary trees, where non-null nodes
// always have either two non-null children or two null children.
//
// kByNodeLevel: nodes occur in level order within the array.  In other
// words, the root is first, followed by the two nodes in the second level,
// followed by the four nodes in the third level, and so on.  In this
// approach the root of the tree is at index zero and the left and right
// children of a node at index I is (2 * I + 1) and (2 * I + 2)
// respectively.
//
// kInKeyOrder: nodes are laid out order of their key values within the
// array.
//
// kAtRandom: nodes occur in a random order within the array.
//
enum class MemoryLayout {
  kByNodeLevel,
  kInKeyOrder,
  kAtRandom,
};

// AccessPattern names the sequence of keys accessed from the tree
// (i.e. passed to LowerBound).
//
// kByNodeLevel: are accessed by ascending depth, left to right within each
// level.
//
// kInOrdered: nodes are laid out order of their key values within the
// array.
//
// kRandomOrdered: nodes occur in a random order within the array.
//
enum class AccessPattern {
  kByNodeLevel,
  kInKeyOrder,
  kAtRandom,
};

std::ostream& operator<<(std::ostream& os, MemoryLayout layout) {
  switch (layout) {
    case MemoryLayout::kByNodeLevel:
      return os << "LayoutByNodeLevel";
    case MemoryLayout::kInKeyOrder:
      return os << "LayoutInKeyOrder";
    case MemoryLayout::kAtRandom:
      return os << "LayoutAtRandom";
  }
  return os << "MemoryLayout(" << static_cast<int>(layout) << ')';
}

std::ostream& operator<<(std::ostream& os, AccessPattern pattern) {
  switch (pattern) {
    case AccessPattern::kByNodeLevel:
      return os << "AccessByNodeLevel";
    case AccessPattern::kInKeyOrder:
      return os << "AccessInKeyOrder";
    case AccessPattern::kAtRandom:
      return os << "AccessAtRandom";
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

    root = nullptr;
    switch (layout) {
      case MemoryLayout::kInKeyOrder: {
        root = LayoutInKeyOrder(nodes);
        break;
      }
      case MemoryLayout::kByNodeLevel: {
        root = LayoutByNodeLevel(nodes);
        break;
      }
      case MemoryLayout::kAtRandom: {
        root = LayoutAtRandom(nodes, /* seed= */ 17);
        break;
      }
    }
    assert(root != nullptr);

    switch (access_pattern) {
      case AccessPattern::kInKeyOrder:
      case AccessPattern::kAtRandom:
        keys = KeysInSymmetricOrder(root);
        break;
      case AccessPattern::kByNodeLevel:
        keys = KeysByLevel(root);
        break;
    }


    constexpr int kUnroll = 32;
    while (keys.size() < kUnroll) {
      keys.reserve(keys.size() * 2);
      keys.insert(keys.end(), keys.begin(), keys.end());
    }
    if (access_pattern == AccessPattern::kAtRandom) {
      std::shuffle(keys.begin(), keys.end(), std::minstd_rand0(42));
    }

    // Ensure that every node has valid pointers.
    for (const Node& node : nodes) {
      if (node.left != nullptr) {
        assert(std::greater_equal()(node.left, &nodes.front()));
        assert(std::less_equal()(node.left, &nodes.back()));
      }
      if (node.right != nullptr) {
        assert(std::greater_equal()(node.right, &nodes.front()));
        assert(std::less_equal()(node.right, &nodes.back()));
      }
    }

    // Ensure that we can format the tree.
    assert(TreeDebugString(root) != "hello world");
  }

  size_t WorkingSetBytes() {
    return nodes.size() * sizeof(nodes[0]) + keys.size() * sizeof(keys[0]);
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
       << " != actual " << actual << "; " << TreeDebugString(fixture.root);
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
       {MemoryLayout::kInKeyOrder, MemoryLayout::kByNodeLevel,
        MemoryLayout::kAtRandom}) {
    for (AccessPattern access :
         {AccessPattern::kInKeyOrder, AccessPattern::kByNodeLevel,
          AccessPattern::kAtRandom}) {
      std::ostringstream os;
      os << "BM_LowerBound/" << layout << '/' << access;
      auto* benchmark = benchmark::RegisterBenchmark(
          os.str().c_str(), [layout, access](benchmark::State& state) {
            BM_LowerBound(state, layout, access);
          });
      const bool kRegisterAllHeights = false;
      if (kRegisterAllHeights) {
        for (int height = 1; height <= 24; ++height) {
          benchmark->Arg(height);
        }
      } else {
        benchmark->RangeMultiplier(2);
        benchmark->Range(2, 24);
      }
    }
  }
}

}  // namespace
}  // namespace lower_bound

int main(int argc, char** argv) {
  // Run the Google Benchmark code.  Most of this is standard boilerplate
  // except for the call to lower_bound::RegisterAll().
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
  lower_bound::RegisterAll();
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
