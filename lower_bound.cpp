#include "lower_bound.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <type_traits>

namespace lower_bound {

#if 1
ATTRIBUTE_NOIPA Node* LowerBound(Node* x, int key) {
  Node* lower = nullptr;
  while (x != nullptr) {
    bool less = !(x->key < key);
#if 1
    lower = less ? x : lower;
    x = x->links[!less];
#else
    if (less) {
      lower = x;
      x = x->left();
    } else {
      x = x->right();
    }
#endif
  }
  return lower;
}
#else
template <typename Integer>
Integer Select(bool condition, Integer a, Integer b) {
  using Unsigned = typename std::make_unsigned<Integer>::type;
  Unsigned mask = condition ? static_cast<Unsigned>(-1) : 0;
  return (mask & a) | (~mask & b);
}

template <typename T>
T* Select(bool condition, T* a, T* b) {
  auto au = reinterpret_cast<std::uintptr_t>(a);
  auto bu = reinterpret_cast<std::uintptr_t>(b);
  auto ru = Select(condition, au, bu);
  return reinterpret_cast<T*>(ru);
}

// LowerBound returns the first node in
// the tree rooted at "x" whose key is
// not less than "key", or null if there
// is no such key.
Node* LowerBound(Node* x, int key) {
  Node* lower = nullptr;
  while (x != nullptr) {
    bool cond = !(x->key < key);
    lower = Select(cond, x, lower);
    x = Select(cond, x->left, x->right);
  }
  return lower;
}
#endif

}  // namespace lower_bound
