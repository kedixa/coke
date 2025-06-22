/**
 * Copyright 2025 Coke Project (https://github.com/kedixa/coke)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Authors: kedixa (https://github.com/kedixa)
 */

#include "coke/utils/rbtree.h"

namespace coke {

RBTreeNode *rbtree_next(RBTreeNode *node) noexcept
{
    if (node->rb_right) {
        node = node->rb_right;
        while (node->rb_left)
            node = node->rb_left;

        return node;
    }

    RBTreeNode *parent = node->rb_parent;
    while (node == parent->rb_right) {
        node   = parent;
        parent = node->rb_parent;
    }

    return (node->rb_right == parent) ? node : parent;
}

RBTreeNode *rbtree_prev(RBTreeNode *node) noexcept
{
    // node is end, prev is rightmost
    if (node->rb_color == RB_RED && node->rb_parent->rb_parent == node)
        return node->rb_right;

    if (node->rb_left) {
        node = node->rb_left;
        while (node->rb_right)
            node = node->rb_right;

        return node;
    }

    RBTreeNode *parent = node->rb_parent;
    while (node == parent->rb_left) {
        node   = parent;
        parent = node->rb_parent;
    }

    return parent;
}

const RBTreeNode *rbtree_next(const RBTreeNode *node) noexcept
{
    return rbtree_next(const_cast<RBTreeNode *>(node));
}

const RBTreeNode *rbtree_prev(const RBTreeNode *node) noexcept
{
    return rbtree_prev(const_cast<RBTreeNode *>(node));
}

void rbtree_clear(RBTreeNode *node) noexcept
{
    while (node) {
        RBTreeNode *next = node->rb_right;
        rbtree_clear(node->rb_left);

        node->rb_parent = nullptr;
        node->rb_left   = nullptr;
        node->rb_right  = nullptr;
        node->rb_color  = RB_BLACK;
        node            = next;
    }
}

void rbtree_insert(RBTreeNode *head, struct rb_root *root, RBTreeNode *parent,
                   RBTreeNode **link, RBTreeNode *node) noexcept
{
    // unlink from head
    if (root->rb_node)
        root->rb_node->rb_parent = nullptr;

    rb_link_node(node, parent, link);

    if (parent) {
        if (head->rb_left == parent && parent->rb_left)
            head->rb_left = parent->rb_left;

        if (head->rb_right == parent && parent->rb_right)
            head->rb_right = parent->rb_right;
    }
    else {
        head->rb_left  = node;
        head->rb_right = node;
    }

    rb_insert_color(node, root);

    // relink to head
    root->rb_node->rb_parent = head;
    head->rb_parent          = root->rb_node;
}

} // namespace coke
