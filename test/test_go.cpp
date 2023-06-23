#include <vector>
#include <gtest/gtest.h>

#include "coke/coke.h"

void simple() { }
int add(int a, int b) { return a + b; }

template<typename T>
T ref(T &a, T b) { a = b; return b; }

TEST(GO, simple) {
    coke::sync_wait(
        coke::go(simple),
        coke::go("queue", simple)
    );
}

TEST(GO, add) {
    int ret = coke::sync_wait(coke::go(add, 1, 2));
    EXPECT_EQ(ret, 3);
}

TEST(GO, ref) {
    std::vector<int> a;
    std::vector<int> b{1, 2, 3, 4};
    coke::sync_wait(coke::go(ref<std::vector<int>>, std::ref(a), b));
    EXPECT_EQ(a, b);
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
