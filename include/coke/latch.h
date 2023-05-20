#ifndef COKE_LATCH_H
#define COKE_LATCH_H

#include <vector>
#include <memory>
#include <cassert>

#include "coke/detail/awaiter_base.h"

namespace coke {

class LatchAwaiter : public AwaiterBase {
public:
    explicit LatchAwaiter(void *task);
    void await_resume() { }
};

class Latch {
public:
    // n should less than unsigned max
    /**
     * Create a latch that can be counted EXACTLY `n` times.
     * n should less than (unsigned max - 1)
    */
    explicit Latch(size_t n);
    virtual ~Latch();

    /**
     * The return value must be awaited, otherwise it will cause memory
     * leak or program crash. co_await should be called EXACTLY once.
     * */
    [[nodiscard]] LatchAwaiter operator co_await() {
        awaited = true;
        return LatchAwaiter(task);
    }

    virtual void count_down();

protected:
    void *task;
    bool awaited{false};
};

} // namespace coke

#endif // COKE_LATCH_H
