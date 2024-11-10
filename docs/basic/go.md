使用下述功能需要包含头文件`coke/go.h`。


## 常量
在`Workflow`框架中，创建计算任务时需要指定一个队列名称，在`Coke`中创建计算任务时，使用下述常量作为默认的队列名称。`Coke`应保证该值一定以`coke:`为前缀，以免与用户定义的值发生冲突，用户不应认为该值一直不变，若需引用应使用常量名称。

```cpp
constexpr std::string_view GO_DEFAULT_QUEUE{"coke:go"};
```

## GoAwaiter
`coke::GoAwaiter`用于异步等待计算任务，其中模板参数`T`表示计算任务的返回值。

```cpp
template<typename T>
class GoAwaiter;
```

### 构造函数
- 指定计算函数的计算任务
    - queue: 计算任务队列
    - executor: 计算任务线程池
    - func: 可调用对象
    - args: 可调用对象的参数

    ```cpp
    template<typename FUNC, typename... ARGS>
        requires std::invocable<FUNC, ARGS...>
    GoAwaiter(ExecQueue *queue, Executor *executor, FUNC &&func, ARGS&&... args);
    ```

- 未指定计算函数的计算任务
    - queue: 计算任务队列
    - executor: 计算任务线程池

    该构造函数专用于`coke::switch_go_thread`，得益于协程的便捷性，计算任务不需要封装成任务，可以切换到计算线程并在当前上下文直接调用计算函数。

    ```cpp
    GoAwaiter(ExecQueue *queue, Executor *executor);
    ```


## 辅助函数

下述辅助函数用于创建`GoAwaiter<T>`对象。

```cpp
// 1. 指定计算队列和线程池
template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
auto go(ExecQueue *queue, Executor *executor, FUNC &&func, ARGS&&... args);

// 2. 指定计算队列名称，使用默认线程池
template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
auto go(const std::string &name, FUNC &&func, ARGS&&... args);

// 3. 使用默认队列和线程池
template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
auto go(FUNC &&func, ARGS&&... args);
```

下述辅助函数用于切换计算线程。

```cpp
// 1. 指定计算队列和线程池
auto switch_go_thread(ExecQueue *queue, Executor *executor);

// 2. 指定计算队列名称，使用默认线程池
auto switch_go_thread(const std::string &name);

// 3. 使用默认队列和线程池
auto switch_go_thread();
```

## 示例
### 基本用法
```cpp
#include <algorithm>
#include <vector>

#include "coke/go.h"
#include "coke/wait.h"

coke::Task<> go() {
    // 使用计算任务，需要写出完整的函数名称，有时还需要考虑参数是引用还是拷贝等问题
    std::vector<int> v1{1, 6, 4, 2, 5, 3};
    co_await coke::go(std::sort<std::vector<int>::iterator>, v1.begin(), v1.end());

    // 使用切换线程的方式，直接调用函数即可
    std::vector<int> v2{1, 6, 4, 2, 5, 3};
    co_await coke::switch_go_thread();
    std::sort(v2.begin(), v2.end());
}

int main() {
    coke::sync_wait(go());
    return 0;
}
```

### 使用自定义Executor
```cpp
#include <iostream>
#include <thread>

#include "coke/go.h"
#include "coke/wait.h"
#include "coke/global.h"
#include "workflow/WFGlobal.h"

coke::Task<> use_executor(Executor *executor) {
    // 主线程
    std::cout << std::this_thread::get_id() << std::endl;

    // 默认计算线程
    co_await coke::switch_go_thread();
    std::cout << std::this_thread::get_id() << std::endl;

    // 自定义计算线程
    auto *que = WFGlobal::get_exec_queue("queue_name");
    co_await coke::switch_go_thread(que, executor);
    std::cout << std::this_thread::get_id() << std::endl;
}

int main() {
    coke::GlobalSettings settings;
    settings.compute_threads = 1;
    coke::library_init(settings);

    Executor executor;
    int ret = executor.init(1);
    if (ret < 0) {
        std::cout << "executor init failed " << ret << std::endl;
        return 1;
    }

    coke::sync_wait(use_executor(&executor));

    executor.deinit();
    return 0;
}
```
