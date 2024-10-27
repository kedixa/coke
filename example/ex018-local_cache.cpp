#include <chrono>
#include <functional>
#include <iostream>
#include <mutex>
#include <memory>
#include <syncstream>

#include "coke/mutex.h"
#include "coke/sleep.h"
#include "coke/wait.h"

/**
 * This example shows how to use coke::Mutex to protect asynchronous operations.
 *
 * A common scenario is DNS cache. When the local cache of a domain name does
 * not exist or expires, only one request is needed to update the DNS, and other
 * requests wait for the updated result and use it directly.
 *
 * The update strategies for different requirements vary greatly. To avoid being
 * verbose, only a simple scenario is shown here.
 */

class CacheHandle {
    using clock_type = std::chrono::steady_clock;
    using time_point_type = clock_type::time_point;

    static auto now() { return clock_type::now(); }

public:
    void set_value(const std::string &value, long expire_ms);
    bool expired() const { return now() > expire_at; }
    const std::string &get_value_ref() const { return value; }

private:
    std::string value;
    time_point_type expire_at;
};

class AsyncCache {
public:
    using handle_ptr_t = std::shared_ptr<CacheHandle>;
    using update_func_t = std::function<coke::Task<handle_ptr_t>()>;

    AsyncCache(update_func_t func) : func(func) { }
    ~AsyncCache() = default;

    handle_ptr_t try_get();
    coke::Task<handle_ptr_t> get_or_update();

private:
    coke::Mutex co_mtx;
    std::mutex mtx;
    handle_ptr_t ptr;
    update_func_t func;
};

std::string current();

coke::Task<AsyncCache::handle_ptr_t> AsyncCache::get_or_update() {
    // 1. Lock to make sure there is at most one worker updating cache.
    coke::UniqueLock<coke::Mutex> lock(co_mtx);
    co_await lock.lock();

    handle_ptr_t hdl = try_get();
    if (hdl) {
        // 5. Other workers will get the updated value directly.
        co_return hdl;
    }

    // 2. The first one who acquire the lock will execute the update process.
    // Assuming there is no exception.
    hdl = co_await func();

    {
        // 3. Update ptr so that other workers get the new value.
        std::lock_guard<std::mutex> lg(mtx);
        ptr = hdl;
    }

    // 4. Return new value and wake up other workers.
    co_return hdl;
}

coke::Task<AsyncCache::handle_ptr_t> updater() {
    static unsigned value = 0;

    std::osyncstream(std::cout) << current() << "Update value" << std::endl;

    auto ptr = std::make_shared<CacheHandle>();
    // Simulate time-consuming asynchronous update operations.
    co_await coke::sleep(1.0);
    ptr->set_value(std::to_string(value++), 1000);

    co_return ptr;
}

coke::Task<> use_cache(int id, AsyncCache &cache) {
    std::osyncstream os(std::cout);
    std::emit_on_flush(os);

    for (int i = 0; i < 5; i++) {
        // Try to check if there is already an unexpired value.
        auto ptr = cache.try_get();

        // If there is no value, update it.
        // If there is already a worker updating, wait for it to complete and
        // get the updated value.
        if (!ptr)
            ptr = co_await cache.get_or_update();

        // Work with value.
        os << current() << "Worker " << id
           << " use value " << ptr->get_value_ref() << std::endl;
        co_await coke::sleep(0.6);
    }
}

int main() {
    AsyncCache cache(updater);

    // Start two workers to use cache.
    coke::sync_wait(
        use_cache(1, cache),
        use_cache(2, cache)
    );

    return 0;
}

void CacheHandle::set_value(const std::string &value, long expire_ms) {
    if (expire_ms <= 0)
        expire_at = time_point_type::max();
    else
        expire_at = now() + std::chrono::milliseconds(expire_ms);

    this->value = value;
}

AsyncCache::handle_ptr_t AsyncCache::try_get() {
    std::lock_guard<std::mutex> lg(mtx);

    if (ptr && ptr->expired())
        ptr.reset();

    return ptr;
}

std::string current() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> d = now - start;
    return "[" + std::to_string(d.count()) + "s] ";
}
