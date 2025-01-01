#include <atomic>
#include <iostream>
#include <mutex>

#include "coke/dag.h"
#include "coke/future.h"
#include "coke/sleep.h"
#include "coke/wait.h"

/**
 * This example shows how to use dag. For tasks with complex dependencies, using
 * dag can simplify the way of executing tasks.
 */

struct MyGraphContext {
    char cancel_in_node{0};
    std::atomic<bool> canceled{false};
};

using MyDagPtr = std::shared_ptr<coke::DagGraph<MyGraphContext>>;

auto create_node(char x) {
    return [x](MyGraphContext &ctx) -> coke::Task<> {
        // Coke's dag does not have a cancellation mechanism, but it can be
        // achieved by setting a flag in the Context.
        if (ctx.canceled.load()) {
            std::cout << x << ": the graph is canceled\n";
            co_return;
        }

        std::cout << x << ": start\n";
        co_await coke::sleep(0.1);
        std::cout << x << ": finish\n";

        if (ctx.cancel_in_node == x)
            ctx.canceled.store(true);
    };
}

MyDagPtr create_dag() {
    coke::DagBuilder<MyGraphContext> builder;

    auto root = builder.root();
    auto A = builder.node(create_node('A'), "This is the node name");
    auto B = builder.node(create_node('B'), "B");
    auto C = builder.node(create_node('C'), "C");
    auto D = builder.node(create_node('D'), "D");
    auto E = builder.node(create_node('E'), "E");
    auto F = builder.node(create_node('F'), "F");

    /**
     * Create DAG
     *            /-> B --\
     * root --> A --> C --> E --> F
     *            \-> D --------/
     */

    using Group = coke::DagNodeGroup<MyGraphContext>;

    // Connect root to A, short for builder.connect(root, A);
    // Each node must be reachable from root.
    root > A;

    // Connect A to B, C, D, short for A > B; A > C; A > D;
    A > Group{B, C, D};

    // Connect B, C to E, short for B > E; C > E;
    // And then connect E to F.
    Group{B, C} > E > F;

    // Connect D to F
    D > F;

    // It can also be simplified to
    // root > A > Group{B, C} > E > F;
    // A > D > F;

    return builder.build();
}

coke::Task<> use_dag() {
    auto dag = create_dag();

    std::cout << "Is this DAG valid? " << (dag->valid() ? "yes!" : "no!") << std::endl;
    std::cout << "The DAG in dot format:\n";
    dag->dump(std::cout);

    std::cout << std::string(64, '-') << std::endl;

    // 1. Use it directly.
    {
        MyGraphContext ctx;
        co_await dag->run(ctx);
    }

    std::cout << std::string(64, '-') << std::endl;

    // 2. Cancel in node.
    {
        MyGraphContext ctx;
        ctx.cancel_in_node = 'C';
        co_await dag->run(ctx);
    }

    std::cout << std::string(64, '-') << std::endl;

    // 3. Cancel outside.
    {
        MyGraphContext ctx;

        // Coke's dag don't support timed wait, use coke::Future.
        coke::Future<void> fut = coke::create_future(dag->run(ctx));
        int fut_ret = co_await fut.wait_for(std::chrono::milliseconds(150));
        if (fut_ret != coke::FUTURE_STATE_READY)
            ctx.canceled.store(true);

        // Wait dag->run finish, before ctx is destroyed.
        co_await fut.wait();
    }
}

int main() {
    coke::sync_wait(use_dag());
    return 0;
}
