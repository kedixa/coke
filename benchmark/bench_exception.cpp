#include <atomic>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <random>

#include "option_parser.h"
#include "bench_common.h"

#include "coke/coke.h"

alignas(64) std::atomic<long long> current;
std::vector<int> width{14, 8, 6, 8, 6, 10};

int total{100000};
int concurrency = 1024;
int max_secs_per_test = 5;
int poller_threads = 6;
int handler_threads = 20;
int times = 1;
bool yes = false;

// Not sure if it's thread safe but it runs fine so far.
std::uniform_int_distribution<int> dist(0, 99);

bool next(long long &cur) {
    cur = current.fetch_add(1, std::memory_order_relaxed);
    if (cur < total)
        return true;

    current.fetch_sub(1, std::memory_order_relaxed);
    return false;
}

coke::Task<> recursive_yield(int depth, std::mt19937_64 &mt, int p) {
    if (depth <= 1) {
        co_await coke::yield();
        if (dist(mt) < p)
            throw std::runtime_error("this is an exception");
    }
    else
        co_await recursive_yield(depth-1, mt, p);
}

coke::Task<> do_test(int dpt, std::mt19937_64 &&mt, int p) {
    long long i;

    while (next(i)) {
        try {
            co_await recursive_yield(dpt, mt, p);
        }
        catch (const std::runtime_error &) { }
    }
}

// benchmark

coke::Task<> bench_normal_yield() {
    long long i;

    while (next(i)) {
        co_await coke::yield();
    }
}

coke::Task<> bench_yield_catch() {
    long long i;

    while (next(i)) {
        try {
            co_await coke::yield();
        }
        catch (...) { }
    }
}

coke::Task<> bench_d1_p0() { co_await do_test(1, std::mt19937_64{}, 0); }
coke::Task<> bench_d2_p0() { co_await do_test(2, std::mt19937_64{}, 0); }
coke::Task<> bench_d5_p0() { co_await do_test(5, std::mt19937_64{}, 0); }

coke::Task<> bench_d1_p100() { co_await do_test(1, std::mt19937_64{}, 100); }
coke::Task<> bench_d2_p100() { co_await do_test(2, std::mt19937_64{}, 100); }
coke::Task<> bench_d5_p100() { co_await do_test(5, std::mt19937_64{}, 100); }

coke::Task<> bench_d1_p1() { co_await do_test(1, std::mt19937_64{}, 1); }
coke::Task<> bench_d1_p5() { co_await do_test(1, std::mt19937_64{}, 5); }
coke::Task<> bench_d1_p10() { co_await do_test(1, std::mt19937_64{}, 10); }
coke::Task<> bench_d1_p20() { co_await do_test(1, std::mt19937_64{}, 20); }
coke::Task<> bench_d1_p50() { co_await do_test(1, std::mt19937_64{}, 50); }

coke::Task<> warm_up() { co_await coke::yield(); }

using bench_func_t = coke::Task<>(*)();
coke::Task<> do_benchmark(const char *name, bench_func_t func) {
    int run_times = 0;
    long long start, total_cost = 0;
    std::vector<long long> costs;
    double mean, stddev, tps;

    for (int i = 0; i < times; i++) {
        std::vector<coke::Task<>> tasks;
        current = 0;

        for (int j = 0; j < concurrency; j++)
            tasks.emplace_back(func());

        start = current_msec();
        co_await coke::async_wait(std::move(tasks));
        costs.push_back(current_msec() - start);
        total_cost += costs.back();

        run_times++;

        if (total_cost >= max_secs_per_test * 1000)
            break;
    }

    data_distribution(costs, mean, stddev);
    tps = 1.0e3 * current / (mean + 1e-9);

    table_line(std::cout, width, name, total_cost, run_times,
               mean, stddev, (long)tps);
}


int main(int argc, char *argv[]) {
    OptionParser args;

    args.add_integral(concurrency, 'c', "concurrency", false,
                      "start these series to do benchmark", 4096);
    args.add_integral(max_secs_per_test, 'm', "max-secs", false,
                      "max seconds for each benchmark", 5);
    args.add_integral(total, 't', "total", false,
                      "total tasks for each benchmark",
                      100000);
    args.add_integral(times, 0, "times", false,
                      "run these times for each benchmark", 1);
    args.add_integral(poller_threads, 0, "poller", false,
                      "number of poller threads", 6);
    args.add_integral(handler_threads, 0, "handler", false,
                      "number of handler threads", 20);
    args.add_flag(yes, 'y', "yes", "skip showing options before start");
    args.set_help_flag('h', "help");

    int ret = parse_args(args, argc, argv, &yes);
    if (ret <= 0)
        return ret;

    coke::GlobalSettings gs;
    gs.poller_threads = poller_threads;
    gs.handler_threads = handler_threads;
    coke::library_init(gs);

    std::cout.precision(2);
    std::cout.setf(std::ios_base::fixed, std::ios_base::floatfield);

    coke::sync_wait(warm_up());

    table_line(std::cout, width,
               "name", "cost", "times",
               "mean(ms)", "stddev", "per sec");
    delimiter(std::cout, width, '-');

#define DO_BENCHMARK(func) coke::sync_wait(do_benchmark(#func, bench_ ## func))
    DO_BENCHMARK(normal_yield);
    DO_BENCHMARK(yield_catch);
    delimiter(std::cout, width);

    DO_BENCHMARK(d1_p0);
    DO_BENCHMARK(d2_p0);
    DO_BENCHMARK(d5_p0);
    delimiter(std::cout, width);

    DO_BENCHMARK(d1_p100);
    DO_BENCHMARK(d2_p100);
    DO_BENCHMARK(d5_p100);
    delimiter(std::cout, width);

    DO_BENCHMARK(d1_p1);
    DO_BENCHMARK(d1_p5);
    DO_BENCHMARK(d1_p10);
    DO_BENCHMARK(d1_p20);
    DO_BENCHMARK(d1_p50);
#undef DO_BENCHMARK

    return 0;
}
