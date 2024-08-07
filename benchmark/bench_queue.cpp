#include <iostream>
#include <vector>
#include <utility>

#include "bench_common.h"
#include "coke/coke.h"
#include "coke/queue.h"

std::vector<int> width{16, 8, 6, 8, 6, 10};

int poller_threads = 6;
int handler_threads = 20;

int times = 1;
int total = 1000000;
int batch_size = 10;
int que_size = 1000;
int max_secs_per_test = 5;
int concurrency = 10;
bool yes = false;

std::atomic<int> counter;

bool acquire_count(int n) {
    constexpr auto relaxed = std::memory_order_relaxed;
    if (counter.fetch_add(n, relaxed) < total)
        return true;
    return false;
}

coke::Task<> que_try_push(coke::Queue<int> &que) {
    co_await coke::yield();

    while (acquire_count(1)) {
        if (!que.full() && que.try_push(0))
            continue;

        co_await que.push(0);
    }
}

coke::Task<> que_push(coke::Queue<int> &que) {
    co_await coke::yield();

    while (acquire_count(1)) {
        co_await que.push(0);
    }
}

coke::Task<> que_push_range(coke::Queue<int> &que) {
    co_await coke::yield();
    std::vector<int> v(batch_size, 0);

    while (acquire_count(batch_size)) {
        auto first = v.begin(), last = v.end();
        if (!que.full())
            first = que.try_push_range(first, last);

        while (first != last) {
            co_await que.push(*first);
            ++first;
        }
    }
}

coke::Task<> que_try_pop(coke::Queue<int> &que) {
    co_await coke::yield();

    int value;
    while (!que.closed()) {
        if (!que.empty() && que.try_pop(value))
            continue;

        co_await que.pop(value);
    }
}

coke::Task<> que_pop(coke::Queue<int> &que) {
    co_await coke::yield();

    int value;
    while (!que.closed()) {
        co_await que.pop(value);
    }
}

coke::Task<> que_pop_range(coke::Queue<int> &que) {
    co_await coke::yield();

    std::vector<int> v(batch_size, 0);
    int value;

    while (!que.closed()) {
        auto first = v.begin(), last = v.end();

        if (!que.empty())
            first = que.try_pop_range(first, last);

        if (first == v.begin())
            co_await que.pop(value);
    }
}

coke::Task<> warm_up() { co_await coke::yield(); }

using push_func_t = coke::Task<>(*)(coke::Queue<int> &);
using pop_func_t = coke::Task<>(*)(coke::Queue<int> &);

coke::Task<> benchmark_que(push_func_t push, pop_func_t pop) {
    std::vector<coke::Task<>> push_tasks;
    std::vector<coke::Future<void>> pop_futs;
    coke::Queue<int> que((std::size_t)que_size);

    push_tasks.reserve(concurrency);
    pop_futs.reserve(concurrency);

    for (int i = 0; i < concurrency; i++) {
        push_tasks.emplace_back(push(que));
        pop_futs.emplace_back(
            coke::create_future(pop(que))
        );
    }

    co_await coke::async_wait(std::move(push_tasks));
    que.close();

    for (int i = 0; i < concurrency; i++) {
        co_await pop_futs[i].wait();
        pop_futs[i].get();
    }
}

coke::Task<> bench_try_push_pop() {
    return benchmark_que(que_try_push, que_try_pop);
}

coke::Task<> bench_push_pop() {
    return benchmark_que(que_push, que_pop);
}

coke::Task<> bench_push_pop_range() {
    return benchmark_que(que_push_range, que_pop_range);
}

using bench_func_t = coke::Task<> (*)();
coke::Task<> do_benchmark(const char *name, bench_func_t func) {
    int run_times = 0;
    long long start, total_cost = 0;
    std::vector<long long> costs;
    double mean, stddev, tps;

    for (int i = 0; i < times; i++) {
        counter = 0;

        start = current_msec();
        co_await func();
        costs.push_back(current_msec() - start);
        total_cost += costs.back();

        run_times++;

        if (total_cost >= max_secs_per_test * 1000)
            break;
    }

    data_distribution(costs, mean, stddev);
    tps = 1.0e3 * total / (mean + 1e-9);

    table_line(std::cout, width, name, total_cost, run_times,
               mean, stddev, (long)tps);
}

int main(int argc, char *argv[]) {
    coke::OptionParser args;

    args.add_integer(concurrency, 'c', "concurrency")
        .set_default(1024)
        .set_description("The number of concurrent during benchmark");
    args.add_integer(max_secs_per_test, 'm', "max-secs")
        .set_default(5)
        .set_description("Max seconds for each benchmark");
    args.add_integer(total, 't', "total")
        .set_default(100000)
        .set_description("Total tasks in each benchmark");
    args.add_integer(times, coke::NULL_SHORT_NAME, "times")
        .set_default(1)
        .set_description("The number of times each benchmark run");
    args.add_integer(que_size, 'q', "que-size")
        .set_default(1000)
        .set_description("Max elements in queue");
    args.add_integer(batch_size, 'b', "batch-size")
        .set_default(10)
        .set_description("Batch size for push/pop by range");
    args.add_integer(poller_threads, coke::NULL_SHORT_NAME, "poller")
        .set_default(6)
        .set_description("Number of poller threads");
    args.add_integer(handler_threads, coke::NULL_SHORT_NAME, "handler")
        .set_default(20)
        .set_description("Number of handler threads");
    args.add_flag(yes, 'y', "yes").set_description("Skip asking before start");
    args.set_help_flag('h', "help");

    int ret = parse_args(args, argc, argv, &yes);
    if (ret <= 0)
        return ret;

    coke::GlobalSettings gs;
    gs.handler_threads = handler_threads;
    gs.poller_threads = poller_threads;
    coke::library_init(gs);

    std::cout.precision(2);
    std::cout.setf(std::ios_base::fixed, std::ios_base::floatfield);

    coke::sync_wait(warm_up());

    table_line(std::cout, width,
               "name", "cost", "times",
               "mean(ms)", "stddev", "per sec");
    delimiter(std::cout, width, '-');

#define DO_BENCHMARK(func) coke::sync_wait(do_benchmark(#func, bench_ ## func))
    DO_BENCHMARK(try_push_pop);
    DO_BENCHMARK(push_pop);
    DO_BENCHMARK(push_pop_range);
#undef DO_BENCHMARK

    return 0;
}
