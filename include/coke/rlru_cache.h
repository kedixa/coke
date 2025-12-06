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

#ifndef COKE_RLRU_CACHE_H
#define COKE_RLRU_CACHE_H

#include <atomic>
#include <chrono>
#include <concepts>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <type_traits>

#include "coke/detail/condition_impl.h"
#include "coke/detail/random.h"
#include "coke/detail/ref_counted.h"
#include "coke/utils/hashtable.h"

namespace coke {

template<typename, typename, typename, typename>
class RlruCache;

namespace detail {

enum _rlru_state : uint16_t {
    RLRU_WAITING = 0,
    RLRU_SUCCESS = 1,
    RLRU_FAILED = 2,
};

struct RlruSharedData {
    std::mutex mtx;
    std::atomic<uint64_t> access_count{0};
};

template<typename K, typename V>
struct RlruEntry : public detail::RefCounted<RlruEntry<K, V>> {
    constexpr static auto relaxed = std::memory_order_relaxed;

    using Key = K;
    using Value = V;

    template<typename U>
    RlruEntry(uint16_t state, RlruSharedData *data, const U &key) noexcept
        : removed(false), state(state), last_access(0), data(data), key(key)
    {
    }

    void update_last_access() noexcept
    {
        last_access.store(data->access_count.fetch_add(1, relaxed), relaxed);
    }

    uint64_t load_last_access() const noexcept
    {
        return last_access.load(relaxed);
    }

    void set_state(uint16_t new_state) noexcept
    {
        state.store(new_state, std::memory_order_release);
    }

    bool check_state(uint16_t expected) const noexcept
    {
        return state.load(std::memory_order_acquire) == expected;
    }

    bool removed;
    std::atomic<uint16_t> state;
    std::atomic<uint64_t> last_access;
    HashtableNode ht;
    RlruSharedData *data;

    Key key;
    std::optional<Value> value;
};

template<typename K, typename V>
class RlruHandle {
public:
    using Entry = RlruEntry<K, V>;
    using Key = typename Entry::Key;
    using Value = typename Entry::Value;

    /**
     * @brief Create an empty handle.
     */
    RlruHandle() noexcept : entry(nullptr) {}

    /**
     * @brief Create from other handle, they share the same entry.
     */
    RlruHandle(const RlruHandle &other) noexcept : entry(other.entry)
    {
        if (entry)
            entry->inc_ref();
    }

    /**
     * @brief Move from other handle.
     */
    RlruHandle(RlruHandle &&other) noexcept : entry(other.entry)
    {
        other.entry = nullptr;
    }

    /**
     * @brief Assign from other handle, they share the same entry.
     */
    RlruHandle &operator=(const RlruHandle &other) noexcept
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
    RlruHandle &operator=(RlruHandle &&other) noexcept
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

    /**
     * @brief Check if the handle is waiting for value.
     */
    bool waiting() const noexcept { return entry->check_state(RLRU_WAITING); }

    /**
     * @brief Check if the handle's value is emplaced or created.
     */
    bool success() const noexcept { return entry->check_state(RLRU_SUCCESS); }

    /**
     * @brief Check if give up to create value for this handle.
     *
     * For example, the coroutine that created the handle failed to obtain value
     * and give up updating it.
     */
    bool failed() const noexcept { return entry->check_state(RLRU_FAILED); }

    /**
     * @brief Emplace value to the handle.
     *
     * @attention The user should wake up the waiting coroutine by
     *            notify_one/notify_all, even if the emplace throws.
     */
    template<typename... Args>
    void emplace_value(Args &&...args)
    {
        std::unique_lock lk(entry->data->mtx);
        entry->value.emplace(std::forward<Args>(args)...);
        entry->update_last_access();
        entry->set_state(RLRU_SUCCESS);
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
        entry->update_access_time();
        entry->set_state(RLRU_SUCCESS);
    }

    /**
     * @brief Give up create value for this handle.
     * @attention The user should wake up the waiting coroutine by
     *            notify_one/notify_all.
     */
    void set_failed() noexcept { entry->set_state(RLRU_FAILED); }

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
     * @return See coke::Condition::wait.
     */
    Task<int> wait()
    {
        std::unique_lock lk(entry->data->mtx);
        int ret = co_await cv_wait(lk, entry, [this] { return !waiting(); });
        co_return ret;
    }

    /**
     * @brief Wait for the handle's value to be emplaced or created, or
     *        set_failed to be called.
     * @return See coke::Condition::wait_for.
     */
    Task<int> wait_for(NanoSec nsec)
    {
        std::unique_lock lk(entry->data->mtx);
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

    ~RlruHandle()
    {
        if (entry)
            entry->dec_ref();
    }

private:
    RlruHandle(Entry *entry) : entry(entry)
    {
        if (entry)
            entry->inc_ref();
    }

private:
    Entry *entry;

    template<typename, typename, typename, typename>
    friend class ::coke::RlruCache;
};

} // namespace detail

template<typename K, typename V, typename H = std::hash<K>,
         typename E = std::equal_to<>>
class RlruCache {
public:
    using Key = K;
    using Value = V;
    using Hash = H;
    using Equal = E;
    using Entry = detail::RlruEntry<K, V>;
    using Handle = detail::RlruHandle<K, V>;
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
    using EntryGuard = std::unique_ptr<Entry>;

