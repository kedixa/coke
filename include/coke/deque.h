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

#ifndef COKE_DEQUE_H
#define COKE_DEQUE_H

#include <atomic>
#include <cstdint>
#include <mutex>
#include <type_traits>
#include <utility>
#include <deque>

#include "coke/detail/basic_concept.h"
#include "coke/condition.h"

namespace coke {

template<Queueable T, typename Alloc=std::allocator<T>>
class Deque {
public:
    using SizeType = std::size_t;
    using ContainerType = std::deque<T, Alloc>;
    using ValueType = T;
    using QueueType = ContainerType;

protected:
    using UniqueLock = std::unique_lock<std::mutex>;

    static constexpr auto acquire = std::memory_order_acquire;
    static constexpr auto release = std::memory_order_release;
    static constexpr auto acq_rel = std::memory_order_acq_rel;
    static constexpr auto pos_front = true;
    static constexpr auto pos_back = false;

    static SizeType min(SizeType a, SizeType b) { return (b < a) ? b : a; }

    struct CountGuard {
        CountGuard(SizeType &m) : n(m) { ++n; }
        ~CountGuard() { --n; }

        SizeType &n;
    };

public:
    /**
     * @brief Create coke::Deque with max_size.
     * @param max_size Max size of the container.
    */
    explicit Deque(SizeType max_size)
        : que_max_size(max_size), que_cur_size(0), que_closed(false),
          push_wait_cnt(0), pop_wait_cnt(0)
    {
        // empty queue not supported
        if (que_max_size == 0)
            que_max_size = 1;
    }

    // Deque is neither copyable nor moveable
    Deque(const Deque &) = delete;
    Deque &operator= (const Deque &) = delete;

    ~Deque() = default;

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
     * succeed until the deque is empty and then it will fail with
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

    // emplace

    /**
     * @brief Try to emplace new element in the front.
     *
     * @param args... Arguments to construct T.
     * @returns Whether new element is pushed.
     * @retval true If new element is pushed into container.
     * @retval false If full or closed, the args... will not be moved or copied.
    */
    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    bool try_emplace_front(Args&&... args) {
        return try_emplace_impl(pos_front, std::forward<Args>(args)...);
    }

    /**
     * @brief Try to emplace new element in the back.
     * @see try_emplace_front.
     */
    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    bool try_emplace_back(Args&&... args) {
        return try_emplace_impl(pos_back, std::forward<Args>(args)...);
    }

    /**
     * @brief Force emplace new element in the front even if full.
     *
     * @param args... Arguments to construct T.
     * @returns Whether new element is pushed.
     * @retval true If new element is pushed into container.
     * @retval false If closed, the args... will not be moved or copied.
    */
    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    bool force_emplace_front(Args&&... args) {
        return force_emplace_impl(pos_front, std::forward<Args>(args)...);
    }

    /**
     * @brief Force emplace new element in the back even if full.
     * @see force_emplace_front.
     */
    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    bool force_emplace_back(Args&&... args) {
        return force_emplace_impl(pos_back, std::forward<Args>(args)...);
    }

    /**
     * @brief Emplace new element in the front.
     *
     * @param args... Arguments to construct T.
     * @returns Coroutine coke::Task that should co_await immediately.
     * @retval See try_emplace_front_for.
    */
    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    Task<int> emplace_front(Args&&... args) {
        return emplace_impl(pos_front, true, NanoSec(0), std::forward<Args>(args)...);
    }

    /**
     * @brief Emplace new element in the back.
     * @see try_emplace_front_for.
    */
    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    Task<int> emplace_back(Args&&... args) {
        return emplace_impl(pos_back, true, NanoSec(0), std::forward<Args>(args)...);
    }

