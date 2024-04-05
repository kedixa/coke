#include <array>
#include <string>
#include <map>

#include <gtest/gtest.h>

#include "coke/coke.h"

struct Copy {
    Copy(const Copy &) = default;
    Copy(Copy &&) = delete;
};

struct Move {
    Move(const Move &) = delete;
    Move(Move &&) = default;
};

struct PrivateDestructor {
    PrivateDestructor() = default;
    PrivateDestructor(PrivateDestructor &&) = default;
private:
    ~PrivateDestructor() = default;
};

void func() { }

TEST(CONCEPT, cokeable) {
    EXPECT_TRUE(coke::Cokeable<int>);
    EXPECT_TRUE(coke::Cokeable<std::string>);
    EXPECT_TRUE((coke::Cokeable<std::map<std::string, bool>>));
    EXPECT_TRUE(coke::Cokeable<Copy>);
    EXPECT_TRUE(coke::Cokeable<Move>);
    EXPECT_TRUE(coke::Cokeable<std::nullptr_t>);
    EXPECT_TRUE(coke::Cokeable<void>);
    EXPECT_TRUE(coke::Cokeable<int *>);
    EXPECT_TRUE((coke::Cokeable<std::array<int, 10>>));

    EXPECT_FALSE(coke::Cokeable<int[]>);
    EXPECT_FALSE(coke::Cokeable<int&>);
    EXPECT_FALSE(coke::Cokeable<const int>);
    EXPECT_FALSE(coke::Cokeable<int &&>);
    EXPECT_FALSE(coke::Cokeable<void(&)()>);
    EXPECT_FALSE(coke::Cokeable<PrivateDestructor>);
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
