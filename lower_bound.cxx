// Benchmark for a "lower bound" search on a binary tree.
#define DOCTEST_CONFIG_IMPLEMENT

#include <functional>
#include <iostream>
#include <random>
#include <span>
#include <sstream>

#include "benchmark/benchmark.h"
#include "doctest/doctest.h"

// ATTRIBUTE_NOIPA disables inline, interprocedural optimizations, etc.
// When compiling with gcc this macro expands to the "noipa" attribute.  On
// clang, which does not support the attribute, it expands to the
// "noinline" attribute.
#ifndef __GNUC__
#error "gcc or clang required"
#endif
#ifdef __clang__
#define ATTRIBUTE_NOIPA __attribute__((noinline))
#else
#define ATTRIBUTE_NOIPA __attribute__((noipa))
#endif

namespace lower_bound {

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

}  // namespace lower_bound

namespace doctest {

// Specialize doctest::StringMaker to std::span<const T> nicely.  See
// https://github.com/doctest/doctest/issues/170
template <::lower_bound::OutputStreamable T>
struct StringMaker<std::span<const T>> {
  static doctest::String convert(std::span<const T> in) {
    std::string str = ::lower_bound::ContainerDebugString(in);
    return doctest::String(str.c_str(), str.size());
  }
};

// Specialize doctest::StringMaker to print std::vector<T> nicely.  See
// https://github.com/doctest/doctest/issues/170
template <::lower_bound::OutputStreamable T>
struct StringMaker<std::vector<T>> {
  static doctest::String convert(const std::vector<T>& in) {
    return StringMaker<std::span<const T>>::convert(in);
  }
};

}  // namespace doctest

