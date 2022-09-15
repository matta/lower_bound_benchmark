#include "lower_bound_test.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "lower_bound.h"

namespace {

std::vector<int> KeysInLayoutOrder(std::span<const lower_bound::Node> nodes) {
  std::vector<int> keys;
  keys.reserve(nodes.size());
  for (const auto& node : nodes) {
    keys.push_back(node.key);
  }
  return keys;
}

TEST(LowerBound, HeightForCount) {
  using lower_bound::HeightForCount;

  EXPECT_EQ(HeightForCount(0), 0);
  EXPECT_EQ(HeightForCount(1), 1);
  EXPECT_EQ(HeightForCount(2), 2);
  EXPECT_EQ(HeightForCount(3), 2);
  EXPECT_EQ(HeightForCount(4), 3);
  EXPECT_EQ(HeightForCount(5), 3);
  EXPECT_EQ(HeightForCount(6), 3);
  EXPECT_EQ(HeightForCount(7), 3);
  EXPECT_EQ(HeightForCount(8), 4);
}

TEST(LowerBound, BasicAssertions) {
  using lower_bound::LowerBound;
  using lower_bound::Node;

  EXPECT_EQ(LowerBound(nullptr, 42), nullptr);

  Node root;
  root.key = 50;
  EXPECT_EQ(LowerBound(&root, 49), &root);
  EXPECT_EQ(LowerBound(&root, 50), &root);
  EXPECT_EQ(LowerBound(&root, 51), nullptr);

  Node left;
  left.key = 50;
  root.left = &left;
  EXPECT_EQ(LowerBound(&root, 49), &left);
  EXPECT_EQ(LowerBound(&root, 50), &left);
  EXPECT_EQ(LowerBound(&root, 51), nullptr);

  Node right;
  right.key = 60;
  root.right = &right;
  EXPECT_EQ(LowerBound(&root, 49), &left);
  EXPECT_EQ(LowerBound(&root, 50), &left);
  EXPECT_EQ(LowerBound(&root, 51), &right);
  EXPECT_EQ(LowerBound(&root, 60), &right);
  EXPECT_EQ(LowerBound(&root, 61), nullptr);

  left.key = 10;
  EXPECT_EQ(LowerBound(&root, 9), &left);
  EXPECT_EQ(LowerBound(&root, 10), &left);
  EXPECT_EQ(LowerBound(&root, 11), &root);
}

TEST(LowerBound, LayoutAscending) {
  using lower_bound::HeightForCount;
  using lower_bound::Node;
  using testing::ElementsAre;

  std::vector<Node> nodes(7);
  EXPECT_EQ(HeightForCount(nodes.size()), 3);

  Node* root = LayoutAscending(nodes);
  std::vector<int> in_order_keys = KeysInOrder(root);
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
  EXPECT_EQ(root->key, 4);
  EXPECT_EQ(nodes.front().key, 1);
  EXPECT_EQ(nodes.back().key, 7);
  EXPECT_THAT(in_order_keys, ElementsAre(1, 2, 3, 4, 5, 6, 7));
  EXPECT_THAT(layout_keys, ElementsAre(1, 2, 3, 4, 5, 6, 7));
}

TEST(LowerBound, LayoutAtRandom) {
  using lower_bound::HeightForCount;
  using lower_bound::Node;
  using testing::ContainerEq;
  using testing::ElementsAre;
  using testing::Not;

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
    EXPECT_EQ(HeightForCount(nodes.size()), 3);

    Node* root = LayoutAtRandom(nodes, seed);
    std::vector<int> in_order_keys = KeysInOrder(root);
    EXPECT_THAT(in_order_keys, ElementsAre(1, 2, 3, 4, 5, 6, 7));

    layouts.push_back(KeysInLayoutOrder(nodes));
    for (int i = 0; i < layouts.size(); ++i) {
      for (int j = i + 1; j < layouts.size(); ++j) {
        EXPECT_THAT(layouts[i], Not(ContainerEq(layouts[j])))
            << "; when i=" << i << " j=" << j;
      }
    }

    ++seed;
  }
}

}  // namespace
