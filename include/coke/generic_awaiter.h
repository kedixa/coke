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
     * GenericAwaiter can neither be copied nor moved. This is to avoid
     * co_await with the new awaiter object, but call set_result or done
     * with the old object(which is already in unknown state if moved).
    */
    GenericAwaiter(GenericAwaiter &&) = delete;

    void await_resume() { }

    /**
     * GenericAwaiter::done should be called EXACTLY ONCE.
    */
    void done() noexcept override {
        AwaiterBase::done();
    }

    void set_result() { }
    void emplace_result() { }

    void take_over(SubTask *task) {
        set_task(task, false);
    }
};

template<typename T>
class GenericAwaiter : public AwaiterBase {
public:
    GenericAwaiter() = default;

    /**
     * GenericAwaiter can neither be copied nor moved. This is to avoid
     * co_await with the new awaiter object, but call set_result or done
     * with the old object(which is already in unknown state if moved).
    */
    GenericAwaiter(GenericAwaiter &&) = delete;

    /**
     * Save result into GenericAwaiter.
     * emplace_result should be called before done.
     * Set or emplace result should be called EXACTLY ONCE.
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
     * Wake up coroutine which is wating on *this.
     * GenericAwaiter::done should be called EXACTLY ONCE.
    */
    void done() noexcept override {
        AwaiterBase::done();
    }

    /**
     * Take over ownership of `task`, after take_over, any operation on task
     * lead to undefined behavior.
    */
    virtual void take_over(SubTask *task) {
        set_task(task, false);
    }

    T await_resume() {
        return std::move(opt.value());
    }

protected:
    std::optional<T> opt;
};

} // namespace coke

#endif // COKE_GENERIC_AWAITER_H
