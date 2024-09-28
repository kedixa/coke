#include <iostream>
#include <chrono>
#include <vector>
#include <string>

#include "coke/sleep.h"
#include "coke/future.h"
#include "coke/wait.h"

/**
 * This example shows how to use coke::Future to implement backup request.
 *
 * In network requests, due to factors such as network jitter or server
 * busyness, a small number of requests will take longer than we can wait,
 * also known as the long tail. We can send a request to the backup server in
 * the hope of completing the task as soon as possible.
 */

using std::chrono::milliseconds;
using Request = std::chrono::milliseconds;
using Response = int;

constexpr std::chrono::milliseconds first_timeout(50);
constexpr std::chrono::milliseconds backup_timeout(40);

std::string current();

coke::Task<Response> request_once(Request req) {
    // Simulate time-consuming network request
    int ret = co_await coke::sleep(req);
    co_return ret;
}

coke::Task<Response> handle_request(milliseconds t1, milliseconds t2) {
    std::cout << "------------------------------\n";
    std::cout << current() << "Handle request\n";

    std::vector<coke::Future<Response>> vec;
    int ret;

    vec.emplace_back(coke::create_future(request_once(t1)));
    ret = co_await vec[0].wait_for(first_timeout);

    // For simplicity, we assume that future has only two states: Ready and Timeout.
    if (ret == coke::FUTURE_STATE_READY) {
        std::cout << current() << "First request success\n";
        co_return vec[0].get();
    }

    std::cout << current() << "First request timeout, try backup request\n";
    vec.emplace_back(coke::create_future(request_once(t2)));

    ret = co_await coke::wait_futures_for(vec, 1, backup_timeout);

    // Note that wait_futures_for returns coke::TOP_*, not FUTURE_STATE_*
    if (ret == coke::TOP_TIMEOUT) {
        std::cout << current() << "Backup request timeout\n";
        co_return -1;
    }
    else {
        if (vec[0].ready()) {
            std::cout << current() << "First request success before backup request\n";
            co_return vec[0].get();
        }
        else {
            std::cout << current() << "Backup request success before first request\n";
            co_return vec[1].get();
        }
    }

    // Note that there may still has request(s) running in the background,
    // whose coke::Future is not ready now.
}

int main() {
    // Case 1: First request success
    coke::sync_wait(handle_request(milliseconds(40), milliseconds(20)));

    // Case 2: First request success before backup request
    coke::sync_wait(handle_request(milliseconds(60), milliseconds(20)));

    // Case 3: Backup request success before first request
    coke::sync_wait(handle_request(milliseconds(80), milliseconds(20)));

    // Note: the first request of case 3 is still running now.
    // We need to wait all tasks to complete before process exit.
    // For simplicity, it is ignored here.
    return 0;
}

std::string current() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> d = now - start;
    return "[" + std::to_string(d.count()) + "s] ";
}
