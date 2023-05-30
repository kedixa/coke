#ifndef COKE_DETAIL_AWAITER_BASE_H
#define COKE_DETAIL_AWAITER_BASE_H

#include <coroutine>

#include "coke/detail/basic_concept.h"

#include "workflow/SubTask.h"

namespace coke {

/**
 * Common base class for awaitable objects in this project.
*/
class AwaiterBase {
    static void *create_series(SubTask *first);

public:
    AwaiterBase() = default;
    AwaiterBase &operator= (const AwaiterBase &) = delete;
    virtual ~AwaiterBase();

    AwaiterBase &operator=(AwaiterBase &&that) {
        if (this != &that) {
            auto hdl = this->hdl;
            this->hdl = that.hdl;
            that.hdl = hdl;

            auto subtask = this->subtask;
            this->subtask = that.subtask;
            that.subtask = subtask;
        }

        return *this;
    }

    AwaiterBase(AwaiterBase &&that)
        : hdl(that.hdl), subtask(that.subtask)
    {
        that.hdl = nullptr;
        that.subtask = nullptr;
    }

    bool await_ready() { return subtask == nullptr; }

    template<PromiseType T>
    void await_suspend(std::coroutine_handle<T> h) {
        this->hdl = h;

        void *series = h.promise().get_series();
        if (series)
            suspend(series);
        else {
            series = create_series(subtask);
            h.promise().set_series(series);
            suspend(series, true);
        }

        // `*this` maybe destroyed after suspend
    }

protected:
    /**
     * `suspend` will be called in `await_suspend`.
     * series: the coroutine is running on this series now.
     * is_new: this is the first awaiter in this coroutine,
     *         this->subtask is the first task of series,
     *         series is not running now.
    */
    virtual void suspend(void *series, bool is_new = false);

    /**
     * This function will be called when this->subtask finishes.
     * Since we are in the callback function of subtask at this time,
     * we temporarily prohibit this function from throwing an exception.
    */
    virtual void done() noexcept {
        subtask = nullptr;
        hdl.resume();
    }

    /**
     * `set_task` can be called at most once, if not, this awaiter
     * will return true in `await_ready` and wait nothing.
     * 
     * task:    A SubTask which will call this->done in it's callback.
     * in_series: Whether task is already in series, for example
     *          reply_task is in server's series.
    */
    void set_task(SubTask *subtask, bool in_series = false) noexcept {
        this->subtask = subtask;
        this->in_series = in_series;
    }

protected:
    std::coroutine_handle<> hdl;
    SubTask *subtask{nullptr};
    bool in_series{false};
};

} // namespace coke

#endif // COKE_DETAIL_AWAITER_BASE_H
