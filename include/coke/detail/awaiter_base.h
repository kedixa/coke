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

    /**
     * @brief This function is only for developers, we make it public just
     *        for convenience, and users should not called it directly.
     *        When this->subtask finishes, call this->done() to wake up the
     *        coroutine.
     *
     * @pre this->subtask is done.
    */
    virtual void done() {
        subtask = nullptr;
        hdl.resume();
    }

protected:
    /**
     * @brief The `suspend` will be called in `await_suspend`, child classes
     *        can override this function to do something else.
     *
     * @param series The coroutine is running on this series.
     *
     * @param is_new Whether this is the first awaiter in this coroutine. If
     *        `is_new` is true, this->subtask is the first task of the series,
     *        and the series is not started now.
    */
    virtual void suspend(void *series, bool is_new = false);

    /**
     * @brief Set `subtask` to this awaiter, `set_task` can be called at most
     *        once, if not, co_await this awaiter will do nothing.
     *
     * @param subtask An instance of SubTask's child class.
     *
     * @param in_series Whether `subtask` is already in current series, if true,
     *        it will not push back to series again, for example, reply_task is
     *        in the server's series.
     *        It is bad behavious if `in_series` is true but not in current
     *        coroutine's series.
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

template<Cokeable T>
class BasicAwaiter;

template<Cokeable T>
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

template<Cokeable T>
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
