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

#ifndef COKE_UTILS_RBTREE_H
#define COKE_UTILS_RBTREE_H

#include <concepts>
#include <cstdint>
#include <iterator>

#include "workflow/rbtree.h"

namespace coke {

using RBTreeNode = struct rb_node;

template<typename R, typename T, typename U>
concept RBComparable = std::predicate<R, T, U> && std::predicate<R, U, T>;

RBTreeNode *rbtree_next(RBTreeNode *node) noexcept;
RBTreeNode *rbtree_prev(RBTreeNode *node) noexcept;
const RBTreeNode *rbtree_next(const RBTreeNode *node) noexcept;
const RBTreeNode *rbtree_prev(const RBTreeNode *node) noexcept;

void rbtree_clear(RBTreeNode *node) noexcept;

void rbtree_insert(RBTreeNode *head, struct rb_root *root, RBTreeNode *parent,
                   RBTreeNode **link, RBTreeNode *node) noexcept;

template<typename T, RBTreeNode T::*Member>
struct RBTreeTraits {
    static auto get_offset() noexcept
    {
        const T *p = nullptr;
        const RBTreeNode *m = &(p->*Member);
        return reinterpret_cast<uintptr_t>(m) - reinterpret_cast<uintptr_t>(p);
    }

    static const RBTreeNode *to_node_ptr(const T *ptr) noexcept
    {
        return &(ptr->*Member);
    }

    static RBTreeNode *to_node_ptr(T *ptr) noexcept { return &(ptr->*Member); }

    static T *to_pointer(RBTreeNode *node) noexcept
    {
        return (T *)((const char *)node - get_offset());
    }

    static const T *to_pointer(const RBTreeNode *node) noexcept
    {
        return (const T *)((const char *)node - get_offset());
    }

    static const T &to_reference(const RBTreeNode *node) noexcept
    {
        return *to_pointer(node);
    }
};

template<typename T, RBTreeNode T::*Member>
struct RBTreeIterator {
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T *;
    using reference = T &;
    using const_pointer = const T *;
    using const_reference = const T &;

    using Node = RBTreeNode;
    using Traits = RBTreeTraits<T, Member>;
    using Self = RBTreeIterator;

    RBTreeIterator() noexcept = default;

    explicit RBTreeIterator(Node *node) noexcept : node_ptr(node) {}

    ~RBTreeIterator() noexcept = default;

    reference operator*() const noexcept { return *get_pointer(); }

    pointer operator->() const noexcept { return get_pointer(); }

    Self &operator++() noexcept
    {
        node_ptr = rbtree_next(node_ptr);
        return *this;
    }

    Self operator++(int) noexcept
    {
        Self tmp = *this;
        ++*this;
        return tmp;
    }

    Self &operator--() noexcept
    {
        node_ptr = rbtree_prev(node_ptr);
        return *this;
    }

    Self operator--(int) noexcept
    {
        Self tmp = *this;
        --*this;
        return tmp;
    }

    friend bool operator==(const Self &lhs, const Self &rhs) noexcept
    {
        return lhs.node_ptr == rhs.node_ptr;
    }

    pointer get_pointer() const noexcept
    {
        return Traits::to_pointer(node_ptr);
    }

    const_pointer get_const_pointer() const noexcept
    {
        return Traits::to_pointer(node_ptr);
    }

    // Only for inner use
    Node *node_ptr;
};

template<typename T, RBTreeNode T::*Member>
struct RBTreeConstIterator {
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = const T *;
    using reference = const T &;
    using const_pointer = const T *;
    using const_reference = const T &;

    using Node = RBTreeNode;
    using Traits = RBTreeTraits<T, Member>;
    using Self = RBTreeConstIterator;
    using Iterator = RBTreeIterator<T, Member>;

    RBTreeConstIterator() noexcept = default;

    explicit RBTreeConstIterator(const Node *node) noexcept : node_ptr(node) {}