    /**
     * @brief Emplace new element in the front before nsec timeout.
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
        requires std::constructible_from<T, Args&&...>
    Task<int> try_emplace_front_for(NanoSec nsec, Args&&... args) {
        return emplace_impl(pos_front, false, nsec, std::forward<Args>(args)...);
    }

    /**
     * @brief Emplace new element in the back before nsec timeout.
     * @see try_emplace_front_for.
    */
    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    Task<int> try_emplace_back_for(NanoSec nsec, Args&&... args) {
        return emplace_impl(pos_back, false, nsec, std::forward<Args>(args)...);
    }

    // push

    /**
     * @brief Try to push new element to the front.
     *
     * @param u Value that will be pushed.
     * @returns Whether new element is pushed.
     * @retval true If new element is pushed into container.
     * @retval false If full or closed, the u will not be moved or copied.
    */
    template<typename U>
        requires std::assignable_from<T&, U&&>
    bool try_push_front(U &&u) {
        return try_push_impl(pos_front, std::forward<U>(u));
    }

    /**
     * @brief Try to push new element to the back.
     * @see try_push_front.
    */
    template<typename U>
        requires std::assignable_from<T&, U&&>
    bool try_push_back(U &&u) {
        return try_push_impl(pos_back, std::forward<U>(u));
    }

    /**
     * @brief Force push new element to the front even if full.
     *
     * @param u Value that will be pushed.
     * @returns Whether new element is pushed.
     * @retval true If new element is pushed into container.
     * @retval false If closed, the u will not be moved or copied.
    */
    template<typename U>
        requires std::assignable_from<T&, U&&>
    bool force_push_front(U &&u) {
        return force_push_impl(pos_front, std::forward<U>(u));
    }

    /**
     * @brief Force push new element to the front even if full.
     * @see force_push_front.
    */
    template<typename U>
        requires std::assignable_from<T&, U&&>
    bool force_push_back(U &&u) {
        return force_push_impl(pos_back, std::forward<U>(u));
    }

    /**
     * @brief Push new element to the front.
     *
     * @param u Value that will push into container.
     * @retval See try_emplace_front_for.
    */
    template<typename U>
        requires std::assignable_from<T&, U&&>
    Task<int> push_front(U &&u) {
        return push_impl(pos_front, true, NanoSec(0), std::forward<U>(u));
    }

    /**
     * @brief Push new element to the back.
     *
     * @param u Value that will push into container.
     * @retval See try_emplace_front_for.
    */
    template<typename U>
        requires std::assignable_from<T&, U&&>
    Task<int> push_back(U &&u) {
        return push_impl(pos_back, true, NanoSec(0), std::forward<U>(u));
    }

    /**
     * @brief Try to push new element to the front before nsec timeout.
     *
     * @param u Value that will push into container.
     * @retval See try_emplace_front_for.
    */
    template<typename U>
        requires std::assignable_from<T&, U&&>
    Task<int> try_push_front_for(NanoSec nsec, U &&u) {
        return push_impl(pos_front, false, nsec, std::forward<U>(u));
    }

    /**
     * @brief Try to push new element to the back before nsec timeout.
     *
     * @param u Value that will push into container.
     * @retval See try_emplace_front_for.
    */
    template<typename U>
        requires std::assignable_from<T&, U&&>
    Task<int> try_push_back_for(NanoSec nsec, U &&u) {
        return push_impl(pos_back, false, nsec, std::forward<U>(u));
    }

    // pop

    /**
     * @brief Try to pop element from front.
     *
     * @param u Reference that receive popped element.
     *
     * @returns Whether element is popped.
     * @retval true if element is assigned to u.
     * @retval false if empty, and u is keep unchanged.
    */
    template<typename U>
        requires std::assignable_from<U&, T&&>
    bool try_pop_front(U &u) {
        return try_pop_impl(pos_front, u);
    }

    /**
     * @brief Try to pop element from back.
     * @see try_pop_front.
     */
    template<typename U>
        requires std::assignable_from<U&, T&&>
    bool try_pop_back(U &u) {
        return try_pop_impl(pos_back, u);
    }

