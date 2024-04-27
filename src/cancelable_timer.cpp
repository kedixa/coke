#include <atomic>
#include <cstdint>
#include <mutex>
#include <map>
#include <list>
#include <new>

#include "coke/detail/constant.h"
#include "coke/detail/timer_task.h"
#include "workflow/WFTask.h" // WFT_STATE_XXX
#include "workflow/WFGlobal.h"

namespace coke::detail {

class CancelInterface;

class alignas(DESTRUCTIVE_ALIGN) CancelableTimerMap {
public:
    static CancelableTimerMap *get_instance(uint64_t uid) {
        static CancelableTimerMap ntm[CANCELABLE_MAP_SIZE];
        return ntm + uid % CANCELABLE_MAP_SIZE;
    }

public:
    using List = std::list<CancelInterface *>;
    using ListIter = List::iterator;
    using Map = std::map<uint64_t, List>;
    using MapIter = Map::iterator;

    CancelableTimerMap() = default;

    // It is bad behavior if there are still unfinished timers.
    ~CancelableTimerMap() = default;

    void add_task(uint64_t uid, CancelInterface *task, bool insert_head);
    std::size_t cancel(uint64_t uid, std::size_t max);
    void del_task_unlocked(MapIter miter, ListIter liter);

private:
    std::mutex mtx;
    Map timer_map;

    friend struct TimerMapLock;
};


struct TimerMapLock final : public std::lock_guard<std::mutex> {
    TimerMapLock(CancelableTimerMap *m) : std::lock_guard<std::mutex>(m->mtx)
    { }
};


class CancelInterface : public TimerTask {
public:
    using ListIter = CancelableTimerMap::ListIter;
    using MapIter = CancelableTimerMap::MapIter;

    static constexpr auto relaxed = std::memory_order_relaxed;
    static constexpr auto acquire = std::memory_order_acquire;
    static constexpr auto release = std::memory_order_release;
    static constexpr auto acq_rel = std::memory_order_acq_rel;

    CancelInterface(CommScheduler *scheduler, const NanoSec &nsec)
        : TimerTask(scheduler, nsec),
          timer_map(nullptr), ref(2), in_map(false), cancel_done(false)
    { }

    virtual ~CancelInterface() {
        if (in_map.load(acquire)) {
            TimerMapLock lk(timer_map);
            if (in_map.load(acquire))
                timer_map->del_task_unlocked(miter, liter);
        }
    }

    /**
     * Cancel the unfinished timer. This function must be called within the
     * timer map lock.
    */
    virtual int cancel_timer_in_map() = 0;

    virtual SubTask *done() override {
        SeriesWork *series = series_of(this);

        this->awaiter->done();

        if (cancel_done.load(acquire) == false)
            cancel_done.wait(false, acquire);

        this->dec_ref();

        return series->pop();
    }

    /**
     * Set the location where the timer is placed. It must be called within the
     * timer map lock and should be called immediately after construction.
    */
    void set_in_map(CancelableTimerMap *m, MapIter mit, ListIter lit) {
        this->timer_map = m;
        this->miter = mit;
        this->liter = lit;
        in_map.store(true, release);
    }

    void dec_ref() {
        if (ref.fetch_sub(1, acq_rel) == 1)
            delete this;
    }

protected:
    ListIter liter;
    MapIter miter;
    CancelableTimerMap *timer_map;

    std::atomic<int> ref;
    std::atomic<bool> in_map;
    std::atomic<bool> cancel_done;
};


class CancelableTimer final : public CancelInterface {
public:
    CancelableTimer(CommScheduler *scheduler, const NanoSec &nsec)
        : CancelInterface(scheduler, nsec),
          switched(false), canceled(false)
    { }

    virtual ~CancelableTimer() = default;

    /**
     * Cancel the unfinished timer. This function must be called within the
     * timer map lock.
     * CancelableTimer guarantees that if `cancel_timer_in_map` is called
     * before `handle` takes effect, the timer's state is `SLEEP_CANCELED`.
    */
    virtual int cancel_timer_in_map() override;

protected:
    virtual void dispatch() override;
    virtual void handle(int state, int error) override;

private:
    std::atomic<bool> switched;
    bool canceled;
};


class InfiniteTimer final : public CancelInterface {
public:
    InfiniteTimer(CommScheduler *scheduler)
        : CancelInterface(scheduler, std::chrono::seconds(1)),
          flag(false)
    { }

