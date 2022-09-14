#include "lower_bound.hxx"

namespace lower_bound {

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

}  // namespace lower_bound
