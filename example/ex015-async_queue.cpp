#include <iostream>
#include <syncstream>

#include "coke/future.h"
#include "coke/queue.h"
#include "coke/sleep.h"
#include "coke/wait.h"

/**
 * This example uses asynchronous queue to distribute tasks to workers.
 */

std::string current();

coke::Task<> do_work(int id, coke::Queue<int> &que) {
    constexpr auto work_cost = std::chrono::milliseconds(500);
    std::osyncstream os(std::cout);
    int ret, data;

    std::emit_on_flush(os);

    while (true) {
        // Pop element from que one by one until que is empty and closed
        ret = co_await que.pop(data);
        if (ret == coke::TOP_CLOSED)
            break;

        os << current() << "Worker " << id << " pop " << data << std::endl;

        // Use coke::sleep to simulate that we are working so hard with this
        // element
        co_await coke::sleep(work_cost);
    }

    os << current() << "Worker " << id << " exit" << std::endl;
}

coke::Task<> start_work(std::chrono::milliseconds interval) {
    std::osyncstream os(std::cout);
    std::vector<coke::Future<void>> workers;
    coke::Queue<int> que(2);

    std::emit_on_flush(os);

    // Start two workers to pop elements from que
    for (int i = 0; i < 2; i++)
        workers.emplace_back(coke::create_future(do_work(i, que)));

    // Push an element into que for each `interval` time
    for (int i = 0; i < 8; i++) {
        if (i != 0)
            co_await coke::sleep(interval);

        os << current() << "Push " << i << std::endl;
        // If try push failed, the que if full, fallback to async push
        if (!que.try_push(i)) {
            os << current() << "Queue full, use async push" << std::endl;
            co_await que.push(i);
        }

        os << current() << "Push success " << i << std::endl;
    }

    que.close();
    os << current() << "Queue closed" << std::endl;

    co_await coke::wait_futures(workers, workers.size());
    os << current() << "All worker done" << std::endl;
}

int main() {
    std::cout << "Example 1: Push faster than work" << std::endl;
    auto interval1 = std::chrono::milliseconds(100);
    coke::sync_wait(start_work(interval1));

    std::cout << std::string(80, '-') << std::endl;

    std::cout << "Example 2: Push slower than work" << std::endl;
    auto interval2 = std::chrono::milliseconds(300);
    coke::sync_wait(start_work(interval2));

    return 0;
}

std::string current() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> d = now - start;
    return "[" + std::to_string(d.count()) + "s] ";
}
