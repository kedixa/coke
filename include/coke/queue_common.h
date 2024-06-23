/**
 * Copyright 2024 Coke Project (https://github.com/kedixa/coke)
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

#ifndef COKE_QUEUE_COMMON_H
#define COKE_QUEUE_COMMON_H

#include <atomic>
#include <cstdint>
#include <mutex>
#include <type_traits>
#include <utility>

#include "coke/detail/task.h"
#include "coke/condition.h"

namespace coke {

/**
 * @class coke::QueueCommon
 * @brief Common CRTP base of coke::Queue, coke::PriorityQueue and coke::Stack.
 *
 * Class QueueCommon requires its subclasses to implement three template
 * functions: `void do_emplace(Args&&...)`, `void do_push(U &&)`, and
 * `void do_pop(U &)` to use the interface provided by this class.
 *
 * @tparam Q Type of subclass.
 * @tparam T Type of container's value.
*/
template<typename Q, typename T>
class QueueCommon {
public:
    // Type used to represent container's size, usually an alias for std::size_t
    using SizeType = std::size_t;

protected:
    using UniqueLock = std::unique_lock<std::mutex>;
    using TimedWaitHelper = detail::TimedWaitHelper;

    static constexpr auto acquire = std::memory_order_acquire;
    static constexpr auto release = std::memory_order_release;
    static constexpr auto acq_rel = std::memory_order_acq_rel;

    static SizeType min(SizeType a, SizeType b) { return (b < a) ? b : a; }

    struct CountGuard {
        CountGuard(SizeType &m) : n(m) { ++n; }
        ~CountGuard() { --n; }

        SizeType &n;
    };

    /**
     * @brief Class Queue cannot be used directly, inherit it and implement its
     *        requirements, see coke::Queue etc.
     * @param max_size Max size of the container.
    */
    QueueCommon(SizeType max_size)
        : que_max_size(max_size), que_cur_size(0), que_closed(false),
          push_wait_cnt(0), pop_wait_cnt(0)
    {
        // empty queue not supported
        if (que_max_size == 0)
            que_max_size = 1;
    }

    ~QueueCommon() = default;

public:
    // Queue is neither copyable nor moveable
    QueueCommon(const QueueCommon &) = delete;
    QueueCommon &operator=(const QueueCommon &) = delete;

    /**
     * @brief Check whether the container is empty.
     * @note In a concurrent environment, this value may no longer be accurate
     *       after returned.
    */
    bool empty() const noexcept { return size() == 0; }

    /**
     * @brief Check whether the container is full.
     * @note In a concurrent environment, this value may no longer be accurate
     *       after returned.
    */
    bool full() const noexcept { return size() >= max_size(); }

    /**
     * @brief Check whether the container is closed.
     * @see close().
    */
    bool closed() const noexcept { return que_closed.load(acquire); }

    /**
     * @brief Get the element count of the container.
     * @note In a concurrent environment, this value may no longer be accurate
     *       after returned.
    */
    SizeType size() const noexcept { return que_cur_size.load(acquire); }

    /**
     * @brief Get the max size of the container, same as the `max_size` param
     *        used to create this container.
    */
    SizeType max_size() const noexcept { return que_max_size; }

    /**
     * @brief Close the container.
     * 
     * When the container is closed, all coroutines waiting for push will wake
     * up and fail in the coke::TOP_CLOSED state, subsequent push operations
     * will directly fail in this state. The coroutine that wants to pop will
     * succeed until the queue is empty and then it will fail with
     * coke::TOP_CLOSED status.
     *
     * @pre The container is not closed().
    */
    void close() {
        UniqueLock lk(que_mtx);

        if (!que_closed.exchange(true, acq_rel)) {
            push_cv.notify_all();
            pop_cv.notify_all();
        }
    }

    /**
     * @brief Reopen a closed container.
     * @pre The container is closed().
    */
    void reopen() noexcept { que_closed.store(false, release); }

// public template member functions
public:

    /**
     * @brief Try to emplace new elements into container.
     *
     * @param args... Arguments to construct T.
     * @returns Whether new element is pushed.
     * @retval true If new element is pushed into container.
     * @retval false If full or closed, the args... will not be moved or copied.
    */
    template<typename... Args>
        requires std::constructible_from<T, Args...>
    bool try_emplace(Args&&... args) {
        if (!push_pred())
            return false;

        UniqueLock lk(que_mtx);
        if (!push_pred())
            return false;

        get().do_emplace(std::forward<Args>(args)...);
        after_push(lk, 1);
        return true;
    }

