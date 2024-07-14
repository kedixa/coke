#include <algorithm>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "coke/go.h"
#include "coke/wait.h"
#include "coke/tools/option_parser.h"

/**
 * This example makes two small optimizations to make merge sort faster on
 * very long arrays.
 *
 * ATTENTION: This code has not been fully tested.
 *
 * 1. Adding an extra buffer to speeds up merge, of course this will take up
 *    a lot of extra memory to trade space for efficiency.
 * 2. Using parallelism during the merge process.
*/

constexpr int MAX_DEPTH = 8;
constexpr std::size_t MIN_DIVIDE_SIZE = 8192;

template<std::random_access_iterator Iter1,
         std::random_access_iterator Iter2, typename Comp>
coke::Task<> merge_impl(Iter1 first1, Iter1 last1, Iter1 first2, Iter1 last2,
                        Iter2 result, Comp cmp, int depth) {
    std::size_t size1 = (std::size_t)(last1 - first1);
    std::size_t size2 = (std::size_t)(last2 - first2);
    std::size_t tot = size1 + size2;

    if (depth < MAX_DEPTH && tot > MIN_DIVIDE_SIZE && size1 > 0 && size2 > 0) {
        Iter1 mid1, mid2;

        // divide to two small stable merge problems
        if (size1 >= size2) {
            mid1 = first1 + size1 / 2;
            mid2 = std::lower_bound(first2, last2, *mid1, cmp);
        }
        else {
            mid2 = first2 + size2 / 2;
            mid1 = std::upper_bound(first1, last1, *mid2, cmp);
        }

        Iter2 rmid = result + (mid1 - first1) + (mid2 - first2);
        co_await coke::async_wait(
            merge_impl(first1, mid1, first2, mid2, result, cmp, depth + 1),
            merge_impl(mid1, last1, mid2, last2, rmid, cmp, depth + 1)
        );
    }
    else {
        // move merge two sorted array into result
        co_await coke::switch_go_thread();
        std::merge(std::move_iterator(first1), std::move_iterator(last1),
                   std::move_iterator(first2), std::move_iterator(last2),
                   result, cmp);
    }
}

template<std::random_access_iterator Iter1,
         std::random_access_iterator Iter2, typename Comp>
coke::Task<> merge_sort_impl(Iter1 first1, Iter1 last1,
                             Iter2 first2, Iter2 last2,
                             Comp cmp, int depth) {
    std::size_t n = (std::size_t)(last1 - first1);

    /**
     * When depth is odd, recurse once more to ensure that we are
     * sorting the user array, not the extra buffer.
     */
    if ((depth < MAX_DEPTH && n > MIN_DIVIDE_SIZE) || depth % 2 == 1) {
        Iter1 mid1 = first1 + n / 2;
        Iter2 mid2 = first2 + n / 2;

        co_await coke::async_wait(
            merge_sort_impl(first2, mid2, first1, mid1, cmp, depth + 1),
            merge_sort_impl(mid2, last2, mid1, last1, cmp, depth + 1)
        );

        co_await merge_impl(first2, mid2, mid2, last2, first1, cmp, depth);
    }
    else {
        co_await coke::switch_go_thread();
        std::stable_sort(first1, last1, cmp);
    }
}

// Note that we do not support sorting std::vector<bool>
template<std::random_access_iterator Iter, typename Comp = std::less<>>
coke::Task<> merge_sort(Iter first, Iter last, Comp cmp = Comp()) {
    using T = typename std::iterator_traits<Iter>::value_type;
    std::size_t n = (std::size_t)(last - first);

    if (n > 1) {
        std::unique_ptr<T[]> ptr(new T[n]);
        T *buf = ptr.get();

        co_await merge_sort_impl(first, last, buf, buf + n, cmp, 0);
    }
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
