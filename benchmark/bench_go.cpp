#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

#include <getopt.h>

#include "coke/coke.h"
#include "workflow/WFTaskFactory.h"

alignas(64) std::atomic<bool> stop_flag;
alignas(64) std::atomic<std::size_t> current;
alignas(64) std::atomic<long long> global_total;

constexpr std::size_t pool_size = 10;
std::string name_pool[pool_size];

std::size_t total{10000};
int concurrency = 32;
int max_secs_per_test = 5;
int compute_threads = -1;
bool yes = false;

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

void do_calculate() {
    volatile int x = 0;
    for (int i = 0; i < 128; i++)
        x = x + i;

    global_total += x;
}

coke::Task<> count_down(uint64_t id, std::chrono::seconds timeout) {
    co_await coke::sleep(id, timeout);
    stop_flag.store(true, std::memory_order_release);
    stop_flag.notify_all();
}

coke::Task<> bench_wf_go_name(std::size_t max) {
    std::size_t i;

    auto creater = [i, max](WFRepeaterTask *) mutable -> SubTask * {
        if (next(i))
            return WFTaskFactory::create_go_task(name_pool[i%max], do_calculate);
        return nullptr;
    };

    coke::GenericAwaiter<void> g;
    WFRepeaterTask *task = WFTaskFactory::create_repeater_task(creater,
        [&g](WFRepeaterTask *) {
            g.done();
        }
    );
    g.take_over(task);
    co_await g;
}

coke::Task<> bench_go_name(std::size_t max) {
    std::size_t i;

    while (next(i)) {
        const std::string &name = name_pool[i%max];
        co_await coke::go(name, do_calculate);
    }
}

coke::Task<> bench_switch_name(std::size_t max) {
    std::size_t i;

    while (next(i)) {
        const std::string &name = name_pool[i%max];
        co_await coke::switch_go_thread(name);
        do_calculate();
    }
}

coke::Task<> bench_wf_go_one_name() { return bench_wf_go_name(1); }

coke::Task<> bench_wf_go_five_name() { return bench_wf_go_name(5); }

coke::Task<> bench_wf_go_ten_name() { return bench_wf_go_name(10); }

coke::Task<> bench_go_one_name() { return bench_go_name(1); }

coke::Task<> bench_go_five_name() { return bench_go_name(5); }

coke::Task<> bench_go_ten_name() { return bench_go_name(10); }

coke::Task<> bench_switch_one_name() { return bench_switch_name(1); }

coke::Task<> bench_switch_five_name() { return bench_switch_name(5); }

coke::Task<> bench_switch_ten_name() { return bench_switch_name(10); }


// helper

void head();
void delimiter(char c = ' ');
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
    global_total = 0;

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
    gs.compute_threads = compute_threads;
    coke::library_init(gs);

    coke::sync_wait(warm_up());

    for (std::size_t i = 0; i < pool_size; i++) {
        name_pool[i] = std::to_string(i);
    }

    head();

#define DO_BENCHMARK(func) coke::sync_wait(do_benchmark(#func, bench_ ## func))
    DO_BENCHMARK(wf_go_one_name);
    DO_BENCHMARK(wf_go_five_name);
    DO_BENCHMARK(wf_go_ten_name);
    delimiter();

    DO_BENCHMARK(go_one_name);
    DO_BENCHMARK(go_five_name);
    DO_BENCHMARK(go_ten_name);
    delimiter();

    DO_BENCHMARK(switch_one_name);
    DO_BENCHMARK(switch_five_name);
    DO_BENCHMARK(switch_ten_name);
#undef DO_BENCHMARK

    return 0;
}

void usage(const char *arg0) {
    std::cout << arg0 << " [OPTION]...\n" <<
R"usage(
    -c concurrency, start `concurrency` series to benchmark go, default 32
    -n n,           set compute threads to n, default -1
    -m sec,         run at most `sec` seconds for each case, default 5
    -t total,       run at most `total` timer for each case, default 10000
    -y              skip showing arguments before start benchmark
)usage"
    << std::endl;
}

int parse_args(int argc, char *argv[]) {
    int copt;

    const char *optstr = "c:n:m:t:y";
    while ((copt = getopt(argc, argv, optstr)) != -1) {
        switch (copt) {
        case 'c': concurrency = std::atoi(optarg); break;
        case 'n': compute_threads = std::atoi(optarg); break;
        case 'm': max_secs_per_test = std::atoi(optarg); break;
        case 't': total = std::atoll(optarg); break;
        case 'y': yes = true; break;

        case '?': usage(argv[0]); return 0;
        default: usage(argv[0]); return -1;
        }
    }

    if (yes)
        return 1;

    std::cout << "Benchmark with"
        << "\ntotal: " << total
        << "\nconcurrency: " << concurrency
        << "\nmax_secs_per_test: " << max_secs_per_test
        << "\ncompute_threads: " << compute_threads
        << "\n\nContinue (y/N)?: ";
    std::cout.flush();

    char ch = std::cin.get();
    if (ch == 'y')
        return 1;

    usage(argv[0]);
    return 0;
}

void delimiter(char c) {
    std::cout << "| " << std::string(16, c)
        << " | " << std::string(8, c)
        << " | " << std::string(14, c)
        << " |" << std::endl;
}

void head() {
    std::cout << "| " << std::setw(16) << "name"
        << " | " << std::setw(8) << "cost"
        << " | " << std::setw(14) << "go per sec"
        << " |" << std::endl;

    delimiter('-');
}