    /**
     * @brief Emplace new elements into container.
     *
     * @param args... Arguments to construct T.
     * @returns Coroutine coke::Task that should co_await immediately.
     * @retval See emplace_for.
    */
    template<typename... Args>
        requires std::constructible_from<T, Args...>
    Task<int> emplace(Args&&... args) {
        // TODO only when really a sync operation.
        if (coke::prevent_recursive_stack())
            co_await coke::yield();

        int ret = TOP_SUCCESS;

        UniqueLock lk(que_mtx);
        if (closed())
            ret = TOP_CLOSED;
        else if (full()) {
            CountGuard cg(push_wait_cnt);
            ret = co_await push_cv.wait(lk, [this]() {
                return push_pred();
            });
        }

        if (ret == TOP_SUCCESS) {
            if (closed())
                ret = TOP_CLOSED;
            else
                get().do_emplace(std::forward<Args>(args)...);
        }

        if (ret == TOP_SUCCESS)
            after_push(lk, 1);

        co_return ret;
    }

    /**
     * @brief Emplace new elements into container before nsec timeout.
     *
     * @param nsec If container is full, wait at most nsec time.
     * @param args... Arguments to construct T.
     *
     * @returns Coroutine coke::Task that should co_await immediately.
     * @retval coke::TOP_SUCCESS If emplace successful.
     * @retval coke::TOP_TIMEOUT If timeout before container is able to push.
     * @retval coke::TOP_ABORTED If process exit.
     * @retval coke::TOP_CLOSED If container is closed.
     * @retval Negative integer to indicate system error, almost never happens.
     * @see coke/global.h
    */
    template<typename... Args>
        requires std::constructible_from<T, Args...>
    Task<int> try_emplace_for(const NanoSec &nsec, Args&&... args) {
        if (coke::prevent_recursive_stack())
            co_await coke::yield();

        int ret = TOP_SUCCESS;

        UniqueLock lk(que_mtx);
        if (closed())
            ret = TOP_CLOSED;
        else if (full()) {
            CountGuard cg(push_wait_cnt);
            ret = co_await push_cv.wait_for(lk, nsec, [this]() {
                return push_pred();
            });
        }

        if (ret == TOP_SUCCESS) {
            if (closed())
                ret = TOP_CLOSED;
            else
                get().do_emplace(std::forward<Args>(args)...);
        }

        if (ret == TOP_SUCCESS)
            after_push(lk, 1);

        co_return ret;
    }

    /**
     * @brief Try to push new elements into container.
     *
     * @param u Value that will push into container.
     *
     * @returns Whether new element is pushed.
     * @retval true If new element is pushed into container.
     * @retval false If full or closed, the u will not be moved or copied.
    */
    template<typename U>
        requires std::assignable_from<T&, U&&>
    bool try_push(U &&u) {
        if (!push_pred())
            return false;

        UniqueLock lk(que_mtx);
        if (!push_pred())
            return false;

        get().do_push(std::forward<U>(u));
        after_push(lk, 1);
        return true;
    }

    /**
     * @brief Push new elements into container.
     *
     * @param u Value that will push into container.
     *
     * @returns Coroutine coke::Task that should co_await immediately.
     * @retval See emplace_for.
    */
    template<typename U>
        requires std::assignable_from<T&, U&&>
    Task<int> push(U &&u) {
        if (coke::prevent_recursive_stack())
            co_await coke::yield();

        int ret = TOP_SUCCESS;

        UniqueLock lk(que_mtx);
        if (closed())
            ret = TOP_CLOSED;
        else if (full()) {
            CountGuard cg(push_wait_cnt);
            ret = co_await push_cv.wait(lk, [this]() {
                return push_pred();
            });
        }

        if (ret == TOP_SUCCESS) {
            if (closed())
                ret = TOP_CLOSED;
            else
                get().do_push(std::forward<U>(u));
        }

        if (ret == TOP_SUCCESS)
            after_push(lk, 1);

        co_return ret;
    }

