#ifndef COKE_BASIC_H
#define COKE_BASIC_H

#include <functional>
#include <concepts>
#include <memory>

/**
 * coke.h includes most of the headers, except for network-related parts
*/

#include "coke/global.h"
#include "coke/fileio.h"
#include "coke/go.h"
#include "coke/latch.h"
#include "coke/sleep.h"
#include "coke/qps_pool.h"
#include "coke/wait.h"
#include "coke/generic_awaiter.h"
#include "coke/series.h"
#include "coke/semaphore.h"
#include "coke/mutex.h"
#include "coke/shared_mutex.h"

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

template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
auto sync_call(FUNC &&func, ARGS&&... args) {
    return sync_wait(make_task(std::forward<FUNC>(func), std::forward<ARGS>(args)...));
}

} // namespace coke

#endif // COKE_BASIC_H
