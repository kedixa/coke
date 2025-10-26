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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <random>
#include <string>

#include "bench_common.h"
#include "coke/global.h"
#include "coke/redis/client.h"
#include "coke/redis/cluster_client.h"
#include "coke/stop_token.h"
#include "coke/wait.h"

std::size_t concurrency = 32;
std::size_t message_size = 16;
std::size_t duration = 5;
std::size_t max_samples = 1000000;

int handler_threads = 20;
int poller_threads = 10;

bool yes = false;
bool cluster = false;

std::string host;
std::string port;
std::string password;

std::atomic<bool> running{true};
std::atomic<std::size_t> succ_cnt{0};
std::atomic<std::size_t> fail_cnt{0};

long current_us()
{
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}

void signal_handler(int)
{
    if (!running.load())
        std::quick_exit(1);

    running.store(false);
}

coke::Task<> countdown(coke::StopToken &tk, std::size_t seconds)
{
    coke::StopToken::FinishGuard guard(&tk);

    co_await tk.wait_stop_for(std::chrono::seconds(seconds));
    running.store(false);
}

template<typename Client>
coke::Task<> run(Client &cli, std::size_t samples, std::vector<long> &costs,
                 std::size_t cid)
{
    std::string message(message_size, 'x');
    std::string_view msg_view = message;
    std::size_t seed = (std::size_t)current_us();
    std::mt19937_64 mt(seed ^ cid);
    std::size_t local_succ{0}, local_fail{0};
    uint64_t rnd;

    for (std::size_t idx = 0; running.load(); ++idx) {
        long start, cost;
        coke::RedisResult res;

        start = current_us();
        res = co_await cli.ping(msg_view);
        cost = current_us() - start;

        if (costs.size() < samples)
            costs.push_back(cost);
        else {
            rnd = mt() % idx;
            if (rnd < samples)
                costs[rnd] = cost;
        }

        if (res.get_state() == coke::STATE_SUCCESS)
            ++local_succ;
        else
            ++local_fail;
    }

    succ_cnt += local_succ;
    fail_cnt += local_fail;
}

template<typename Client>
void benchmark(Client &cli)
{
    std::vector<coke::Task<>> tasks;
    std::vector<std::vector<long>> costs_vec;
    std::size_t samples;

    samples = max_samples / concurrency;
    if (max_samples % concurrency != 0)
        ++samples;

    tasks.reserve(concurrency);
    costs_vec.resize(concurrency);

    coke::StopToken tk(1);
    coke::detach(countdown(tk, duration));

    for (std::size_t i = 0; i < concurrency; i++) {
        costs_vec[i].reserve(samples);
        tasks.push_back(run(cli, samples, costs_vec[i], i));
    }

    long start, total_cost;

    start = current_us();
    coke::sync_wait(std::move(tasks));
    total_cost = current_us() - start;

    tk.request_stop();
    coke::sync_wait(tk.wait_finish());

    std::size_t total = succ_cnt.load() + fail_cnt.load();
    double qps = 1.0e6 * total / total_cost;

    std::vector<long> costs;
    costs.reserve(samples * concurrency);

    for (auto &c : costs_vec)
        costs.insert(costs.end(), c.begin(), c.end());

    std::size_t p999pos = (std::size_t)std::floor(0.999 * costs.size());
    std::size_t p99pos = (std::size_t)std::floor(0.99 * costs.size());
    std::size_t p95pos = (std::size_t)std::floor(0.95 * costs.size());

    std::sort(costs.begin(), costs.end());

    printf("| %6s | %8s | %8s | %10s | %8s | %8s | %8s | %8s |\n", "conc",
           "cost(s)", "qps", "reqs", "fail", "p95(us)", "p99(us)", "p999(us)");

    printf("| %6zu | %8.2lf | %8.0lf | %10zu | %8zu | %8zu | %8zu | %8zu |\n",
           concurrency, total_cost / 1.0e6, qps, total, fail_cnt.load(),
           costs[p95pos], costs[p99pos], costs[p999pos]);
}

int main(int argc, char *argv[])
{
    coke::OptionParser args;

    args.add_integer(concurrency, 'c', "concurrency")
        .set_default(32)
        .set_description("Number of concurrent.");
    args.add_integer(message_size, 's', "message-size")
        .set_default(16)
        .set_description("Message size when send PING.");
    args.add_integer(duration, 'd', "duration")
        .set_default(5)
        .set_description("Seconds to run benchmark.");
    args.add_integer(max_samples, 0, "max-samples")
        .set_default(1000000)
        .set_description("Max number of samples to calculate latency.");
    args.add_integer(handler_threads, 0, "handler")
        .set_default(20)
        .set_description("Number of handler threads.");
    args.add_integer(poller_threads, 0, "poller")
        .set_default(10)
        .set_description("Number of poller threads.");
    args.add_string(host, 0, "host", true)
        .set_description("Host of redis server.");
    args.add_string(port, 0, "port")
        .set_default("6379")
        .set_description("Port of redis server.");
    args.add_string(password, 0, "password")
        .set_description("Password of redis server.");
    args.add_flag(cluster, 0, "cluster")
        .set_description("The redis server is cluster.");
    args.add_flag(yes, 'y', "yes").set_description("Skip asking before start.");
    args.set_help_flag('h', "help");

    int ret = parse_args(args, argc, argv, &yes);
    if (ret <= 0)
        return ret;

    signal(SIGINT, signal_handler);

    coke::GlobalSettings gs;
    gs.poller_threads = poller_threads;
    gs.handler_threads = handler_threads;
    gs.endpoint_params.max_connections = 60000;
    coke::library_init(gs);

    if (cluster) {
        coke::RedisClusterClientParams params;
        params.host = host;
        params.port = port;
        params.password = password;
        params.read_replica = true;

        coke::RedisClusterClient cli(params);
        benchmark(cli);
    }
    else {
        coke::RedisClientParams params;
        params.host = host;
        params.port = port;
        params.password = password;

        coke::RedisClient cli(params);
        benchmark(cli);
    }

    return 0;
}
