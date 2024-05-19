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

#ifndef COKE_GENERIC_AWAITER_H
#define COKE_GENERIC_AWAITER_H

#include <optional>
#include <type_traits>

#include "coke/detail/awaiter_base.h"

namespace coke {

/**
 * GenericAwaiter is used with task types under workflow framework but not
 * currently supported by coke. For example:
 * 
 *  class MyTask : public SubTask {
 *  public:
 *      void set_callback(std::function<void(MyTask *)>);
 *      // ... other functions
 *  };
 *
 *  Then we can use GenericAwaiter like this:
 *  coke::Task<> f() {
 *      GenericAwaiter<int> awaiter;
 * 
 *      MyTask *task = create_mytask(some args ...);
 *      task->set_callback([&awaiter](MyTask *task) {
 *          // do something and finally call set_result and done
 *          awaiter.set_result(task->get_int());
 *          awaiter.done();
 *      });
 * 
 *      awaiter.take_over(task);
 *
 *      int ret = co_await awaiter;
 *  }
*/
template<typename T>
class GenericAwaiter;

template<>
class GenericAwaiter<void> : public AwaiterBase {
public:
    GenericAwaiter() = default;

    /**
     * @brief GenericAwaiter can neither be copied nor moved. See BasicAwaiter
     *        if you need a movable awaiter.
    */
    GenericAwaiter(GenericAwaiter &&) = delete;

    void await_resume() { }

    /**
     * @brief Resume the coroutine, usually call it on task's callback.
     * @attention GenericAwaiter::done should be called EXACTLY ONCE.
    */
    void done() noexcept override {
        AwaiterBase::done();
    }

    void set_result() { }
    void emplace_result() { }

    /**
     * @brief Take over ownership of `task`. Do not perform other operations on
     *        task after calling take_over.
     * @attention If continue to operate the task after take_over, make sure
     *            you understand the life cycle of the workflow task.
    */
    void take_over(SubTask *task) {
        set_task(task, false);
    }
};

template<typename T>
class GenericAwaiter : public AwaiterBase {
public:
    GenericAwaiter() = default;

    /**
     * @brief GenericAwaiter can neither be copied nor moved. See BasicAwaiter
     *        if you need a movable awaiter.
    */
    GenericAwaiter(GenericAwaiter &&) = delete;

    /**
     * @brief Set result into GenericAwaiter.
     * @attention Set or emplace result should be called EXACTLY ONCE.
    */
    template<typename... ARGS>
        requires std::is_constructible_v<T, ARGS...>
    void emplace_result(ARGS&&... args) {
        opt.emplace(std::forward<ARGS>(args)...);
    }

    void set_result(T &&t) {
        opt = std::move(t);
    }

    void set_result(const T &t) {
        opt = t;
    }

    /**
     * @brief Resume the coroutine, usually call it on task's callback.
     * @attention GenericAwaiter::done should be called EXACTLY ONCE.
    */
    void done() noexcept override {
        AwaiterBase::done();
    }

    /**
     * @brief Take over ownership of `task`. Do not perform other operations on
     *        task after calling take_over.
     * @attention If continue to operate the task after take_over, make sure
     *            you understand the life cycle of the workflow task.
    */
    virtual void take_over(SubTask *task) {
        set_task(task, false);
    }

    /**
     * @brief Return the co_await result set by emplace_result/set_result.
    */
    T await_resume() {
        return std::move(opt.value());
    }

protected:
    std::optional<T> opt;
};

} // namespace coke

#endif // COKE_GENERIC_AWAITER_H
