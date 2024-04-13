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
                return std::invoke(func, std::unwrap_reference_t<ARGS>(args)...);
            }
        );

        this->go_task = new detail::GoTask<T>(queue, executor, std::move(go));
        go_task->set_awaiter(this);

        this->set_task(go_task);
    }

    GoAwaiter(GoAwaiter &&that)
        : AwaiterBase(std::move(that)),
          go_task(that.go_task)
    {
        that.go_task = nullptr;

        if (this->go_task)
            this->go_task->set_awaiter(this);
    }

    GoAwaiter &operator= (GoAwaiter &&that) {
        if (this != &that) {
            this->AwaiterBase::operator=(std::move(that));

            detail::GoTask<T> *task = this->go_task;
            this->go_task = that.go_task;
            that.go_task = task;

            if (this->go_task)
                this->go_task->set_awaiter(this);

            if (that.go_task)
                that.go_task->set_awaiter(&that);
        }

        return *this;
    }

    T await_resume() {
        if constexpr (!std::is_same_v<T, void>)
            return std::move(go_task->get_result());
    }

private:
    detail::GoTask<T> *go_task;
};


template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
[[nodiscard]]
auto go(ExecQueue *queue, Executor *executor, FUNC &&func, ARGS&&... args) {
    using result_t = std::remove_cvref_t<std::invoke_result_t<FUNC, ARGS...>>;

    return GoAwaiter<result_t>(queue, executor,
                               std::forward<FUNC>(func),
                               std::forward<ARGS>(args)...);
}

template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
[[nodiscard]]
auto go(const std::string &name, FUNC &&func, ARGS&&... args) {
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
