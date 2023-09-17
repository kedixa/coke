#include <iostream>
#include <chrono>
#include <atomic>

#include "coke/future.h"
#include "coke/coke.h"

using std::chrono::milliseconds;

struct StopToken {
    std::atomic<bool> stop_wait{false};
    std::atomic<int> progress{0};
};

std::string current();

/**
 * This example uses coke::Future to show how to wait for an asynchronous result,
 * and do different things depending on whether the wait times out.
 *
 * NOTICE: StopToken is not coupled with coke::Future. coke::Future/coke::Promise
 * aims to provide a mechanism that supports timeout and waits for asynchronous
 * results. Users can add additional auxiliary methods according to actual
 * conditions (such as StopToken in this example) to make things better.
*/

coke::Task<> process(StopToken &token, coke::Promise<int> p) {
    // Suppose this is a complex routine, it needs many steps to finish work

    // Because each step can take a long time, we use `token` to inform the
    // caller of our progress

    // Step 1
    token.progress = 1;
    co_await coke::sleep(milliseconds(200));
    // after each step, check to see if the caller has given up waiting
    if (token.stop_wait) {
        p.set_value(-1);
        co_return;
    }

    // Step 2
    token.progress = 2;
    co_await coke::sleep(milliseconds(300));
    if (token.stop_wait) {
        p.set_value(-1);
        co_return;
    }

    // Step 3
    token.progress = 3;
    co_await coke::sleep(milliseconds(500));
    // WOW! We finish the work
    p.set_value(1);
}

coke::Task<> wait_process(int first_ms, int second_ms) {
    StopToken token;
    coke::Promise<int> pro;
    coke::Future<int> fut = pro.get_future();
    int ret;

    // 1. Start process
    process(token, std::move(pro)).start();
    std::cout << current() << "Wait process\n";

    // 2. Wait for `first_ms` milliseconds
    ret = co_await fut.wait_for(milliseconds(first_ms));

    if (ret == coke::FUTURE_STATE_TIMEOUT) {
        std::cout << current() << "Wait timeout after first_ms, on progress "
                  << token.progress << "\n";

        // 3. Although it is not completed yet, it has reached a critical step.
        //    We can tolerate it for `second_ms` millisecond and try not to
        //    waste all our efforts.
        if (token.progress >= 3) {
            std::cout << current() << "Try to wait second_ms\n";
            ret = co_await fut.wait_for(milliseconds(second_ms));
        }
    }

    if (ret == coke::FUTURE_STATE_READY) {
        std::cout << current() << "Future is ready now\n";
    }
    else {
        // 4. The work progress is very slow, terminate the wait,
        //    notify it to end early so as not to waste more resources
        std::cout << current() << "Future is not ready, try to stop wait\n";
        token.stop_wait = true;

        // `process` has a reference to local `token` variable, we should wait
        // until it finish, otherwise there is no need to wait promise if
        // we don't need the value.

        // wait until promise set_value
        ret = co_await fut.wait();

        int fret = -1;
        if (ret == coke::FUTURE_STATE_READY)
            fret = fut.get();

        std::cout << current() << "Wait finished and future get " << fret << std::endl;
    }
}

int main() {
    std::cout << current() << "First case:\n";
    coke::sync_wait(wait_process(300, 300));

    std::cout << "\n";
    std::cout << current() << "Second case:\n";
    coke::sync_wait(wait_process(800, 300));

    return 0;
}

std::string current() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> d = now - start;
    return "[" + std::to_string(d.count()) + "s] ";
}
