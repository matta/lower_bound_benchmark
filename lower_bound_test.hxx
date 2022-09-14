#ifndef LOWER_BOUND_TEST_HXX
#define LOWER_BOUND_TEST_HXX

#include <cassert>
#include <concepts>
#include <functional>
#include <iostream>
#include <ostream>
#include <random>
#include <span>
#include <sstream>
#include <vector>

#include "lower_bound.hxx"

namespace lower_bound {

template <std::integral T>
int HeightForCount(T node_count) {
  int height = 0;
  while (node_count > 0) {
    ++height;
    node_count /= 2;
  }
  return height;
}

inline void VisitInOrder(const Node* node,
                         const std::function<void(const Node*)>& visit) {
  if (node == nullptr) return;
  VisitInOrder(node->left, visit);
  visit(node);
  VisitInOrder(node->right, visit);
}

inline std::vector<int> OffsetsForHeight(int height) {
  std::vector<int> offsets;
  for (int depth = 0; depth < height; ++depth) {
    offsets.push_back((1 << depth) - 1);
  }
  return offsets;
}

inline std::vector<int> OffsetsForNodeCount(int node_count) {
  const int maximum_height = HeightForCount(node_count);
  return OffsetsForHeight(maximum_height);
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

inline std::ostream& operator<<(std::ostream& os, TreeProperties& properties) {
  return os << "height:" << properties.height << " size:" << properties.size;
}

// Return the TreeProperties (height and size) of the given tree.  This
// function also verifies that the nodes are in proper symmetric order with
// respect to their key falues and will abort otherwise.
inline TreeProperties ComputeTreeProperties(Node* node,
                                            const int* minimum = nullptr,
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
inline std::string TreeDebugString(Node* node) {
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

inline std::vector<int> KeysInSymmetricOrder(const Node* root) {
  std::vector<int> keys;
  VisitInOrder(root, [&](const Node* node) { keys.push_back(node->key); });
  return keys;
}

inline void KeysByLevelRecur(Node* node, int depth,
                             std::span<const int> ordered_keys,
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

inline std::vector<int> KeysByLevel(Node* root) {
  const std::vector<int> ordered_keys = KeysInSymmetricOrder(root);
  TreeProperties properties = ComputeTreeProperties(root);
  std::vector<int> offsets = OffsetsForHeight(properties.height);
  std::vector<int> keys_by_level;
  keys_by_level.resize(ordered_keys.size());
  KeysByLevelRecur(root, /* depth= */ 0, ordered_keys, offsets, keys_by_level);
  return keys_by_level;
}

inline Node* LayoutInKeyOrderRecur(int& key, std::span<Node>::iterator& next,
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

inline Node* LayoutInKeyOrder(std::span<Node> nodes) {
  int maximum_height = HeightForCount(nodes.size());
  int key = 1;
  std::span<Node>::iterator next = nodes.begin();
  return LayoutInKeyOrderRecur(key, next, nodes.end(), maximum_height);
}

inline Node* LayoutByNodeLevelRecur(int& next_key, std::vector<int>& offsets,
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

inline Node* LayoutByNodeLevel(std::span<Node> nodes) {
  std::vector<int> offsets = OffsetsForNodeCount(nodes.size());
  int next_key = 1;
  return LayoutByNodeLevelRecur(next_key, offsets, /* depth= */ 0, nodes);
}

inline Node* LayoutAtRandomRecur(int maximum_height, std::span<int> mapping,
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

inline Node* LayoutAtRandom(std::span<Node> nodes, int seed) {
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

inline std::vector<int> KeysInLayoutOrder(std::span<const Node> nodes) {
  std::vector<int> keys;
  keys.reserve(nodes.size());
  for (const Node& node : nodes) {
    keys.push_back(node.key);
  }
  return keys;
}

}  // namespace lower_bound

#endif
