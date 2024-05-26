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

#include <functional>
#include <atomic>
#include <gtest/gtest.h>

#include "coke/coke.h"
#include "workflow/Workflow.h"
#include "workflow/WFTaskFactory.h"

coke::Task<> generic() {
    std::atomic<int> count = 0;
    auto func = [&count]() {
        ++count;
    };

    ParallelWork *p = Workflow::create_parallel_work(nullptr);
    for (int i = 0; i < 10; i++) {
        auto *t = WFTaskFactory::create_go_task("", func);
        auto *s = Workflow::create_series_work(t, nullptr);
        p->add_series(s);
    }

    coke::GenericAwaiter<int> g;
    p->set_callback([&g](const ParallelWork *) {
        g.set_result(1234);
        g.done();
    });

    g.take_over(p);

    int ret = co_await g;
    EXPECT_EQ(ret, 1234);
    EXPECT_EQ(count.load(), 10);
}

int main(int argc, char *argv[]) {
    coke::GlobalSettings s;
    s.poller_threads = 2;
    s.handler_threads = 2;
    s.compute_threads = 2;
    coke::library_init(s);

    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
