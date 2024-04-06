#include "coke/go.h"

#include "workflow/WFGlobal.h"

namespace coke::detail {

ExecQueue *get_exec_queue(const std::string &name) {
    return WFGlobal::get_exec_queue(name);
}

Executor *get_compute_executor() {
    return WFGlobal::get_compute_executor();
}

SubTask *GoTaskBase::done() {
    SeriesWork *series = series_of(this);

    awaiter->done();

    delete this;
    return series->pop();
}

} // namespace coke::detail
