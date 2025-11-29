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

#include <algorithm>
#include <chrono>
#include <gtest/gtest.h>
#include <mutex>
#include <random>
#include <vector>

#include "coke/coke.h"
#include "coke/dag.h"

struct Context {
    std::vector<char> v;
    std::mutex mtx;
};

using DagGroup = coke::DagNodeGroup<Context>;
using DagBuilder = coke::DagBuilder<Context>;

uint64_t rand64()
{
    static std::mt19937_64 m(
        std::chrono::system_clock::now().time_since_epoch().count());
    static thread_local std::mt19937_64 lm(m());
    return lm();
}

coke::Task<> push(Context &ctx, char x)
{
    co_await coke::yield();

    {
        std::lock_guard<std::mutex> lg(ctx.mtx);
        ctx.v.push_back(x);
    }

    co_await coke::sleep(std::chrono::milliseconds(rand64() % 10));
}

auto create_node_func(char x)
{
    return [x](Context &ctx) {
        return push(ctx, x);
    };
}

void expect_before(const std::vector<char> &v, const std::vector<char> &l,
                   const std::vector<char> &r, bool any = false)
{
    std::vector<std::size_t> li;
    std::vector<std::size_t> ri;

    ASSERT_FALSE(l.empty());
    ASSERT_FALSE(r.empty());

    for (const auto &x : l)
        li.push_back(std::find(v.begin(), v.end(), x) - v.begin());

    for (const auto &x : r)
        ri.push_back(std::find(v.begin(), v.end(), x) - v.begin());

    std::sort(li.begin(), li.end());
    std::sort(ri.begin(), ri.end());

    ASSERT_LT(li.back(), v.size());
    ASSERT_LT(ri.back(), v.size());

    if (any)
        EXPECT_LE(li.front(), ri.front());
    else
        EXPECT_LE(li.back(), ri.front());
}

std::shared_ptr<coke::DagGraph<Context>> create_dag1()
{
    DagBuilder builder;

    /**
     * root -> A -> B -> C
     */
    builder.root()
        .then(create_node_func('A'))
        .then(create_node_func('B'))
        .then(create_node_func('C'));

    return builder.build();
}

void validate_dag1(Context &ctx)
{
    expect_before(ctx.v, {'A'}, {'B', 'C'});
    expect_before(ctx.v, {'B'}, {'C'});
}

std::shared_ptr<coke::DagGraph<Context>> create_dag2()
{
    DagBuilder builder;

    /**
     * root --> A --> C
     *      \-> B -/
     */
    auto root = builder.root();
    auto a = builder.node(create_node_func('A'));
    auto b = builder.node(create_node_func('B'));
    auto c = builder.node(create_node_func('C'));

    root > DagGroup{a, b} > c;

    return builder.build();
}

void validate_dag2(Context &ctx)
{
    expect_before(ctx.v, {'A', 'B'}, {'C'});
}

std::shared_ptr<coke::DagGraph<Context>> create_dag3()
{
    DagBuilder builder;

    /**
     *      /-> A --\
     * root --> B ~~> D
     *      \-> C ~~/
     *
     * Use ~ to indicate weak connections.
     */

    auto root = builder.root();
    auto a = builder.node(create_node_func('A'));
    auto b = builder.node(create_node_func('B'));
    auto c = builder.node(create_node_func('C'));
    auto d = builder.node(create_node_func('D'));

    root > DagGroup{a, b, c};
    a > d;
    DagGroup{b, c} >= d;

    return builder.build();
}

void validate_dag3(Context &ctx)
{
    expect_before(ctx.v, {'A'}, {'D'});
    expect_before(ctx.v, {'B', 'C'}, {'D'}, true);
}

using creator_t = std::shared_ptr<coke::DagGraph<Context>> (*)();
using validator_t = void (*)(Context &);

void test_dag(creator_t c, validator_t v)
{
    Context ctx;
    auto dag = c();
    EXPECT_TRUE(dag->valid());

    coke::sync_wait(dag->run(ctx));
    v(ctx);
}

TEST(DAG, test1)
{
    test_dag(create_dag1, validate_dag1);
}
TEST(DAG, test2)
{
    test_dag(create_dag2, validate_dag2);
}
TEST(DAG, test3)
{
    test_dag(create_dag3, validate_dag3);
}

int main(int argc, char *argv[])
{
    coke::GlobalSettings s;
    s.poller_threads = 2;
    s.handler_threads = 4;
    s.compute_threads = 4;
    coke::library_init(s);

    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
