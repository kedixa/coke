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

#ifndef COKE_LRU_CACHE_H
#define COKE_LRU_CACHE_H

#include <concepts>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>

#include "coke/detail/condition_impl.h"
#include "coke/detail/ref_counted.h"
#include "coke/utils/hashtable.h"
#include "coke/utils/list.h"

namespace coke {

template<typename, typename, typename, typename>
class LruCache;

namespace detail {

enum : uint16_t {
    LRU_WAITING = 0,
    LRU_SUCCESS = 1,
    LRU_FAILED = 2,
};

template<typename K, typename V>
struct LruEntry : public RefCounted<LruEntry<K, V>> {
    using Key = K;
    using Value = V;

    template<typename U>
    LruEntry(uint16_t state, std::mutex *mtx, const U &key)
        : removed(false), state(state), mtx(mtx), key(key)
    {
    }

    bool removed;
    std::atomic<uint16_t> state;
    HashtableNode ht;
    ListNode lst;
    std::mutex *mtx;

    Key key;
    std::optional<Value> value;
};

template<typename K, typename V>
class LruHandle {
public:
    using Entry = LruEntry<K, V>;
    using Key = typename Entry::Key;
    using Value = typename Entry::Value;

    /**
     * @brief Create an empty handle.
     */
    LruHandle() noexcept : entry(nullptr) {}

    /**
     * @brief Create from other handle, they share the same entry.
     */
    LruHandle(const LruHandle &other) noexcept : entry(other.entry)
    {
        if (entry)
            entry->inc_ref();
    }

    /**
     * @brief Move from other handle.
     */
    LruHandle(LruHandle &&other) noexcept : entry(other.entry)
    {
        other.entry = nullptr;
    }

    /**
     * @brief Assign from other handle, they share the same entry.
     */
    LruHandle &operator=(const LruHandle &other) noexcept
    {
        if (this != &other) {
            if (entry)
                entry->dec_ref();

            entry = other.entry;
            if (entry)
                entry->inc_ref();
        }

        return *this;
    }

    /**
     * @brief Move assign from other handle.
     */
    LruHandle &operator=(LruHandle &&other) noexcept
    {
        if (this != &other) {
            if (entry)
                entry->dec_ref();

            entry = other.entry;
            other.entry = nullptr;
        }

        return *this;
    }

    /**
     * @brief Check if the handle is not empty.
     */
    operator bool() const noexcept { return entry != nullptr; }

    uint16_t get_state() const noexcept
    {
        return entry->state.load(std::memory_order_acquire);
    }

    /**
     * @brief Check if the handle is waiting for value.
     */
    bool waiting() const noexcept { return get_state() == LRU_WAITING; }

    /**
     * @brief Check if the handle's value is emplaced or created.
     */
    bool success() const noexcept { return get_state() == LRU_SUCCESS; }

    /**
     * @brief Check if give up to create value for this handle.
     *
     * For example, the coroutine that created the handle failed to obtain value
     * and give up updating it.
     */
    bool failed() const noexcept { return get_state() == LRU_FAILED; }

    /**
     * @brief Emplace value to the handle.
     *
     * @attention The user should wake up the waiting coroutine by
     *            notify_one/notify_all, even if the emplace throws.
     */
    template<typename... Args>
    void emplace_value(Args &&...args)
    {
        std::unique_lock lk(*entry->mtx);
        entry->value.emplace(std::forward<Args>(args)...);
        entry->state.store(LRU_SUCCESS, std::memory_order_release);
    }

    /**
     * @brief Create value for the handle.
     *
     * @param creater A callable object that takes std::option<Value> & as
     *        argument, and creates the value for the entry.
     *
     * @attention The user should wake up the waiting coroutine by
     *            notify_one/notify_all, even if the creater throws.
     */
    template<typename ValueCreater>
    void create_value(ValueCreater &&creater)
    {
        std::unique_lock lk(*entry->mtx);
        creater(entry->value);
        entry->state.store(LRU_SUCCESS, std::memory_order_release);
    }

    /**
     * @brief Give up create value for this handle.
     * @attention The user should wake up the waiting coroutine by
     *            notify_one/notify_all.
     */
    void set_failed() noexcept
    {
        entry->state.store(LRU_FAILED, std::memory_order_release);
    }

    /**
     * @brief Wake up one coroutine that waiting for the handle.
     */
    void notify_one() { cv_notify(entry, 1); }

