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

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>

#include "coke/coke.h"

constexpr size_t BUF_SIZE = 1024;
char buf[BUF_SIZE];
char data[BUF_SIZE];

coke::Task<> read_write(int fd) {
    EXPECT_GE(fd, 0);
    if (fd < 0)
        co_return;

    for (size_t i = 0; i < BUF_SIZE; i++)
        buf[i] = (i % 256);

    coke::FileResult w = co_await coke::pwrite(fd, buf, BUF_SIZE, 0);
    coke::FileResult r = co_await coke::pread(fd, data, BUF_SIZE, 0);

    EXPECT_EQ(w.state, coke::STATE_SUCCESS);
    EXPECT_EQ(r.state, coke::STATE_SUCCESS);

    EXPECT_EQ(w.nbytes, (long)BUF_SIZE);
    EXPECT_EQ(r.nbytes, (long)BUF_SIZE);

    int cmp = memcmp(buf, data, BUF_SIZE);
    EXPECT_EQ(cmp, 0);
}

TEST(FILEIO, read_write) {
    std::FILE *file = tmpfile();

    EXPECT_NE(file, nullptr);
    if (file) {
        coke::sync_wait(read_write(fileno(file)));

        std::fclose(file);
    }
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
