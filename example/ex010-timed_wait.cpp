#include <atomic>
#include <chrono>
#include <iostream>

#include "coke/coke.h"
#include "coke/future.h"

using std::chrono::milliseconds;

std::string current();

/**
 * This example uses coke::Future to show how to wait for an asynchronous
 * result, and do different things depending on whether the wait times out.
 */

coke::Task<int> process(std::atomic<bool> &stop, std::atomic<int> &cur_step)
{
    // Suppose this is a complex routine, it needs many steps to finish work

    // Because each step can take a long time, we use `cur_step` to inform the
    // caller of our progress

    for (int step = 1; step <= 3; step++) {
        cur_step = step;
        if (step != 1 && stop) {
            // The caller has given up waiting, and we didn't finish the work
            co_return -1;
        }

        // Do async works
        co_await coke::sleep(milliseconds(200));
    }

    // WOW! We finish the work
    co_return 1;
}

coke::Task<> wait_process(int first_ms, int second_ms)
{
    std::atomic<bool> stop_flag{false};
    std::atomic<int> cur_step{0};
    int ret;
    int process_ret = -2;

    // 1. Start process
    coke::Future<int> fut = coke::create_future(process(stop_flag, cur_step));
    std::cout << current() << "Wait process\n";

    // 2. Wait for `first_ms` milliseconds
    ret = co_await fut.wait_for(milliseconds(first_ms));

    if (ret == coke::FUTURE_STATE_TIMEOUT) {
        std::cout << current() << "Wait timeout after " << first_ms
                  << "ms, on progress " << cur_step << "\n";

        // 3. Although it is not completed yet, it has reached a critical step.
        //    We can tolerate it for `second_ms` millisecond and try not to
        //    waste all our efforts
        if (cur_step >= 3) {
            std::cout << current() << "Try to wait another " << second_ms
                      << "ms\n";
            ret = co_await fut.wait_for(milliseconds(second_ms));
        }
    }

    if (ret == coke::FUTURE_STATE_READY) {
        process_ret = fut.get();
        std::cout << current() << "Future is ready now, result is "
                  << process_ret << "\n";
    }
    else {
        // 4. The work progress is very slow, terminate the wait,
        //    notify it to end early so as not to waste more resources
        stop_flag = true;
        std::cout << current() << "Future is not ready, try to stop wait\n";

        // `process` has a reference to local variable, we should wait
        // until it finish

        ret = co_await fut.wait();

        if (ret == coke::FUTURE_STATE_READY)
            process_ret = fut.get();

        std::cout << current() << "Wait finished and process returns "
                  << process_ret << std::endl;
    }
}

int main()
{
    std::cout << current() << "First case:\n";
    coke::sync_wait(wait_process(300, 200));

    std::cout << "\n";
    std::cout << current() << "Second case:\n";
    coke::sync_wait(wait_process(500, 200));

    return 0;
}

std::string current()
{
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> d = now - start;
    return "[" + std::to_string(d.count()) + "s] ";
}
