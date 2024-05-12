#ifndef COKE_CONDITION_H
#define COKE_CONDITION_H

#include <functional>
#include <mutex>

#include "coke/sleep.h"

namespace coke {

class TimedCondition {
public:
    /**
     * @brief Create a TimedCondition.
    */
    TimedCondition() : uid(get_unique_id()) { }

    TimedCondition(const TimedCondition &) = delete;
    TimedCondition &operator=(const TimedCondition &) = delete;

    ~TimedCondition() = default;

    /**
     * @brief Blocks the current coroutine until the condition is awakened.
     *
     * @return An awaitable object that needs to be awaited immediately. The
     *         await result is an integer, which is almost always
     *         coke::TOP_SUCCESS, unless the sleep task fail, this
     *         will return coke::TOP_ABORTED or any negative numbers. See
     *         `coke/global.h` for more description.
    */
    [[nodiscard]]
    Task<int> wait(std::unique_lock<std::mutex> &lock) { 
        return wait_impl(lock, detail::TimedWaitHelper{});
    }

    /**
     * @brief Blocks the current coroutine until the condition is awakened and
     *        pred() returns true. pred
     *
     * @return Same as wait(lock);
    */
    [[nodiscard]]
    Task<int> wait(std::unique_lock<std::mutex> &lock,
                   std::function<bool()> pred) {
        return wait_impl(lock, detail::TimedWaitHelper{}, std::move(pred));
    }

    /**
     * @brief Blocks the current coroutine until the condition is awakened or
     *        `nsec` timeout.
     *
     * @return An awaitable object that needs to be awaited immediately. The
     *         await result is an integer, which may be
     *         coke::TOP_SUCCESS if wake up before timeout,
     *         coke::TOP_TIMEOUT if wait timeout,
     *         coke::TOP_ABORTED if process exit,
     *         any negative numbers if there are system errors.
    */
    [[nodiscard]]
    Task<int> wait_for(std::unique_lock<std::mutex> &lock,
                       const NanoSec &nsec) { 
        return wait_impl(lock, detail::TimedWaitHelper{nsec});
    }

    /**
     * @brief Blocks the current coroutine until the condition is (awakened and
     *        pred returns true) or `nsec` timeout.
     *
     * @return Same as wait_for(lock, nsec), but coke::TOP_SUCCESS is returned
     *         only if `pred` returns true.
     */
    [[nodiscard]]
    Task<int> wait_for(std::unique_lock<std::mutex> &lock, const NanoSec &nsec,
                       std::function<bool()> pred) {
        return wait_impl(lock, detail::TimedWaitHelper{nsec}, std::move(pred));
    }

    /**
     * @brief If any coroutines are waiting on *this, calling notify_one
     *        unblocks one of them.
    */
    void notify_one() { notify(1); }

    /**
     * @brief If any coroutines are waiting on *this, calling notify
     *        unblocks n of them.
     *
     * @param n The number of coroutines waiting on *this to wakeup, if less
     *        than n, wakeup all.
    */
    void notify(std::size_t n) { cancel_sleep_by_id(uid, n); }

    /**
     * @brief If any coroutines are waiting on *this, calling notify_all
     *        unblocks all of them.
    */
    void notify_all() { cancel_sleep_by_id(uid); }

private:
    Task<int> wait_impl(std::unique_lock<std::mutex> &lock,
                        detail::TimedWaitHelper helper);

    Task<int> wait_impl(std::unique_lock<std::mutex> &lock,
                        detail::TimedWaitHelper helper,
                        std::function<bool()> pred);

private:
    uint64_t uid;
};

} // namespace coke

#endif // COKE_CONDITION_H
