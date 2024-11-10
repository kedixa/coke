使用下述功能需要包含头文件`coke/series.h`。


## 基本概念
任务流`SeriesWork`是`Workflow`中用于组织任务的方式，`Coke`支持在协程和任务流之间自由切换，关于任务流的详细情况请参考`Workflow`相关文档。

`Coke`支持两种任务流到协程的切换方式。第一种是切换到正在运行的任务流上，参考`detach_on_series`，使用这种方式，要求任务流必须正在运行，且其中已经没有其他任务，协程被分离(`detach`)到任务流上后，当前上下文不应再操作该任务流。第二种方式是指定任务流创建器，创建新的任务流并在其上启动协程，参考`detach_on_new_series`。

默认的任务流创建器等价于`Workflow::create_series_work`。使用任务流创建器，用户可以使用自定义的`SeriesWork`的子类类型作为任务流，或者在创建任务流时设置自定义的参数。

从协程切换到任务流比较简单，在协程中获取当前正在运行的任务流，向其中加入下一个任务，然后立刻结束协程即可。仅当协程以分离模式(`detach`)启动时可以通过该方式进行切换，否则被加入的任务何时运行是不确定的。

## 类型别名
- 别名`SeriesCreater`指代一种函数指针，该函数接收`SubTask *`参数，并返回使用该任务创建的`SeriesWork *`。

    ```cpp
    using SeriesCreater = SeriesWork * (*)(SubTask *);
    ```


## 辅助函数

- 获取当前协程的任务流
    - 若当前协程已经绑定任务流，则返回该任务流；否则创建一个新的任务流，绑定到当前协程并返回该任务流

    ```cpp
    SeriesAwaiter current_series();
    ```

- 为当前未绑定任务流的协程创建任务流
    - 等价于`current_series()`，对齐`WFEmptyTask`语义

    ```cpp
    SeriesAwaiter empty();
    ```

- 等待`ParallelWork`完成
    - 在`Workflow`中，`ParallelWork`也是一种任务，该函数用于异步等待`ParallelWork`完成
    - 在`Coke`中，并行启动多个任务可以通过`coke::async_wait`等方式实现

    ```cpp
    ParallelAwaiter wait_parallel(ParallelWork *parallel);
    ```

- 设置默认的`SeriesWork`创建器，并返回旧的

    ```cpp
    SeriesCreater set_series_creater(SeriesCreater creater);
    ```

- 返回当前的`SeriesWork`创建器

    ```cpp
    SeriesCreater get_series_creater();
    ```

- 在一个正在运行的`SeriesWork`上启动协程

    ```cpp
    template<Cokeable T>
    void detach_on_series(Task<T> &&task, SeriesWork *series);
    ```

- 使用创建器创建新的`SeriesWork`，并在其之上启动协程
    - 这里的`creater`可以是有状态的函数对象，不约束其必须是函数指针

    ```cpp
    template<Cokeable T, typename SeriesCreaterType>
    void detach_on_new_series(Task<T> &&task, SeriesCreaterType &&creater);
    ```

- 以分离的方式启动协程，但不显式指定任务流
    - 该函数可通过包含头文件`coke/task.h`使用，不必包含`coke/series.h`
    - 使用该方式启动协程，任务流的创建会被推迟到首个可等待对象启动时
    - 使用该方式启动协程，不保证该协程以及其子协程一定运行在相同的任务流上，除非在该协程开始时立刻启动一个可等待对象以默认方式创建任务流，例如通过`co_await coke::empty();`等方式

    ```cpp
    template<Cokeable T>
    void detach(Task<T> &&task);
    ```


## 示例
### 协程和任务流切换

```cpp
#include <iostream>

#include "workflow/WFTaskFactory.h"
#include "workflow/WFFacilities.h"

#include "coke/series.h"
#include "coke/sleep.h"
#include "coke/wait.h"

coke::Task<> run_coke() {
    std::cout << "3. 协程启动" << std::endl;

    co_await coke::sleep(0.1);
    SeriesWork *series = co_await coke::current_series();
    std::cout << "5. 协程休眠完成 series:" << (void *)series << std::endl;

    WFGoTask *go = WFTaskFactory::create_go_task("", []{
        std::cout << "6. 计算任务" << std::endl;
    });

    series->push_back(go);

    // 向当前任务流加入新任务，并立刻结束协程，可将协程切换回任务流模式
    // 若当前协程不是以分离模式启动的（即由其他协程直接调用），后续任务的执行顺序可能不符合预期
}

int main() {
    WFFacilities::WaitGroup wg(1);
    auto series_callback = [&wg](const SeriesWork *) {
        std::cout << "7. 任务流完成" << std::endl;
        wg.done();
    };

    auto timer_callback = [](WFTimerTask *timer) {
        SeriesWork *series = series_of(timer);
        std::cout << "2. 休眠任务回调 series:" << (void *)series << std::endl;

        // 将任务流切换到协程，由于新协程以分离模式运行，不要将局部变量以引用的方式传递到协程
        coke::detach_on_series(run_coke(), series);

        // 切换一旦完成，不能在当前上下文继续操作任务流
        std::cout << "4. 任务流切换到协程完成，当前上下文不应再有其他操作" << std::endl;
    };

    WFTimerTask *timer = WFTaskFactory::create_timer_task(1, 0, timer_callback);
    SeriesWork *series = Workflow::create_series_work(timer, series_callback);

    std::cout << "1. 启动任务流" << std::endl;
    series->start();

    wg.wait();
    return 0;
}
```

### 自定义`SeriesWork`类型

```cpp
#include <iostream>
#include <any>
#include <latch>
#include <string>
#include <memory>

#include "workflow/Workflow.h"
#include "coke/sleep.h"
#include "coke/series.h"
#include "coke/wait.h"

class MySeries : public SeriesWork {
public:
    void set_any_data(std::any a) { data = std::move(a); }
    std::any get_any_data() { return std::move(data); }
    std::any &get_data_ref() { return data; }

protected:
    MySeries(SubTask *first) : SeriesWork(first, nullptr) { }
    friend SeriesWork *myseries_creater(SubTask *);

private:
    std::any data;
};

SeriesWork *myseries_creater(SubTask *first) {
    // 创建自定义的SeriesWork类型，可以在任务流创建器中设置自定义的值
    std::unique_ptr<MySeries> series(new MySeries(first));
    series->set_any_data(std::string("hello"));
    return series.release();
}

void show_any_data(std::any &a) {
    if (int *i = std::any_cast<int>(&a))
        std::cout << "int data " << *i << std::endl;
    else if (std::string *str = std::any_cast<std::string>(&a))
        std::cout << "string data " << *str << std::endl;
    else
        std::cout << "unknown data type " << a.type().name() << std::endl;
}

coke::Task<> sub_func() {
    SeriesWork *series = co_await coke::current_series();
    MySeries *my_series = static_cast<MySeries *>(series);

    std::any &data = my_series->get_data_ref();
    show_any_data(data);

    // 在sub_func中设置新的值，并在func中访问
    my_series->set_any_data(std::make_any<int>(123));
}

coke::Task<> func(std::latch &lt) {
    co_await sub_func();

    // 访问sub_func中设置的值
    SeriesWork *series = co_await coke::current_series();
    MySeries *my_series = static_cast<MySeries *>(series);

    std::any &data = my_series->get_data_ref();
    show_any_data(data);

    lt.count_down();
}

int main() {
    std::latch lt(1);

    coke::detach_on_new_series(func(lt), myseries_creater);

    // 协程是以分离模式启动的，需要等待其运行结束后，再退出进程
    lt.wait();
    return 0;
}
```
