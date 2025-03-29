#include <chrono>
#include <iostream>
#include <string>
#include <syncstream>

#include "coke/lru_cache.h"
#include "coke/sleep.h"
#include "coke/wait.h"

/**
 * This example shows how to use coke::LruCache.
 *
 * The LruHandle returned by the LruCache supports asynchronous waiting. After
 * a new key is created using get_or_create, the value is updated by the creator
 * and other coroutines wait to be awakened.
 */

#define LOG std::osyncstream(std::cout)
using StrCache = coke::LruCache<std::string, std::string>;

StrCache cache(5);

std::string current() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> d = now - start;
    return "[" + std::to_string(d.count()) + "s] ";
}

coke::Task<void> get_or_create(const std::string &key) {
    co_await coke::yield();

    auto [hdl, created] = cache.get_or_create(key);
    if (created) {
        // We created this data, and it's up to us to update and share it
        // for everyone to use.
        LOG << current() << "Handle created\n";

        // Simulate the process of updating data.
        co_await coke::sleep(0.2);

        LOG << current() << "Update value\n";

        hdl.emplace_value("world");
        hdl.notify_all();
    }
    else {
        if (hdl.waiting()) {
            // Someone is updating it, just wait
            LOG << current() << "Wait value\n";

            co_await hdl.wait();

            // check wait ret if use wait_for, may be wait timeout
        }

        if (hdl.success())
            LOG << current() << "Get value " << hdl.value() << "\n";
        else if (hdl.failed())
            LOG << current() << "Value is faield\n";
    }
}

coke::Task<void> get(const std::string &key) {
    co_await coke::yield();

    auto hdl = cache.get(key);
    if (hdl) {
        const std::string &value = hdl.value();
        LOG << current() << "Get value " << value << "\n";
    }
    else {
        LOG << current() << "No such key " << key << "\n";
    }
}

int main() {
    coke::sync_wait(get("hello"));

    coke::sync_wait(
        get_or_create("hello"),
        get_or_create("hello"),
        get_or_create("hello")
    );

    coke::sync_wait(get("hello"));

    return 0;
}
