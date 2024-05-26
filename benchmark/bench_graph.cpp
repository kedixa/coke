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

#include <iostream>
#include <vector>
#include <utility>

#include "option_parser.h"
#include "bench_common.h"

#include "coke/coke.h"
#include "coke/dag.h"
#include "workflow/WFTaskFactory.h"

std::vector<int> width{16, 8, 6, 8, 6, 10};

int poller_threads = 6;
int handler_threads = 20;

int num_nodes = 128;
int times = 1;
int total = 500;
int group_size = 10;
int task_per_node = 3;
int max_secs_per_test = 5;
bool yes = false;

template<typename RootCreater, typename NodeCreater>
WFGraphTask *wf_create_chain(RootCreater &&root_creater,
                             NodeCreater &&node_creater) {
    WFGraphTask *g = WFTaskFactory::create_graph_task(nullptr);
    WFGraphNode *a, *b;

    a = root_creater(g);
    b = NULL;

    for (int i = 1; i < num_nodes; i++) {
        b = node_creater(g);
        *a > *b;
        a = b;
    }

    return g;
}

template<typename RootCreater, typename NodeCreater>
WFGraphTask *wf_create_tree(RootCreater &&root_creater,
                            NodeCreater &&node_creater) {
    WFGraphTask *g = WFTaskFactory::create_graph_task(nullptr);
    std::vector<WFGraphNode *> nodes;

    nodes.reserve(num_nodes);
    nodes.push_back(root_creater(g));

    for (int i = 1; i < num_nodes; i++) {
        nodes.push_back(node_creater(g));
        *(nodes[i/2]) > *(nodes[i]);
    }

    return g;
}

template<typename RootCreater, typename NodeCreater>
WFGraphTask *wf_create_net(RootCreater &&root_creater,
                           NodeCreater &&node_creater) {
    WFGraphTask *g = WFTaskFactory::create_graph_task(nullptr);
    std::vector<WFGraphNode *> nodes;
    int last_start = 0, last_stop = 1;
    int cur_start, cur_stop;

    nodes.reserve(num_nodes);
    nodes.push_back(root_creater(g));

    int i = 1;
    while (i < num_nodes) {
        cur_start = i;
        cur_stop = std::min(cur_start + group_size, num_nodes);
        for(; i < cur_stop; i++) {
            nodes.push_back(node_creater(g));

            for (int j = last_start; j < last_stop; j++)
                *(nodes[j]) > *(nodes[i]);
        }

        last_start = cur_start;
        last_stop = cur_stop;
    }

    return g;
}

template<typename RootCreater, typename NodeCreater>
WFGraphTask *wf_create_flower(RootCreater &&root_creater,
                              NodeCreater &&node_creater) {
    WFGraphTask *g = WFTaskFactory::create_graph_task(nullptr);
    WFGraphNode *a, *b;
    a = root_creater(g);

    for (int i = 1; i < num_nodes; i++) {
        b = node_creater(g);
        *a > *b;
    }

    return g;
}

template<typename RootFunc, typename NodeFunc>
auto coke_create_chain(RootFunc &&root_func, NodeFunc &&node_func) {
    coke::DagBuilder<void> builder;
    coke::DagNodeRef a = builder.root();
    coke::DagNodeRef b = builder.node(root_func);
    a > b;
    a = b;

    for (int i = 1; i < num_nodes; i++) {
        b = builder.node(node_func);
        a > b;
        a = b;
    }

    return builder.build();
}

template<typename RootFunc, typename NodeFunc>
auto coke_create_tree(RootFunc &&root_func, NodeFunc &&node_func) {
    coke::DagBuilder<void> builder;
    std::vector<coke::DagNodeRef<void>> nodes;
    nodes.reserve(num_nodes);

    coke::DagNodeRef root = builder.root();
    nodes.push_back(builder.node(root_func));
    root > nodes[0];

    for (int i = 1; i < num_nodes; i++) {
        nodes.push_back(builder.node(node_func));
        nodes[i/2] > nodes[i];
    }

    return builder.build();
}

template<typename RootFunc, typename NodeFunc>
auto coke_create_net(RootFunc &&root_func, NodeFunc &&node_func) {
    coke::DagBuilder<void> builder;
    std::vector<coke::DagNodeRef<void>> nodes;
    int last_start = 0, last_stop = 1;
    int cur_start, cur_stop;

    nodes.reserve(num_nodes);

    coke::DagNodeRef root = builder.root();
    nodes.push_back(builder.node(root_func));
    root > nodes[0];

    int i = 1;
    while (i < num_nodes) {
        cur_start = i;
        cur_stop = std::min(cur_start + group_size, num_nodes);
        for (; i < cur_stop; i++) {
            nodes.push_back(builder.node(node_func));

            for (int j = last_start; j < last_stop; j++)
                nodes[j] > nodes[i];
        }

        last_start = cur_start;
        last_stop = cur_stop;
    }

    return builder.build();
}

template<typename RootFunc, typename NodeFunc>
auto coke_create_flower(RootFunc &&root_func, NodeFunc &&node_func) {
    coke::DagBuilder<void> builder;
    coke::DagNodeRef root = builder.root();
    coke::DagNodeRef first = builder.node(root_func);

    root > first;

    for (int i = 1; i < num_nodes; i++) {
        first > builder.node(node_func);
    }

    return builder.build();
}

