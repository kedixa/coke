#include <atomic>
#include <string>
#include <vector>

#include "option_parser.h"
#include "bench_common.h"

#include "coke/coke.h"
#include "workflow/WFTaskFactory.h"

alignas(64) std::atomic<long long> current;
alignas(64) std::atomic<long long> global_total;

constexpr int pool_size = 10;
std::string name_pool[pool_size];
std::vector<int> width{16, 8, 6, 8, 6, 10};

long long total{100000};
int concurrency = 32;
int max_secs_per_test = 5;
int compute_threads = -1;
int times = 1;
bool yes = false;

bool next(long long &cur) {
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

coke::Task<> bench_wf_go_name(int max) {
    long long i = 0;

    auto creater = [i, max](WFRepeaterTask *) mutable -> SubTask * {
        if (next(i))
            return WFTaskFactory::create_go_task(name_pool[i%max], do_calculate);
        return nullptr;
    };

    coke::GenericAwaiter<void> g;
    WFRepeaterTask *task = WFTaskFactory::create_repeater_task(creater,
        [&g](WFRepeaterTask *) { g.done(); }
    );
    g.take_over(task);
    co_await g;
}

coke::Task<> bench_go_name(int max) {
    long long i;

    while (next(i)) {
        const std::string &name = name_pool[i%max];
        co_await coke::go(name, do_calculate);
    }
}

coke::Task<> bench_switch_name(int max) {
    long long i;

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

coke::Task<> warm_up() { co_await coke::switch_go_thread(); }

using bench_func_t = coke::Task<>(*)();
coke::Task<> do_benchmark(const char *name, bench_func_t func) {
    int run_times = 0;
    long long start, total_cost = 0;
    std::vector<long long> costs;
    double mean, stddev, tps;

    for (int i = 0; i < times; i++) {
        std::vector<coke::Task<>> tasks;

        current = 0;
        global_total = 0;

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

    args.add_integral(compute_threads, 0, "compute", false,
                      "set compute threads", -1);
    args.add_integral(total, 't', "total", false,
                      "total tasks for each benchmark",
                      (long long)100000);
    args.add_integral(concurrency, 'c', "concurrency", false,
                      "start these series to do benchmark", 32);
    args.add_integral(times, 0, "times", false,
                      "run these times for each benchmark", 1);
    args.add_integral(max_secs_per_test, 'm', "max-secs", false,
                      "max seconds for each benchmark", 5);
    args.add_flag(yes, 'y', "yes", "skip showing options before start");
    args.set_help_flag('h', "help");

    int ret = parse_args(args, argc, argv, &yes);
    if (ret <= 0)
        return ret;

    coke::GlobalSettings gs;
    gs.compute_threads = compute_threads;
    coke::library_init(gs);

    std::cout.precision(2);
    std::cout.setf(std::ios_base::fixed, std::ios_base::floatfield);

    coke::sync_wait(warm_up());

    for (int i = 0; i < pool_size; i++)
        name_pool[i] = std::to_string(i);

    table_line(std::cout, width,
               "name", "cost", "times",
               "mean(ms)", "stddev", "per sec");
    delimiter(std::cout, width, '-');

#define DO_BENCHMARK(func) coke::sync_wait(do_benchmark(#func, bench_ ## func))
    DO_BENCHMARK(wf_go_one_name);
    DO_BENCHMARK(wf_go_five_name);
    DO_BENCHMARK(wf_go_ten_name);
    delimiter(std::cout, width);

    DO_BENCHMARK(go_one_name);
    DO_BENCHMARK(go_five_name);
    DO_BENCHMARK(go_ten_name);
    delimiter(std::cout, width);

    DO_BENCHMARK(switch_one_name);
    DO_BENCHMARK(switch_five_name);
    DO_BENCHMARK(switch_ten_name);
#undef DO_BENCHMARK

    return 0;
}
