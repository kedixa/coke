#include <vector>
#include <string>
#include <random>
#include <optional>
#include <iterator>
#include <gtest/gtest.h>

#include "coke/global.h"
#include "coke/wait.h"
#include "coke/queue.h"
#include "coke/future.h"

constexpr int LOOP_HINT = 10;

uint64_t now_nsec() {
    auto now = std::chrono::system_clock::now();
    return (uint64_t)now.time_since_epoch().count();
}

template<typename Q>
coke::Task<void> single_push(Q &que, uint64_t seed, uint64_t count) {
    // don't block main thread, switch to handler
    co_await coke::yield();

    constexpr std::chrono::nanoseconds nsec_base(20000);
    std::chrono::nanoseconds nsec(20000);
    std::mt19937_64 mt(now_nsec() ^ seed);
    int loop_cnt = 0;
    int ret;

    for (uint64_t i = 0; i < count; i++) {
        uint64_t x = mt();

        switch (x % 6) {
        case 0:
            if (que.try_emplace(20, 'a' + x % 26))
                break;
            [[fallthrough]];
        case 1:
            co_await que.emplace(25, 'a' + x % 26);
            break;
        case 2:
            do {
                ret = co_await que.try_emplace_for(nsec, 30, 'a' + x % 26);
                if (++loop_cnt > LOOP_HINT) {
                    loop_cnt = 0;
                    nsec += nsec_base;
                }
            } while (ret != coke::TOP_SUCCESS);
            break;
        case 3:
            if (que.try_push(std::string(35, 'a' + x % 26)))
                break;
            [[fallthrough]];
        case 4:
            co_await que.push(std::string(36, 'a' + x % 26));
            break;
        case 5:
            do {
                std::string tmp(12, 'a' + x % 26);
                ret = co_await que.try_push_for(nsec, tmp);
                if (++loop_cnt > LOOP_HINT) {
                    loop_cnt = 0;
                    nsec += nsec_base;
                }
            } while (ret != coke::TOP_SUCCESS);
            break;
        }
    }
}

template<typename Q>
coke::Task<void> single_pop(Q &que, uint64_t seed, uint64_t &count) {
    co_await coke::yield();

    constexpr std::chrono::nanoseconds nsec_base(20000);
    std::chrono::nanoseconds nsec(20000);
    std::mt19937_64 mt(now_nsec() ^ seed);
    int ret = coke::TOP_SUCCESS;
    int loop_cnt = 0;

    std::string tmp;
    std::optional<std::string> tmp_opt;

    count = 0;

    while (ret != coke::TOP_CLOSED) {
        uint64_t x = mt();

        switch (x % 6) {
        case 0:
            if (!que.empty() && que.try_pop(tmp))
                break;
            [[fallthrough]];
        case 1:
            ret = co_await que.pop(tmp);
            break;
        case 2:
            do {
                ret = co_await que.try_pop_for(nsec, tmp);
                if (++loop_cnt > LOOP_HINT) {
                    loop_cnt = 0;
                    nsec += nsec_base;
                }
            } while (ret == coke::TOP_TIMEOUT);
            break;
        case 3:
            if (que.try_pop(tmp_opt))
                break;
            [[fallthrough]];
        case 4:
            ret = co_await que.pop(tmp_opt);
            break;
        case 5:
            do {
                ret = co_await que.try_pop_for(nsec, tmp_opt);
                if (++loop_cnt > LOOP_HINT) {
                    loop_cnt = 0;
                    nsec += nsec_base;
                }
            } while (ret == coke::TOP_TIMEOUT);
            break;
        }

        if (ret == coke::TOP_SUCCESS)
            ++count;
    }
}

template<typename Q>
coke::Task<> batch_push(Q &que, uint64_t batch_size, uint64_t count) {
    co_await coke::yield();

    std::vector<std::string> vs;
    std::mt19937_64 mt(now_nsec());

    for (uint64_t i = 0; i < count; i++) {
        vs.clear();
        for (uint64_t j = 0; j < batch_size; j++) {
            vs.emplace_back(std::string(mt() % 40, 'a' + mt() % 26));
        }

        auto first = std::move_iterator(vs.begin());
        auto last = std::move_iterator(vs.end());

        while (first != last) {
            first = que.try_push_range(first, last);

            if (first != last) {
                co_await que.push(*first);
                ++first;
            }
        }
    }
}

template<typename Q>
coke::Task<> batch_pop(Q &que, uint64_t batch_size, uint64_t &total_pop) {
    co_await coke::yield();

    std::mt19937_64 mt(now_nsec());
    std::vector<std::string> vs;
    std::string tmp;
    int ret;

    while (true) {
        if (que.empty() && que.closed())
            break;

        if (mt() % 2 == 0) {
            vs.resize(batch_size);

            auto first = vs.begin(), last = vs.end();
            auto rit = que.try_pop_range(first, last);
            total_pop += std::distance(first, rit);

            if (first == rit && !que.closed()) {
                ret = co_await que.pop(tmp);
                if (ret == coke::TOP_SUCCESS)
                    ++total_pop;
            }
        }
        else {
            vs.clear();
            auto n = que.try_pop_n(std::back_inserter(vs), batch_size);
            total_pop += n;

            if (n == 0 && !que.closed()) {
                ret = co_await que.pop(tmp);
                if (ret == coke::TOP_SUCCESS)
                    ++total_pop;
            }
        }
    }
}

