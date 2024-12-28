#include <iostream>

#include "coke/tools/scope.h"
#include "coke/wait.h"
#include "coke/mutex.h"

coke::Mutex m;

coke::Task<> scope_example(bool release) {
    co_await m.lock();

    coke::ScopeExit scope([]() noexcept { m.unlock(); });

    std::cout << "A" << std::endl;
    co_await coke::sleep(1);
    std::cout << "B" << std::endl;

    if (release) {
        m.unlock();
        scope.release();
    }
}

int main() {
    coke::sync_wait(scope_example(true), scope_example(false));
    return 0;
}
