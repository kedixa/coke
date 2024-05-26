/**
 * Copyright 2024 Coke Project (https://github.com/kedixa/coke)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Authors: kedixa (https://github.com/kedixa)
*/

#include <atomic>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "option_parser.h"
#include "bench_common.h"

#include "coke/coke.h"
#include "workflow/WFTaskFactory.h"

using std::chrono::microseconds;

alignas(64) std::atomic<long long> current;

constexpr int pool_size = 10;
std::string name_pool[pool_size];
uint64_t id_pool[pool_size];
std::vector<int> width{18, 8, 6, 8, 6, 10};

long long total{100000};
int concurrency = 4096;
int times = 1;
int max_secs_per_test = 5;
int handler_threads = 20;
int poller_threads = 6;
int compute_threads = -1;
bool yes = false;

// Not sure if it's thread safe but it runs fine so far.
std::uniform_int_distribution<int> dist(300, 700);

bool next(long long &cur) {
    cur = current.fetch_add(1, std::memory_order_relaxed);
    if (cur < total)
        return true;

    current.fetch_sub(1, std::memory_order_relaxed);
    return false;
}

template<typename Awaiter>
coke::Task<> detach(Awaiter awaiter) {
    co_await awaiter;
}

template<typename Awaiter>
coke::Task<> detach3(Awaiter a, Awaiter b, Awaiter c) {
    co_await coke::async_wait(std::move(a), std::move(b), std::move(c));
}

// benchmark

coke::Task<> bench_wf_repeat() {
    std::mt19937_64 mt(current_msec());
    long long i;

    coke::GenericAwaiter<void> g;
    auto create = [&] (WFRepeaterTask *) -> SubTask * {
        if (next(i)) {
            int nsec = dist(mt) * 1000;
            return WFTaskFactory::create_timer_task(0, nsec, nullptr);
        }
        return nullptr;
    };
    auto callback = [&] (WFRepeaterTask *) { g.done(); };

    auto *rep = WFTaskFactory::create_repeater_task(create, callback);
    g.take_over(rep);

    co_await g;
}

coke::Task<> bench_default_timer() {
    std::mt19937_64 mt(current_msec());
    long long i;

    while (next(i)) {
        co_await coke::sleep(microseconds(dist(mt)));
    }
}

coke::Task<> bench_yield() {
    long long i;

    while (next(i)) {
        co_await coke::yield();
    }
}

coke::Task<> bench_timer_in_task() {
    std::mt19937_64 mt(current_msec());
    long long i;

    while (next(i)) {
        co_await detach(coke::sleep(microseconds(dist(mt))));
    }
}

// bench by name

coke::Task<> bench_timer_by_name() {
    std::mt19937_64 mt(current_msec());
    long long i;
    std::string name;

    while (next(i)) {
        name = std::to_string(i);
        co_await coke::sleep(name, microseconds(dist(mt)));
    }
}

coke::Task<> bench_cancel_by_name() {
    std::mt19937_64 mt(current_msec());
    long long i;
    std::string name;

    while (next(i)) {
        name = std::to_string(i);
        auto awaiter = coke::sleep(name, microseconds(dist(mt)));
        coke::cancel_sleep_by_name(name);
        co_await awaiter;
    }
}

coke::Task<> bench_detach_by_name() {
    std::mt19937_64 mt(current_msec());
    long long i;
    std::string name;

    co_await coke::switch_go_thread();

    while (next(i)) {
        name = std::to_string(i);
        auto awaiter = coke::sleep(name, microseconds(dist(mt)));
        detach(std::move(awaiter)).start();

        coke::cancel_sleep_by_name(name);
    }
}

coke::Task<> bench_detach3_by_name() {
    std::mt19937_64 mt(current_msec());
    long long i;
    std::string name;

    co_await coke::switch_go_thread();

    while (next(i)) {
        name = std::to_string(i);
        auto a = coke::sleep(name, microseconds(dist(mt)));
        auto b = coke::sleep(name, microseconds(dist(mt)));
        auto c = coke::sleep(name, microseconds(dist(mt)));

        detach3(std::move(a), std::move(b), std::move(c)).start();

        coke::cancel_sleep_by_name(name);
    }
}

coke::Task<> bench_pool_name(int max) {
    std::mt19937_64 mt(current_msec());
    long long i;

    while (next(i)) {
        const std::string &name = name_pool[i%max];
        co_await coke::sleep(name, microseconds(dist(mt)));
    }
    co_return;
}

coke::Task<> bench_one_name() { return bench_pool_name(1); }

coke::Task<> bench_two_name() { return bench_pool_name(2); }

coke::Task<> bench_ten_name() { return bench_pool_name(10); }

coke::Task<> bench_name_one_by_one() {
    auto sleep = std::chrono::seconds(10);
    auto first = std::chrono::milliseconds(10);
    std::string name = name_pool[0];
    long long i;

    while (next(i)) {
        if (i == 0)
            co_await coke::sleep(name, first);
        else
            co_await coke::sleep(name, sleep);

        coke::cancel_sleep_by_name(name, 1);
    }
}

// bench by id

coke::Task<> bench_timer_by_id() {
    std::mt19937_64 mt(current_msec());
    uint64_t id;
    long long i;

    while (next(i)) {
        id = coke::get_unique_id();
        co_await coke::sleep(id, microseconds(dist(mt)));
    }
}

coke::Task<> bench_cancel_by_id() {
    std::mt19937_64 mt(current_msec());
    uint64_t id;
    long long i;

    while (next(i)) {
        id = coke::get_unique_id();
        auto awaiter = coke::sleep(id, microseconds(dist(mt)));
        coke::cancel_sleep_by_id(id);
        co_await awaiter;
    }
}

