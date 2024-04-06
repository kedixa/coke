#ifndef COKE_DETAIL_GO_TASK_H
#define COKE_DETAIL_GO_TASK_H

#include <functional>
#include <optional>
#include <string>

#include "workflow/ExecRequest.h"
#include "coke/detail/awaiter_base.h"

namespace coke::detail {

ExecQueue *get_exec_queue(const std::string &name);
Executor *get_compute_executor();

class GoTaskBase : public ExecRequest {
public:
    GoTaskBase(ExecQueue *queue, Executor *executor)
        : ExecRequest(queue, executor)
    {
        this->state = -1; // WFT_STATE_UNDEFINED
        this->error = 0;
        this->awaiter = nullptr;
    }

    virtual ~GoTaskBase() = default;

    int get_state() const { return this->state; }
    int get_error() const { return this->error; }

private:
    virtual SubTask *done() override;

public:
    AwaiterBase *awaiter;
};

template<typename T>
class GoTask : public GoTaskBase {
public:
    GoTask(ExecQueue *queue, Executor *executor,
           std::function<T()> &&go)
        : GoTaskBase(queue, executor), go(std::move(go))
    { }

    std::optional<T> result;

private:
    virtual void execute() override {
        if (go)
            result.emplace(go());
    }

    std::function<T()> go;
};

template<>
class GoTask<void> : public GoTaskBase {
public:
    GoTask(ExecQueue *queue, Executor *executor,
           std::function<void()> &&go)
        : GoTaskBase(queue, executor), go(std::move(go))
    { }

private:
    virtual void execute() override {
        if (go)
            go();
    }

    std::function<void()> go;
};

} // namespace coke::detail

#endif // COKE_DETAIL_GO_TASK_H