    RBTreeConstIterator(const Iterator &other) noexcept
        : node_ptr(other.node_ptr)
    {
    }

    ~RBTreeConstIterator() noexcept = default;

    Iterator remove_const() const noexcept
    {
        return Iterator(const_cast<Node *>(node_ptr));
    }

    reference operator*() const noexcept { return *get_pointer(); }

    pointer operator->() const noexcept { return get_pointer(); }

    Self &operator++() noexcept
    {
        node_ptr = rbtree_next(node_ptr);
        return *this;
    }

    Self operator++(int) noexcept
    {
        Self tmp = *this;
        ++*this;
        return tmp;
    }

    Self &operator--() noexcept
    {
        node_ptr = rbtree_prev(node_ptr);
        return *this;
    }

    Self operator--(int) noexcept
    {
        Self tmp = *this;
        --*this;
        return tmp;
    }

    friend bool operator==(const Self &lhs, const Self &rhs) noexcept
    {
        return lhs.node_ptr == rhs.node_ptr;
    }

    pointer get_pointer() const noexcept
    {
        return Traits::to_pointer(node_ptr);
    }

    const_pointer get_const_pointer() const noexcept
    {
        return Traits::to_pointer(node_ptr);
    }

    // Only for inner use
    const Node *node_ptr;
};

template<typename T, RBTreeNode T::*Member, typename Cmp>
    requires std::predicate<Cmp, T, T>
class RBTree {
public:
    using iterator = RBTreeIterator<T, Member>;
    using const_iterator = RBTreeConstIterator<T, Member>;
    using value_type = T;
    using key_compare = Cmp;
    using size_type = std::size_t;
    using pointer = T *;
    using const_pointer = const T *;

    using Node = RBTreeNode;
    using Traits = RBTreeTraits<T, Member>;

    RBTree() noexcept { reinit(); }

    explicit RBTree(const key_compare &cmp) noexcept : cmp(cmp) { reinit(); }

    RBTree(RBTree &&other) noexcept
        : head(other.head), root(other.root), tree_size(other.tree_size),
          cmp(other.cmp)
    {
        other.reinit();
    }

    RBTree(const RBTree &other) = delete;
    RBTree &operator=(const RBTree &other) = delete;

    ~RBTree() noexcept { clear(); }

    iterator begin() noexcept { return iterator(head.rb_left); }
    const_iterator begin() const noexcept { return cbegin(); }
    const_iterator cbegin() const noexcept
    {
        return const_iterator(head.rb_left);
    }

    iterator end() noexcept { return iterator(&head); }
    const_iterator end() const noexcept { return const_iterator(&head); }
    const_iterator cend() const noexcept { return const_iterator(&head); }

    pointer front() noexcept { return Traits::to_pointer(head.rb_left); }
    const_pointer front() const noexcept
    {
        return Traits::to_pointer(head.rb_left);
    }

    pointer back() noexcept { return Traits::to_pointer(head.rb_right); }
    const_pointer back() const noexcept
    {
        return Traits::to_pointer(head.rb_right);
    }

    bool empty() const noexcept { return size() == 0; }
    size_type size() const noexcept { return tree_size; }

    void clear_unsafe() noexcept { reinit(); }

    void clear() noexcept
    {
        rbtree_clear(head.rb_parent);
        reinit();
    }

    iterator insert(const_iterator pos, T *ptr) noexcept
    {
        Node *cur = pos.remove_const().node_ptr;
        Node **link;
        Node *parent;

        if (cur == &head) {
            if (cur->rb_parent) {
                parent = head.rb_right;
                link = &parent->rb_right;
            }
            else {
                parent = nullptr;
                link = &root.rb_node;
            }
        }
        else if (cur->rb_left) {
            cur = cur->rb_left;
            while (cur->rb_right)
                cur = cur->rb_right;

            parent = cur;
            link = &cur->rb_right;
        }
        else {
            parent = cur;
            link = &cur->rb_left;
        }

        Node *node_ptr = Traits::to_node_ptr(ptr);
        rbtree_insert(&head, &root, parent, link, node_ptr);

        ++tree_size;
        return iterator(node_ptr);
    }

