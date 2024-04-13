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

    AwaiterBase *get_awaiter() const { return awaiter; }
    void set_awaiter(AwaiterBase *awaiter) { this->awaiter = awaiter; }

private:
    virtual SubTask *done() override;

protected:
    AwaiterBase *awaiter;
};

template<typename T>
class GoTask : public GoTaskBase {
public:
    GoTask(ExecQueue *queue, Executor *executor,
           std::function<T()> &&func)
        : GoTaskBase(queue, executor), func(std::move(func))
    { }

    void set_function(std::function<T()> func) { this->func = std::move(func); }

    T &get_result() { return result.value(); }

private:
    virtual void execute() override {
        if (func)
            result.emplace(func());
    }

private:
    std::function<T()> func;
    std::optional<T> result;
};

template<>
class GoTask<void> : public GoTaskBase {
public:
    GoTask(ExecQueue *queue, Executor *executor,
           std::function<void()> &&func)
        : GoTaskBase(queue, executor), func(std::move(func))
    { }

    void set_function(std::function<void()> func) { this->func = std::move(func); }

private:
    virtual void execute() override {
        if (func)
            func();
    }

private:
    std::function<void()> func;
};

} // namespace coke::detail

#endif // COKE_DETAIL_GO_TASK_H
