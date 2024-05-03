#ifndef COKE_MAKE_TASK_H
#define COKE_MAKE_TASK_H

#include "coke/detail/task.h"

namespace coke {

template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
auto make_task(FUNC &&func, ARGS&&... args) {
    using result_t = std::invoke_result_t<FUNC, ARGS...>;
    static_assert(is_task_v<result_t>, "FUNC(ARGS...) must return coke::Task<T>");

    result_t res;
    if constexpr (std::is_rvalue_reference_v<FUNC&&>) {
        std::shared_ptr<FUNC> ctx = std::make_shared<FUNC>(std::forward<FUNC>(func));
        res = std::invoke(*ctx, std::forward<ARGS>(args)...);
        res.set_context(ctx);
    }
    else {
        res = std::invoke(std::forward<FUNC>(func), std::forward<ARGS>(args)...);
    }

    return res;
}

} // namespace coke

#endif // COKE_MAKE_TASK_H