    /**
     * @brief Wake up all coroutines waiting for the handle.
     */
    void notify_all() { cv_notify(entry); }

    /**
     * @brief Wait for the handle's value to be emplaced or created, or
     *        set_failed to be called.
     * @return Same as Condition::wait.
     */
    Task<int> wait()
    {
        std::unique_lock lk(*entry->mtx);
        int ret = co_await cv_wait(lk, entry, [this] { return !waiting(); });
        co_return ret;
    }

    /**
     * @brief Wait for the handle's value to be emplaced or created, or
     *        set_failed to be called.
     * @return Same as Condition::wait_for.
     */
    Task<int> wait_for(NanoSec nsec)
    {
        std::unique_lock lk(*entry->mtx);
        int ret = co_await cv_wait_for(lk, entry, nsec,
                                       [this] { return !waiting(); });
        co_return ret;
    }

    /**
     * @brief Get the const reference of the key.
     */
    const Key &key() const noexcept { return entry->key; }

    /**
     * @brief Get the const reference of the value.
     * @pre The handle's value is emplaced or created sucessfully.
     */
    const Value &value() const noexcept { return entry->value.value(); }

    /**
     * @brief Get the refernce of the value.
     * @pre The handle's value is emplaced or created sucessfully.
     * @attention The user should make sure the use of value is thread safe.
     */
    Value &mutable_value() noexcept { return entry->value.value(); }

    /**
     * @brief Release the handle.
     * @post The handle is empty, the associated entry's ref count is decreased.
     */
    void release() noexcept
    {
        if (entry) {
            entry->dec_ref();
            entry = nullptr;
        }
    }

    ~LruHandle()
    {
        if (entry)
            entry->dec_ref();
    }

private:
    LruHandle(Entry *entry) : entry(entry)
    {
        if (entry)
            entry->inc_ref();
    }

private:
    Entry *entry;

    template<typename, typename, typename, typename>
    friend class coke::LruCache;
};

} // namespace detail

template<typename K, typename V, typename H = std::hash<K>,
         typename E = std::equal_to<>>
class LruCache {
public:
    using Key = K;
    using Value = V;
    using Hash = H;
    using Equal = E;
    using Entry = detail::LruEntry<K, V>;
    using Handle = detail::LruHandle<K, V>;
    using SizeType = std::size_t;

    struct EntryHash {
        template<typename U>
        auto operator()(const U &key) const
        {
            if constexpr (std::is_same_v<std::remove_cvref_t<U>, Entry>)
                return hash(key.key);
            else
                return hash(key);
        }

        [[no_unique_address]] Hash hash;
    };

    struct EntryEqual {
        template<typename U>
        auto operator()(const Entry &lhs, const U &rhs) const
        {
            if constexpr (std::is_same_v<std::remove_cvref_t<U>, Entry>)
                return equal(lhs.key, rhs.key);
            else
                return equal(lhs.key, rhs);
        }

        [[no_unique_address]] Equal equal;
    };

    using HtableType = Hashtable<Entry, &Entry::ht, EntryHash, EntryEqual>;
    using ListType = List<Entry, &Entry::lst>;
    using EntryGuard = std::unique_ptr<Entry>;

    static_assert(
        HashEqualComparable<Hash, Equal, Key, Key>,
        "LruCache requires HashEqualComparable<Hash, Equal, Key, Key>");

public:
    /**
     * @brief Create lru cache.
     * @param max_size The max size of cache, 0 means no limit. If cache is
     *        full and new element is added, another element will be dropped.
     * @param hash Hash function for key.
     * @param equal Equal function for key.
     */
    explicit LruCache(SizeType max_size = 0, const Hash &hash = Hash(),
                      const Equal &equal = Equal())
        : entry_table(EntryHash{hash}, EntryEqual{equal}),
          cap(max_size == 0 ? SizeType(-1) : max_size)
    {
    }

    /**
     * @brief Lru cache is not copyable.
     */
    LruCache(const LruCache &) = delete;
    LruCache &operator=(const LruCache &) = delete;

    /**
     * @brief Lru cache is not movable.
     */
    LruCache(LruCache &&) = delete;
    LruCache &operator=(LruCache &&) = delete;

    /**
     * @brief Destroy the cache.
     * @pre All the handle returned by get_or_create are no longer in the
     *      waiting state.
     */
    ~LruCache() { clear(); }

