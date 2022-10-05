// Benchmark for a "lower bound" search on a binary tree.
#include <functional>
#include <iostream>
#include <random>
#include <span>
#include <sstream>

#include "absl/log/check.h"
#include "absl/random/random.h"
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
// kAscending: Nodes occur in memory with the same order as their
// keys.
//
// kRandom: Nodes occur in a uniformly random order within the
// array.
//
enum class MemoryLayout {
  kAscending,
  kRandom,
};

// AccessPattern names the sequence of keys accessed from the tree
// (i.e. passed to LowerBound).
//
// kAscending: nodes are laid out order of their key values within the
// array.
//
// kRandomOrdered: nodes occur in a random order within the array.
//
enum class AccessPattern {
  kAscending,
  kRandom,
};

std::ostream& operator<<(std::ostream& os, MemoryLayout layout) {
  switch (layout) {
    case MemoryLayout::kAscending:
      return os << "LayoutAscending";
    case MemoryLayout::kRandom:
      return os << "LayoutRandom";
  }
  return os << "MemoryLayout(" << static_cast<int>(layout) << ')';
}

std::ostream& operator<<(std::ostream& os, AccessPattern pattern) {
  switch (pattern) {
    case AccessPattern::kAscending:
      return os << "AccessAscending";
    case AccessPattern::kRandom:
      return os << "AccessRandom";
  }
  return os << "AccessPattern(" << static_cast<int>(pattern) << ')';
}

constexpr bool kDebugLog = false;

struct Fixture {
  std::vector<Node> nodes;
  std::vector<int> keys;
  Node* root;

  static int EstimateWorkingSetBytes(int key_count) {
    return key_count * sizeof(Node) + key_count * sizeof(int);
  }

  size_t WorkingSetBytes() {
    return nodes.size() * sizeof(nodes[0]) + keys.size() * sizeof(keys[0]);
  }

  ATTRIBUTE_NOIPA Fixture(int key_count, MemoryLayout layout,
                          AccessPattern access_pattern) {
    nodes.resize(key_count);

    absl::BitGen bitgen;
    root = nullptr;
    switch (layout) {
      case MemoryLayout::kAscending: {
        root = LayoutAscending(nodes);
        break;
      }
      case MemoryLayout::kRandom: {
        root = LayoutAtRandom(nodes, bitgen);
        break;
      }
    }
    CHECK(root != nullptr);

    switch (access_pattern) {
      case AccessPattern::kAscending:
      case AccessPattern::kRandom:
        keys = KeysInOrder(root);
        break;
    }

    constexpr int kUnroll = 32;
    while (keys.size() < kUnroll) {
      keys.reserve(keys.size() * 2);
      keys.insert(keys.end(), keys.begin(), keys.end());
    }
    switch (access_pattern) {
      case AccessPattern::kAscending:
        break;
      case AccessPattern::kRandom:
        std::shuffle(keys.begin(), keys.end(), bitgen);
        break;
    }

    // Ensure that every node has valid pointers.
    for (const Node& node : nodes) {
      if (node.left() != nullptr) {
        DCHECK(std::greater_equal()(node.left(), &nodes.front()));
        DCHECK(std::less_equal()(node.left(), &nodes.back()));
      }
      if (node.right() != nullptr) {
        DCHECK(std::greater_equal()(node.right(), &nodes.front()));
        DCHECK(std::less_equal()(node.right(), &nodes.back()));
      }
    }

    // Ensure that we can format the tree.
    DCHECK(TreeDebugString(root) != "hello world");
  }
};

int NodesForHeight(int height) { return (1U << height) - 1; }

void BM_LowerBound(benchmark::State& state, MemoryLayout layout,
                   AccessPattern access_pattern) {
  TreeProperties expected;
  expected.height = state.range(0);
  expected.size = NodesForHeight(expected.height);
  Fixture fixture(expected.size, layout, access_pattern);
  TreeProperties actual = ComputeTreeProperties(fixture.root);
  if (expected.height != actual.height || expected.size != actual.size) {
    std::ostringstream os;
    os << "tree height or size mismatch; expected " << expected
       << " != actual " << actual << "; " << TreeDebugString(fixture.root);
    state.SkipWithError(os.str().c_str());
    return;
  }

  Node* const root = fixture.root;
  if (false) {
    while (state.KeepRunningBatch(fixture.keys.size())) {
      for (int key : fixture.keys) {
        benchmark::DoNotOptimize(LowerBound(root, key));
      }
    }
  } else {
    const std::size_t kMaxBatchSize = 100000;
    const std::size_t kBatchSize =
        std::min<std::size_t>(fixture.keys.size(), kMaxBatchSize);

    const auto keys_end = fixture.keys.end();
    auto it = fixture.keys.end();
    while (state.KeepRunningBatch(kBatchSize)) {
      if (it == keys_end) {
        it = fixture.keys.begin();
      }
      auto batch_end = it + std::min<std::size_t>(kBatchSize, keys_end - it);
      while (it != batch_end) {
        benchmark::DoNotOptimize(LowerBound(root, *it));
        it++;
      }
    }
  }

  state.counters["height"] =
      benchmark::Counter(static_cast<double>(expected.height));
  state.counters["nodes"] =
      benchmark::Counter(static_cast<double>(expected.size));
  if (false) {
    state.counters["time_per_node_traversed"] =
        benchmark::Counter(static_cast<double>(expected.height),
                           benchmark::Counter::kIsIterationInvariantRate |
                               benchmark::Counter::kInvert);
  }
  state.counters["mem"] = benchmark::Counter(
      static_cast<double>(fixture.WorkingSetBytes()),
      benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

int MaxCacheSize() {
  using benchmark::CPUInfo;

  int max_cache_size = 0;
  for (const CPUInfo::CacheInfo& info : CPUInfo::Get().caches) {
    max_cache_size = std::max(max_cache_size, info.size);
  }
  return max_cache_size;
}

void RegisterAll() {
  // Benchmark up to half the L3 cache size.
  //
  // This used to run benchmarks up to 10x the maximum cache size.  These
  // benchmarks were effectively testing main memory latency more than
  // anything else.  They were also quite succeptible to variance induced
  // by apparent background processes, and seemed to exhibit bimodal
  // behavior in some cases.
  int max_cache_size = MaxCacheSize();
  int target_working_set_size = max_cache_size / 2;

  for (MemoryLayout layout :
       {MemoryLayout::kAscending, MemoryLayout::kRandom}) {
    for (AccessPattern access :
         {AccessPattern::kAscending, AccessPattern::kRandom}) {
      std::ostringstream os;
      os << "LowerBound/" << layout << '/' << access;
      auto* benchmark = benchmark::RegisterBenchmark(
          os.str().c_str(), [layout, access](benchmark::State& state) {
            BM_LowerBound(state, layout, access);
          });
      for (int height = 1; height <= 30; ++height) {
        benchmark->Arg(height);
        if (Fixture::EstimateWorkingSetBytes(NodesForHeight(height)) >=
            target_working_set_size) {
          break;
        }
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
  if (benchmark::ReportUnrecognizedArguments(argc, argv))
    return 1;
  lower_bound::RegisterAll();
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
