#include "coke/go.h"

#include "workflow/WFTask.h"
#include "workflow/WFTaskFactory.h"

namespace coke {

namespace detail {

void *create_go_task(const std::string &queue_name, std::function<void()> &&func) {
    return WFTaskFactory::create_go_task(queue_name, std::move(func));
}

} // namespace detail

} // namespace coke
