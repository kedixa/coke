#include <iostream>
#include <chrono>

#include "coke/coke.h"

/**
 * This example outputs "Hello World" to stdout `n` times,
 * and the interval between two outputs is `ms` milliseconds.
*/

coke::Task<> helloworld(size_t n, std::chrono::milliseconds ms) {
    for (size_t i = 0; i < n; i++) {
        if (i != 0)
            co_await coke::sleep(ms);

        std::cout << "Hello World" << std::endl;
    }
}

int main() {
    // Start a coroutine function, and sync wait until finish.
    coke::sync_wait(helloworld(3, std::chrono::milliseconds(500)));

    return 0;
}