    /**
     * @brief Get the size of the cache.
     */
    SizeType size() const
    {
        std::lock_guard lg(mtx);
        return entry_table.size();
    }

    /**
     * @brief Get the capacity of the cache.
     */
    SizeType capacity() const noexcept { return cap; }

    /**
     * @brief Get lru handle by key. If not found, return an empty handle.
     */
    template<typename U>
        requires HashEqualComparable<Hash, Equal, Key, U>
    Handle get(const U &key)
    {
        std::lock_guard lg(mtx);

        auto it = entry_table.find(key);
        if (it != entry_table.end()) {
            Entry *entry = it.get_pointer();
            entry_list.erase(entry);
            entry_list.push_back(entry);
            return Handle(entry);
        }

        return Handle();
    }

    /**
     * @brief Get lru handle by key. If the key is not found, create a new
     *        one and return it.
     *
     * @return The first element is the handle, the second element is whether
     *         the object is newly created.
     * @attention The one who created the object should be responsible for
     *            create the value, and wake up the coroutine waiting for it.
     */
    template<typename U>
        requires (HashEqualComparable<Hash, Equal, Key, U> &&
                  std::constructible_from<Key, const U &>)
    [[nodiscard]]
    std::pair<Handle, bool> get_or_create(const U &key)
    {
        std::lock_guard lg(mtx);

        auto it = entry_table.find(key);
        if (it != entry_table.end())
            return std::make_pair(Handle(it.get_pointer()), false);

        if (entry_table.size() >= cap)
            remove_entry(entry_list.front());

        EntryGuard entry(new Entry(detail::LRU_WAITING, next_mtx(), key));
        entry_table.insert(entry.get());
        entry_list.push_back(entry.get());

        return std::make_pair(Handle(entry.release()), true);
    }

    /**
     * @brief Put a new key value pair into the cache.
     * @return The handle of the new object.
     */
    template<typename U, typename... Args>
        requires (HashEqualComparable<Hash, Equal, Key, const U &> &&
                  std::constructible_from<Key, const U &> &&
                  std::constructible_from<Value, Args && ...>)
    Handle put(const U &key, Args &&...args)
    {
        EntryGuard entry(new Entry(detail::LRU_SUCCESS, nullptr, key));
        entry->value.emplace(std::forward<Args>(args)...);

        std::lock_guard lg(mtx);

        auto it = entry_table.find(key);
        if (it != entry_table.end())
            remove_entry(it.get_pointer());
        else if (entry_table.size() >= cap)
            remove_entry(entry_list.front());

        entry_table.insert(entry.get());
        entry_list.push_back(entry.get());

        return Handle(entry.release());
    }

    /**
     * @brief Remove the key from the cache.
     */
    template<typename U>
        requires HashEqualComparable<Hash, Equal, Key, const U &>
    void remove(const U &key)
    {
        std::lock_guard lg(mtx);

        auto it = entry_table.find(key);
        if (it != entry_table.end())
            remove_entry(it.get_pointer());
    }

    /**
     * @brief Remove the handle from the cache.
     */
    void remove(const Handle &handle)
    {
        std::lock_guard lg(mtx);
        remove_entry(handle.entry);
    }

    /**
     * @brief Clear the cache.
     */
    void clear()
    {
        Entry *entry;
        std::lock_guard lg(mtx);

        // Clear the table before dec_ref.
        entry_table.clear();

        while (!entry_list.empty()) {
            entry = entry_list.pop_front();
            entry->removed = true;
            entry->dec_ref();
        }
    }

private:
    void remove_entry(Entry *entry)
    {
        if (!entry->removed) {
            entry->removed = true;
            entry_table.erase(entry);
            entry_list.erase(entry);
            entry->dec_ref();
        }
    }

    constexpr static SizeType MUTEX_TBL_SIZE = 4;

    std::mutex *next_mtx()
    {
        auto id = mid.fetch_add(1, std::memory_order_relaxed);
        return &mtx_tbl[id % MUTEX_TBL_SIZE];
    }

private:
    mutable std::mutex mtx;
    HtableType entry_table;
    ListType entry_list;
    SizeType cap;

    // mutex table for entry
    std::atomic<unsigned> mid;
    std::mutex mtx_tbl[MUTEX_TBL_SIZE];
};

} // namespace coke

#endif // COKE_LRU_CACHE_H