    /**
     * @brief Pop element from front.
     * @retval See try_pop_front_for.
    */
    template<typename U>
        requires std::assignable_from<U&, T&&>
    Task<int> pop_front(U &u) {
        return pop_impl(pos_front, true, NanoSec(0), u);
    }

    /**
     * @brief Pop element from back.
     * @retval See try_pop_front_for.
    */
    template<typename U>
        requires std::assignable_from<U&, T&&>
    Task<int> pop_back(U &u) {
        return pop_impl(pos_back, true, NanoSec(0), u);
    }

    /**
     * @brief Try to pop element from front before nsec timeout.
     *
     * @param nsec If container is empty, wait at most nsec time.
     * @param u Reference that receive popped element.
     *
     * @returns coke::Task<int> that should co_await immediately.
     * @retval coke::TOP_SUCCESS If pop successful.
     * @retval coke::TOP_TIMEOUT If timeout before container is able to pop.
     * @retval coke::TOP_ABORTED If process exit.
     * @retval coke::TOP_CLOSED If container is closed.
     * @retval Negative integer to indicate system error, almost never happens.
     * @see coke/global.h
    */
    template<typename U>
        requires std::assignable_from<U&, T&&>
    Task<int> try_pop_front_for(NanoSec nsec, U &u) {
        return pop_impl(pos_front, false, nsec, u);
    }

    /**
     * @brief Try to pop element from back before nsec timeout.
     * @see try_pop_front_for.
     */
    template<typename U>
        requires std::assignable_from<U&, T&&>
    Task<int> try_pop_back_for(NanoSec nsec, U &u) {
        return pop_impl(pos_back, false, nsec, u);
    }

    // range

