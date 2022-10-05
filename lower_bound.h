#ifndef LOWER_BOUND_H
#define LOWER_BOUND_H

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

// Node is a binary tree node.  It has the usual left and right links and
// an integral key.
struct Node {
  int key = 0;
  Node* links[2] = {nullptr, nullptr};

  using NodePtr = Node*;
  NodePtr& left() { return links[0]; }
  NodePtr& right() { return links[1]; }
  NodePtr left() const { return links[0]; }
  NodePtr right() const { return links[1]; }
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
ATTRIBUTE_NOIPA Node* LowerBound(Node* x, int key);

}  // namespace lower_bound

#endif
