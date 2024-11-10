使用下述功能需要包含头文件`coke/basic_awaiter.h`。

`coke::BasicAwaiter<T>`是一个通用的可等待对象类型，对于用户自定义的`Workflow`任务类型，可通过继承`coke::BasicAwaiter<T>`为新任务类型创建可等待对象类型。

在`Coke`中，每种可等待对象都是可移动的，在继承时注意派生类也要支持移动构造和移动赋值。在下述示例中，为自定义任务设置回调函数时使用`this->get_info`获取一个`coke::AwaiterInfo<T>`对象，并在回调函数中使用该对象再次获取可等待对象，这是因为可等待对象可以被移动到其他实例当中去，所以此处不能简单地捕获`this`指针。

如果想要自定义一个任务类型，并仅在`Coke`当中使用，可以不使用设置回调函数的方式。读者可参考`coke/detail/timer_task.h`和`coke/sleep.h`，了解`coke::detail::TimerTask`和`coke::SleepAwaiter`的实现方式。


## 示例
```cpp
#include <iostream>
#include <thread>

#include "workflow/Workflow.h"
#include "coke/basic_awaiter.h"
#include "coke/wait.h"

// 实现一个自定义的任务类型，该任务产生一个整数结果
class MyTask : public SubTask {
public:
    void dispatch() override {
        std::thread([this]() {
            this->set_result(1);
            this->subtask_done();
        }).detach();
    }

    SubTask *done() override {
        SeriesWork *series = series_of(this);

        if (callback)
            callback(this);

        delete this;
        return series->pop();
    }

    void set_callback(std::function<void (MyTask *)> cb) {
        callback = std::move(cb);
    }

    void set_result(int x) { result = x; }
    int get_result() const { return result; }

protected:
    MyTask() = default;
    friend MyTask *create_my_task();

private:
    int result;
    std::function<void (MyTask *)> callback;
};

MyTask *create_my_task() {
    return new MyTask();
}

// 实现MyTask的可等待对象类型
class MyTaskAwaiter : public coke::BasicAwaiter<int> {
public:
    MyTaskAwaiter(MyTask *task) {
        // 为自定义任务类型设置回调函数
        task->set_callback([info = this->get_info()] (MyTask *t) {
            // 在回调函数中设置返回值，并通知任务执行结束
            auto *awaiter = info->get_awaiter<MyTaskAwaiter>();
            awaiter->emplace_result(t->get_result());
            awaiter->done();
        });

        // 将该任务设置到可等待对象当中
        set_task(task);
    }

    MyTaskAwaiter(MyTaskAwaiter &&) = default;
};

coke::Task<> my_awaiter() {
    int ret = co_await MyTaskAwaiter(create_my_task());
    std::cout << "The result is " << ret << std::endl;
}

int main() {
    coke::sync_wait(my_awaiter());
    return 0;
}
```
