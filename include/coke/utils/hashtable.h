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

#ifndef COKE_UTILS_HASHTABLE_H
#define COKE_UTILS_HASHTABLE_H

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <vector>

namespace coke {

struct HashtableNode {
    HashtableNode *next;
    HashtableNode *prev;
    std::size_t hash_code;
    std::size_t node_index;
};

template<typename Hash, typename T>
concept Hashable = requires(const Hash &h, const T &t) {
    { h(t) } -> std::convertible_to<std::size_t>;
};

template<typename Equal, typename T, typename U>
concept EqualComparable = requires(const Equal &eq, const T &t, const U &u) {
    { eq(t, u) } -> std::convertible_to<bool>;
};

template<typename Hash, typename Equal, typename T, typename U>
concept HashEqualComparable = Hashable<Hash, T> && Hashable<Hash, U> &&
                              EqualComparable<Equal, T, U>;

namespace detail {

class HashtableBase {
public:
    using size_type = std::size_t;
    using Node = HashtableNode;

    HashtableBase() noexcept;

    HashtableBase(const HashtableBase &) = delete;
    HashtableBase &operator=(const HashtableBase &) = delete;

    HashtableBase(HashtableBase &&other) noexcept;
    HashtableBase &operator=(HashtableBase &&other) noexcept;

    /**
     * @brief Check if the hashtable is empty.
     * @return true if the hashtable is empty, false otherwise.
     */
    bool empty() const noexcept { return nodes.empty(); }

    /**
     * @brief Get the number of elements in the hashtable.
     * @return The number of elements in the hashtable.
     */
    size_type size() const noexcept { return nodes.size(); }

    /**
     * @brief Get the number of buckets in the hashtable.
     * @return The number of buckets in the hashtable.
     */
    size_type bucket_count() const noexcept { return table.size(); }

    /**
     * @brief Get the number of elements in the specified bucket.
     * @param bkt_index The index of the bucket.
     * @return The number of elements in the specified bucket.
     * @pre bkt_index < bucket_count()
     */
    size_type bucket_size(size_type bkt_index) const;

    /**
     * @brief Get the current load factor of the hashtable.
     * @return The current load factor of the hashtable.
     */
    double load_factor() const noexcept;

    /**
     * @brief Get the maximum load factor of the hashtable.
     * @return The maximum load factor of the hashtable.
     */
    double max_load_factor() const noexcept { return max_factor; }

    /**
     * @brief Set the maximum load factor of the hashtable.
     * @param m The new maximum load factor.
     */
    void max_load_factor(double m) noexcept;

    /**
     * @brief Reserve space for at least the specified number of elements.
     * @param cap The number of elements to reserve space for.
     */
    void reserve(size_type cap)
    {
        if (cap > next_resize)
            reserve_impl(cap);
    }

    /**
     * @brief Clear the hashtable.
     * @attention This does not destroy the elements in the hashtable.
     */
    void clear() noexcept { reinit(); }

protected:
    size_type get_bucket_index(size_type hash_code) const noexcept
    {
        return hash_code % table.size();
    }

    void insert_at(Node *node, size_type bkt_index, Node *pos) noexcept;

    void insert_front(Node *node, size_type bkt_index) noexcept;

    void erase_node(Node *node) noexcept;

    Node *find_by_hash(size_type hash_code, size_type bkt_index) noexcept
    {
        const HashtableBase *const_this = this;
        const Node *node = const_this->find_by_hash(hash_code, bkt_index);
        return const_cast<Node *>(node);
    }

    const Node *find_by_hash(size_type hash_code,
                             size_type bkt_index) const noexcept;

    const Node *get_end_node() const noexcept { return &head; }

    bool need_expand(size_type cnt) const noexcept
    {
        if (next_resize > nodes.size())
            return next_resize - nodes.size() < cnt;
        else
            return true;
    }

    void expand(size_type cnt);

    void reserve_impl(size_type cap);

    Node *node_at(size_type idx) { return nodes.at(idx); }

    const Node *node_at(size_type idx) const { return nodes.at(idx); }

private:
    void reinit() noexcept
    {
        head.next = &head;
        head.prev = &head;
        head.hash_code = 0;
        head.node_index = 0;
        table.clear();
        nodes.clear();
        next_resize = 0;
    }

protected:
    Node head;
    std::vector<Node *> table;
    std::vector<Node *> nodes;

    double max_factor;
    size_type next_resize;
};

template<typename T, HashtableNode T::*Member>
struct HashtableTraits {
    static auto get_offset() noexcept
    {
        const T *p = nullptr;
        const HashtableNode *m = &(p->*Member);
        return reinterpret_cast<uintptr_t>(m) - reinterpret_cast<uintptr_t>(p);
    }

    static const HashtableNode *to_node_ptr(const T *ptr) noexcept
    {
        return &(ptr->*Member);
    }

    static HashtableNode *to_node_ptr(T *ptr) noexcept
    {
        return &(ptr->*Member);
    }

    static const T *to_pointer(const HashtableNode *node) noexcept
    {
        return (T *)((const char *)node - get_offset());
    }

    static T *to_pointer(HashtableNode *node) noexcept
    {
        return (T *)((const char *)node - get_offset());
    }
};

template<typename T, HashtableNode T::*Member>
struct HashtableIterator {
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T *;
    using reference = T &;

    using Node = HashtableNode;
    using Self = HashtableIterator;
    using Traits = HashtableTraits<T, Member>;

    HashtableIterator() noexcept = default;

    explicit HashtableIterator(Node *node) noexcept : node_ptr(node) {}

