#include <atomic>
#include <cstdint>
#include <mutex>
#include <map>
#include <list>

#include "coke/sleep.h"
#include "workflow/WFTaskFactory.h"

namespace coke::detail {

constexpr uint64_t CANCELABLE_MAP_MAX = 128;
class CancelableTimer;

class CancelableTimerMap {
public:
    static CancelableTimerMap *get_instance(uint64_t uid) {
        static CancelableTimerMap ntm[CANCELABLE_MAP_MAX];
        return ntm + uid % CANCELABLE_MAP_MAX;
    }

public:
    using List = std::list<CancelableTimer *>;
    using ListIter = List::iterator;
    using Map = std::map<uint64_t, List>;
    using MapIter = Map::iterator;

    void add_task(uint64_t uid, CancelableTimer *task, bool insert_head);
    std::size_t cancel(uint64_t uid, std::size_t max);
    void del_task_unlocked(MapIter miter, ListIter liter);

private:
    std::mutex mtx;
    Map timer_map;

    friend struct TimerMapLock;
};

struct TimerMapLock final : public std::lock_guard<std::mutex> {
    TimerMapLock(CancelableTimerMap *m) : std::lock_guard<std::mutex>(m->mtx) { }
};

class CancelableTimer : public WFTimerTask {
    using ListIter = CancelableTimerMap::ListIter;
    using MapIter = CancelableTimerMap::MapIter;

    static constexpr auto acquire = std::memory_order_acquire;
    static constexpr auto release = std::memory_order_release;

protected:
    int duration(struct timespec *value) override {
        value->tv_sec = seconds;
        value->tv_nsec = nanoseconds;
        return 0;
    }

    virtual void dispatch() override;
    virtual void handle(int state, int error) override;

public:
    int cancel_timer_unlocked() {
        int ret;

        if (dispatched)
            ret = this->WFTimerTask::cancel();
        else {
            seconds = 0;
            nanoseconds = 0;
            ret = 0;
        }

        this->in_map.clear(release);
        return ret;
    }

    // called in timer lock
    void set_in_map(CancelableTimerMap *timer_map, MapIter miter, ListIter liter) {
        this->timer_map = timer_map;
        this->miter = miter;
        this->liter = liter;
        this->in_map.test_and_set(release);
    }

public:
    CancelableTimer(time_t seconds, long nanoseconds,
                    CommScheduler *scheduler, timer_callback_t&& cb)
        : WFTimerTask(scheduler, std::move(cb)), dispatched(false),
          seconds(seconds), nanoseconds(nanoseconds),
          timer_map(nullptr)
    { }

    virtual ~CancelableTimer() {
        if (in_map.test(acquire)) {
            TimerMapLock lk(timer_map);
            if (in_map.test(acquire))
                timer_map->del_task_unlocked(miter, liter);
        }
    }

private:
    bool dispatched;
    std::atomic_flag in_map;

    time_t seconds;
    long nanoseconds;
    ListIter liter;
    MapIter miter;
    CancelableTimerMap *timer_map;
};

void CancelableTimerMap::add_task(uint64_t uid, CancelableTimer *task, bool insert_head) {
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
    CancelableTimer *timer;

    while (cnt < max && !lst.empty()) {
        timer = lst.front();
        lst.pop_front();
        ++cnt;

        timer->cancel_timer_unlocked();
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
    int ret;

    {
        TimerMapLock lk(timer_map);
        dispatched = true;
        ret = this->scheduler->sleep(this);
    }

    if (ret < 0)
        this->handle(SS_STATE_ERROR, errno);
}

void CancelableTimer::handle(int state, int error) {
    bool canceled = !in_map.test(acquire);

    if (state == WFT_STATE_SUCCESS && canceled) {
        state = WFT_STATE_SYS_ERROR;
        error = ECANCELED;
    }

    if (!canceled) {
        TimerMapLock lk(timer_map);
        if (in_map.test(acquire)) {
            timer_map->del_task_unlocked(miter, liter);
            in_map.clear(release);
        }
    }

    this->WFTimerTask::handle(state, error);
}

} // namespace coke::detail

namespace coke {

WFTimerTask *create_cancelable_timer(uint64_t id, bool insert_head,
                                     time_t seconds, long nanoseconds,
                                     timer_callback_t callback)
{
    auto *timer_map = detail::CancelableTimerMap::get_instance(id);
    auto *task = new detail::CancelableTimer(seconds, nanoseconds, WFGlobal::get_scheduler(),
                                             std::move(callback));
    timer_map->add_task(id, task, insert_head);
    return task;
}

std::size_t cancel_sleep_by_id(uint64_t id, std::size_t max) {
    auto *timer_map = detail::CancelableTimerMap::get_instance(id);
    return timer_map->cancel(id, max);
}

} // namespace coke
