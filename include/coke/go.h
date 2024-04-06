#ifndef COKE_GO_H
#define COKE_GO_H

#include <string_view>

#include "coke/detail/go_task.h"

namespace coke {

// Default ExecQueue name when use coke::go without name
constexpr std::string_view GO_DEFAULT_QUEUE{"coke:go"};

template<typename T>
class GoAwaiter : public AwaiterBase {
public:
    template<typename FUNC, typename... ARGS>
        requires std::invocable<FUNC, ARGS...>
    GoAwaiter(ExecQueue *queue, Executor *executor,
              FUNC &&func, ARGS&&... args)
    {
        std::function<T()> go(
            [func=std::forward<FUNC>(func), ...args=std::forward<ARGS>(args)]() mutable {
                return std::invoke(std::forward<std::decay_t<FUNC>>(func),
                                   std::forward<std::decay_t<ARGS>>(args)...);
            }
        );

        auto *task = new detail::GoTask<T>(queue, executor, std::move(go));
        task->awaiter = this;

        this->awaiter_ptr = &(task->awaiter);
        this->res_ptr = &(task->result);
        this->set_task(task);
    }

    GoAwaiter(GoAwaiter &&that)
        : AwaiterBase(std::move(that)),
          awaiter_ptr(that.awaiter_ptr),
          res_ptr(that.res_ptr)
    {
        that.awaiter_ptr = nullptr;
        that.res_ptr = nullptr;

        if (this->awaiter_ptr)
            *(this->awaiter_ptr) = this;
    }

    GoAwaiter &operator= (GoAwaiter &&that) {
        if (this != &that) {
            this->AwaiterBase::operator=(std::move(that));

            auto pa = this->awaiter_ptr;
            this->awaiter_ptr = that.awaiter_ptr;
            that.awaiter_ptr = pa;

            auto pr = this->res_ptr;
            this->res_ptr = that.res_ptr;
            that.res_ptr = pr;

            if (this->awaiter_ptr)
                *(this->awaiter_ptr) = this;

            if (that.awaiter_ptr)
                *(that.awaiter_ptr) = &that;
        }

        return *this;
    }

    T await_resume() {
        return std::move(res_ptr->value());
    }

private:
    AwaiterBase **awaiter_ptr;
    std::optional<T> *res_ptr;
};

template<>
class GoAwaiter<void> : public AwaiterBase {
public:
    template<typename FUNC, typename... ARGS>
        requires std::invocable<FUNC, ARGS...>
    GoAwaiter(ExecQueue *queue, Executor *executor,
              FUNC &&func, ARGS&&... args)
    {
        std::function<void()> go(
            [func=std::forward<FUNC>(func), ...args=std::forward<ARGS>(args)]() mutable {
                return std::invoke(std::forward<std::decay_t<FUNC>>(func),
                                   std::forward<std::decay_t<ARGS>>(args)...);
            }
        );

        auto *task = new detail::GoTask<void>(queue, executor, std::move(go));
        task->awaiter = this;

        this->awaiter_ptr = &(task->awaiter);
        this->set_task(task);
    }

    GoAwaiter(GoAwaiter &&that)
        : AwaiterBase(std::move(that)),
          awaiter_ptr(that.awaiter_ptr)
    {
        that.awaiter_ptr = nullptr;

        if (this->awaiter_ptr)
            *(this->awaiter_ptr) = this;
    }

    GoAwaiter &operator= (GoAwaiter &&that) {
        if (this != &that) {
            this->AwaiterBase::operator=(std::move(that));

            auto pa = this->awaiter_ptr;
            this->awaiter_ptr = that.awaiter_ptr;
            that.awaiter_ptr = pa;

            if (this->awaiter_ptr)
                *(this->awaiter_ptr) = this;

            if (that.awaiter_ptr)
                *(that.awaiter_ptr) = &that;
        }

        return *this;
    }

    void await_resume() { }

private:
    AwaiterBase **awaiter_ptr;
};


template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
[[nodiscard]]
auto go(ExecQueue *queue, Executor *executor,
        FUNC &&func, ARGS&&... args) {
    using result_t = std::remove_cvref_t<std::invoke_result_t<FUNC, ARGS...>>;

    return GoAwaiter<result_t>(queue, executor,
                               std::forward<FUNC>(func),
                               std::forward<ARGS>(args)...);
}

template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
[[nodiscard]]
auto go(const std::string &name,
        FUNC &&func, ARGS&&... args) {
    auto *queue = detail::get_exec_queue(name);
    auto *executor = detail::get_compute_executor();

    return go(queue, executor,
              std::forward<FUNC>(func),
              std::forward<ARGS>(args)...);
}

template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
[[nodiscard]]
auto go(FUNC &&func, ARGS&&... args) {
    return go(std::string(GO_DEFAULT_QUEUE),
              std::forward<FUNC>(func),
              std::forward<ARGS>(args)...);
}

/**
 * @brief Switch to a thread with `queue` and `executor`
*/
[[nodiscard]]
inline auto switch_go_thread(ExecQueue *queue, Executor *executor) {
    return go(queue, executor, []{});
}

/**
 * @brief Switch to a thread in the compute thread pool with `name`
*/
[[nodiscard]]
inline auto switch_go_thread(const std::string &name) {
    return go(name, []{});
}

/**
 * @brief Switch to a thread in the compute thread pool with default name
*/
[[nodiscard]]
inline auto switch_go_thread() {
    return go([]{});
}

} // namespace coke

#endif // COKE_GO_H
