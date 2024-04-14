#include <iostream>
#include <iomanip>
#include <vector>
#include <utility>
#include <cstdint>
#include <cstdlib>

#include <getopt.h>

#include "coke/coke.h"
#include "coke/dag.h"
#include "workflow/WFGraphTask.h"
#include "workflow/WFTaskFactory.h"

int max_nodes = 128;
int loop_times = 500;
int net_group_size = 10;
int task_per_node = 3;
bool yes;

int64_t current_msec() {
    auto dur = std::chrono::steady_clock::now().time_since_epoch();
    auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(dur);
    return msec.count();
}

template<typename RootCreater, typename NodeCreater>
WFGraphTask *wf_create_chain(RootCreater &&root_creater,
                             NodeCreater &&node_creater) {
    WFGraphTask *g = WFTaskFactory::create_graph_task(nullptr);
    WFGraphNode *a, *b;

    a = root_creater(g);
    b = NULL;

    for (int i = 1; i < max_nodes; i++) {
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

    nodes.reserve(max_nodes);
    nodes.push_back(root_creater(g));

    for (int i = 1; i < max_nodes; i++) {
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

    nodes.reserve(max_nodes);
    nodes.push_back(root_creater(g));

    int i = 1;
    while (i < max_nodes) {
        cur_start = i;
        cur_stop = std::min(cur_start + net_group_size, max_nodes);
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

    for (int i = 1; i < max_nodes; i++) {
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

    for (int i = 1; i < max_nodes; i++) {
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
    nodes.reserve(max_nodes);

    coke::DagNodeRef root = builder.root();
    nodes.push_back(builder.node(root_func));
    root > nodes[0];

    for (int i = 1; i < max_nodes; i++) {
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

    nodes.reserve(max_nodes);

    coke::DagNodeRef root = builder.root();
    nodes.push_back(builder.node(root_func));
    root > nodes[0];

    int i = 1;
    while (i < max_nodes) {
        cur_start = i;
        cur_stop = std::min(cur_start + net_group_size, max_nodes);
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

    for (int i = 1; i < max_nodes; i++) {
        first > builder.node(node_func);
    }

    return builder.build();
}

auto wf_timer_creater(WFGraphTask *g) {
    auto &x = g->create_graph_node(WFTaskFactory::create_timer_task(0, 0, nullptr));
    return &x;
}

auto wf_creater(WFGraphTask *g) {
    auto *c = WFTaskFactory::create_repeater_task(
        [i=0](WFRepeaterTask *) mutable -> SubTask * {
            if (i++ < task_per_node) {
                return WFTaskFactory::create_timer_task(0, 0, nullptr);
                //return WFTaskFactory::create_go_task("", []{});
            }
            return nullptr;
        },
        nullptr
    );

    auto &x = g->create_graph_node(c);
    return &x;
}

coke::Task<> coke_yield_func() { co_await coke::yield(); }

coke::Task<> coke_func() {
    for (int i = 0; i < task_per_node; i++) {
        co_await coke::yield();
    }
}

using wf_creater_t = decltype(wf_timer_creater);

template<typename Creater>
coke::Task<> do_bench_wf(Creater &&creater) {
    auto create = [cnt=0, creater] (WFRepeaterTask *) mutable -> SubTask * {
        if (cnt++ < loop_times)
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
    for (int i = 0; i < loop_times; i++) {
        auto g = coke_create_chain(coke_yield_func, coke_func);
        co_await g->run();
    }
}

coke::Task<> bench_coke_chain() {
    auto g = coke_create_chain(coke_yield_func, coke_func);
    for (int i = 0; i < loop_times; i++)
        co_await g->run();
}

coke::Task<> bench_wf_tree() {
    co_await do_bench_wf(wf_create_tree<wf_creater_t, wf_creater_t>);
}

coke::Task<> bench_coke_tree_once() {
    for (int i = 0; i < loop_times; i++) {
        auto g = coke_create_tree(coke_yield_func, coke_func);
        co_await g->run();
    }
}

coke::Task<> bench_coke_tree() {
    auto g = coke_create_tree(coke_yield_func, coke_func);
    for (int i = 0; i < loop_times; i++)
        co_await g->run();
}

coke::Task<> bench_wf_net() {
    co_await do_bench_wf(wf_create_net<wf_creater_t, wf_creater_t>);
}

coke::Task<> bench_coke_net_once() {
    for (int i = 0; i < loop_times; i++) {
        auto g = coke_create_net(coke_yield_func, coke_func);
        co_await g->run();
    }
}

coke::Task<> bench_coke_net() {
    auto g = coke_create_net(coke_yield_func, coke_func);
    for (int i = 0; i < loop_times; i++)
        co_await g->run();
}

coke::Task<> bench_wf_flower() {
    co_await do_bench_wf(wf_create_flower<wf_creater_t, wf_creater_t>);
}

coke::Task<> bench_coke_flower_once() {
    for (int i = 0; i < loop_times; i++) {
        auto g = coke_create_flower(coke_yield_func, coke_func);
        co_await g->run();
    }
}

coke::Task<> bench_coke_flower() {
    auto g = coke_create_flower(coke_yield_func, coke_func);
    for (int i = 0; i < loop_times; i++)
        co_await g->run();
}

using bench_func_t = coke::Task<>(*)();
coke::Task<> do_benchmark(const char *name, bench_func_t func) {
    int64_t start, cost;
    std::vector<coke::Task<>> tasks;

    start = current_msec();
    co_await func();
    cost = current_msec() - start;

    if (cost == 0)
        cost = 1;

    double tps = 1.0e3 * loop_times / cost;

    std::cout << "| " << std::setw(16) << name
        << " | " << std::setw(8) << cost
        << " | " << std::setw(14) << std::size_t(tps)
        << " |" << std::endl;
}

// helper
void head();
void delimiter(char c = ' ');
void usage(const char *arg0);
int parse_args(int, char *[]);

int main(int argc, char *argv[]) {
    int ret = parse_args(argc, argv);
    if (ret != 1)
        return ret;

    head();

#define DO_BENCHMARK(func) coke::sync_wait(do_benchmark(#func, bench_ ## func))
    DO_BENCHMARK(wf_chain);
    DO_BENCHMARK(coke_chain_once);
    DO_BENCHMARK(coke_chain);
    delimiter();

    DO_BENCHMARK(wf_tree);
    DO_BENCHMARK(coke_tree_once);
    DO_BENCHMARK(coke_tree);
    delimiter();

    DO_BENCHMARK(wf_net);
    DO_BENCHMARK(coke_net_once);
    DO_BENCHMARK(coke_net);
    delimiter();

    DO_BENCHMARK(wf_flower);
    DO_BENCHMARK(coke_flower_once);
    DO_BENCHMARK(coke_flower);
#undef DO_BENCHMARK

    return 0;
}

void usage(const char *arg0) {
    std::cout << arg0 << " [OPTION]...\n" <<
R"usage(
    -g count        `count` nodes in each group in net graph, default 10
    -n count        `count` nodes in each graph, default 128
    -t total        run `total` graphs for each case, default 500
    -k count        `count` tasks in each node, default 3
    -y              skip showing arguments before start benchmark
)usage"
    << std::endl;
}

int parse_args(int argc, char *argv[]) {
    const char *optstr = "g:k:n:t:y";
    int copt;

    while ((copt = getopt(argc, argv, optstr)) != -1) {
        switch (copt) {
        case 'g': net_group_size = std::atoi(optarg);   break;
        case 'n': max_nodes = std::atoi(optarg);        break;
        case 't': loop_times = std::atoi(optarg);       break;
        case 'k': task_per_node = std::atoi(optarg);    break;
        case 'y': yes = true;                           break;

        case '?': usage(argv[0]); return 0;
        default: usage(argv[0]); return -1;
        }
    }

    if (yes)
        return 1;

    std::cout << "Benchmark with"
        << "\nnet group size: " << net_group_size
        << "\ngraph nodes: " << max_nodes
        << "\ntask per node: " << task_per_node
        << "\nloop times: " << loop_times
        << "\n\nContinue (y/N)?:";
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
        << " | " << std::setw(14) << "graph per sec"
        << " |" << std::endl;

    delimiter('-');
}