    static_assert(
        HashEqualComparable<Hash, Equal, Key, Key>,
        "RlruCache requires HashEqualComparable<Hash, Equal, Key, Key>");

public:
    /**
     * @brief Create rlru cache.
     * @param max_size The max size of cache, 0 means no limit. If cache is
     *        full and new element is added, another element will be dropped.
     * @param max_scan How many elements to scan when dropping an element.
     * @param hash Hash function for key.
     * @param equal Equal function for key.
     */
    explicit RlruCache(SizeType max_size = 0, SizeType max_scan = 5,
                       const Hash &hash = Hash(), const Equal &equal = Equal())
        : entry_table(EntryHash{hash}, EntryEqual{equal}),
          cap(max_size == 0 ? SizeType(-1) : max_size), max_scan(max_scan)
    {
    }

    /**
     * @brief Rlru cache is not copyable.
     */
    RlruCache(const RlruCache &) = delete;
    RlruCache &operator=(const RlruCache &) = delete;

    /**
     * @brief Rlru cache is not movable.
     */
    RlruCache(RlruCache &&) = delete;
    RlruCache &operator=(RlruCache &&) = delete;

    /**
     * @brief Destroy the cache.
     * @pre All the handle returned by get_or_create are no longer in the
     *      waiting state.
     */
    ~RlruCache() { clear(); }

    /**
     * @brief Get the size of the cache.
     */
    SizeType size() const
    {
        std::shared_lock lk(mtx);
        return entry_table.size();
    }

    /**
     * @brief Get the capacity of the cache.
     */
    SizeType capacity() const noexcept { return cap; }

    /**
     * @brief Get rlru handle by key. If the key is not found, return an empty
     *        handle.
     */
    template<typename U>
        requires HashEqualComparable<Hash, Equal, Key, U>
    Handle get(const U &key)
    {
        std::shared_lock lk(mtx);

        auto it = entry_table.find(key);
        if (it != entry_table.end()) {
            Entry *entry = it.get_pointer();
            entry->update_last_access();
            return Handle(entry);
        }

        return Handle();
    }

    /**
     * @brief Get rlru handle by key. If the key is not found, create a new
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
        {
            std::shared_lock lk(mtx);

            auto it = entry_table.find(key);
            if (it != entry_table.end()) {
                Entry *entry = it.get_pointer();
                entry->update_last_access();
                return std::make_pair(Handle(entry), false);
            }
        }

        std::unique_lock lk(mtx);

        auto it = entry_table.find(key);
        if (it != entry_table.end()) {
            Entry *entry = it.get_pointer();
            entry->update_last_access();
            return std::make_pair(Handle(entry), false);
        }

        if (entry_table.size() >= cap)
            drop_one_element();

        EntryGuard entry(new Entry(detail::RLRU_WAITING, &data, key));
        entry->update_last_access();
        entry_table.insert(entry.get());

        return std::make_pair(Handle(entry.release()), true);
    }

    /**
     * @brief Put a new key value pair into the cache.
     * @return The handle of the new object.
     */
    template<typename U, typename... Args>
        requires (HashEqualComparable<Hash, Equal, Key, U> &&
                  std::constructible_from<Key, const U &> &&
                  std::constructible_from<Value, Args && ...>)
    Handle put(const U &key, Args &&...args)
    {
        EntryGuard entry(new Entry(detail::RLRU_SUCCESS, &data, key));
        entry->value.emplace(std::forward<Args>(args)...);
        entry->update_last_access();

        std::unique_lock lk(mtx);

        auto it = entry_table.find(key);
        if (it != entry_table.end())
            remove_entry(it.get_pointer());
        else if (entry_table.size() >= cap)
            drop_one_element();

        entry_table.insert(entry.get());

        return Handle(entry.release());
    }

    /**
     * @brief Remove the key from the cache.
     */
    template<typename U>
        requires HashEqualComparable<Hash, Equal, Key, const U &>
    void remove(const U &key)
    {
        std::unique_lock lk(mtx);

        auto it = entry_table.find(key);
        if (it != entry_table.end())
            remove_entry(it.get_pointer());
    }

    /**
     * @brief Remove the handle from the cache.
     */
    void remove(const Handle &handle)
    {
        std::unique_lock lk(mtx);
        remove_entry(handle.entry);
    }

    /**
     * @brief Clear the cache.
     */
    void clear()
    {
        std::unique_lock lk(mtx);
        Entry *entry;

        auto it = entry_table.begin();
        while (it != entry_table.end()) {
            entry = it.get_pointer();
            it = entry_table.erase(it);

            entry->removed = true;
            entry->dec_ref();
        }
    }

private:
    void remove_entry(Entry *entry)
    {
        if (!entry->removed) {
            entry_table.erase(entry);
            entry->removed = true;
            entry->dec_ref();
        }
    }

    void drop_one_element()
    {
        if (entry_table.empty())
            return;

        SizeType scan_cnt, pos;

        if (entry_table.size() <= max_scan) {
            pos = 0;
            scan_cnt = entry_table.size();
        }
        else {
            pos = (SizeType)(rand_u64() % entry_table.size());
            scan_cnt = max_scan;
        }

        auto min_it = entry_table.element_at(pos);
        uint64_t min_access = min_it->load_last_access();

        for (SizeType i = 1; i < scan_cnt; i++) {
            pos = (pos + 1) % entry_table.size();

            auto it = entry_table.element_at(pos);
            uint64_t last_access = it->load_last_access();
            if (min_access > last_access) {
                min_access = last_access;
                min_it = it;
            }
        }

        remove_entry(min_it.get_pointer());
    }

private:
    mutable std::shared_mutex mtx;
    HtableType entry_table;
    SizeType cap;
    SizeType max_scan;

    detail::RlruSharedData data;
};

} // namespace coke

#endif // COKE_RLRU_CACHE_H
