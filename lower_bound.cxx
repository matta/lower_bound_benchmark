#include <random>
#include <queue>

#include "benchmark/benchmark.h"

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

// Insert function definition.
Node* Insert(Node* root, Node* node) {
    if (!root) {
        return node;
    }
    if (node->key > root->key) {
        root->right = Insert(root->right, node);
    } else {
        root->left = Insert(root->left, node);
    }
    return root;
}


struct Fixture {
    std::vector<Node> nodes;
    std::vector<int> keys;
    Node* root;

    ATTRIBUTE_NOIPA Fixture(int key_count) {
        std::minstd_rand0 bitgen(42);
        std::uniform_int_distribution<> distribution(1, 100000);

        nodes.resize(key_count);
        keys.reserve(nodes.size());
        std::queue<std::pair<int, int>> queue;
        queue.push({1,key_count});
        root = nullptr;
        for (Node& node : nodes) {
          assert(!queue.empty());
          int low = queue.front().first;
          int high = queue.front().second;
          queue.pop();
          int mid = (low + high) / 2;

          if (low <= mid-1) {
            queue.push({low, mid-1});
          }
          if (mid+1 <= high) {
            queue.push({mid + 1, high});
          }

          node.key = mid;
          keys.push_back(node.key);
          root = Insert(root, &node);
        }
        assert(keys.size() == key_count);
        assert(queue.empty());

        std::shuffle(keys.begin(), keys.end(), bitgen);
    }
    ATTRIBUTE_NOIPA ~Fixture() {}
};

void BM_LowerBound(benchmark::State& state) {
    Fixture fixture(state.range(0));

    Node* root = fixture.root;
    while (state.KeepRunningBatch(fixture.keys.size())) {
        for (long key : fixture.keys) {
            benchmark::DoNotOptimize(LowerBound(root, key));
        }
    }
}
BENCHMARK(BM_LowerBound)->Range(8,1<<20);