    ~HashtableIterator() = default;

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

template<typename T, HashtableNode T::*Member>
struct HashtableConstIterator {
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = const T *;
    using reference = const T &;

    using Node = HashtableNode;
    using Self = HashtableConstIterator;
    using Traits = HashtableTraits<T, Member>;
    using Iterator = HashtableIterator<T, Member>;

    HashtableConstIterator() = default;

    explicit HashtableConstIterator(const Node *node) noexcept : node_ptr(node)
    {
    }

    HashtableConstIterator(const Iterator &other) noexcept
        : node_ptr(other.node_ptr)
    {
    }

    ~HashtableConstIterator() = default;

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

template<typename Key, HashtableNode Key::*Member,
         typename Hash = std::hash<Key>, typename Equal = std::equal_to<>>
    requires HashEqualComparable<Hash, Equal, Key, Key>
class Hashtable : public detail::HashtableBase {
public:
    using iterator = detail::HashtableIterator<Key, Member>;
    using const_iterator = detail::HashtableConstIterator<Key, Member>;
    using pointer = Key *;
    using const_pointer = const Key *;
    using size_type = detail::HashtableBase::size_type;
    using hasher = Hash;
    using key_equal = Equal;

    using Node = detail::HashtableBase::Node;
    using Traits = detail::HashtableTraits<Key, Member>;

    /**
     * @brief Constructs an empty hashtable.
     */
    explicit Hashtable(const hasher &hash = hasher(),
                       const key_equal &equal = key_equal())
        : detail::HashtableBase(), hash(hash), equal(equal)
    {
    }

    /**
     * @brief Hashtable is not copyable.
     */
    Hashtable(const Hashtable &) = delete;
    Hashtable &operator=(const Hashtable &) = delete;

    /**
     * @brief Move constructor and move assignment operator.
     */
    Hashtable(Hashtable &&other) noexcept = default;
    Hashtable &operator=(Hashtable &&other) noexcept = default;

    ~Hashtable() = default;

    iterator begin() noexcept { return iterator(head.next); }
    const_iterator begin() const noexcept { return const_iterator(head.next); }
    const_iterator cbegin() const noexcept { return const_iterator(head.next); }

    iterator end() noexcept { return iterator(&head); }
    const_iterator end() const noexcept { return const_iterator(&head); }
    const_iterator cend() const noexcept { return const_iterator(&head); }

    /**
     * @brief Inserts an element to the hashtable.
     * @param ptr Pointer to the element to be inserted.
     * @return An iterator to the inserted element.
     */
    iterator insert(pointer ptr)
    {
        if (need_expand(1))
            expand(1);

        Node *pos;
        size_type bkt_index;
        const Node *node_end = get_end_node();
        Node *node = Traits::to_node_ptr(ptr);

        node->hash_code = hash(*ptr);
        bkt_index = get_bucket_index(node->hash_code);
        pos = find_by_hash(node->hash_code, bkt_index);

        while (pos != node_end && pos->hash_code == node->hash_code) {
            if (equal(*Traits::to_pointer(pos), *ptr))
                break;

            pos = pos->next;
        }

        if (pos == node_end)
            insert_front(node, bkt_index);
        else
            insert_at(node, bkt_index, pos);

        return iterator(node);
    }

    /**
     * @brief Erases an element from the hashtable.
     * @param pos Iterator pointing to the element to be erased.
     * @return An iterator pointing to the element following the erased element.
     */
    iterator erase(const_iterator pos) noexcept
    {
        return erase(pos.remove_const().get_pointer());
    }

    /**
     * @brief Erases an element from the hashtable.
     * @param ptr Pointer to the element to be erased.
     * @return An iterator pointing to the element following the erased element.
     */
    iterator erase(pointer ptr) noexcept
    {
        Node *node = Traits::to_node_ptr(ptr);
        Node *next = node->next;
        erase_node(node);
        return iterator(next);
    }

    /**
     * @brief Checks if the hashtable contains an element with the given key.
     * @param key The key to be searched for.
     * @return true if the hashtable contains an element with the given key,
     *         false otherwise.
     */
    template<typename K>
        requires Hashable<Hash, K> && EqualComparable<Equal, Key, K>
    bool contains(const K &key) const
    {
        return find(key) != cend();
    }

    /**
     * @brief Finds an element with the given key.
     * @param key The key to be searched for.
     * @return An iterator to the element if found, end() otherwise.
     */
    template<typename K>
        requires Hashable<Hash, K> && EqualComparable<Equal, Key, K>
    iterator find(const K &key)
    {
        const Hashtable *const_this = this;
        return const_this->find(key).remove_const();
    }

    /**
     * @brief Finds an element with the given key.
     * @param key The key to be searched for.
     * @return An iterator to the element if found, cend() otherwise.
     */
    template<typename K>
        requires Hashable<Hash, K> && EqualComparable<Equal, Key, K>
    const_iterator find(const K &key) const
    {
        if (!empty()) {
            size_type hash_code = hash(key);
            size_type bkt_index = get_bucket_index(hash_code);
            const Node *node = find_by_hash(hash_code, bkt_index);
            const Node *end_node = get_end_node();

            while (node != end_node && node->hash_code == hash_code) {
                if (equal(*Traits::to_pointer(node), key))
                    return const_iterator(node);

                node = node->next;
            }
        }

        return cend();
    }

    iterator element_at(size_type idx) { return iterator(node_at(idx)); }

    const_iterator element_at(size_type idx) const
    {
        return const_iterator(node_at(idx));
    }

private:
    [[no_unique_address]] hasher hash;
    [[no_unique_address]] key_equal equal;
};

} // namespace coke

#endif // COKE_UTILS_HASHTABLE_H
