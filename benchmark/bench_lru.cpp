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
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "bench_common.h"
#include "coke/coke.h"
#include "coke/lru_cache.h"
#include "coke/rlru_cache.h"

struct BenchParams {
    std::size_t nthread;
    std::size_t nkeys;
    std::size_t cache_size;
    long total_ops;
    double zipf_s;
};

class ZipfSampler {
public:
    explicit ZipfSampler(std::size_t n, double s = 1.0)
        : probs_table(n), alias_table(n)
    {
        std::vector<std::size_t> small, large;
        double sum = 0;

        for (std::size_t i = 0; i < n; ++i) {
            probs_table[i] = std::pow(i + 1, -s);
            sum += probs_table[i];
        }

        for (std::size_t i = 0; i < n; i++) {
            probs_table[i] = probs_table[i] / sum * n;
            alias_table[i] = i;

            if (probs_table[i] < 1.0)
                small.push_back(i);
            else if (probs_table[i] > 1.0)
                large.push_back(i);
        }

        std::size_t small_idx, large_idx;
        while (!small.empty() && !large.empty()) {
            small_idx = small.back();
            large_idx = large.back();
            small.pop_back();
            large.pop_back();

            probs_table[large_idx] -= 1.0 - probs_table[small_idx];
            alias_table[small_idx] = large_idx;

            if (probs_table[large_idx] < 1.0)
                small.push_back(large_idx);
            else if (probs_table[large_idx] > 1.0)
                large.push_back(large_idx);
        }
    }

    std::size_t sample(std::mt19937_64 &gen)
    {
        std::size_t i = gen() % probs_table.size();
        std::uniform_real_distribution<double> prob_dist(0.0, 1.0);

        if (prob_dist(gen) < probs_table[i])
            return i;

        return alias_table[i];
    }

private:
    std::vector<double> probs_table;
    std::vector<std::size_t> alias_table;
};

std::atomic<long> total_cnt;
std::atomic<long> miss_cnt;
std::size_t max_threads = 8;
long total_ops = 1000000;
bool yes = false;

std::vector<int> width{8, 8, 8, 12, 12, 12};
std::vector<std::size_t> nthreads{2, 3, 4, 6, 8, 12, 16};
std::vector<double> zipfs{1.172, 1.278, 1.552, 2.103};

template<typename Cache>
void bench_thread(Cache &cache, ZipfSampler &zipf, std::size_t seed)
{
    std::mt19937_64 mt(seed);
    std::string key;

    while (total_cnt-- > 0) {
        auto kid = zipf.sample(mt);
        key = std::to_string(kid);

        auto hdl = cache.get(key);
        if (!hdl) {
            ++miss_cnt;
            cache.put(key, key);
        }
    }
}

template<typename Cache>
void bench_impl(const char *name, BenchParams params)
{
    Cache cache(params.cache_size);
    ZipfSampler zipf(params.nkeys, params.zipf_s);

    total_cnt = params.total_ops;
    miss_cnt = 0;

    auto start = current_msec();
    std::vector<std::thread> vths;

    for (std::size_t i = 0; i < params.nthread; i++)
        vths.emplace_back(bench_thread<Cache>, std::ref(cache), std::ref(zipf),
                          i);

    for (auto &th : vths)
        th.join();

    auto cost = current_msec() - start;
    std::size_t qps = (params.total_ops * 1000 / cost);
    double miss_per = 100.0 * miss_cnt.load() / params.total_ops;
    table_line(std::cout, width, name, params.nthread, params.zipf_s, miss_per,
               cost, qps);
}

void benchmark()
{
    using RlruCache = coke::RlruCache<std::string, std::string>;
    using LruCache = coke::LruCache<std::string, std::string>;

    table_line(std::cout, width, "name", "nthread", "zipf_s", "miss%",
               "cost(ms)", "qps");
    delimiter(std::cout, width, '-');

    BenchParams params;

    params.nkeys = 1000000;
    params.cache_size = 50000;
    params.total_ops = total_ops;

    for (std::size_t th : nthreads) {
        if (th > max_threads)
            break;

        delimiter(std::cout, width);

        for (double s : zipfs) {
            params.nthread = th;
            params.zipf_s = s;

            bench_impl<RlruCache>("rlru", params);
        }
    }

    for (std::size_t th : nthreads) {
        if (th > max_threads)
            break;

        delimiter(std::cout, width);

        for (double s : zipfs) {
            params.nthread = th;
            params.zipf_s = s;

            bench_impl<LruCache>("lru", params);
        }
    }
}

int main(int argc, char *argv[])
{
    coke::OptionParser args;

    args.add_integer(total_ops, 'n', "total-ops")
        .set_default(1000000)
        .set_description("Total operations per test.");
    args.add_integer(max_threads, 't', "max-threads")
        .set_default(8)
        .set_description("Max threads to run benchmark.");
    args.add_flag(yes, 'y', "yes").set_description("Skip asking before start.");
    args.set_help_flag('h', "help");

    int ret = parse_args(args, argc, argv, &yes);
    if (ret <= 0)
        return ret;

    std::cout.precision(3);
    std::cout.setf(std::ios_base::fixed, std::ios_base::floatfield);

    benchmark();

    return 0;
}
