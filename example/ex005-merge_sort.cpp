#include <algorithm>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <random>
#include <string>
#include <vector>

#include "coke/go.h"
#include "coke/wait.h"
#include "coke/tools/option_parser.h"

/**
 * This example implements a simple parallel stable merge sort to show how to
 * call coroutines recursively.
*/

constexpr int MAX_DEPTH = 8;
constexpr std::size_t MIN_DIVIDE_SIZE = 8192;

template<std::random_access_iterator Iter, typename Comp>
coke::Task<> merge_sort_impl(Iter first, Iter last, Comp cmp, int depth) {
    std::size_t n = (std::size_t)(last - first);

    // When n is large, use divide-and-conquer to solve small problems.
    if (depth < MAX_DEPTH && n > MIN_DIVIDE_SIZE) {
        Iter mid = first + n / 2;

        // Async wait two coroutine until finish
        co_await coke::async_wait(
            merge_sort_impl(first, mid, cmp, depth + 1),
            merge_sort_impl(mid, last, cmp, depth + 1)
        );

        std::inplace_merge(first, mid, last, cmp);
    }
    else {
        /**
         * For CPU-intensive operations, we need to operate in an independent
         * thread pool to avoid blocking the current thread.
         * We can run `func` in workflow's compute thread using
         * `co_await coke::go(func, arg1, arg2, ...)`, or just switch to
         * workflow's compute thread using `co_await coke::switch_go_thread()`
         * and write code as flexible as we want.
        */
        co_await coke::switch_go_thread();

        /**
         * For a smaller n, just std::stable_sort. Because recursively calling
         * functions and creating coroutines will also have a certain overhead,
         * recursion too deep will make it worse.
        */
        std::stable_sort(first, last, cmp);
    }
}

// Note that we do not support sorting std::vector<bool>
template<std::random_access_iterator Iter, typename Comp = std::less<>>
coke::Task<> merge_sort(Iter first, Iter last, Comp cmp = Comp()) {
    co_await merge_sort_impl(first, last, cmp, 0);
}

// Helper functions to run this example.

long current_usec();

template<typename T>
void generate(std::vector<T> &vec, std::size_t n, std::size_t seed);

void show_cost(const char *title, long cost, long base = 100) {
    double percent = 100.0 * cost / base;
    std::cout << std::setw(20) << title
        << std::setw(10) << cost << "us"
        << std::setw(10)
        << std::fixed << std::setprecision(2) << percent << "%"
        << std::endl;
}

template<typename T>
void run_merge_sort(std::string type, std::size_t n, std::size_t seed) {
    std::vector<T> v1, v2;
    long start, base;

    std::cout << "Run merge sort on " << n << " random value of type "
        << type << std::endl;

    // 1. Prepare
    generate(v1, n, seed);
    v2 = v1;

    std::cout << std::string(64, '-') << std::endl;

    // 2. Parallel merge sort
    start = current_usec();
    coke::sync_wait(merge_sort(v1.begin(), v1.end(), std::less<T>{}));
    base = current_usec() - start;
    show_cost("ParallelMergeSort", base, base);

    // 3. std::stable_sort
    start = current_usec();
    std::stable_sort(v2.begin(), v2.end(), std::less<T>{});
    show_cost("StdStableSort", current_usec() - start, base);

    if (v1 != v2) {
        std::cout << "Sort Failed" << std::endl;
        return;
    }

    std::cout << std::string(64, '-') << std::endl;

    // 4. Parallel reverse
    start = current_usec();
    coke::sync_wait(merge_sort(v1.begin(), v1.end(), std::greater<T>{}));
    base = current_usec() - start;
    show_cost("ParallelReverse", base, base);

    // 5. std reverse
    start = current_usec();
    std::stable_sort(v2.begin(), v2.end(), std::greater<T>{});
    show_cost("StdReverse", current_usec() - start, base);

    if (v1 != v2)
        std::cout << "Sort Failed" << std::endl;
    else
        std::cout << "Sort Success" << std::endl;
}

int main(int argc, char *argv[]) {
    std::size_t n = 10000000;
    std::size_t seed = 0;
    std::string type = "int";
    int compute_threads = -1;

    coke::OptionParser args;
    args.add_integer(n, 'n', "num").set_default(10000000)
        .set_description("Number of elements to sort");
    args.add_integer(seed, 's', "seed").set_default(0)
        .set_description("Random generator seed");
    args.add_integer(compute_threads, 'c', "compute-threads").set_default(-1)
        .set_description("Set compute threads");
    args.add_string(type, 't', "type").set_default("int")
        .set_description("Element type, one of int, double, string");
    args.set_help_flag('h', "help");

    if (args.parse(argc, argv) != 0) {
        args.usage(std::cout);
        return 0;
    }

    coke::GlobalSettings g;
    g.compute_threads = compute_threads;
    coke::library_init(g);

    auto warmup = []() -> coke::Task<> {
        co_await coke::switch_go_thread();
    };
    coke::sync_wait(warmup());

    if (type == "int")
        run_merge_sort<int>(type, n, seed);
    else if (type == "double")
        run_merge_sort<double>(type, n, seed);
    else if (type == "string")
        run_merge_sort<std::string>(type, n, seed);
    else
        std::cout << "Unsupported type " << type << std::endl;

    return 0;
}

long current_usec() {
    auto dur = std::chrono::steady_clock::now().time_since_epoch();
    auto usec = std::chrono::duration_cast<std::chrono::microseconds>(dur);
    return usec.count();
}

template<typename T>
void generate(std::vector<T> &vec, std::size_t n, std::size_t seed) {
    std::mt19937_64 mt(seed);
    vec.resize(n);

    if constexpr (std::is_integral_v<T>) {
        std::uniform_int_distribution<T> g;
        for (std::size_t i = 0; i < n; i++)
            vec[i] = g(mt);
    }
    else if constexpr (std::is_floating_point_v<T>) {
        std::uniform_real_distribution<T> g;
        for (std::size_t i = 0; i < n; i++)
            vec[i] = g(mt);
    }
    else if constexpr (std::is_same_v<T, std::string>) {
        const std::size_t buf_size = n + 24;
        std::string buf(buf_size, 0);

        for (std::size_t i = 0; i < buf_size; i++)
            buf[i] = mt() % 26 + 'a';

        for (std::size_t i = 0; i < n; i++) {
            auto offset = mt() % n;
            auto len = mt() % 16 + 8;
            vec[i].assign(buf.data() + offset, len);
        }
    }
    else {
        static_assert(!std::is_same_v<T, T>, "unsupported type");
    }
}
