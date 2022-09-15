#ifndef LOWER_BOUND_TEST_H
#define LOWER_BOUND_TEST_H

#include <cassert>
#include <concepts>
#include <functional>
#include <iostream>
#include <ostream>
#include <random>
#include <span>
#include <sstream>
#include <vector>

#include "absl/random/bit_gen_ref.h"
#include "lower_bound.h"

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

inline std::vector<int> KeysInOrder(const Node* root) {
  std::vector<int> keys;
  VisitInOrder(root, [&](const Node* node) { keys.push_back(node->key); });
  return keys;
}

inline Node* LayoutAscendingRecur(int& key, std::span<Node>::iterator& next,
                                  std::span<Node>::iterator end,
                                  int maximum_height) {
  if (next == end || maximum_height == 0) {
    return nullptr;
  }
  Node* left = LayoutAscendingRecur(key, next, end, maximum_height - 1);
  Node* node = &*next++;
  node->left = left;
  node->key = key++;
  node->right = LayoutAscendingRecur(key, next, end, maximum_height - 1);
  return node;
}

inline Node* LayoutAscending(std::span<Node> nodes) {
  int maximum_height = HeightForCount(nodes.size());
  int key = 1;
  std::span<Node>::iterator next = nodes.begin();
  return LayoutAscendingRecur(key, next, nodes.end(), maximum_height);
}

// See LayoutAtRandom.
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

// LayoutAtRandom populates 'nodes' with a complete binary tree with keys
// ascending from 1 to the node count.  Return the trees root.
//
// The location of nodes within the tree are chosen with uniform
// randomness.
inline Node* LayoutAtRandom(std::span<Node> nodes, absl::BitGenRef bitgen) {
  const int maximum_height = HeightForCount(nodes.size());

  std::vector<int> mapping;
  for (int i = 0; i < nodes.size(); ++i) {
    mapping.push_back(i);
  }
  std::shuffle(mapping.begin(), mapping.end(), bitgen);

  int next_key = 1;
  int next_index = 0;
  return LayoutAtRandomRecur(maximum_height, mapping, nodes, next_key,
                             next_index);
}

}  // namespace lower_bound

#endif
