#include <iostream>
#include <algorithm>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>

#include "coke/coke.h"

using std::size_t;

/**
 * This example performs a little optimization on parallel merge sort.
 * Coroutines are about writing efficient asynchronous code in a synchronous
 * form without any extra cost.
*/

coke::Task<> merge_impl(size_t *first1,size_t *last1,
                        size_t *first2, size_t *last2, size_t *result)
{
    co_await coke::switch_go_thread();
    std::merge(first1, last1, first2, last2, result);
}

coke::Task<> merge_sort_impl(size_t *first1, size_t *last1,
                             size_t *first2, size_t *last2, int depth)
{
    size_t n = last1 - first1;

    if ((depth < 7 && n > 1024) || depth % 2 == 1) {
        size_t *mid1 = first1 + n / 2, *mid2 = first2 + n / 2;

        // Async wait two sub coroutine until finish
        co_await coke::async_wait(
            merge_sort_impl(first2, mid2, first1, mid1, depth + 1),
            merge_sort_impl(mid2, last2, mid1, last1, depth + 1)
        );

        // Parallel merge two sorted sequence to bigger one.
        // For the sake of simplicity, this is not a complete binary search.

        std::size_t left_size = mid2 - first2;

        if (left_size != 0) {
            size_t *quarter, *quarter3, key;
            quarter = first2 + left_size / 2;
            key = *quarter;
            quarter = std::upper_bound(quarter, mid2, key);
            quarter3 = std::upper_bound(mid2, last2, key);

            size_t *merge_mid = first1 + (quarter - first2) + (quarter3 - mid2);

            co_await coke::async_wait(
                merge_impl(first2, quarter, mid2, quarter3, first1),
                merge_impl(quarter, mid2, quarter3, last2, merge_mid)
            );
        }
        else
            std::merge(first2, mid2, mid2, last2, first1);
    }
    else {
        co_await coke::switch_go_thread();
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
    size_t n = 512;
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

    // 4. check result
    bool ok = v == v2;
    std::cout << "Sort " << (ok ? "Success" : "Failed") << std::endl;

    return 0;
}
