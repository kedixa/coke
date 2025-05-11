/**
 * Copyright 2025 Coke Project (https://github.com/kedixa/coke)
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
#include <memory>
#include <thread>
#include <vector>

#include "coke/coke.h"
#include "coke/nspolicy/weighted_least_conn_policy.h"
#include "coke/nspolicy/weighted_random_policy.h"
#include "coke/nspolicy/weighted_round_robin_policy.h"

#include "bench_common.h"

std::atomic<long> current;
std::vector<int> width{20, 10, 10, 8, 6, 8, 8, 10};

long long total = 1000000;
int concurrency = 4;
int times       = 1;
bool yes        = false;

bool next(long long &cur)
{
    cur = current.fetch_add(1, std::memory_order_relaxed);
    if (cur < total)
        return true;

    current.fetch_sub(1, std::memory_order_relaxed);
    return false;
}

void thread_func(coke::NSPolicy *policy, int fail_ratio)
{
    ParsedURI uri;
    long long i;

    uri.state = URI_STATE_SUCCESS;

    while (next(i)) {
        auto *addr = policy->select_address(uri, {});
        if (addr == nullptr)
            continue;

        if (i % fail_ratio != 0)
            policy->addr_success(addr);
        else
            policy->addr_failed(addr);

        addr->dec_ref();
    }
}

void bench_policy(coke::NSPolicy *policy, int fail_ratio)
{
    std::vector<std::thread> vth;

    vth.reserve(concurrency);

    for (int i = 0; i < concurrency; i++)
        vth.emplace_back(thread_func, policy, fail_ratio);

    for (int i = 0; i < concurrency; i++)
        vth[i].join();
}

void do_benchmark(const char *name, coke::NSPolicy *policy, int num_addr,
                  int fail_ratio)
{
    int run_times = 0;
    long long start, total_cost = 0;
    std::vector<long long> costs;
    double mean, stddev, tps;

    for (int i = 0; i < times; i++) {
        current = 0;
        start   = current_msec();

        bench_policy(policy, fail_ratio);

        costs.push_back(current_msec() - start);
        total_cost += costs.back();
        run_times++;
    }

    data_distribution(costs, mean, stddev);
    tps = 1.0e3 * current / (mean + 1e-9);

    std::string ratio = "1/" + std::to_string(fail_ratio);
    table_line(std::cout, width, name, num_addr, ratio, total_cost, run_times,
               mean, stddev, (long)tps);
}

void init(coke::NSPolicy *policy, int num_addr)
{
    coke::AddressParams params;
    for (int i = 0; i < num_addr; i++) {
        std::string host = "test-" + std::to_string(i);
        policy->add_address(host, "80", params);
    }
}

int main(int argc, char *argv[])
{
    coke::OptionParser args;

    args.add_integer(concurrency, 'c', "concurrency")
        .set_default(4)
        .set_description("Number of threads to run concurrently");
    args.add_integer(total, 't', "total")
        .set_default(1000000)
        .set_description("Total number of select to perform");
    args.add_integer(times, coke::NULL_SHORT_NAME, "times")
        .set_default(1)
        .set_description("Number of times for each benchmark to run");
    args.add_flag(yes, 'y', "yes").set_description("Skip asking before start");
    args.set_help_flag('h', "help");

    int ret = parse_args(args, argc, argv, &yes);
    if (ret <= 0)
        return ret;

    coke::NSPolicyParams params;
    params.break_timeout_ms = 0;
    params.max_fail_marks   = 1;

    constexpr int N = 4;
    int naddrs[N]   = {1000, 10000, 100000, 1000000};

    using coke::WeightedLeastConnPolicy;
    using coke::WeightedRandomPolicy;
    using coke::WeightedRoundRobinPolicy;

    std::vector<std::unique_ptr<WeightedRandomPolicy>> wrs;
    std::vector<std::unique_ptr<WeightedLeastConnPolicy>> lcs;
    std::vector<std::unique_ptr<WeightedRoundRobinPolicy>> rrs;

    for (int i = 0; i < N; i++) {
        wrs.emplace_back(std::make_unique<WeightedRandomPolicy>(params));
        lcs.emplace_back(std::make_unique<WeightedLeastConnPolicy>(params));
        rrs.emplace_back(std::make_unique<WeightedRoundRobinPolicy>(params));

        init(wrs[i].get(), naddrs[i]);
        init(lcs[i].get(), naddrs[i]);
        init(rrs[i].get(), naddrs[i]);
    }

    std::cout.precision(2);
    std::cout.setf(std::ios_base::fixed, std::ios_base::floatfield);

    table_line(std::cout, width, "name", "num addrs", "fail ratio", "cost(ms)",
               "times", "mean(ms)", "stddev", "per sec");
    delimiter(std::cout, width, '-');

    do_benchmark("weighted_random", wrs[0].get(), naddrs[0], 10000);
    do_benchmark("weighted_random", wrs[1].get(), naddrs[1], 10000);
    do_benchmark("weighted_random", wrs[2].get(), naddrs[2], 10000);
    do_benchmark("weighted_random", wrs[3].get(), naddrs[3], 10000);
    delimiter(std::cout, width, ' ');

    do_benchmark("weighted_random", wrs[1].get(), naddrs[1], 1000);
    do_benchmark("weighted_random", wrs[1].get(), naddrs[1], 100);
    do_benchmark("weighted_random", wrs[1].get(), naddrs[1], 10);
    delimiter(std::cout, width, ' ');

    do_benchmark("weighted_least_conn", lcs[0].get(), naddrs[0], 10000);
    do_benchmark("weighted_least_conn", lcs[1].get(), naddrs[1], 10000);
    do_benchmark("weighted_least_conn", lcs[2].get(), naddrs[2], 10000);
    do_benchmark("weighted_least_conn", lcs[3].get(), naddrs[3], 10000);
    delimiter(std::cout, width, ' ');

    do_benchmark("weighted_least_conn", lcs[1].get(), naddrs[1], 1000);
    do_benchmark("weighted_least_conn", lcs[1].get(), naddrs[1], 100);
    do_benchmark("weighted_least_conn", lcs[1].get(), naddrs[1], 10);
    delimiter(std::cout, width, ' ');

    do_benchmark("weighted_round_robin", rrs[0].get(), naddrs[0], 10000);
    do_benchmark("weighted_round_robin", rrs[1].get(), naddrs[1], 10000);
    do_benchmark("weighted_round_robin", rrs[2].get(), naddrs[2], 10000);
    do_benchmark("weighted_round_robin", rrs[3].get(), naddrs[3], 10000);
    delimiter(std::cout, width, ' ');

    do_benchmark("weighted_round_robin", rrs[1].get(), naddrs[1], 1000);
    do_benchmark("weighted_round_robin", rrs[1].get(), naddrs[1], 100);
    do_benchmark("weighted_round_robin", rrs[1].get(), naddrs[1], 10);

    return 0;
}