namespace lower_bound {
namespace {

// Node is a binary tree node.  It has the usual left and right links and
// an integral key.
struct Node {
  Node* left = nullptr;
  Node* right = nullptr;
  int key = 0;
};

// LowerBound returns the first node in the tree rooted at "x" whose key is
// not less than "key", or null if there is no such key.
//
// Another way to phrase the same specification: LowerBound returns the
// first node in the tree rooted at "x" whose key is greater than or equal
// to "key".
//
// A key insight is that this algorithm returns the leftmost key in the
// face of duplicates, so the search must always proceed to a leaf of the
// tree.
ATTRIBUTE_NOIPA Node* LowerBound(Node* x, int key) {
  Node* lower = nullptr;
  while (x != nullptr) {
    if (!(x->key < key)) {
      lower = x;
      x = x->left;
    } else {
      x = x->right;
    }
  }
  return lower;
}

TEST_CASE("LowerBound") {
  CHECK(LowerBound(nullptr, 42) == nullptr);

  Node root;
  root.key = 50;
  CHECK(LowerBound(&root, 49) == &root);
  CHECK(LowerBound(&root, 50) == &root);
  CHECK(LowerBound(&root, 51) == nullptr);

  Node left;
  left.key = 50;
  root.left = &left;
  CHECK(LowerBound(&root, 49) == &left);
  CHECK(LowerBound(&root, 50) == &left);
  CHECK(LowerBound(&root, 51) == nullptr);

  Node right;
  right.key = 60;
  root.right = &right;
  CHECK(LowerBound(&root, 49) == &left);
  CHECK(LowerBound(&root, 50) == &left);
  CHECK(LowerBound(&root, 51) == &right);
  CHECK(LowerBound(&root, 60) == &right);
  CHECK(LowerBound(&root, 61) == nullptr);

  left.key = 10;
  CHECK(LowerBound(&root, 9) == &left);
  CHECK(LowerBound(&root, 10) == &left);
  CHECK(LowerBound(&root, 11) == &root);
}

// Tree holds a few key properties of a tree.  Its height, and its size in
// number of nodes.
//
// The height h(X) of a tree rooted at X is defined recursively as zero if
// X is null and 1+max{h(x.left), h(y.left)} otherwise.  In other words, it
// is the maximum number of nodes one may traverse before reaching a left
// node (i.e. null).
//
// The size is simply the total number of nodes in the tree.
struct TreeProperties {
  int height = 0;
  int size = 0;
};

std::ostream& operator<<(std::ostream& os, TreeProperties& properties) {
  return os << "height:" << properties.height << " size:" << properties.size;
}

// Return the TreeProperties (height and size) of the given tree.  This
// function also verifies that the nodes are in proper symmetric order with
// respect to their key falues and will abort otherwise.
TreeProperties ComputeTreeProperties(Node* node, const int* minimum = nullptr,
                                     const int* maximum = nullptr) {
  if (node == nullptr) {
    return TreeProperties();
  }

  // Here a null "minimum" or "maximum" represents a negative or positive
  // infinite bound.  When non null, "*minimum" holds an inclusive lower
  // bound and "*maximum" an inclusive upper bound.  This allows the tree
  // to hold duplicate key values.
  bool error = false;
  if (minimum != nullptr && maximum != nullptr && *minimum == *maximum) {
    std::cerr
        << "Node " << node->key
        << " has parents that constrained its key space to the empty range\n";
  } else if (minimum != nullptr && !(*minimum <= node->key)) {
    std::cerr
        << "Node " << node->key
        << " is less than the minimum allowable key implied by its parents\n";
    error = true;
  } else if (maximum != nullptr && !(node->key <= *maximum)) {
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

// TreeDebugString returns a representation of the tree rooted at "node" as
// a string.
std::string TreeDebugString(Node* node) {
  std::ostringstream os;
  os << '(';
  if (node != nullptr) {
    os << node->key;
    if (!(node->left == nullptr && node->right == nullptr)) {
      os << ' ' << TreeDebugString(node->left) << ' '
         << TreeDebugString(node->right);
    }
  } else {
    os << "nil";
  }
  os << ')';
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

template <std::integral T>
int HeightForCount(T node_count) {
  int height = 0;
  while (node_count > 0) {
    ++height;
    node_count /= 2;
  }
  return height;
}

TEST_CASE("HeightForCount") {
  CHECK(HeightForCount(0) == 0);
  CHECK(HeightForCount(1) == 1);
  CHECK(HeightForCount(2) == 2);
  CHECK(HeightForCount(3) == 2);
  CHECK(HeightForCount(4) == 3);
  CHECK(HeightForCount(5) == 3);
  CHECK(HeightForCount(6) == 3);
  CHECK(HeightForCount(7) == 3);
  CHECK(HeightForCount(8) == 4);
}

Node* LayoutInKeyOrderRecur(int& key, std::span<Node>::iterator& next,
                            std::span<Node>::iterator end,
                            int maximum_height) {
  if (next == end || maximum_height == 0) {
    return nullptr;
  }
  Node* left = LayoutInKeyOrderRecur(key, next, end, maximum_height - 1);
  Node* node = &*next++;
  node->left = left;
  node->key = key++;
  node->right = LayoutInKeyOrderRecur(key, next, end, maximum_height - 1);
  return node;
}

Node* LayoutInKeyOrder(std::span<Node> nodes) {
  int maximum_height = HeightForCount(nodes.size());
  int key = 1;
  std::span<Node>::iterator next = nodes.begin();
  return LayoutInKeyOrderRecur(key, next, nodes.end(), maximum_height);
}

Node* LayoutByNodeLevelRecur(int& next_key, std::vector<int>& offsets,
                             int depth, std::span<Node> nodes) {
  if (depth == offsets.size()) {
    return nullptr;
  }

  Node* left = LayoutByNodeLevelRecur(next_key, offsets, depth + 1, nodes);

  int offset = offsets[depth]++;
  if (offset >= nodes.size()) {
    std::abort();
  }

  Node& node = nodes[offset];
  node.key = next_key++;
  node.left = left;
  node.right = LayoutByNodeLevelRecur(next_key, offsets, depth + 1, nodes);
  return &node;
}

std::vector<int> OffsetsForHeight(int height) {
  std::vector<int> offsets;
  for (int depth = 0; depth < height; ++depth) {
    offsets.push_back((1 << depth) - 1);
  }
  return offsets;
}

std::vector<int> OffsetsForNodeCount(int node_count) {
  const int maximum_height = HeightForCount(node_count);
  return OffsetsForHeight(maximum_height);
}

Node* LayoutByNodeLevel(std::span<Node> nodes) {
  std::vector<int> offsets = OffsetsForNodeCount(nodes.size());
  int next_key = 1;
  return LayoutByNodeLevelRecur(next_key, offsets, /* depth= */ 0, nodes);
}

Node* LayoutAtRandomRecur(int maximum_height, std::span<int> mapping,
                          std::span<Node> nodes, int& next_key,
                          int& next_index) {
  if (maximum_height == 0 || next_index == mapping.size()) {
    return nullptr;
  }

  Node& node = nodes[mapping[next_index++]];
  node.left = LayoutAtRandomRecur(maximum_height - 1, mapping, nodes, next_key,
                                  next_index);
  node.key = next_key++;
  node.right = LayoutAtRandomRecur(maximum_height - 1, mapping, nodes,
                                   next_key, next_index);
  return &node;
}

Node* LayoutAtRandom(std::span<Node> nodes, int seed) {
  const int maximum_height = HeightForCount(nodes.size());

  std::vector<int> mapping;
  for (int i = 0; i < nodes.size(); ++i) {
    mapping.push_back(i);
  }
  std::shuffle(mapping.begin(), mapping.end(), std::minstd_rand0(seed));

  int next_key = 1;
  int next_index = 0;
  return LayoutAtRandomRecur(maximum_height, mapping, nodes, next_key,
                             next_index);
}

void VisitInOrder(const Node* node,
                  const std::function<void(const Node*)>& visit) {
  if (node == nullptr) return;
  VisitInOrder(node->left, visit);
  visit(node);
  VisitInOrder(node->right, visit);
}

std::vector<int> KeysInSymmetricOrder(const Node* root) {
  std::vector<int> keys;
  VisitInOrder(root, [&](const Node* node) { keys.push_back(node->key); });
  return keys;
}

std::vector<int> KeysInLayoutOrder(std::span<const Node> nodes) {
  std::vector<int> keys;
  keys.reserve(nodes.size());
  for (const Node& node : nodes) {
    keys.push_back(node.key);
  }
  return keys;
}

void KeysByLevelRecur(Node* node, int depth, std::span<const int> ordered_keys,
                      std::span<int> offsets,
                      std::vector<int>& keys_by_level) {
  if (node == nullptr) {
    return;
  }
  assert(depth < offsets.size());
  int offset = offsets[depth]++;
  assert(offset < keys_by_level.size());
  keys_by_level[offset] = node->key;
  KeysByLevelRecur(node->left, depth + 1, ordered_keys, offsets,
                   keys_by_level);
  KeysByLevelRecur(node->right, depth + 1, ordered_keys, offsets,
                   keys_by_level);
}

std::vector<int> KeysByLevel(Node* root) {
  const std::vector<int> ordered_keys = KeysInSymmetricOrder(root);
  TreeProperties properties = ComputeTreeProperties(root);
  std::vector<int> offsets = OffsetsForHeight(properties.height);
  std::vector<int> keys_by_level;
  keys_by_level.resize(ordered_keys.size());
  KeysByLevelRecur(root, /* depth= */ 0, ordered_keys, offsets, keys_by_level);
  return keys_by_level;
}

TEST_CASE("KeysByLevel") {
  std::vector<Node> nodes(7);
  CHECK(HeightForCount(nodes.size()) == 3);

  Node* root = LayoutInKeyOrder(nodes);

  CHECK(KeysByLevel(root) == std::vector<int>{4, 2, 6, 1, 3, 5, 7});
}

TEST_CASE("LayoutInKeyOrder") {
  std::vector<Node> nodes(7);
  CHECK(HeightForCount(nodes.size()) == 3);

  Node* root = LayoutInKeyOrder(nodes);
  std::vector<int> in_order_keys = KeysInSymmetricOrder(root);
  std::vector<int> layout_keys = KeysInLayoutOrder(nodes);

  // A complete tree of 7 nodes has this structure:
  //
  //             4
  //           /   \
  //          2     5
  //         / \   / \
  //        1   3 6   7
  //
  // The in ordered layout of this tree will place the nodes in their
  // symmetrical order:
  //
  //   1 2 3 4 5 6 7
  //
  CHECK(root->key == 4);
  CHECK(nodes.front().key == 1);
  CHECK(nodes.back().key == 7);
  CHECK(in_order_keys == std::vector<int>{1, 2, 3, 4, 5, 6, 7});
  CHECK(layout_keys == std::vector<int>{1, 2, 3, 4, 5, 6, 7});
}

TEST_CASE("LayoutByNodeLevel") {
  std::vector<Node> nodes(7);
  CHECK(HeightForCount(nodes.size()) == 3);

  Node* root = LayoutByNodeLevel(nodes);
  std::vector<int> in_order_keys = KeysInSymmetricOrder(root);
  std::vector<int> layout_keys = KeysInLayoutOrder(nodes);

  // A complete tree of 7 nodes has this structure:
  //
  //             4
  //           /   \
  //          2     5
  //         / \   / \
  //        1   3 6   7
  //
  // The level ordered layout of this tree will place
  // the nodes in the following order:
  //
  //   4 2 6 1 3 5 7
  //
  CHECK(root->key == 4);
  CHECK(root == &nodes.front());
  CHECK(nodes.back().key == 7);
  CHECK(in_order_keys == std::vector<int>{1, 2, 3, 4, 5, 6, 7});
  CHECK(layout_keys == std::vector<int>{4, 2, 6, 1, 3, 5, 7});
}

TEST_CASE("LayoutAtRandom") {
  // A complete tree of 7 nodes has this structure:
  //
  //             4
  //           /   \
  //          2     5
  //         / \   / \
  //        1   3 6   7
  //
  // A random layout of this tree will place the nodes in arbitrary order
  // within the node array.  Test this by generating multiple permutations,
  // using different seeds, and ensure they are all different.
  std::vector<std::vector<int>> layouts;
  for (int seed = 17; layouts.size() != 32; seed += 17) {
    std::vector<Node> nodes(7);
    REQUIRE(HeightForCount(nodes.size()) == 3);

    Node* root = LayoutAtRandom(nodes, seed);
    std::vector<int> in_order_keys = KeysInSymmetricOrder(root);
    REQUIRE(in_order_keys == std::vector<int>{1, 2, 3, 4, 5, 6, 7});

    layouts.push_back(KeysInLayoutOrder(nodes));
    for (int i = 0; i < layouts.size(); ++i) {
      CAPTURE(i);
      for (int j = i + 1; j < layouts.size(); ++j) {
        CAPTURE(j);
        REQUIRE(layouts[i] != layouts[j]);
      }
    }

    ++seed;
  }
}

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
  {
    // Run all doctest tests before Google Benchmark code runs.  This
    // always occurs; no command line configuration is possible.
    doctest::Context context;
    int res = context.run();
    if (res != 0 || context.shouldExit()) {
      return res;
    }
  }

  // Run the Google Benchmark code.  Most of this is standard boilerplate
  // except for the call to lower_bound::RegisterAll().
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
  lower_bound::RegisterAll();
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