coke::Task<> bench_detach_by_id() {
    std::mt19937_64 mt(current_msec());
    uint64_t id;
    long long i;

    co_await coke::switch_go_thread();

    while (next(i)) {
        id = coke::get_unique_id();
        auto awaiter = coke::sleep(id, microseconds(dist(mt)));
        detach(std::move(awaiter)).start();
        coke::cancel_sleep_by_id(id);
    }
}

coke::Task<> bench_detach3_by_id() {
    std::mt19937_64 mt(current_msec());
    uint64_t id;
    long long i;

    co_await coke::switch_go_thread();

    while (next(i)) {
        id = coke::get_unique_id();
        auto a = coke::sleep(id, microseconds(dist(mt)));
        auto b = coke::sleep(id, microseconds(dist(mt)));
        auto c = coke::sleep(id, microseconds(dist(mt)));

        detach3(std::move(a), std::move(b), std::move(c)).start();
        coke::cancel_sleep_by_id(id);
    }
}

coke::Task<> bench_detach_inf_by_id() {
    uint64_t id;
    long long i;

    co_await coke::switch_go_thread();

    while (next(i)) {
        id = coke::get_unique_id();
        auto awaiter = coke::sleep(id, coke::InfiniteDuration{});
        detach(std::move(awaiter)).start();
        coke::cancel_sleep_by_id(id);
    }
}

coke::Task<> bench_detach3_inf_by_id() {
    uint64_t id;
    long long i;

    co_await coke::switch_go_thread();

    while (next(i)) {
        id = coke::get_unique_id();
        auto a = coke::sleep(id, coke::InfiniteDuration{});
        auto b = coke::sleep(id, coke::InfiniteDuration{});
        auto c = coke::sleep(id, coke::InfiniteDuration{});

        detach3(std::move(a), std::move(b), std::move(c)).start();
        coke::cancel_sleep_by_id(id);
    }
}

coke::Task<> bench_pool_id(int max) {
    std::mt19937_64 mt(current_msec());
    long long i;

    while (next(i)) {
        uint64_t id = id_pool[i%max];
        co_await coke::sleep(id, microseconds(dist(mt)));
    }
    co_return;
}

coke::Task<> bench_one_id() { return bench_pool_id(1); }

coke::Task<> bench_two_id() { return bench_pool_id(2); }

coke::Task<> bench_ten_id() { return bench_pool_id(10); }

coke::Task<> bench_id_one_by_one() {
    auto sleep = std::chrono::seconds(10);
    auto first = std::chrono::milliseconds(10);
    auto id = id_pool[0];
    long long i;

    while (next(i)) {
        if (i == 0)
            co_await coke::sleep(id, first);
        else
            co_await coke::sleep(id, sleep);

        coke::cancel_sleep_by_id(id, 1);
    }
}

coke::Task<> warm_up() {
    co_await coke::yield();
    co_await coke::switch_go_thread();
}

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
                      (long long)100000);
    args.add_integral(times, 0, "times", false,
                      "run these times for each benchmark", 1);
    args.add_integral(poller_threads, 0, "poller", false,
                      "number of poller threads", 6);
    args.add_integral(handler_threads, 0, "handler", false,
                      "number of handler threads", 20);
    args.add_integral(compute_threads, 0, "compute", false,
                      "number of compute threads", -1);
    args.add_flag(yes, 'y', "yes", "skip showing options before start");
    args.set_help_flag('h', "help");

    int ret = parse_args(args, argc, argv, &yes);
    if (ret <= 0)
        return ret;

    coke::GlobalSettings gs;
    gs.poller_threads = poller_threads;
    gs.handler_threads = handler_threads;
    gs.compute_threads = compute_threads;
    coke::library_init(gs);

    std::cout.precision(2);
    std::cout.setf(std::ios_base::fixed, std::ios_base::floatfield);

    coke::sync_wait(warm_up());

    // for bench fixed name or id
    for (int i = 0; i < pool_size; i++) {
        name_pool[i] = std::to_string(i);
        id_pool[i] = coke::get_unique_id();
    }

    table_line(std::cout, width,
               "name", "cost", "times",
               "mean(ms)", "stddev", "per sec");
    delimiter(std::cout, width, '-');

#define DO_BENCHMARK(func) coke::sync_wait(do_benchmark(#func, bench_ ## func))
    DO_BENCHMARK(wf_repeat);
    DO_BENCHMARK(default_timer);
    DO_BENCHMARK(yield);
    DO_BENCHMARK(timer_in_task);
    delimiter(std::cout, width);

    DO_BENCHMARK(timer_by_name);
    DO_BENCHMARK(cancel_by_name);
    DO_BENCHMARK(detach_by_name);
    DO_BENCHMARK(detach3_by_name);
    DO_BENCHMARK(one_name);
    DO_BENCHMARK(two_name);
    DO_BENCHMARK(ten_name);

    if (concurrency > 1)
        DO_BENCHMARK(name_one_by_one);
    delimiter(std::cout, width);

    DO_BENCHMARK(timer_by_id);
    DO_BENCHMARK(cancel_by_id);
    DO_BENCHMARK(detach_by_id);
    DO_BENCHMARK(detach3_by_id);
    DO_BENCHMARK(detach_inf_by_id);
    DO_BENCHMARK(detach3_inf_by_id);
    DO_BENCHMARK(one_id);
    DO_BENCHMARK(two_id);
    DO_BENCHMARK(ten_id);

    if (concurrency > 1)
        DO_BENCHMARK(id_one_by_one);
#undef DO_BENCHMARK

    return 0;
}