    /**
     * @brief Try to push a range of elements in the back.
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
    auto try_push_back_range(Iter first, Iter last, SizeType size_hint = 0) {
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
                que.push_back(*first);
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
     * @brief Try to pop elements into range fron front.
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
    auto try_pop_front_range(Iter first, Iter last, SizeType size_hint = 0) {
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
                *first = std::move(que.front());
                que.pop_front();

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
     * @brief Try to pop at most max_pop elements from front.
     *
     * @param iter Iterator to recieve element. E.g. std::back_insert_iterator.
     * @param max_pop Max elements to pop.
     *
     * @returns Number of elements popped.
    */
    template<typename Iter>
        requires std::is_assignable_v<decltype(*std::declval<Iter>()), T&&>
                 && std::output_iterator<Iter, T>
    SizeType try_pop_front_n(Iter iter, SizeType max_pop) {
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
                *iter = std::move(que.front());
                que.pop_front();

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

private:
    bool cannot_push() const noexcept { return full() || closed(); }

    bool pop_pred() const noexcept { return !empty() || closed(); }

    bool push_pred() const noexcept { return !full() || closed(); }

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

    template<typename... Args>
    bool try_emplace_impl(bool pos, Args&&... args) {
        if (cannot_push())
            return false;

        UniqueLock lk(que_mtx);
        if (cannot_push())
            return false;

        if (pos == pos_front)
            que.emplace_front(std::forward<Args>(args)...);
        else
            que.emplace_back(std::forward<Args>(args)...);

        after_push(lk, 1);
        return true;
    }

    template<typename... Args>
    bool force_emplace_impl(bool pos, Args&&... args) {
        UniqueLock lk(que_mtx);
        if (closed())
            return false;

        if (pos == pos_front)
            que.emplace_front(std::forward<Args>(args)...);
        else
            que.emplace_back(std::forward<Args>(args)...);

        after_push(lk, 1);
        return true;
    }

    template<typename... Args>
    Task<int> emplace_impl(bool pos, bool inf, NanoSec nsec, Args&&... args) {
        if (coke::prevent_recursive_stack())
            co_await coke::yield();

        int ret = TOP_SUCCESS;

        UniqueLock lk(que_mtx);
        if (closed())
            ret = TOP_CLOSED;
        else if (full()) {
            CountGuard cg(push_wait_cnt);

            if (inf) {
                ret = co_await push_cv.wait(lk, [this]() {
                    return push_pred();
                });
            }
            else {
                ret = co_await push_cv.wait_for(lk, nsec, [this]() {
                    return push_pred();
                });
            }
        }

        if (ret == TOP_SUCCESS) {
            if (closed())
                ret = TOP_CLOSED;
            else if (pos == pos_front)
                que.emplace_front(std::forward<Args>(args)...);
            else
                que.emplace_back(std::forward<Args>(args)...);
        }

        if (ret == TOP_SUCCESS)
            after_push(lk, 1);

        co_return ret;
    }

    template<typename U>
    bool try_push_impl(bool pos, U &&u) {
        if (cannot_push())
            return false;

        UniqueLock lk(que_mtx);
        if (cannot_push())
            return false;

        if (pos == pos_front)
            que.push_front(std::forward<U>(u));
        else
            que.push_back(std::forward<U>(u));

        after_push(lk, 1);
        return true;
    }

    template<typename U>
    bool force_push_impl(bool pos, U &&u) {
        UniqueLock lk(que_mtx);
        if (closed())
            return false;

        if (pos == pos_front)
            que.push_front(std::forward<U>(u));
        else
            que.push_back(std::forward<U>(u));

        after_push(lk, 1);
        return true;
    }

    template<typename U>
    Task<int> push_impl(bool pos, bool inf, NanoSec nsec, U &&u) {
        if (coke::prevent_recursive_stack())
            co_await coke::yield();

        int ret = TOP_SUCCESS;

        UniqueLock lk(que_mtx);
        if (closed())
            ret = TOP_CLOSED;
        else if (full()) {
            CountGuard cg(push_wait_cnt);

            if (inf) {
                ret = co_await push_cv.wait(lk, [this]() {
                    return push_pred();
                });
            }
            else {
                ret = co_await push_cv.wait_for(lk, nsec, [this]() {
                    return push_pred();
                });
            }
        }

        if (ret == TOP_SUCCESS) {
            if (closed())
                ret = TOP_CLOSED;
            else if (pos == pos_front)
                que.push_front(std::forward<U>(u));
            else
                que.push_back(std::forward<U>(u));
        }

        if (ret == TOP_SUCCESS)
            after_push(lk, 1);

        co_return ret;
    }

    template<typename U>
    bool try_pop_impl(bool pos, U &u) {
        if (empty())
            return false;

        UniqueLock lk(que_mtx);
        if (empty())
            return false;

        if (pos == pos_front) {
            u = std::move(que.front());
            que.pop_front();
        }
        else {
            u = std::move(que.back());
            que.pop_back();
        }

        after_pop(lk, 1);
        return true;
    }

    template<typename U>
    Task<int> pop_impl(bool pos, bool inf, NanoSec nsec, U &u) {
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

            if (inf) {
                ret = co_await pop_cv.wait(lk, [this]() {
                    return pop_pred();
                });
            }
            else {
                ret = co_await pop_cv.wait_for(lk, nsec, [this]() {
                    return pop_pred();
                });
            }
        }

        if (ret == TOP_SUCCESS) {
            if (empty())
                ret = TOP_CLOSED;
            else if (pos == pos_front) {
                u = std::move(que.front());
                que.pop_front();
            }
            else {
                u = std::move(que.back());
                que.pop_back();
            }
        }

        if (ret == TOP_SUCCESS)
            after_pop(lk, 1);

        co_return ret;
    }

private:
    SizeType que_max_size;
    std::atomic<SizeType> que_cur_size;
    std::atomic<bool> que_closed;

    mutable std::mutex que_mtx;
    Condition push_cv;
    Condition pop_cv;

    SizeType push_wait_cnt;
    SizeType pop_wait_cnt;
    QueueType que;
};

} // namespace coke

#endif // COKE_DEQUE_H
