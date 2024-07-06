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

#ifndef COKE_QUEUE_H
#define COKE_QUEUE_H

#include <queue>
#include <stack>

#include "coke/detail/basic_concept.h"
#include "coke/queue_common.h"

namespace coke {

/**
 * @class coke::Queue
 * @brief coke::Queue is a container that gives the functionality of a queue,
 *        a FIFO data structure.
 *
 * coke::Queue derive from coke::QueueCommon, which implement most of
 * the public member functions.
 *
 * @tparam T,Container Directly used to create std::queue.
*/
template<Queueable T, typename Container = std::deque<T>>
class Queue final : public QueueCommon<Queue<T, Container>, T> {
    using BaseType = QueueCommon<Queue<T, Container>, T>;

public:
    using SizeType = typename BaseType::SizeType;
    using ContainerType = Container;
    using ValueType = typename Container::value_type;
    using QueueType = std::queue<T, Container>;

    static_assert(std::is_same_v<T, ValueType>);

    /**
     * @brief Create coke::Queue with max_size.
     *
     * @param max_size Max elements in the container. Must greater than 0.
    */
    explicit Queue(SizeType max_size) : BaseType(max_size) { }

    /**
     * @brief Create coke::Queue with max_size and alloc.
     *
     * @param max_size Max elements in the container.
     * @param alloc Allocator used to create std::queue.
    */
    template<typename Alloc>
        requires std::is_same_v<Alloc, typename Container::allocator_type>
    Queue(SizeType max_size, const Alloc &alloc)
        : BaseType(max_size), que(alloc)
    { }

    ~Queue() = default;

private:
    friend BaseType;

    template<typename... Args>
    void do_emplace(Args&&... args) {
        que.emplace(std::forward<Args>(args)...);
    }

    template<typename U>
    void do_push(U &&u) {
        que.push(std::forward<U>(u));
    }

    template<typename U>
    void do_pop(U &u) {
        u = std::move(que.front());
        que.pop();
    }

private:
    QueueType que;
};


/**
 * @class coke::PriorityQueue
 * @brief coke::PriorityQueue is a container that pop the largest(default)
 *        element, like std::priority_queue.
 *
 * coke::PriorityQueue derive from coke::QueueCommon, which implement
 * most of the public member functions.
 *
 * @tparam T,Container,Compare Directly used to create std::priority_queue.
*/
template<
    Queueable T,
    typename Container = std::vector<T>,
    typename Compare = std::less<typename Container::value_type>
>
class PriorityQueue final
    : public QueueCommon<PriorityQueue<T, Container, Compare>, T>
{
    using BaseType = QueueCommon<PriorityQueue<T, Container, Compare>, T>;

public:
    using SizeType = typename BaseType::SizeType;
    using ContainerType = Container;
    using ValueType = typename Container::value_type;
    using QueueType = std::priority_queue<T, Container, Compare>;
    using CompareType = Compare;

    static_assert(std::is_same_v<T, ValueType>);

    /**
     * @brief Create coke::PriorityQueue with max_size.
     *
     * @param max_size Max elements in the container. Must greater than 0.
    */
    explicit PriorityQueue(SizeType max_size) : BaseType(max_size) { }

    /**
     * @brief Create coke::PriorityQueue with max_size and alloc.
     *
     * @param max_size Max elements in the container.
     * @param alloc Allocator used to create std::priority_queue.
    */
    template<typename Alloc>
        requires std::is_same_v<Alloc, typename Container::allocator_type>
    PriorityQueue(SizeType max_size, const Alloc &alloc)
        : BaseType(max_size), que(alloc)
    { }

    /**
     * @brief Create coke::PriorityQueue with max_size and comp.
     *
     * @param max_size Max elements in the container.
     * @param comp CompareType value that providing a strict weak ordering.
    */
    PriorityQueue(SizeType max_size, const CompareType &comp)
        : BaseType(max_size), que(comp)
    { }

    /**
     * @brief Create coke::PriorityQueue with max_size, comp and alloc.
     *
     * @param max_size Max elements in the container.
     * @param comp CompareType value that providing a strict weak ordering.
     * @param alloc Allocator used to create std::priority_queue.
    */
    template<typename Alloc>
        requires std::is_same_v<Alloc, typename Container::allocator_type>
    PriorityQueue(SizeType max_size, const CompareType &comp,
                  const Alloc &alloc)
        : BaseType(max_size), que(comp, alloc)
    { }

    ~PriorityQueue() = default;

private:
    friend BaseType;

    template<typename... Args>
    void do_emplace(Args&&... args) {
        que.emplace(std::forward<Args>(args)...);
    }

    template<typename U>
    void do_push(U &&u) {
        que.push(std::forward<U>(u));
    }

    template<typename U>
    void do_pop(U &u) {
        u = std::move(que.top());
        que.pop();
    }

private:
    QueueType que;
};


/**
 * @class coke::Stack
 * @brief coke::Stack is a container that gives the functionality of a stack,
 *        a LIFO data structure.
 *
 * coke::Stack derive from coke::QueueCommon, which implement most of
 * the public member functions.
 *
 * @tparam T,Container Directly used to create std::stack.
*/
template<Queueable T, typename Container = std::deque<T>>
class Stack final : public QueueCommon<Stack<T, Container>, T> {
    using BaseType = QueueCommon<Stack<T, Container>, T>;

public:
    using SizeType = typename BaseType::SizeType;
    using ContainerType = Container;
    using ValueType = typename Container::value_type;
    using QueueType = std::stack<T, Container>;

    static_assert(std::is_same_v<T, ValueType>);

    /**
     * @brief Create coke::Stack with max_size.
     *
     * @param max_size Max elements in the container. Must greater than 0.
    */
    explicit Stack(SizeType max_size) : BaseType(max_size) { }

    /**
     * @brief Create coke::Stack with max_size and alloc.
     *
     * @param max_size Max elements in the container.
     * @param alloc Allocator used to create std::stack.
    */
    template<typename Alloc>
        requires std::is_same_v<Alloc, typename Container::allocator_type>
    Stack(SizeType max_size, const Alloc &alloc)
        : BaseType(max_size), que(alloc)
    { }

    ~Stack() = default;

private:
    friend BaseType;

    template<typename... Args>
    void do_emplace(Args&&... args) {
        que.emplace(std::forward<Args>(args)...);
    }

    template<typename U>
    void do_push(U &&u) {
        que.push(std::forward<U>(u));
    }

    template<typename U>
    void do_pop(U &u) {
        u = std::move(que.top());
        que.pop();
    }

private:
    QueueType que;
};

} // namespace coke

#endif // COKE_QUEUE_H
