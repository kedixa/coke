#include <iostream>

#include "workflow/WFFacilities.h"
#include "workflow/WFTaskFactory.h"

#include "coke/go.h"
#include "coke/series.h"
#include "coke/sleep.h"
#include "coke/task.h"

/**
 * This example shows how to switch freely between coke::Task and SeriesWork.
 *
 * Many users are good at using the callback mode of SeriesWork and WFXxxTask,
 * but want to try coke only in part of the process. This can be achieved in the
 * following way.
 */

void func()
{
    std::cout << "Go func running" << std::endl;
}

coke::Task<> do_something_with_coke(std::string str)
{
    std::cout << "Coke sleep 0.1s" << std::endl;
    co_await coke::sleep(0.1);

    std::cout << "Coke switch to go thread" << std::endl;
    co_await coke::switch_go_thread();

    std::cout << "Param str " << str << std::endl;

    SeriesWork *series = co_await coke::current_series();
    std::cout << "coke::Task running on series " << (void *)series << std::endl;

    // Switch back to SeriesWork at the end of coke::Task.
    WFGoTask *go = WFTaskFactory::create_go_task("name", func);
    series->push_front(go);

    // No more co_await operation, unless you know what's going to happen.
}

int main()
{
    WFFacilities::WaitGroup wg(1);
    auto series_callback = [&wg](const SeriesWork *) {
        std::cout << "Series callback" << std::endl;
        wg.done();
    };

    auto timer_callback = [](WFTimerTask *timer) {
        SeriesWork *series = series_of(timer);
        std::cout << "First timer running on series " << (void *)series
                  << std::endl;

        // Switch to coke::Task. Since detach is required, it is necessary to
        // ensure that there are no references in the parameters.
        coke::detach_on_series(do_something_with_coke("Hello world"), series);

        // The process has been transferred to coke::Task, and no other
        // operations are allowed after detach.
    };

    auto *timer = WFTaskFactory::create_timer_task(0, 100'000'000,
                                                   timer_callback);
    SeriesWork *series = Workflow::create_series_work(timer, series_callback);
    series->start();

    wg.wait();
    return 0;
}
