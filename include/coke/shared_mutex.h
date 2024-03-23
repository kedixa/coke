#ifndef COKE_SHARED_MUTEX_H
#define COKE_SHARED_MUTEX_H

#include <cstdint>
#include <mutex>

#include "coke/sleep.h"

namespace coke {

class SharedTimedMutex {
    enum class State : std::uint8_t {
        Idle        = 0,
        Reading     = 1,
        Writing     = 2,
    };

public:
    template<typename Rep, typename Period>
    using Duration = std::chrono::duration<Rep, Period>;

    using count_type = std::uint32_t;

    /**
     * @brief Create a SharedTimedMutex, the rid/wid is automatically
     *        obtained from `coke::get_unique_id`.
    */
    SharedTimedMutex()
        : SharedTimedMutex(INVALID_UNIQUE_ID, INVALID_UNIQUE_ID)
    { }

    /**
     * @brief Create a SharedTimedMutex with rid/wid specified by user.
    */
    SharedTimedMutex(uint64_t rid, uint64_t wid)
        : read_doing(0), read_waiting(0), write_waiting(0),
          state(State::Idle), rid(rid), wid(wid)
    {
        if (this->rid == INVALID_UNIQUE_ID)
            this->rid = get_unique_id();
        if (this->wid == INVALID_UNIQUE_ID)
            this->wid = get_unique_id();
    }

    /**
     * @brief SharedTimedMutex is not copy or move constructible.
    */
    SharedTimedMutex(const SharedTimedMutex &) = delete;
    SharedTimedMutex &operator= (const SharedTimedMutex &) = delete;

    ~SharedTimedMutex() = default;

    /**
     * @brief Get the read unique id.
    */
    uint64_t get_rid() const { return rid; }

    /**
     * @brief Get the write unique id.
    */
    uint64_t get_wid() const { return wid; }

    /**
     * @brief Try to lock the mutex for exclusive ownership.
     *
     * @return Return true if lock success, else false.
     *
     * @pre Current coroutine doesn't owns the mutex in any mode(shared or
     *      exclusive).
    */
    bool try_lock() {
        std::lock_guard<std::mutex> lg(mtx);

        if (state == State::Idle) {
            state = State::Writing;
            return true;
        }

        return false;
    }

    /**
     * @brief Try to lock the mutex for shared ownership.
     *
     * @return Return true if lock success, else false.
     *
     * @pre Current coroutine doesn't owns the mutex in any mode(shared or
     *      exclusive).
    */
    bool try_lock_shared() {
        std::lock_guard<std::mutex> lg(mtx);

        if (state == State::Idle || state == State::Reading) {
            state = State::Reading;
            read_doing++;
            return true;
        }

        return false;
    }

    /**
     * @brief Try to upgrade the mutex from shared to exclusive ownership.
     *
     * @return If upgrade success, return true and current coroutine has
     *         exclusive ownership, else return false and current coroutine
     *         has shared ownership, just like before calling `try_upgrade`.
     *
     * @pre Current coroutine already has shared ownership.
    */
    bool try_upgrade() {
        std::lock_guard<std::mutex> lg(mtx);

        if (state == State::Reading && read_doing == 1) {
            read_doing = 0;
            state = State::Writing;
            return true;
        }

        return false;
    }

    /**
     * @brief Unlock the mutex.
     *
     * @pre Current coroutine must owns the mutex in any mode(shared or
     *      exclusive).
    */
    void unlock();

    /**
     * @brief Lock the mutex for exclusive ownership, block until success.
     *
     * @return An awaitable object that needs to be awaited immediately. The
     *         await result is an integer, which may be coke::TOP_SUCCESS,
     *         coke::TOP_TIMEOUT(for try_lock_for and try_lock_shared_for),
     *         coke::TOP_ABORTED or any negative number. See `coke/global.h`
     *         for more description.
     *
     * @pre Current coroutine doesn't owns the mutex in any mode(shared or
     *      exclusive).
    */
    [[nodiscard]]
    Task<int> lock() { return lock_impl(detail::TimedWaitHelper{}); }

    /**
     * @brief Lock the mutex for exclusive ownership, block until success or
     *        `time_duration` timeout.
     *
     * @return See `lock`.
     *
     * @param time_duration Max time to block, should be an instance of
     *        std::chrono::duration.
     *
     * @pre Current coroutine doesn't owns the mutex in any mode(shared or
     *      exclusive).
    */
    template<typename Rep, typename Period>
    [[nodiscard]]
    Task<int> try_lock_for(const Duration<Rep, Period> &time_duration) {
        using std::chrono::nanoseconds;
        auto nano = std::chrono::duration_cast<nanoseconds>(time_duration);
        return lock_impl(detail::TimedWaitHelper(nano));
    }

    /**
     * @brief Lock the mutex for shared ownership, block until success.
     *
     * @return See `lock`.
     *
     * @pre Current coroutine doesn't owns the mutex in any mode(shared or
     *      exclusive).
    */
    [[nodiscard]]
    Task<int> lock_shared() {
        detail::TimedWaitHelper h;
        return lock_shared_impl(h);
    }

    /**
     * @brief Lock the mutex for shared ownership, block until success or
     *        `time_duration` timeout.
     *
     * @param time_duration Max time to block, should be an instance of
     *        std::chrono::duration.
     *
     * @return See `lock`.
     *
     * @pre Current coroutine doesn't owns the mutex in any mode(shared or
     *      exclusive).
    */
    template<typename Rep, typename Period>
    [[nodiscard]]
    Task<int> try_lock_shared_for(const Duration<Rep, Period> &time_duration) {
        using std::chrono::nanoseconds;
        auto nano = std::chrono::duration_cast<nanoseconds>(time_duration);
        return lock_shared_impl(detail::TimedWaitHelper(nano));
    }

protected:
    Task<int> lock_impl(detail::TimedWaitHelper helper);
    Task<int> lock_shared_impl(detail::TimedWaitHelper helper);

    bool can_lock_shared() const noexcept {
        return (state == State::Idle || state == State::Reading)
               && write_waiting == 0;
    }

private:
    std::mutex mtx;
    count_type read_doing;
    count_type read_waiting;
    count_type write_waiting;
    State state;

    uint64_t rid;
    uint64_t wid;
};

} // namespace coke

#endif // COKE_SHARED_MUTEX_H
