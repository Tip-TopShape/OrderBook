//
// Created by Eleazar Vega on 2/21/26.
//

#ifndef RBTREE_H
#define RBTREE_H

#include "priceLevel.h"
#include <memory>
#include <unistd.h>
#include <mach/mach_types.h>

template <typename T, typename CT>
class RBTree {
    size_t _size = 0;
public:

    RBTree() {}

    enum class Color {
        BLACK = 0,
        RED = 1
    };

    enum class Direction {
        LEFT = 0,
        RIGHT = 1
    };


    struct Node {
        Node *parent;
        std::unique_ptr<CT> level;
        T price; // order price that you represent
        union {
            struct {
                Node *left = nullptr;
                Node *right = nullptr;
            };
            Node *children[2];
        };
        Color color;

        Node() : parent(nullptr), left(nullptr), right(nullptr), color(Color::BLACK) {}
        Node(T p) : price(p), parent(nullptr), left(nullptr), right(nullptr),
                    color(Color::RED), level(std::make_unique<CT>()) {}
    }__attribute__((aligned(64)));


    Node *headIndex = nullptr;
    Node *minNode = nullptr;  // leftmost node (lowest price)
    Node *maxNode = nullptr;  // rightmost node (highest price)

    Node* find(const T &price, Node *curr, Node* parent) {
        if (!curr) {
            auto newLevel = new Node(price);
            if (!parent) {
                // Empty tree - this is the root
                headIndex = newLevel;
                newLevel->color = Color::BLACK;
                newLevel->parent = nullptr;
                minNode = newLevel;
                maxNode = newLevel;
                ++_size;
            } else {
                Direction dir = price < parent->price ? Direction::LEFT : Direction::RIGHT;
                insertIndexNode(newLevel, parent, dir);
            }
            return newLevel;
        }
        if (curr->price == price) {
            return curr;
        }
        if (price < curr->price) {
            return find(price, curr->left, curr);
        }
        return find(price, curr->right, curr);
    }

    bool empty() const {
        return headIndex == nullptr;
    }

    size_t size() const {
        return _size;
    }

    Node* rotateSubtree(Node *sub, Direction dir) {
        // Node* sub_parent = sub->parent;
        // Node* new_root = sub->child[1 - dir]; // 1 - dir is the opposite direction
        // Node* new_child = new_root->child[dir]
        auto subParent = sub->parent;
        Node *newRoot = sub->children[1-static_cast<int>(dir)];
        Node *newChild = newRoot->children[static_cast<int>(dir)];

        sub->children[1-static_cast<int>(dir)] = newChild;
        newRoot->children[static_cast<int>(dir)] = sub;

        if (newChild)
            newChild->parent = sub;

        newRoot->parent = subParent;
        sub->parent = newRoot;
        if (subParent) {
            if (sub == subParent->right) {
                subParent->right = newRoot;
            } else {
                subParent->left = newRoot;
            }
        } else {
            headIndex = newRoot;
        }

        return newRoot;
    }

    Direction direction(Node* node) {
        return node->parent->left == node ? Direction::LEFT : Direction::RIGHT;
    }

    void remove(Node *node) {
        Node* parent = node->parent;

        Node* sibling;
        Node* close_nephew;
        Node* distant_nephew;

        Direction dir = direction(node);

        parent->children[static_cast<int>(dir)] = nullptr;
        goto start_balance;

        do {
            dir = direction(node);
        start_balance:
            sibling = parent->children[1-static_cast<int>(dir)];
            distant_nephew = sibling->children[1-static_cast<int>(dir)];
            close_nephew = sibling->children[static_cast<int>(dir)];
            if (sibling->color == Color::RED) {
                // case_3
                rotateSubtree(parent, dir);
                parent->color = Color::RED;
                sibling->color = Color::BLACK;
                sibling = close_nephew;

                distant_nephew = sibling->children[1-static_cast<int>(dir)];
                if (distant_nephew && distant_nephew->color == Color::RED) {
                    goto case_6;
                }
                close_nephew = sibling->children[static_cast<int>(dir)];
                if (close_nephew && close_nephew->color == Color::RED) {
                    goto case_5;
                }

                // case_4
                sibling->color = Color::RED;
                parent->color = Color::BLACK;
                return;
            }

            if (distant_nephew && distant_nephew->color == Color::RED) {
                goto case_6;
            }

            if (close_nephew && close_nephew->color == Color::RED) {
                goto case_5;
            }

            if (!parent) {
                // case_1
                return;
            }

            if (parent->color == Color::RED) {
                // case_4
                sibling->color = Color::RED;
                parent->color = Color::BLACK;
                return;
            }

            // case_2
            sibling->color = Color::RED;
            node = parent;

        } while ((parent = node->parent));

        case_5:
            rotateSubtree(sibling, static_cast<Direction>(1-static_cast<int>(dir)));
            sibling->color = Color::RED;
            close_nephew->color = Color::BLACK;
            distant_nephew = sibling;
            sibling =close_nephew;

        case_6:
            rotateSubtree(parent, dir);
            sibling->color = parent->color;
            parent->color = Color::BLACK;
            distant_nephew->color = Color::BLACK;
            return;

    }