template<typename Q>
coke::Task<> test_single(int num_co, int op_per_co,
                         std::size_t max_qsize) {
    std::vector<coke::Task<>> push_tasks;
    std::vector<coke::Future<void>> pop_futs;
    std::vector<uint64_t> vcount(num_co, 0);

    Q que(max_qsize);

    push_tasks.reserve(num_co);
    pop_futs.reserve(num_co);

    for (int i = 0; i < num_co; i++)
        push_tasks.emplace_back(single_push(que, (uint64_t)i, op_per_co));

    for (int i = 0; i < num_co; i++) {
        pop_futs.emplace_back(
            coke::create_future(single_pop(que, (uint64_t)i, vcount[i]))
        );
    }

    co_await coke::async_wait(std::move(push_tasks));
    que.close();

    for (auto &fut : pop_futs) {
        co_await fut.wait();
        fut.get();
    }

    uint64_t total_push = (uint64_t)num_co * op_per_co;
    uint64_t total_pop = 0;
    for (int i = 0; i < num_co; i++)
        total_pop += vcount[i];

    EXPECT_EQ(total_push, total_pop);
}

template<typename Q>
coke::Task<> test_batch(int num_co, int op_per_co,
                        std::size_t batch_size, std::size_t max_qsize)
{
    std::vector<coke::Task<>> push_tasks;
    std::vector<coke::Future<void>> pop_futs;
    std::vector<uint64_t> vcount(num_co, 0);

    Q que(max_qsize);

    push_tasks.reserve(num_co);
    pop_futs.reserve(num_co);

    for (int i = 0; i < num_co; i++)
        push_tasks.emplace_back(batch_push(que, batch_size, op_per_co));

    for (int i = 0; i < num_co; i++) {
        pop_futs.emplace_back(
            coke::create_future(single_pop(que, batch_size, vcount[i]))
        );
    }

    co_await coke::async_wait(std::move(push_tasks));
    que.close();

    for (auto &fut : pop_futs) {
        co_await fut.wait();
        fut.get();
    }

    uint64_t total_push = (uint64_t)num_co * op_per_co * batch_size;
    uint64_t total_pop = 0;
    for (int i = 0; i < num_co; i++)
        total_pop += vcount[i];

    EXPECT_EQ(total_push, total_pop);
}

template<typename Q>
void test_order(std::vector<int> in, std::vector<int> out) {
    Q que(in.size());
    bool ret;

    for (auto &i : in) {
        ret = que.try_emplace(i);
        EXPECT_EQ(ret, true);
    }

    int val;
    for (std::size_t i = 0; i < out.size(); i++) {
        ret = que.try_pop(val);

        EXPECT_EQ(ret, true);
        EXPECT_EQ(val, out[i]);
    }
}

/// Tests.

TEST(QUEUE, queue_single) {
    using Queue = coke::Queue<std::string>;
    coke::sync_wait(test_single<Queue>(20, 200, (uint64_t)15));
}

TEST(QUEUE, stack_single) {
    using Stack = coke::Stack<std::string>;
    coke::sync_wait(test_single<Stack>(20, 200, (uint64_t)15));
}

TEST(QUEUE, priority_queue_single) {
    using PriorityQueue = coke::PriorityQueue<std::string>;
    coke::sync_wait(test_single<PriorityQueue>(20, 200, (uint64_t)15));
}

TEST(QUEUE, queue_batch) {
    using Queue = coke::Queue<std::string>;
    coke::sync_wait(test_batch<Queue>(10, 100, 10, (uint64_t)95));
}

TEST(QUEUE, stack_batch) {
    using Stack = coke::Stack<std::string>;
    coke::sync_wait(test_batch<Stack>(10, 100, 10, (uint64_t)95));
}

TEST(QUEUE, priority_queue_batch) {
    using PriorityQueue = coke::PriorityQueue<std::string>;
    coke::sync_wait(test_batch<PriorityQueue>(10, 100, 10, (uint64_t)95));
}

TEST(QUEUE, queue_order) {
    test_order<coke::Queue<int>>({1, 4, 7, 2, 5, 8}, {1, 4, 7, 2, 5, 8});
}

TEST(QUEUE, stack_order) {
    test_order<coke::Stack<int>>({1, 4, 7, 2, 5, 8}, {8, 5, 2, 7, 4, 1});
}

TEST(QUEUE, priority_queue_order) {
    test_order<coke::PriorityQueue<int>>({1, 4, 7, 2, 5, 8},
                                         {8, 7, 5, 4, 2, 1});
}

int main(int argc, char *argv[]) {
    coke::GlobalSettings s;
    s.poller_threads = 4;
    s.handler_threads = 8;
    coke::library_init(s);

    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
