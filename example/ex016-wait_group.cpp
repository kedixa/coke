#include <chrono>
#include <iostream>
#include <string>
#include <syncstream>

#include "coke/sleep.h"
#include "coke/wait_group.h"
#include "coke/wait.h"

std::string current();

coke::Task<> subworker(coke::WaitGroup &wg, int i) {
    std::osyncstream os(std::cout);
    auto ms = std::chrono::milliseconds(i * 100);

    co_await coke::sleep(ms);

    os << current() << "SubWorker " << i << " done" << std::endl;
    os.emit();
    wg.done();
}

coke::Task<> worker(coke::WaitGroup &wg, int i) {
    std::osyncstream os(std::cout);
    auto ms = std::chrono::milliseconds(i * 100);

    std::emit_on_flush(os);

    co_await coke::sleep(ms);

    if (i % 2 == 0) {
        // The worker can add count to wg and then detach subworkers,
        // but just before wg.done is called.
        wg.add(1);

        os << current() << "Detach subworker " << i << std::endl;
        coke::detach(subworker(wg, i));
    }

    os << current() << "Worker " << i << " done" << std::endl;
    wg.done();

    // WaitGroup::done usually should be called before the coroutine ends.
    // After done is called, wg maybe destroyed, DO NOT use it any more.
}

coke::Task<> async_main(int nworkers) {
    std::osyncstream os(std::cout);
    coke::WaitGroup wg;

    std::emit_on_flush(os);

    for (int i = 0; i < nworkers; i++) {
        wg.add(1);

        os << current() << "Detach worker " << i << std::endl;
        coke::detach(worker(wg, i));
    }

    co_await wg.wait();
    os << current() << "Wait done" << std::endl;
}

int main() {
    coke::sync_wait(async_main(6));
    return 0;
}

std::string current() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> d = now - start;
    return "[" + std::to_string(d.count()) + "s] ";
}