    ~InfiniteTimer() = default;

protected:
    virtual void dispatch() override {
        if (flag.exchange(true, acq_rel)) {
            if (this->scheduler->sleep(this) >= 0) {
                this->cancel();

                cancel_done.store(true, release);
                cancel_done.notify_one();
            }
            else {
                cancel_done.store(true, relaxed);
                this->handle(SS_STATE_ERROR, errno);
            }

            this->dec_ref();
        }
    }

    virtual void handle(int state, int error) override {
        if (state == WFT_STATE_SUCCESS) {
            state = WFT_STATE_SYS_ERROR;
            error = ECANCELED;
        }

        this->TimerTask::handle(state, error);
    }

    virtual int cancel_timer_in_map() override {
        // `in_map` can be modified in advance because InfiniteTimer can only be
        // canceled and no race conditions will occur.
        this->in_map.store(false, relaxed);
        this->dispatch();
        return 0;
    }

private:
    std::atomic<bool> flag;
};


void CancelableTimerMap::add_task(uint64_t uid,
                                  CancelInterface *task,
                                  bool insert_head) {
    TimerMapLock lk(this);

    MapIter miter = timer_map.lower_bound(uid);
    if (miter == timer_map.end() || miter->first != uid)
        miter = timer_map.insert(miter, {uid, List{}});

    List &lst = miter->second;
    ListIter liter;

    if (insert_head)
        liter = lst.insert(lst.begin(), task);
    else
        liter = lst.insert(lst.end(), task);

    task->set_in_map(this, miter, liter);
}


std::size_t CancelableTimerMap::cancel(uint64_t uid, std::size_t max) {
    TimerMapLock lk(this);

    MapIter miter = timer_map.find(uid);
    if (miter == timer_map.end())
        return 0;

    std::size_t cnt = 0;
    List &lst = miter->second;
    CancelInterface *timer;

    while (cnt < max && !lst.empty()) {
        timer = lst.front();
        lst.pop_front();
        ++cnt;

        timer->cancel_timer_in_map();
        // The timer may have been destroyed at this time
    }

    if (lst.empty())
        timer_map.erase(miter);

    return cnt;
}


void CancelableTimerMap::del_task_unlocked(MapIter miter, ListIter liter) {
    List &lst = miter->second;
    lst.erase(liter);

    if (lst.empty())
        timer_map.erase(miter);
}


void CancelableTimer::dispatch() {
    int ret = this->scheduler->sleep(this);

    if (ret < 0) {
        cancel_done.store(true, relaxed);
        this->handle(SS_STATE_ERROR, errno);
    }
    else {
        if (switched.exchange(true, acq_rel))
            this->TimerTask::cancel();

        cancel_done.store(true, release);
        cancel_done.notify_one();
    }

    this->dec_ref();
}


void CancelableTimer::handle(int state, int error) {
    if (in_map.load(acquire)) {
        TimerMapLock lk(timer_map);
        if (in_map.load(acquire)) {
            timer_map->del_task_unlocked(miter, liter);
            in_map.store(false, release);
        }
    }

    // when canceled before callback, it will always be canceled
    if (state == WFT_STATE_SUCCESS && canceled) {
        state = WFT_STATE_SYS_ERROR;
        error = ECANCELED;
    }

    this->TimerTask::handle(state, error);
}


int CancelableTimer::cancel_timer_in_map() {
    int ret = 0;
    canceled = true;

    // sync with dispatch
    if (switched.exchange(true, acq_rel)) {
        // this cancel is protected by `in_map`, so `cancel_done` is not needed
        cancel_done.store(true, relaxed);
        ret = this->TimerTask::cancel();
    }

    // sync with handle and destructor
    in_map.store(false, release);
    return ret;
}


TimerTask *create_timer(uint64_t id, const NanoSec &nsec, bool insert_head)
{
    auto *timer_map = CancelableTimerMap::get_instance(id);
    auto *task = new CancelableTimer(WFGlobal::get_scheduler(), nsec);
    timer_map->add_task(id, task, insert_head);
    return task;
}

TimerTask *create_infinite_timer(uint64_t id, bool insert_head) {
    auto *timer_map = CancelableTimerMap::get_instance(id);
    auto *task = new InfiniteTimer(WFGlobal::get_scheduler());
    timer_map->add_task(id, task, insert_head);
    return task;
}

} // namespace coke::detail

namespace coke {

std::size_t cancel_sleep_by_id(uint64_t id, std::size_t max) {
    auto *timer_map = detail::CancelableTimerMap::get_instance(id);
    return timer_map->cancel(id, max);
}

} // namespace coke
