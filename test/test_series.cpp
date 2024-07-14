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

#include <map>
#include <string>
#include <gtest/gtest.h>

#include "coke/coke.h"

using CtxMap = std::map<std::string, std::string>;

class MySeries : public SeriesWork {
public:
    static SeriesWork *create(SubTask *first) {
        return new MySeries(first);
    }

    CtxMap &get_ctx() {
        return m;
    }

    MySeries(SubTask *first) : SeriesWork(first , nullptr) { }

private:
    CtxMap m;
};

std::string key("key"), value("value");

coke::Task<> worker() {
    co_await coke::sleep(0.1);

    SeriesWork *series = co_await coke::current_series();
    MySeries *my_series = dynamic_cast<MySeries *>(series);
    CtxMap &m = my_series->get_ctx();
    m[key] = value;

    co_await coke::sleep(0.1);
}

coke::Task<> my_series(coke::SyncLatch &lt) {
    co_await worker();

    SeriesWork *series = co_await coke::current_series();
    MySeries *my_series = dynamic_cast<MySeries *>(series);
    auto m = my_series->get_ctx();

    EXPECT_EQ(m[key], value);

    lt.count_down();
}

coke::Task<> default_series(coke::SyncLatch &lt) {
    SeriesWork *s1 = co_await coke::current_series();
    co_await coke::sleep(0.1);

    SeriesWork *s2 = co_await coke::current_series();

    EXPECT_EQ(s1, s2);

    lt.count_down();
}

TEST(SERIES, my) {
    coke::SyncLatch lt(1);
    coke::detach(default_series(lt));
    lt.wait();
}

TEST(SERIES, default) {
    coke::SyncLatch lt(1);
    coke::detach_on_new_series(my_series(lt), MySeries::create);
    lt.wait();
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
