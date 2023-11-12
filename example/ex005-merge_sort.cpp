#include <iostream>
#include <algorithm>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>

#include "coke/coke.h"

using std::size_t;

/**
 * This example shows how to call a coroutine function recursively using merge sort,
 * it behaves similarly to WFSortTask, but has less code and is easier to understand.
*/

coke::Task<> merge_sort_impl(size_t *first, size_t *last, int depth) {
    size_t n = last - first;

    // When n is large, use a divide-and-conquer to solve two small problems.
    if (depth < 7 && n >= 32) {
        size_t *mid = first + n / 2;

        // Async wait two sub coroutine until finish
        co_await coke::async_wait(
            merge_sort_impl(first, mid, depth + 1),
            merge_sort_impl(mid, last, depth + 1)
        );

        // Merge two sorted sequence to bigger one.
        std::inplace_merge(first, mid, last);
    }
    else {
        // For CPU-intensive operations, we need to operate in an independent thread
        // pool to avoid blocking the current thread.
        // We can use `co_await coke::go(func, arg1, arg2, ...)`, or just switch thread
        // using `co_await coke::switch_go_thread()` and write code as flexible as we want.
        co_await coke::switch_go_thread();

        // For a smaller n, just std::sort. Because recursively calling functions and
        // creating coroutines will also have a certain overhead, recursion too deep
        // will make it worse.
        std::sort(first, last);
    }
}

coke::Task<> merge_sort(size_t *first, size_t *last) {
    co_await merge_sort_impl(first, last, 0);
}

long current_usec() {
    auto dur = std::chrono::steady_clock::now().time_since_epoch();
    auto usec = std::chrono::duration_cast<std::chrono::microseconds>(dur);
    return usec.count();
}

int main(int argc, char *argv[]) {
    size_t n = 65535;
    long start, cost;

    if (argc > 1)
        n = (size_t)std::atoll(argv[1]);

    std::random_device rd;
    std::mt19937_64 mt(rd());
    std::vector<size_t> v(n, 0), v2;

    std::cout << "Test sort on " << n << " random value" << std::endl;

    // 1. prepare random value
    start = current_usec();

    for (size_t i = 0; i < n; i++)
        v[i] = mt();
    v2 = v;

    cost = current_usec() - start;
    std::cout << std::setw(14) << "Prepare " << std::setw(12) << cost << "us\n";

    // 2. parallel sort
    start = current_usec();
    coke::sync_wait(merge_sort(v.data(), v.data() + n));
    cost = current_usec() - start;
    std::cout << std::setw(14) << "ParallelSort " << std::setw(12) << cost << "us\n";

    // 3. std::sort
    start = current_usec();
    std::sort(v2.begin(), v2.end());
    cost = current_usec() - start;
    std::cout << std::setw(14) << "Sort " << std::setw(12) << cost << "us\n";

    // 3. check result
    bool ok = v == v2;
    std::cout << "Sort " << (ok ? "Success" : "Failed") << std::endl;

    return 0;
}