    /**
     * @brief Push new elements into container before nsec timeout.
     *
     * @param nsec If container is full, wait at most nsec time.
     * @param u Value that will push into container.
     *
     * @returns Coroutine coke::Task that should co_await immediately.
     * @retval See emplace_for.
    */
    template<typename U>
        requires std::assignable_from<T&, U&&>
    Task<int> try_push_for(const NanoSec &nsec, U &&u) {
        if (coke::prevent_recursive_stack())
            co_await coke::yield();

        int ret = TOP_SUCCESS;

        UniqueLock lk(que_mtx);
        if (closed())
            ret = TOP_CLOSED;
        else if (full()) {
            CountGuard cg(push_wait_cnt);
            ret = co_await push_cv.wait_for(lk, nsec, [this]() {
                return push_pred();
            });
        }

        if (ret == TOP_SUCCESS) {
            if (closed())
                ret = TOP_CLOSED;
            else
                get().do_push(std::forward<U>(u));
        }

        if (ret == TOP_SUCCESS)
            after_push(lk, 1);

        co_return ret;
    }

    /**
     * @brief Try to pop element from container.
     *
     * @param u Reference that receive popped element.
     *
     * @returns Whether element is popped.
     * @retval true if element is assigned to u.
     * @retval false if empty, and u is keep unchanged.
    */
    template<typename U>
        requires std::assignable_from<U&, T&&>
    bool try_pop(U &u) {
        if (empty())
            return false;

        UniqueLock lk(que_mtx);
        if (empty())
            return false;

        get().do_pop(u);
        after_pop(lk, 1);
        return true;
    }

    /**
     * @brief Pop element from container.
     *
     * @param u Reference that receive popped element.
     *
     * @returns Coroutine that should co_await immediately.
     * @retval See pop_for.
    */
    template<typename U>
        requires std::assignable_from<U&, T&&>
    Task<int> pop(U &u) {
        return pop_impl(TimedWaitHelper{}, u);
    }

    /**
     * @brief Pop element from container.
     *
     * @param nsec If container is empty, wait at most nsec time.
     * @param u Reference that receive popped element.
     *
     * @returns Coroutine that should co_await immediately.
     * @retval coke::TOP_SUCCESS If pop successful.
     * @retval coke::TOP_TIMEOUT If timeout before container is able to pop.
     * @retval coke::TOP_ABORTED If process exit.
     * @retval coke::TOP_CLOSED If container is closed.
     * @retval Negative integer to indicate system error, almost never happens.
     * @see coke/global.h
    */
    template<typename U>
        requires std::assignable_from<U&, T&&>
    Task<int> try_pop_for(const NanoSec &nsec, U &u) {
        return pop_impl(TimedWaitHelper{nsec}, u);
    }

    /**
     * @brief Try to push a range of elements into container.
     *
     * @param first,last Element range [first,last)
     * @param size_hint Only try to push element when the remaining space of the
     *        container is >= size_hint. This feature can be used to atomically
     *        push or reject an entire range of data, when size_hint equals to
     *        std::distance(first, last).
     *
     * @returns Iterator for next value to push. If no element pushed, return
     *          first, if all elements are pushed, return last.
    */
    template<std::input_iterator Iter>
        requires std::assignable_from<T&, typename Iter::value_type>
    auto try_push_range(Iter first, Iter last, SizeType size_hint = 0) {
        SizeType cur_qsize = size(), max_qsize = max_size();

        if (cur_qsize >= max_qsize || max_qsize - cur_qsize < size_hint)
            return first;

        UniqueLock lk(que_mtx);
        cur_qsize = size();
        max_qsize = max_size();

        if (cur_qsize >= max_qsize || max_qsize - cur_qsize < size_hint)
            return first;

        SizeType n = 0, m = max_qsize - cur_qsize;
        try {
            while (first != last && n < m) {
                get().do_push(*first);
                ++n;
                ++first;
            }
        }
        catch (...) {
            // Because no way to roll back, accept what was pushed.
            if (n)
                after_push(lk, n);

            std::rethrow_exception(std::current_exception());
        }

        after_push(lk, n);
        return first;
    }

