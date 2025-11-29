#include <atomic>
#include <chrono>
#include <iostream>

#include "coke/sleep.h"
#include "coke/stop_token.h"
#include "coke/wait.h"

/**
 * This example starts a background coroutine that outputs `counter` every
 * interval time and exits immediately when the main tasks are done.
 */

std::atomic<std::size_t> counter{0};

coke::Task<> background(coke::StopToken &tk, std::chrono::nanoseconds interval)
{
    // call tk.set_finished() when this coroutine is finished
    coke::StopToken::FinishGuard guard(&tk);
    bool stop;

    do {
        stop = co_await tk.wait_stop_for(interval);
        std::cout << "Counter is " << counter.load() << std::endl;
    } while (!stop);

    std::cout << "Stop background counter" << std::endl;
}

coke::Task<> async_main()
{
    coke::StopToken tk(1);
    auto interval = std::chrono::seconds(1);

    // Detach background clock
    coke::detach(background(tk, interval));

    // Do some work
    for (int i = 0; i < 7; i++) {
        co_await coke::sleep(0.3);
        ++counter;
    }

    // Stop background clock, and wait until it finishes
    tk.request_stop();
    co_await tk.wait_finish();
}

int main()
{
    coke::sync_wait(async_main());
    return 0;
}