    void erase(Node* node) {
        --_size;

        if (node->left && node->right) {
            // Find in-order successor
            Node* successor = node->right;
            while (successor->left)
                successor = successor->left;

            // Copy data up, delete successor instead
            node->price = successor->price;
            node->level = std::move(successor->level);
            node = successor; // now node has at most one child
        }

        // og node or successor
        if (node == minNode)
            minNode = node->parent; // min has no left child, successor is parent or right
        if (node == maxNode)
            maxNode = node->parent;

        Node* child = node->left ? node->left : node->right;
        if (child) {
            child->parent = node->parent;
            if (node->parent) {
                // parent is not root
                auto dir = direction(node);
                node->parent->children[static_cast<int>(dir)] = child;
            } else {
                // parent is root
                headIndex = child;
            }

            child->color = Color::BLACK;
        }

        else if (!node->parent) {
            headIndex = nullptr;
        } else {
            if (node->color == Color::RED) {
                auto dir = direction(node);
                node->parent->children[static_cast<int>(dir)] = nullptr;
            } else {
                remove(node);
            }
        }

        // delete node
        delete node;
    }

    void insertIndexNode(Node *node, Node *parent, Direction dir) {
        ++_size;
        node->color = Color::RED;
        node->parent = parent;
        parent->children[static_cast<int>(dir)] = node;

        // Update min/max pointers
        if (dir == Direction::LEFT && parent == minNode) {
            minNode = node;
        }
        if (dir == Direction::RIGHT && parent == maxNode) {
            maxNode = node;
        }

        // Fix Red-Black Tree properties
        do {

            if (parent->color == Color::BLACK) {
                break;
            }

            auto grandparent = parent->parent;
            if (!grandparent) {
                parent->color = Color::BLACK;
                break;
            }
            Direction parentDir = (parent == grandparent->left) ? Direction::LEFT : Direction::RIGHT;
            Node *uncle = grandparent->children[1 - static_cast<int>(parentDir)];
            if (!uncle || uncle->color == Color::BLACK) {
                if (node == parent->children[1 - static_cast<int>(parentDir)]) {
                    rotateSubtree(parent, parentDir);
                    std::swap(node, parent);
                }
                rotateSubtree(grandparent, static_cast<Direction>(1 - static_cast<int>(parentDir)));
                parent->color = Color::BLACK;
                grandparent->color = Color::RED;
            } else {
                parent->color = Color::BLACK;
                uncle->color = Color::BLACK;
                grandparent->color = Color::RED;
            }

            parent->color = Color::BLACK;
            parent->color = Color::BLACK;
            grandparent->color = Color::RED;
            node = grandparent;
        } while ((parent = node->parent));
    }

    void insert(Node *entry) {

    }

    Node* findByprice(const T &price) {
        return find(price, headIndex, nullptr);
    }

    Node* getMin() const {
        if (!headIndex) return nullptr;
        if (!minNode) {
            auto curr = headIndex;
            while (curr->left != nullptr)
                curr = curr->left;
            return curr;
        }
        return minNode;
    }

    Node* getMax() const {
        if (!headIndex) return nullptr;
        if (!maxNode) {
            auto curr = headIndex;
            while (curr->right != nullptr)
                curr = curr->right;
            return curr;
        }
        return maxNode;
    }

    Node* front() const {

    }
};

#endif //RBTREE_H
