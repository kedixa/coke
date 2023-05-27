#include <cstdio>
#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

#include "coke/coke.h"
#include "coke/fileio.h"

constexpr size_t BUF_SIZE = 1024;
char buf[BUF_SIZE];
char data[BUF_SIZE];

coke::Task<size_t> do_write(int fd) {
    coke::FileResult res;
    size_t pos = 0;

    while (pos < BUF_SIZE) {
        size_t nleft = BUF_SIZE - pos;
        res = co_await coke::pwrite(fd, buf + pos, nleft, pos);
        EXPECT_EQ(res.state, 0);

        if (res.state != 0 || res.ret == 0)
            break;

        pos += res.ret;
    }

    co_return pos;
}

coke::Task<size_t> do_read(int fd) {
    coke::FileResult res;
    size_t pos = 0;

    while (pos < BUF_SIZE) {
        size_t nleft = BUF_SIZE - pos;
        res = co_await coke::pread(fd, data + pos, nleft, pos);
        EXPECT_EQ(res.state, 0);

        if  (res.state != 0 || res.ret == 0)
            break;

        pos += res.ret;
    }

    co_return pos;
}

coke::Task<> read_write(int fd) {
    EXPECT_GE(fd, 0);
    if (fd < 0)
        co_return;

    for (size_t i = 0; i < BUF_SIZE; i++)
        buf[i] = (i % 256);

    size_t tot_write = co_await do_write(fd);
    size_t tot_read = co_await do_read(fd);
    EXPECT_EQ(tot_write, BUF_SIZE);
    EXPECT_EQ(tot_read, BUF_SIZE);

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
