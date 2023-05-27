#include <iostream>
#include <algorithm>
#include <vector>
#include <random>
#include <chrono>

#include "coke/coke.h"

using std::size_t;
constexpr size_t SMALL_HINT = 1 << 16;

/**
 * This example shows how to call a coroutine function recursively using merge sort.
 * Parallel sorting can also be implemented more efficiently.
 * For simplicity, only a simple merge sort example is given here.
*/

coke::Task<> merge_sort_impl(size_t *first1, size_t *last1, size_t *first2, size_t *last2, int depth) {
    size_t n = last1 - first1;

    // When n is large, use a divide-and-conquer to solve two small problems.
    if ((n > SMALL_HINT && depth < 6) || depth % 2 == 1) {
        size_t *mid1 = first1 + n / 2, *mid2 = first2 + n / 2;

        // Async wait two sub coroutine until finish
        co_await coke::async_wait(
            merge_sort_impl(first2, mid2, first1, mid1, depth + 1),
            merge_sort_impl(mid2, last2, mid1, last1, depth + 1)
        );

        // Merge two sorted sequence to bigger one.
        // By the way, parallel merge will be more faster,
        // after reading this example, I believe you can achieve it with coke.
        std::merge(first2, mid2, mid2, last2, first1);
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
        std::sort(first1, last1);
    }
}

coke::Task<> merge_sort(size_t *first, size_t *last) {
    size_t n = last - first;
    if (n > 1) {
        // We didn't initialize buf[] because std::is_trival_v<size_t>
        size_t *buf = (size_t *)malloc(sizeof(size_t) * n);
        co_await merge_sort_impl(first, last, buf, buf + n, 0);
        free(buf);
    }
}

long current_usec() {
    auto dur = std::chrono::steady_clock::now().time_since_epoch();
    auto usec = std::chrono::duration_cast<std::chrono::microseconds>(dur);
    return usec.count();
}

int main(int argc, char *argv[]) {
    size_t n = SMALL_HINT * 8;
    long start, cost;

    if (argc > 1)
        n = (size_t)std::atoll(argv[1]);

    std::random_device rd;
    std::mt19937_64 mt(rd());
    std::vector<size_t> v(n, 0);

    // 1. prepare random value
    start = current_usec();

    for (size_t i = 0; i < n; i++)
        v[i] = mt();

    cost = current_usec() - start;
    std::cout << "Prepare " << n << " random value cost " << cost << "us\n";

    // 2. sort
    start = current_usec();

    coke::sync_wait(merge_sort(v.data(), v.data() + n));

    cost = current_usec() - start;
    std::cout << "Sort " << n << " random value cost " << cost << "us\n";

    // 3. check result
    bool sorted = std::is_sorted(v.begin(), v.end());
    std::cout << "Sort " << (sorted ? "Success" : "Failed") << std::endl;

    return 0;
}
