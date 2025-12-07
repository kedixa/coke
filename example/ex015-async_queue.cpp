#include <iostream>
#include <mutex>

#include "coke/future.h"
#include "coke/queue.h"
#include "coke/sleep.h"
#include "coke/wait.h"

/**
 * This example uses asynchronous queue to distribute tasks to workers.
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

coke::Task<> do_work(int id, coke::Queue<int> &que)
{
    constexpr auto work_cost = std::chrono::milliseconds(500);
    int ret, data;

    while (true) {
        // Pop element from que one by one until que is empty and closed
        ret = co_await que.pop(data);
        if (ret == coke::TOP_CLOSED)
            break;

        print("Worker ", id, " pop ", data);

        // Use coke::sleep to simulate that we are working so hard with this
        // element
        co_await coke::sleep(work_cost);
    }

    print("Worker ", id, " exit");
}

coke::Task<> start_work(std::chrono::milliseconds interval)
{
    std::vector<coke::Future<void>> workers;
    coke::Queue<int> que(2);

    // Start two workers to pop elements from que
    for (int i = 0; i < 2; i++)
        workers.emplace_back(coke::create_future(do_work(i, que)));

    // Push an element into que for each `interval` time
    for (int i = 0; i < 8; i++) {
        if (i != 0)
            co_await coke::sleep(interval);

        print("Push ", i);
        // If try push failed, the que if full, fallback to async push
        if (!que.try_push(i)) {
            print("Queue full, use async push");
            co_await que.push(i);
        }

        print("Push success ", i);
    }

    que.close();
    print("Queue closed");

    co_await coke::wait_futures(workers, workers.size());
    print("All worker done");
}

int main()
{
    std::cout << "Example 1: Push faster than work" << std::endl;
    auto interval1 = std::chrono::milliseconds(100);
    coke::sync_wait(start_work(interval1));

    std::cout << std::string(80, '-') << std::endl;

    std::cout << "Example 2: Push slower than work" << std::endl;
    auto interval2 = std::chrono::milliseconds(300);
    coke::sync_wait(start_work(interval2));

    return 0;
}

std::string current()
{
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    std::chrono::duration<double> d = now - start;
    return "[" + std::to_string(d.count()) + "s] ";
}
