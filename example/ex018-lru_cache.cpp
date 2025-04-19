#include <chrono>
#include <iostream>
#include <mutex>
#include <string>

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

using StrCache = coke::LruCache<std::string, std::string>;

StrCache cache(5);
std::mutex print_mtx;

std::string current();

template<typename... Args>
void print(const Args &...args)
{
    std::lock_guard<std::mutex> lg(print_mtx);
    std::cout << current();
    (std::cout << ... << args) << std::endl;
}

coke::Task<void> get_or_create(const std::string &key)
{
    co_await coke::yield();

    auto [hdl, created] = cache.get_or_create(key);
    if (created) {
        // We created this data, and it's up to us to update and share it
        // for everyone to use.
        print("Handle created");

        // Simulate the process of updating data.
        co_await coke::sleep(0.2);

        print("Update value");

        hdl.emplace_value("world");
        hdl.notify_all();
    }
    else {
        if (hdl.waiting()) {
            // Someone is updating it, just wait
            print("Wait value");

            co_await hdl.wait();

            // check wait ret if use wait_for, may be wait timeout
        }

        if (hdl.success())
            print("Get value ", hdl.value());
        else if (hdl.failed())
            print("Value is failed");
    }
}

coke::Task<void> get(const std::string &key)
{
    co_await coke::yield();

    auto hdl = cache.get(key);
    if (hdl)
        print("Get value ", hdl.value());
    else
        print("No such key ", key);
}

int main()
{
    coke::sync_wait(get("hello"));

    coke::sync_wait(get_or_create("hello"), get_or_create("hello"),
                    get_or_create("hello"));

    coke::sync_wait(get("hello"));

    return 0;
}

std::string current()
{
    static auto start = std::chrono::steady_clock::now();
    auto now          = std::chrono::steady_clock::now();

    std::chrono::duration<double> d = now - start;
    return "[" + std::to_string(d.count()) + "s] ";
}
