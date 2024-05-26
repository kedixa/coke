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

#ifndef COKE_DETAIL_TASK_H
#define COKE_DETAIL_TASK_H

#include <coroutine>
#include <memory>
#include <optional>
#include <exception>

#include "coke/detail/basic_concept.h"

namespace coke::detail {

/**
 * @brief Base class for coke::Task<T>::promise_type
*/
class PromiseBase {
    using void_handle = std::coroutine_handle<>;

public:
    PromiseBase() = default;
    PromiseBase(const PromiseBase &) = delete;
    PromiseBase(PromiseBase &&) = delete;
    virtual ~PromiseBase() { }

    void set_context(std::shared_ptr<void> ctx) noexcept { context = ctx; }

    void_handle previous_handle() const noexcept { return prev; }
    void set_previous_handle(void_handle prev) noexcept { this->prev = prev; }

    void set_series(void *series) noexcept { this->series = series; }
    void *get_series() const noexcept { return this->series; }

    void set_detached(bool detached) noexcept { this->detached = detached; }
    bool get_detached() const noexcept { return detached; }

    void unhandled_exception() { eptr = std::current_exception(); }

protected:
    std::exception_ptr eptr{nullptr};

private:
    std::shared_ptr<void> context;
    void_handle prev;
    void *series{nullptr};
    bool detached{false};
};

template<Cokeable T>
class CoPromise;


template<Cokeable T>
struct FinalAwaiter {
    using promise_type = CoPromise<T>;
    using void_handle = std::coroutine_handle<>;
    using handle_type = std::coroutine_handle<promise_type>;

    FinalAwaiter(bool detached) : detached(detached) { }

    bool await_ready() noexcept { return detached; }
    void await_resume() noexcept { }

    void_handle await_suspend(handle_type h) noexcept {
        auto prev = h.promise().previous_handle();
        if (prev)
            return prev;

        return std::noop_coroutine();
    }

private:
    bool detached{false};
};


template<Cokeable T>
struct TaskAwaiter {
    using promise_type = CoPromise<T>;
    using handle_type = std::coroutine_handle<promise_type>;

    TaskAwaiter(handle_type hdl) : hdl(hdl) { }

    bool await_ready() noexcept { return false; }

    T await_resume() {
        if constexpr (std::is_same_v<T, void>)
            return hdl.promise().value();
        else
            return std::move(hdl.promise().value());
    }

    template<typename U>
    auto await_suspend(std::coroutine_handle<CoPromise<U>> h) {
        CoPromise<T> &p = hdl.promise();
        p.set_previous_handle(h);
        p.set_series(h.promise().get_series());
        return hdl;
    }

private:
    handle_type hdl;
};


// clang deduce failed if use Cokeable

template<typename T=void>
class Task {
public:
    using promise_type = CoPromise<T>;
    using handle_type = std::coroutine_handle<promise_type>;

    Task() { }
    Task(Task &&that) : hdl(that.hdl) { that.hdl = nullptr; }
    ~Task() { if (hdl) hdl.destroy(); }

    Task &operator=(Task &&that) noexcept {
        if (this != &that) {
            auto h = this->hdl;
            this->hdl = that.hdl;
            that.hdl = h;
        }

        return *this;
    }

    /**
     * co_await coke::Task to start and await the result.
     * If this function is called, the returned TaskAwaiter must be awaited.
    */
    [[nodiscard]]
    TaskAwaiter<T> operator co_await() noexcept { return TaskAwaiter<T>{hdl}; }

    /**
     * Start and detach this task, without waiting.
    */
    void start() {
        handle_type h = hdl;
        hdl = nullptr;

        h.promise().set_detached(true);
        h.resume();
    }

    /**
     * Start and detach this task on `series`, without waiting.
     * series is already started before calling this, this function
     * is mostly used when we are in server's process.
    */
    void start_on_series(void *series) {
        handle_type h = hdl;
        hdl = nullptr;

        h.promise().set_series(series);
        h.promise().set_detached(true);
        h.resume();
    }

    /**
     * `ctx` is a type-erased object, may be useful in the following scenarios:
     *
     *  std::string s = "Hello";
     *
     *  auto &&task = [=] () -> coke::Task<> { do something with s }();
     *  task.start();   // Oooooooooops!!! The lambda is destroyed,
     *                  // use captured `s` will crash
     *
     * The following method will save callable object in ctx, avoid the problem
     * of captured content being released.
     *  coke::make_task([=]() -> coke::Task<> { ... });
     * 
     * There may be a better way, welcome to discuss.
    */
    void set_context(std::shared_ptr<void> ctx) noexcept {
        hdl.promise().set_context(ctx);
    }

private:
    Task(handle_type hdl) : hdl(hdl) { }

private:
    handle_type hdl;
    friend promise_type;
};


template<Cokeable T>
class CoPromise : public PromiseBase {
    using handle_type = std::coroutine_handle<CoPromise<T>>;

public:
    auto get_return_object() noexcept {
        return Task<T>(handle_type::from_promise(*this));
    }

    auto initial_suspend() noexcept { return std::suspend_always{}; }
    auto final_suspend() noexcept { return FinalAwaiter<T>(get_detached()); }

    void return_value(const T &t)
        noexcept(std::is_nothrow_copy_constructible_v<T>)
    {
        result.emplace(t);
    }

    void return_value(T &&t)
        noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        result.emplace(std::move(t));
    }

    T &value() & {
        if (eptr)
            std::rethrow_exception(eptr);
        return result.value();
    }

private:
    std::optional<T> result;
};


template<>
class CoPromise<void> : public PromiseBase {
    using handle_type = std::coroutine_handle<CoPromise<void>>;

public:
    auto get_return_object() noexcept {
        return Task<void>(handle_type::from_promise(*this));
    }

    auto initial_suspend() noexcept { return std::suspend_always{}; }
    auto final_suspend() noexcept { return FinalAwaiter<void>(get_detached()); }
    void return_void() noexcept { }

    void value() {
        if (eptr)
            std::rethrow_exception(eptr);
    }
};


template<typename T>
struct task_helper {
    constexpr static bool value = false;
};


template<typename T>
struct task_helper<Task<T>> {
    using inner_type = T;
    constexpr static bool value = true;
};

} // namespace coke::detail


namespace coke {

using detail::Task;

/**
 * @brief Check whether T is coke::Task type.
*/
template<typename T>
constexpr inline bool is_task_v = detail::task_helper<T>::value;

/**
 * If T is coke::Task<U>, then task_inner_t is U
*/
template<typename T>
using task_inner_t = typename detail::task_helper<T>::inner_type;

} // namespace coke

#endif //COKE_DETAIL_TASK_H