    iterator insert(T *ptr) noexcept
    {
        Node **link = &root.rb_node;
        Node *parent = nullptr;

        while (*link) {
            parent = *link;

            if (cmp(*ptr, Traits::to_reference(parent)))
                link = &(*link)->rb_left;
            else
                link = &(*link)->rb_right;
        }

        Node *node_ptr = Traits::to_node_ptr(ptr);
        rbtree_insert(&head, &root, parent, link, node_ptr);

        ++tree_size;
        return iterator(node_ptr);
    }

    void erase(const_iterator pos) noexcept
    {
        erase(pos.remove_const().get_pointer());
    }

    void erase(T *ptr) noexcept
    {
        Node *cur = Traits::to_node_ptr(ptr);

        if (size() > 1) {
            if (head.rb_left == cur)
                head.rb_left = rbtree_next(cur);

            if (head.rb_right == cur)
                head.rb_right = rbtree_prev(cur);
        }
        else {
            head.rb_left = &head;
            head.rb_right = &head;
        }

        root.rb_node->rb_parent = nullptr;
        rb_erase(cur, &root);

        head.rb_parent = root.rb_node;
        if (root.rb_node)
            root.rb_node->rb_parent = &head;

        --tree_size;
    }

    template<typename K>
        requires RBComparable<key_compare, T, K>
    iterator find(const K &key) noexcept
    {
        const RBTree *const_this = this;
        return const_this->find(key).remove_const();
    }

    template<typename K>
        requires RBComparable<key_compare, T, K>
    const_iterator find(const K &key) const noexcept
    {
        auto it = lower_bound(key);
        auto end_it = end();

        if (it != end_it && cmp(key, Traits::to_reference(it.node_ptr)))
            return end_it;

        return it;
    }

    template<typename K>
        requires RBComparable<key_compare, T, K>
    bool contains(const K &key) const noexcept
    {
        return find(key) != cend();
    }

    template<typename K>
        requires RBComparable<key_compare, T, K>
    iterator lower_bound(const K &key) noexcept
    {
        const RBTree *const_this = this;
        return const_this->lower_bound(key).remove_const();
    }

    template<typename K>
        requires RBComparable<key_compare, T, K>
    const_iterator lower_bound(const K &key) const noexcept
    {
        const Node *cur = head.rb_parent;
        const Node *pos = &head;

        while (cur) {
            if (!cmp(Traits::to_reference(cur), key)) {
                pos = cur;
                cur = cur->rb_left;
            }
            else {
                cur = cur->rb_right;
            }
        }

        return const_iterator(pos);
    }

    template<typename K>
        requires RBComparable<key_compare, T, K>
    iterator upper_bound(const K &key) noexcept
    {
        const RBTree *const_this = this;
        return const_this->upper_bound(key).remove_const();
    }

    template<typename K>
        requires RBComparable<key_compare, T, K>
    const_iterator upper_bound(const K &key) const noexcept
    {
        const Node *cur = head.rb_parent;
        const Node *pos = &head;

        while (cur) {
            if (cmp(key, Traits::to_reference(cur))) {
                pos = cur;
                cur = cur->rb_left;
            }
            else {
                cur = cur->rb_right;
            }
        }

        return const_iterator(pos);
    }

    key_compare key_comp() const noexcept { return cmp; }

private:
    void reinit() noexcept
    {
        head.rb_left = &head;
        head.rb_right = &head;
        head.rb_parent = nullptr;
        head.rb_color = RB_RED;
        root.rb_node = nullptr;
        tree_size = 0;
    }

private:
    RBTreeNode head;
    struct rb_root root;
    size_type tree_size;
    [[no_unique_address]] key_compare cmp;
};

} // namespace coke

#endif // COKE_UTILS_RBTREE_H
