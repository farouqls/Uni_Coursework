// ParcelBST.hpp   -- TASK 1: Self-balancing Binary Search Tree (AVL), keyed by Parcel ID
//
// WHY A BST: the parcel registry is the "hub" every other module looks parcels up in.
// A BST gives search/insert/remove in O(log n) and an in-order walk that lists parcels
// sorted by ID for free.
//
// WHY BALANCED: Parcel IDs are issued in order (PCL001, PCL002, ...). In a plain BST
// that makes every insert go to the right, and the tree collapses into a linked list
// (height n, O(n) search). The AVL rule fixes this: after every insert/remove, the
// heights of a node's two subtrees may differ by at most 1; if they differ by 2, a
// rotation restores the balance. Height therefore stays about log2(n).
#ifndef PARCELBST_HPP
#define PARCELBST_HPP

#include <cstring>
#include "Types.hpp"

struct ParcelNode {
    Parcel      data;
    ParcelNode* left;
    ParcelNode* right;
    int         height;      // height of the subtree rooted here (a leaf has height 1)
    explicit ParcelNode(const Parcel& p) : data(p), left(nullptr), right(nullptr), height(1) {}
};

class ParcelBST {
    ParcelNode* root;
    int         count;

    // ---------- AVL helpers ----------
    static int heightOf(const ParcelNode* n) { return n == nullptr ? 0 : n->height; }

    static void updateHeight(ParcelNode* n) {
        int l = heightOf(n->left), r = heightOf(n->right);
        n->height = 1 + (l > r ? l : r);
    }

    static int balanceOf(const ParcelNode* n) { return heightOf(n->left) - heightOf(n->right); }

    // Right rotation (left side too tall): the left child x becomes the new top.
    //     y  becomes  x      x = y.left,  y.left = x.right,  x.right = y
    // The tree stays in sorted order because B (x.right) lies between x and y.
    static ParcelNode* rotateRight(ParcelNode* y) {
        ParcelNode* x = y->left;
        y->left = x->right;
        x->right = y;
        updateHeight(y);
        updateHeight(x);
        return x;
    }

    static ParcelNode* rotateLeft(ParcelNode* x) {      // mirror image: right side too tall
        ParcelNode* y = x->right;
        x->right = y->left;
        y->left = x;
        updateHeight(x);
        updateHeight(y);
        return y;
    }

    // Restores the AVL rule at one node (4 cases: LL, LR, RR, RL).
    static ParcelNode* rebalance(ParcelNode* n) {
        updateHeight(n);
        int balance = balanceOf(n);
        if (balance > 1) {                                       // left-heavy
            if (balanceOf(n->left) < 0) n->left = rotateLeft(n->left);   // left-right case
            return rotateRight(n);
        }
        if (balance < -1) {                                      // right-heavy
            if (balanceOf(n->right) > 0) n->right = rotateRight(n->right); // right-left case
            return rotateLeft(n);
        }
        return n;
    }

    // ---------- insert / remove ----------
    static ParcelNode* insertNode(ParcelNode* node, const Parcel& p, bool& inserted) {
        if (node == nullptr) { inserted = true; return new ParcelNode(p); }
        int cmp = std::strcmp(p.id, node->data.id);
        if (cmp < 0)      node->left  = insertNode(node->left,  p, inserted);
        else if (cmp > 0) node->right = insertNode(node->right, p, inserted);
        else { inserted = false; return node; }                 // duplicate key rejected
        return rebalance(node);
    }

    static ParcelNode* minNode(ParcelNode* node) {
        while (node->left != nullptr) node = node->left;
        return node;
    }

    // Three cases: leaf, one child, two children (replace with in-order successor).
    static ParcelNode* removeNode(ParcelNode* node, const char* id, bool& removed) {
        if (node == nullptr) return nullptr;
        int cmp = std::strcmp(id, node->data.id);
        if (cmp < 0)      node->left  = removeNode(node->left,  id, removed);
        else if (cmp > 0) node->right = removeNode(node->right, id, removed);
        else {
            removed = true;
            if (node->left == nullptr)  { ParcelNode* r = node->right; delete node; return r; }
            if (node->right == nullptr) { ParcelNode* l = node->left;  delete node; return l; }
            ParcelNode* successor = minNode(node->right);
            node->data = successor->data;
            bool dummy = false;
            node->right = removeNode(node->right, node->data.id, dummy);
        }
        return rebalance(node);
    }

    static void walk(const ParcelNode* node, void (*visit)(const Parcel&, void*), void* user) {
        if (node == nullptr) return;
        walk(node->left, visit, user);
        visit(node->data, user);
        walk(node->right, visit, user);
    }

    static void destroy(ParcelNode* node) {
        if (node == nullptr) return;
        destroy(node->left);
        destroy(node->right);
        delete node;
    }

    // Verifies ordering-independent AVL invariants (used by the self-test).
    static bool checkNode(const ParcelNode* node, int& heightOut) {
        if (node == nullptr) { heightOut = 0; return true; }
        int lh = 0, rh = 0;
        if (!checkNode(node->left, lh) || !checkNode(node->right, rh)) return false;
        int diff = lh - rh;
        if (diff > 1 || diff < -1) return false;
        heightOut = 1 + (lh > rh ? lh : rh);
        return node->height == heightOut;
    }

public:
    ParcelBST() : root(nullptr), count(0) {}
    ParcelBST(const ParcelBST&) = delete;
    ParcelBST& operator=(const ParcelBST&) = delete;
    ~ParcelBST() { destroy(root); }

    bool insert(const Parcel& p) {
        bool inserted = false;
        root = insertNode(root, p, inserted);
        if (inserted) count++;
        return inserted;
    }

    bool remove(const char* id) {
        bool removed = false;
        root = removeNode(root, id, removed);
        if (removed) count--;
        return removed;
    }

    // Returns a pointer to the stored record so callers can update it in place.
    Parcel* find(const char* id) const {
        ParcelNode* node = root;
        while (node != nullptr) {
            int cmp = std::strcmp(id, node->data.id);
            if (cmp == 0) return &node->data;
            node = (cmp < 0) ? node->left : node->right;
        }
        return nullptr;
    }

    void inorder(void (*visit)(const Parcel&, void*), void* user) const { walk(root, visit, user); }
    int  size() const    { return count; }
    bool isEmpty() const { return count == 0; }
    int  height() const  { return heightOf(root); }
    bool isBalanced() const { int h = 0; return checkNode(root, h); }
};

#endif
