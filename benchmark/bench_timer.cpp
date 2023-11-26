#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <random>
#include <string>
#include <vector>

#include <getopt.h>

#include "coke/coke.h"

using std::chrono::microseconds;
std::atomic<std::size_t> current;
std::atomic<bool> stop_flag;

constexpr std::size_t pool_size = 10;
std::string name_pool[pool_size];
uint64_t id_pool[pool_size];

std::size_t total{10000};
int concurrency = 1024;
int max_secs_per_test = 5;
int handler_threads = 20;
int poller_threads = 4;
bool yes = false;

// Not sure if it's thread safe but it runs fine so far.
std::uniform_int_distribution<int> dist(300, 700);

int64_t current_msec() {
    auto dur = std::chrono::steady_clock::now().time_since_epoch();
    auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(dur);
    return msec.count();
}

bool next(std::size_t &cur) {
    if (stop_flag.load(std::memory_order_acquire))
        return false;

    cur = current.fetch_add(1, std::memory_order_relaxed);
    if (cur < total)
        return true;

    current.fetch_sub(1, std::memory_order_relaxed);
    return false;
}

coke::Task<> count_down(uint64_t id, std::chrono::seconds timeout) {
    co_await coke::sleep(id, timeout);
    stop_flag.store(true, std::memory_order_release);
    stop_flag.notify_all();
}

coke::Task<> detach(coke::SleepAwaiter awaiter) {
    co_await awaiter;
}

coke::Task<> detach3(coke::SleepAwaiter a, coke::SleepAwaiter b, coke::SleepAwaiter c) {
    co_await coke::async_wait(std::move(a), std::move(b), std::move(c));
}

// benchmark

coke::Task<> bench_default_timer() {
    std::mt19937_64 mt(current_msec());
    std::size_t i;

    while (next(i)) {
        co_await coke::sleep(microseconds(dist(mt)));
    }
}

coke::Task<> bench_dismiss_timer() {
    std::mt19937_64 mt(current_msec());
    std::size_t i;

    co_await coke::switch_go_thread();

    while (next(i)) {
        auto awaiter = coke::sleep(microseconds(dist(mt)));
        (void)awaiter;
    }
}

coke::Task<> bench_timer_by_name() {
    std::mt19937_64 mt(current_msec());
    std::size_t i;
    std::string name;

    while (next(i)) {
        name = std::to_string(i);
        co_await coke::sleep(name, microseconds(dist(mt)));
    }
}

coke::Task<> bench_pool_name(std::size_t max) {
    std::mt19937_64 mt(current_msec());
    std::size_t i;

    while (next(i)) {
        const std::string &name = name_pool[i%max];
        co_await coke::sleep(name, microseconds(dist(mt)));
    }
    co_return;
}

coke::Task<> bench_one_name() {
    return bench_pool_name(1);
}

coke::Task<> bench_two_name() {
    return bench_pool_name(2);
}

coke::Task<> bench_ten_name() {
    return bench_pool_name(10);
}

coke::Task<> bench_cancel_by_name() {
    std::mt19937_64 mt(current_msec());
    std::size_t i;
    std::string name;

    while (next(i)) {
        name = std::to_string(i);
        auto awaiter = coke::sleep(name, microseconds(dist(mt)));
        coke::cancel_sleep_by_name(name);
        co_await awaiter;
    }
}

coke::Task<> bench_dismiss_by_name() {
    std::mt19937_64 mt(current_msec());
    std::size_t i;
    std::string name;

    co_await coke::switch_go_thread();

    while (next(i)) {
        name = std::to_string(i);
        auto awaiter = coke::sleep(name, microseconds(dist(mt)));
        (void)awaiter;
    }
}