auto wf_timer_creater(WFGraphTask *g) {
    auto &x = g->create_graph_node(WFTaskFactory::create_go_task("", []{}));
    return &x;
}

auto wf_creater(WFGraphTask *g) {
    auto *c = WFTaskFactory::create_repeater_task(
        [i=0](WFRepeaterTask *) mutable -> SubTask * {
            if (i++ < task_per_node) {
                return WFTaskFactory::create_go_task("", []{});
            }
            return nullptr;
        },
        nullptr
    );

    auto &x = g->create_graph_node(c);
    return &x;
}

coke::Task<> coke_yield_func() { co_await coke::switch_go_thread(""); }

coke::Task<> coke_func() {
    for (int i = 0; i < task_per_node; i++) {
        co_await coke::switch_go_thread("");
    }
}

using wf_creater_t = decltype(wf_timer_creater);

template<typename Creater>
coke::Task<> do_bench_wf(Creater &&creater) {
    auto create = [cnt=0, creater] (WFRepeaterTask *) mutable -> SubTask * {
        if (cnt++ < total)
            return creater(wf_timer_creater, wf_creater);
        return nullptr;
    };

    coke::GenericAwaiter<void> g;
    auto callback = [&g] (WFRepeaterTask *) { g.done(); };
    auto *rep = WFTaskFactory::create_repeater_task(create, callback);
    g.take_over(rep);

    co_await g;
}

// benchmarks

coke::Task<> bench_wf_chain() {
    co_await do_bench_wf(wf_create_chain<wf_creater_t, wf_creater_t>);
}

coke::Task<> bench_coke_chain_once() {
    for (int i = 0; i < total; i++) {
        auto g = coke_create_chain(coke_yield_func, coke_func);
        co_await g->run();
    }
}

coke::Task<> bench_coke_chain() {
    auto g = coke_create_chain(coke_yield_func, coke_func);
    for (int i = 0; i < total; i++)
        co_await g->run();
}

coke::Task<> bench_wf_tree() {
    co_await do_bench_wf(wf_create_tree<wf_creater_t, wf_creater_t>);
}

coke::Task<> bench_coke_tree_once() {
    for (int i = 0; i < total; i++) {
        auto g = coke_create_tree(coke_yield_func, coke_func);
        co_await g->run();
    }
}

coke::Task<> bench_coke_tree() {
    auto g = coke_create_tree(coke_yield_func, coke_func);
    for (int i = 0; i < total; i++)
        co_await g->run();
}

coke::Task<> bench_wf_net() {
    co_await do_bench_wf(wf_create_net<wf_creater_t, wf_creater_t>);
}

coke::Task<> bench_coke_net_once() {
    for (int i = 0; i < total; i++) {
        auto g = coke_create_net(coke_yield_func, coke_func);
        co_await g->run();
    }
}

coke::Task<> bench_coke_net() {
    auto g = coke_create_net(coke_yield_func, coke_func);
    for (int i = 0; i < total; i++)
        co_await g->run();
}

coke::Task<> bench_wf_flower() {
    co_await do_bench_wf(wf_create_flower<wf_creater_t, wf_creater_t>);
}

coke::Task<> bench_coke_flower_once() {
    for (int i = 0; i < total; i++) {
        auto g = coke_create_flower(coke_yield_func, coke_func);
        co_await g->run();
    }
}

coke::Task<> bench_coke_flower() {
    auto g = coke_create_flower(coke_yield_func, coke_func);
    for (int i = 0; i < total; i++)
        co_await g->run();
}

coke::Task<> warm_up() { co_await coke::yield(); }

using bench_func_t = coke::Task<>(*)();
coke::Task<> do_benchmark(const char *name, bench_func_t func) {
    int run_times = 0;
    long long start, total_cost = 0;
    std::vector<long long> costs;
    double mean, stddev, tps;

    for (int i = 0; i < times; i++) {
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
    OptionParser args;

    args.add_integral(total, 't', "total", false,
                      "run total graphs for each benchmark", 500);
    args.add_integral(num_nodes, 'n', "num-nodes", false,
                      "number of nodes in each graph", 128);
    args.add_integral(times, 0, "times", false,
                      "run these times for each benchmark", 1);
    args.add_integral(group_size, 'g', "group-size", false,
                      "number of nodes in each group in net graph", 10);
    args.add_integral(task_per_node, 'p', "task-per-node", false,
                      "number of tasks in each node", 3);
    args.add_integral(max_secs_per_test, 'm', "max-secs", false,
                      "max seconds for each benchmark", 5);
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
    DO_BENCHMARK(wf_chain);
    DO_BENCHMARK(coke_chain_once);
    DO_BENCHMARK(coke_chain);
    delimiter(std::cout, width);

    DO_BENCHMARK(wf_tree);
    DO_BENCHMARK(coke_tree_once);
    DO_BENCHMARK(coke_tree);
    delimiter(std::cout, width);

    DO_BENCHMARK(wf_net);
    DO_BENCHMARK(coke_net_once);
    DO_BENCHMARK(coke_net);
    delimiter(std::cout, width);

    DO_BENCHMARK(wf_flower);
    DO_BENCHMARK(coke_flower_once);
    DO_BENCHMARK(coke_flower);
#undef DO_BENCHMARK

    return 0;
}
