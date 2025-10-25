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

#ifndef COKE_UTILS_LIST_H
#define COKE_UTILS_LIST_H

#include <cstdint>
#include <iterator>
#include <utility>

namespace coke {

struct ListNode {
    ListNode *next;
    ListNode *prev;
};

namespace detail {

template<typename T, ListNode T::*Member>
struct ListTraits {
    static auto get_offset() noexcept
    {
        const T *p = nullptr;
        const ListNode *m = &(p->*Member);
        return reinterpret_cast<uintptr_t>(m) - reinterpret_cast<uintptr_t>(p);
    }

    static const ListNode *to_node_ptr(const T *ptr) noexcept
    {
        return &(ptr->*Member);
    }

    static ListNode *to_node_ptr(T *ptr) noexcept { return &(ptr->*Member); }

    static const T *to_pointer(const ListNode *node) noexcept
    {
        return (T *)((const char *)node - get_offset());
    }

    static T *to_pointer(ListNode *node) noexcept
    {
        return (T *)((const char *)node - get_offset());
    }
};

template<typename T, ListNode T::*Member>
struct ListIterator {
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T *;
    using reference = T &;

    using Node = ListNode;
    using Self = ListIterator;
    using Traits = ListTraits<T, Member>;

    ListIterator() noexcept = default;

    explicit ListIterator(Node *node) noexcept : node_ptr(node) {}

    ~ListIterator() = default;

    reference operator*() noexcept { return *get_pointer(); }

    pointer operator->() noexcept { return get_pointer(); }

    Self &operator++() noexcept
    {
        node_ptr = node_ptr->next;
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
        node_ptr = node_ptr->prev;
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

    pointer get_pointer() noexcept { return Traits::to_pointer(node_ptr); }

    // Only for inner use
    Node *node_ptr;
};

template<typename T, ListNode T::*Member>
struct ListConstIterator {
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = const T *;
    using reference = const T &;

    using Node = ListNode;
    using Self = ListConstIterator;
    using Traits = ListTraits<T, Member>;
    using Iterator = ListIterator<T, Member>;

    ListConstIterator() = default;

    explicit ListConstIterator(const Node *node) noexcept : node_ptr(node) {}

    ListConstIterator(const Iterator &other) noexcept : node_ptr(other.node_ptr)
    {
    }

    ~ListConstIterator() = default;

    // Only for inner use
    Iterator remove_const() const noexcept
    {
        return Iterator(const_cast<Node *>(node_ptr));
    }

    reference operator*() const noexcept { return *get_pointer(); }

    pointer operator->() const noexcept { return get_pointer(); }

    Self &operator++() noexcept
    {
        node_ptr = node_ptr->next;
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
        node_ptr = node_ptr->prev;
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

    // Only for inner use
    const Node *node_ptr;
};

} // namespace detail

/**
 * @brief Instrusive doubly linked list.
 *
 * This class only manages the linkage between nodes, the lifetime of nodes
 * is managed by the user.
 */
template<typename T, ListNode T::*Member>
class List {
public:
    using iterator = detail::ListIterator<T, Member>;
    using const_iterator = detail::ListConstIterator<T, Member>;
    using pointer = T *;
    using const_pointer = const T *;
    using size_type = std::size_t;

    using Node = ListNode;
    using Traits = detail::ListTraits<T, Member>;

    /**
     * @brief Create an empty list.
     */
    List() noexcept { reinit(); }

    /**
     * @brief Move constructor.
     */
    List(List &&other) noexcept : head(other.head), list_size(other.list_size)
    {
        other.reinit();
    }

    /**
     * @brief Move assignment.
     */
    List &operator=(List &&other) noexcept
    {
        if (this != &other) {
            head = other.head;
            list_size = other.list_size;
            other.reinit();
        }

        return *this;
    }

    /**
     * @brief List is not copyable.
     */
    List(const List &other) = delete;
    List &operator=(const List &other) = delete;

    ~List() = default;

    pointer front() noexcept { return Traits::to_pointer(head.next); }
    const_pointer front() const noexcept
    {
        return Traits::to_pointer(head.next);
    }

    pointer back() noexcept { return Traits::to_pointer(head.prev); }
    const_pointer back() const noexcept
    {
        return Traits::to_pointer(head.prev);
    }

    iterator begin() noexcept { return iterator(head.next); }
    const_iterator begin() const noexcept { return const_iterator(head.next); }
    const_iterator cbegin() const noexcept { return const_iterator(head.next); }

    iterator end() noexcept { return iterator(&head); }
    const_iterator end() const noexcept { return const_iterator(&head); }
    const_iterator cend() const noexcept { return const_iterator(&head); }

    bool empty() const noexcept { return size() == 0; }
    size_type size() const noexcept { return list_size; }

    /**
     * @brief Clear the list.
     * @attention This will not delete the elements in the list.
     */
    void clear() noexcept { reinit(); }

    void push_back(pointer ptr) noexcept { insert(end(), ptr); }

    pointer pop_back() noexcept
    {
        pointer ptr = back();
        erase(ptr);
        return ptr;
    }

    void push_front(pointer ptr) noexcept { insert(begin(), ptr); }

    pointer pop_front() noexcept
    {
        pointer ptr = front();
        erase(ptr);
        return ptr;
    }

    /**
     * @brief Erase the element at pos.
     * @param pos Iterator pointing to the element to be erased.
     * @return Iterator pointing to the next element (or end()).
     */
    iterator erase(const_iterator pos) noexcept
    {
        Node *node = pos.remove_const().node_ptr;
        Node *prev = node->prev;
        Node *next = node->next;

        prev->next = next;
        next->prev = prev;
        --list_size;

        return iterator(next);
    }

    /**
     * @brief Erase the element pointed by ptr.
     * @param ptr Pointer to the element to be erased.
     * @return Iterator pointing to the next element (or end()).
     */
    iterator erase(pointer ptr) noexcept
    {
        Node *node = Traits::to_node_ptr(ptr);
        return erase(const_iterator(node));
    }

    /**
     * @brief Insert element before pos.
     * @param pos Iterator pointing to the position to insert before.
     * @param ptr Pointer to the element to be inserted.
     * @return Iterator pointing to the inserted element.
     */
    iterator insert(const_iterator pos, pointer ptr) noexcept
    {
        Node *node = Traits::to_node_ptr(ptr);
        Node *next = pos.remove_const().node_ptr;
        Node *prev = next->prev;

        node->prev = prev;
        node->next = next;
        prev->next = node;
        next->prev = node;
        ++list_size;

        return iterator(node);
    }

    /**
     * @brief Swap with another list.
     * @param other Another list to swap with.
     */
    void swap(List &other) noexcept
    {
        if (this != &other) {
            std::swap(head, other.head);
            std::swap(list_size, other.list_size);
        }
    }

private:
    void reinit() noexcept
    {
        head.next = &head;
        head.prev = &head;
        list_size = 0;
    }

private:
    Node head;
    size_type list_size;
};

} // namespace coke

#endif // COKE_UTILS_LIST_H