coke::Task<> bench_detach_by_name() {
    std::mt19937_64 mt(current_msec());
    std::size_t i;
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
    std::size_t i;
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

coke::Task<> bench_timer_by_id() {
    std::mt19937_64 mt(current_msec());
    std::size_t id, i;

    while (next(i)) {
        id = coke::get_unique_id();
        co_await coke::sleep(id, microseconds(dist(mt)));
    }
}

coke::Task<> bench_pool_id(std::size_t max) {
    std::mt19937_64 mt(current_msec());
    std::size_t i;

    while (next(i)) {
        uint64_t id = id_pool[i%max];
        co_await coke::sleep(id, microseconds(dist(mt)));
    }
    co_return;
}

coke::Task<> bench_one_id() {
    return bench_pool_id(1);
}

coke::Task<> bench_two_id() {
    return bench_pool_id(2);
}

coke::Task<> bench_ten_id() {
    return bench_pool_id(10);
}

coke::Task<> bench_cancel_by_id() {
    std::mt19937_64 mt(current_msec());
    std::size_t id, i;

    while (next(i)) {
        id = coke::get_unique_id();
        auto awaiter = coke::SleepAwaiter(id, microseconds(dist(mt)));
        coke::cancel_sleep_by_id(id);
        co_await awaiter;
    }
}

coke::Task<> bench_dismiss_by_id() {
    std::mt19937_64 mt(current_msec());
    std::size_t id, i;

    co_await coke::switch_go_thread();

    while (next(i)) {
        id = coke::get_unique_id();
        auto awaiter = coke::SleepAwaiter(id, microseconds(dist(mt)));
        (void)awaiter;
    }
}

coke::Task<> bench_detach_by_id() {
    std::mt19937_64 mt(current_msec());
    std::size_t id, i;

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
    std::size_t id, i;

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


// helper

void head();
void delimiter();
void usage(const char *arg0);
int parse_args(int, char *[]);

coke::Task<> warm_up() {
    co_await coke::yield();
    co_await coke::switch_go_thread();
}

using bench_func_t = coke::Task<>(*)();
coke::Task<> do_benchmark(const char *name, bench_func_t func) {
    int64_t start, cost;
    uint64_t cid = coke::get_unique_id();
    std::vector<coke::Task<>> tasks;

    current = 0;
    stop_flag = false;

    for (int i = 0; i < concurrency; i++)
        tasks.emplace_back(func());

    count_down(cid, std::chrono::seconds(max_secs_per_test)).start();

    start = current_msec();
    co_await coke::async_wait(std::move(tasks));
    cost = current_msec() - start;

    coke::cancel_sleep_by_id(cid);
    stop_flag.wait(false, std::memory_order_acquire);

    if (cost == 0)
        cost = 1;

    double tps = 1.0e3 * current / cost;

    std::cout << "| " << std::setw(16) << name
        << " | " << std::setw(8) << cost
        << " | " << std::setw(14) << std::size_t(tps)
        << " |" << std::endl;
}

int main(int argc, char *argv[]) {
    int ret = parse_args(argc, argv);
    if (ret != 1)
        return ret;

    coke::GlobalSettings gs;
    gs.poller_threads = poller_threads;
    gs.handler_threads = handler_threads;
    coke::library_init(gs);

    coke::sync_wait(warm_up());

    // for bench fixed name or id
    for (std::size_t i = 0; i < pool_size; i++) {
        name_pool[i] = std::to_string(i);
        id_pool[i] = coke::get_unique_id();
    }

    head();

#define DO_BENCHMARK(func) coke::sync_wait(do_benchmark(#func, bench_ ## func))
    DO_BENCHMARK(default_timer);
    DO_BENCHMARK(dismiss_timer);
    delimiter();

    DO_BENCHMARK(timer_by_name);
    DO_BENCHMARK(cancel_by_name);
    DO_BENCHMARK(dismiss_by_name);
    DO_BENCHMARK(detach_by_name);
    DO_BENCHMARK(detach3_by_name);
    DO_BENCHMARK(one_name);
    DO_BENCHMARK(two_name);
    DO_BENCHMARK(ten_name);
    delimiter();

    DO_BENCHMARK(timer_by_id);
    DO_BENCHMARK(cancel_by_id);
    DO_BENCHMARK(dismiss_by_id);
    DO_BENCHMARK(detach_by_id);
    DO_BENCHMARK(detach3_by_id);
    DO_BENCHMARK(one_id);
    DO_BENCHMARK(two_id);
    DO_BENCHMARK(ten_id);
#undef DO_BENCHMARK

    return 0;
}

void usage(const char *arg0) {
    std::cout << arg0 << " [OPTION]...\n" <<
R"usage(
    -c concurrency, start `concurrency` series to benchmark timer, default 1024
                    this argument should greater or equal to 1024, because
                    each timer sleep about 500us
    -h n,           set handler threads to n, default 20
    -p n,           set poller threads to n, default 4
    -m sec,         run at most `sec` seconds for each case, default 15
    -t total,       run at most `total` timer for each case, default 10000
    -y              skip showing arguments before start benchmark
)usage"
    << std::endl;
}

int parse_args(int argc, char *argv[]) {
    int copt;

    const char *optstr = "c:h:m:t:p:y";
    while ((copt = getopt(argc, argv, optstr)) != -1) {
        switch (copt) {
        case 'c': concurrency = std::atoi(optarg); break;
        case 'h': handler_threads = std::atoi(optarg); break;
        case 'm': max_secs_per_test = std::atoi(optarg); break;
        case 't': total = std::atoll(optarg); break;
        case 'p': poller_threads = std::atoi(optarg); break;
        case 'y': yes = true; break;

        case '?': usage(argv[0]); return 0;
        default: usage(argv[0]); return -1;
        }
    }

    if (yes)
        return 1;

    std::cout << "Benchmark with"
        << "\ntotal: " << total
        << "\nconcurrency(>=1024): " << concurrency
        << "\nmax_secs_per_test: " << max_secs_per_test
        << "\nhandler_threads: " << handler_threads
        << "\npoller_threads: " << poller_threads
        << "\n\nContinue (y/N)?: ";
    std::cout.flush();

    char ch = std::cin.get();
    if (ch == 'y')
        return 1;

    usage(argv[0]);
    return 0;
}

void delimiter() {
    std::cout << "| " << std::string(16, '-')
        << " | " << std::string(8, '-')
        << " | " << std::string(14, '-')
        << " |" << std::endl;
}

void head() {
    std::cout << "| " << std::setw(16) << "name"
        << " | " << std::setw(8) << "cost"
        << " | " << std::setw(14) << "sleep per sec"
        << " |" << std::endl;

    delimiter();
}
