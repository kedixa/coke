#include <chrono>
#include <iostream>
#include <mutex>
#include <string>

#include "coke/sleep.h"
#include "coke/wait.h"
#include "coke/wait_group.h"

/**
 * This example shows the basic usage of coke::WaitGroup.
 */

std::mutex print_mtx;

std::string current();

template<typename... Args>
void print(const Args &...args)
{
    std::lock_guard<std::mutex> lg(print_mtx);
    std::cout << current();
    (std::cout << ... << args) << std::endl;
}

coke::Task<> subworker(coke::WaitGroup &wg, int i)
{
    auto ms = std::chrono::milliseconds(i * 100);
    co_await coke::sleep(ms);

    print("SubWorker ", i, " done");
    wg.done();
}

coke::Task<> worker(coke::WaitGroup &wg, int i)
{
    auto ms = std::chrono::milliseconds(i * 100);
    co_await coke::sleep(ms);

    if (i % 2 == 0) {
        // The worker can add count to wg and then detach subworkers,
        // but just before wg.done is called.
        wg.add(1);

        print("Detach subworker ", i);
        coke::detach(subworker(wg, i));
    }

    print("Worker ", i, " done");
    wg.done();

    // WaitGroup::done usually should be called before the coroutine ends.
    // After done is called, wg maybe destroyed, DO NOT use it any more.
}

coke::Task<> async_main(int nworkers)
{
    coke::WaitGroup wg;

    for (int i = 0; i < nworkers; i++) {
        wg.add(1);

        print("Detach worker ", i);
        coke::detach(worker(wg, i));
    }

    co_await wg.wait();
    print("Wait done");
}

int main()
{
    coke::sync_wait(async_main(6));
    return 0;
}

std::string current()
{
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    std::chrono::duration<double> d = now - start;
    return "[" + std::to_string(d.count()) + "s] ";
}
