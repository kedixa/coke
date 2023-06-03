#ifndef COKE_DETAIL_AWAITER_BASE_H
#define COKE_DETAIL_AWAITER_BASE_H

#include <coroutine>
#include <optional>

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

            bool in_series = this->in_series;
            this->in_series = that.in_series;
            that.in_series = in_series;
        }

        return *this;
    }

    AwaiterBase(AwaiterBase &&that)
        : hdl(that.hdl), subtask(that.subtask), in_series(that.in_series)
    {
        that.hdl = nullptr;
        that.subtask = nullptr;
        that.in_series = false;
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

template<SimpleType T>
class BasicAwaiter;

template<SimpleType T>
struct AwaiterInfo {
    AwaiterInfo(AwaiterBase *ptr) : ptr(ptr) { }
    AwaiterInfo(const AwaiterBase &) = delete;

    // The caller make sure that type(*ptr) is Awaiter A
    template<typename A = BasicAwaiter<T>>
    A *get_awaiter() {
        return static_cast<A *>(ptr);
    }

    friend class BasicAwaiter<T>;

private:
    AwaiterBase *ptr = nullptr;
    std::optional<T> opt;
};

template<>
struct AwaiterInfo<void> {
    AwaiterInfo(AwaiterBase *ptr) : ptr(ptr) { }
    AwaiterInfo(const AwaiterBase &) = delete;

    // The caller make sure that type(*ptr) is Awaiter A
    template<typename A = BasicAwaiter<void>>
    A *get_awaiter() {
        return static_cast<A *>(ptr);
    }

    friend class BasicAwaiter<void>;

private:
    AwaiterBase *ptr = nullptr;
};

template<SimpleType T>
class BasicAwaiter : public AwaiterBase {
public:
    BasicAwaiter() : info(new AwaiterInfo<T>(this)) { }
    BasicAwaiter(const BasicAwaiter &) = delete;
    ~BasicAwaiter() { delete info; }

    BasicAwaiter(BasicAwaiter &&that)
        : AwaiterBase(std::move(that)), info(that.info)
    {
        that.info = nullptr;

        if (this->info)
            this->info->ptr = this;
    }

    BasicAwaiter &operator= (BasicAwaiter &&that) {
        if (this != &that) {
            this->AwaiterBase::operator=(std::move(that));

            auto *info = that.info;
            that.info = this->info;
            this->info = info;

            if (this->info)
                this->info->ptr = this;
            if (that.info)
                that.info->ptr = &that;
        }

        return *this;
    }

    AwaiterInfo<T> *get_info() const {
        return info;
    }

    template<typename... ARGS>
    void emplace_result(ARGS&&... args) {
        if constexpr (!std::is_same_v<T, void>)
            info->opt.emplace(std::forward<ARGS>(args)...);
    }

    T await_resume() {
        if constexpr (!std::is_same_v<T, void>)
            return std::move(info->opt.value());
    }

protected:
    AwaiterInfo<T> *info;
};

} // namespace coke

#endif // COKE_DETAIL_AWAITER_BASE_H