    /**
     * @brief Try to pop elements into range.
     *
     * The iter in [first, ret_iter) are dereferenced exactly once, the iter in
     * [ret_iter, last) will not be dereferenced. [first, last) are traversed
     * at most once, an output iterator can be used in this situation.
     *
     * @tparam Iter An output iterator type that can be assigned by T&&.
     * @param first,last Element range [first, last).
     * @param size_hint Only try to pop element when the remaining elements are
     *        >= size_hint.
     *
     * @return Iterator for the next position to store element.
    */
    template<typename Iter>
        requires std::is_assignable_v<decltype(*std::declval<Iter>()), T&&>
                 && std::output_iterator<Iter, T>
    auto try_pop_range(Iter first, Iter last, SizeType size_hint = 0) {
        SizeType cur_size = size();

        if (cur_size == 0 || cur_size < size_hint)
            return first;

        UniqueLock lk(que_mtx);
        cur_size = size();

        if (cur_size == 0 || cur_size < size_hint)
            return first;

        SizeType n = 0;
        try {
            while (first != last && n < cur_size) {
                get().do_pop(*first);
                ++n;
                ++first;
            }
        }
        catch (...) {
            // Because no way to roll back, discard what was popped.
            if (n)
                after_pop(lk, n);

            std::rethrow_exception(std::current_exception());
        }

        after_pop(lk, n);
        return first;
    }

    /**
     * @brief Try to pop at most max_pop elements from container.
     *
     * @param iter Iterator to recieve element. E.g. std::back_insert_iterator.
     * @param max_pop Max elements to pop.
     *
     * @returns Number of elements popped.
    */
    template<typename Iter>
        requires std::is_assignable_v<decltype(*std::declval<Iter>()), T&&>
                 && std::output_iterator<Iter, T>
    SizeType try_pop_n(Iter iter, SizeType max_pop) {
        SizeType cur_size = size();

        if (cur_size == 0 || max_pop == 0)
            return 0;

        UniqueLock lk(que_mtx);
        cur_size = size();

        if (cur_size == 0)
            return 0;

        SizeType n = 0, m = min(cur_size, max_pop);
        try {
            while (n < m) {
                get().do_pop(*iter);
                ++n;
                ++iter;
            }
        }
        catch (...) {
            // Because no way to roll back, discard what was popped.
            if (n)
                after_pop(lk, n);

            std::rethrow_exception(std::current_exception());
        }

        after_pop(lk, n);
        return n;
    }

protected:
    bool pop_pred() const { return closed() || !empty(); }

    bool push_pred() const { return closed() || !full(); }

    void after_push(UniqueLock &lk, SizeType push_cnt) {
        SizeType wake_cnt = min(push_cnt, pop_wait_cnt);
        que_cur_size.fetch_add(push_cnt, acq_rel);

        lk.unlock();

        if (wake_cnt)
            pop_cv.notify(wake_cnt);
    }

    void after_pop(UniqueLock &lk, SizeType pop_cnt) {
        SizeType wake_cnt = min(pop_cnt, push_wait_cnt);
        que_cur_size.fetch_sub(pop_cnt, acq_rel);

        lk.unlock();

        if (wake_cnt)
            push_cv.notify(wake_cnt);
    }

    template<typename U>
    Task<int> pop_impl(TimedWaitHelper h, U &u) {
        if (coke::prevent_recursive_stack())
            co_await coke::yield();

        int ret;

        UniqueLock lk(que_mtx);
        if (!empty())
            ret = TOP_SUCCESS;
        else if (closed())
            ret = TOP_CLOSED;
        else {
            CountGuard cg(pop_wait_cnt);

            if (h.infinite()) {
                ret = co_await pop_cv.wait(lk, [this]() {
                    return pop_pred();
                });
            }
            else {
                ret = co_await pop_cv.wait_for(lk, h.time_left(), [this]() {
                    return pop_pred();
                });
            }
        }

        if (ret == TOP_SUCCESS) {
            if (!empty())
                get().do_pop(u);
            else
                ret = TOP_CLOSED;
        }

        if (ret == TOP_SUCCESS)
            after_pop(lk, 1);

        co_return ret;
    }

protected:
    SizeType que_max_size;
    std::atomic<SizeType> que_cur_size;
    std::atomic<bool> que_closed;

    mutable std::mutex que_mtx;
    TimedCondition push_cv;
    TimedCondition pop_cv;

    SizeType push_wait_cnt;
    SizeType pop_wait_cnt;

private:
    Q &get() noexcept { return static_cast<Q &>(*this); }

    const Q &get() const noexcept { return static_cast<const Q &>(*this); }
};


} // namespace coke

#endif // COKE_QUEUE_COMMON_H
