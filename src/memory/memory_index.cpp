#include "memory/memory_index.h"

#include <algorithm>
#include <cstdint>
#include <utility>

// frees every node in the subtree. does left and right first then deletes the node (post order)
void MemoryIndex::destroy(Node* node) {
    if (node == nullptr) {
        return;
    }

    destroy(node->left);
    destroy(node->right);

    delete node;
}

// height is counted in nodes so a empty tree is 0
std::size_t MemoryIndex::height_of(const Node* node) {
    if (node == nullptr) {
        return 0;
    }

    std::size_t left_height = height_of(node->left);
    std::size_t right_height = height_of(node->right);

    return 1 + std::max(left_height, right_height);
}

// in order traversal so it visits the ids from smallest to biggest
void MemoryIndex::in_order(
    const Node* node,
    const std::function<void(const MemoryRecord&)>& fn) {

    if (node == nullptr) {
        return;
    }

    in_order(node->left, fn);
    fn(node->rec);
    in_order(node->right, fn);
}

// destructor just calls clear so all the nodes get freed
MemoryIndex::~MemoryIndex() {
    clear();
}

// insert with a loop instead of recursion, returns false if the id is already their
bool MemoryIndex::insert(MemoryRecord rec) {

    if (root_ == nullptr) {
        root_ = new Node{rec};
        ++count_;
        return true;
    }

    Node* current = root_;

    while (current != nullptr) {

        if (rec.id == current->rec.id) {
            return false;
        }

        if (rec.id < current->rec.id) {

            if (current->left == nullptr) {
                current->left = new Node{rec};
                ++count_;
                return true;
            }

            current = current->left;

        } else {

            if (current->right == nullptr) {
                current->right = new Node{rec};
                ++count_;
                return true;
            }

            current = current->right;
        }
    }

    return false;
}

// walk down the tree going left or right depending on the id
const MemoryRecord* MemoryIndex::find(
    std::uint64_t id) const {

    const Node* current = root_;

    while (current != nullptr) {

        if (id == current->rec.id) {
            return &current->rec;
        }

        if (id < current->rec.id) {
            current = current->left;
        } else {
            current = current->right;
        }
    }

    return nullptr;
}

// removes the node with this id and keeps the tree in order
bool MemoryIndex::remove(std::uint64_t id) {

    Node** target = &root_;

    while (*target != nullptr) {

        if ((*target)->rec.id == id) {
            break;
        }

        if (id < (*target)->rec.id) {
            target = &(*target)->left;
        } else {
            target = &(*target)->right;
        }
    }

    if (*target == nullptr) {
        return false;
    }

    Node* removed = *target;

    // case 1 and 2: no children or only one child, just link the child up to the parent
    if (removed->left == nullptr ||
        removed->right == nullptr) {

        Node* child = nullptr;

        if (removed->left != nullptr) {
            child = removed->left;
        } else {
            child = removed->right;
        }

        *target = child;
        delete removed;
    }

    // case 3: two children
    else {

        // find the predecessor (biggest node in the left subtree)
        Node** predecessor_link = &removed->left;

        while ((*predecessor_link)->right != nullptr) {
            predecessor_link = &(*predecessor_link)->right;
        }

        Node* predecessor = *predecessor_link;

        // copy the predecessors record into this node
        removed->rec = std::move(predecessor->rec);

        // predecessor cant have a right child so its left subtree just moves up into its spot
        *predecessor_link = predecessor->left;

        delete predecessor;
    }

    --count_;
    return true;
}

// number of records in the tree
std::size_t MemoryIndex::size() const noexcept {
    return count_;
}

// longest path from the root to a leaf
std::size_t MemoryIndex::height() const {
    return height_of(root_);
}

// calls fn on every record in sorted order
void MemoryIndex::for_each_in_order(
    const std::function<void(const MemoryRecord&)>& fn) const {

    in_order(root_, fn);
}

// delete the whole tree and reset root and count
void MemoryIndex::clear() {

    destroy(root_);

    root_ = nullptr;
    count_ = 0;
}
