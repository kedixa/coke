#ifndef COKE_LATCH_H
#define COKE_LATCH_H

#include <vector>
#include <mutex>

#include "coke/detail/awaiter_base.h"

namespace coke {

class LatchAwaiter : public BasicAwaiter<void> {
private:
    LatchAwaiter() { }
    explicit LatchAwaiter(SubTask *task);

    friend class Latch;
};

class Latch final {
public:
    /**
     * Create a latch that can be counted EXACTLY `n` times.
     * n should >= 0
    */
    explicit Latch(long n) : expected(n) { }
    ~Latch() { }

    Latch(const Latch &) = delete;

    /**
     * See Latch::wait.
     * 
     *  Latch l(...);
     * 
     *  co_await l; is same as co_await l.wait();
    */
    [[nodiscard]] LatchAwaiter operator co_await() {
        return wait();
    }

    /**
     * Return an awaiter, co_await the awaiter will wait for the Latch
     * count down to zero. If already zero, co_await the awaiter will
     * return immediately.
     *
     * The return value must be awaited, otherwise undefined behavior.
     */
    [[nodiscard]] LatchAwaiter wait() noexcept {
        return create_awaiter(0);
    }

    /**
     * Count down the Latch and return an awaiter, co_await the awaiter
     * will wait for the Latch count down to zero. If already zero,
     * co_await the awaiter will return immediately.
     * 
     * The sum of all n's in `arrive_and_wait` and `count_down` MUST equal
     * to the `n` passed into Latch's constructor.
     *
     * The return value must be awaited, otherwise undefined behavior.
    */
    [[nodiscard]] LatchAwaiter arrive_and_wait(long n = 1) noexcept {
        return create_awaiter(n);
    }

    /**
     * Count down the Latch. n should >= 1.
     *
     * The sum of all n's in `arrive_and_wait` and `count_down` MUST equal
     * to the `n` passed into Latch's constructor.
    */
    void count_down(long n = 1) noexcept;

private:
    LatchAwaiter create_awaiter(long n) noexcept;

    static void wake_up(std::vector<SubTask *> tasks) noexcept;

private:
    std::mutex mtx;
    long expected;
    std::vector<SubTask *> tasks;
};

} // namespace coke

#endif // COKE_LATCH_H
