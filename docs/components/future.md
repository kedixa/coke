使用下述功能需要包含头文件`coke/future.h`。


## 常量
下述`int`类型常量与`coke::Future`的状态有关，位于`coke`命名空间。

```cpp
// 结果已经被设置
constexpr int FUTURE_STATE_READY        = 0;
// 等待结果超时
constexpr int FUTURE_STATE_TIMEOUT      = 1;
// 等待结果时进程退出
constexpr int FUTURE_STATE_ABORTED      = 2;
// 关联的coke::Promise已经析构，但未设置结果或异常
constexpr int FUTURE_STATE_BROKEN       = 3;
// 异常已经被设置
constexpr int FUTURE_STATE_EXCEPTION    = 4;
// coke::Promise尚未通知任何状态
constexpr int FUTURE_STATE_NOTSET       = 5;
```


## coke::Future
`coke::Future`提供等待和获取异步操作结果的机制。每个`coke::Promise`实例与唯一一个`coke::Future`关联，其中`coke::Promise`用于设置结果或异常，`coke::Future`用于等待结果或异常。

```cpp
template<Cokeable Res>
class Future;
```

### 成员函数
- 构造函数/析构函数

    可默认构造、可移动构造，不可复制构造。默认构造的对象不与任何`coke::Promise`关联，通常应从构造好的`coke::Promise`对象获取`coke::Future`实例。

    ```cpp
    Future();

    Future(Future &&);

    Future(const Future &) = delete;

    ~Future();
    ```

- 检查状态是否有效

    返回当前`Future`的状态是否有效。默认构造的`Future`无效，通过`Promise`获取的`Future`有效。

    ```cpp
    bool valid() const;
    ```

- 获取`Future`的内部状态

    获取`Future`的当前状态，可参考常量`coke::FUTURE_STATE_XXX`。

    ```cpp
    int get_state() const;
    ```

- 检查是否已经设置了结果

    前提是`valid()`返回`true`。

    ```cpp
    bool ready() const;
    ```

- 检查是否未设置结果或异常但`Promise`被销毁

    前提是`valid()`返回`true`。

    ```cpp
    bool broken() const;
    ```

- 检查是否设置了异常

    前提是`valid()`返回`true`。

    ```cpp
    bool has_exception() const;
    ```

- 阻塞直到`Future`就绪

    协程返回值参考`coke::FUTURE_STATE_XXX`常量。

    ```cpp
    coke::Task<int> wait();
    ```

- 阻塞直到`Future`就绪或超时

    协程返回值参考`coke::FUTURE_STATE_XXX`常量。

    ```cpp
    coke::Task<int> wait_for(const coke::NanoSec &nsec);
    ```

- 获取由`Promise`设置的值

    前提是`ready()`返回`true`。如果有异常，将重新抛出。该函数最多只能调用一次。

    ```cpp
    Res get();
    ```

- 获取关联的异常

    前提是`has_exception()`返回`true`。

    ```cpp
    std::exception_ptr get_exception();
    ```

- 设置回调函数

    设置一个回调函数，当`Future`状态不再是`FUTURE_STATE_NOTSET`时调用该函数，若已不是则立即调用。该函数可用于帮助实现异步等待多个`Future`完成，可参考下文的辅助函数部分。

    ```cpp
    void set_callback(std::function<void(int)> callback);
    ```

- 移除回调函数

    移除之前设置的回调函数。

    ```cpp
    void remove_callback();
    ```


## coke::Promise
`Promise`用于设置异步操作的结果，与唯一一个`Future`关联。

```cpp
template<Cokeable Res>
class Promise;
```

### 成员函数
- 构造函数/析构函数

    可默认构造，可移动构造，不可复制构造。析构函数会在`Promise`未设置值的情况下标记为`FUTURE_STATE_BROKEN`状态。

    ```cpp
    Promise();

    Promise(Promise &&) = default;

    Promise(const Promise &) = delete;

    ~Promise();
    ```

- 获取与`Promise`关联的`Future`

    获取与该`Promise`关联的`Future`，该函数最多只能调用一次。

    ```cpp
    coke::Future<Res> get_future();
    ```

- 设置值并唤醒`Future`

    设置值并唤醒与之关联的`Future`，每个`Promise`仅可修改一次内部状态，如果状态已经设置，将返回`false`。

    ```cpp
    template<typename U> requires std::is_same_v<Res, std::remove_cvref_t<U>>
    bool set_value(U &&value);
    ```

- 设置值的特化版本（`Res`为`void`）

    设置值并唤醒与之关联的`Future`，每个`Promise`仅可修改一次内部状态，如果状态已经设置，将返回`false`。

    ```cpp
    bool set_value() requires std::is_same_v<Res, void>;
    ```

- 设置异常并唤醒`Future`

    设置异常并唤醒与之关联的`Future`，每个`Promise`仅可修改一次内部状态，如果状态已经设置，将返回`false`。

    ```cpp
    bool set_exception(const std::exception_ptr &eptr);
    ```

### 示例
```cpp
#include <iostream>
#include <chrono>
#include <string>

#include "coke/future.h"
#include "coke/sleep.h"
#include "coke/wait.h"

using namespace std::chrono_literals;

constexpr int SET_VALUE = 0;
constexpr int SET_EXCEPTION = 1;
constexpr int SET_NOTHING = 2;

coke::Task<> set_promise(coke::Promise<std::string> p, int s) {
    try {
        co_await coke::sleep(100ms);
        if (s == SET_EXCEPTION)
            throw 1;
    }
    catch(...) {
        p.set_exception(std::current_exception());
    }

    if (s == SET_VALUE)
        p.set_value(std::string{"hello"});
}

coke::Task<> wait_future(int s) {
    coke::Promise<std::string> p;
    coke::Future<std::string> f = p.get_future();

    // 以分离模式运行协程，保证会在未来某个时刻为p设置值
    coke::detach(set_promise(std::move(p), s));

    // 当前协程不会阻塞，可继续其他操作，或尝试等待结果
    while (true) {
        int ret = co_await f.wait_for(30ms);
        if (ret == coke::FUTURE_STATE_TIMEOUT) {
            std::cout << "wait timeout" << std::endl;
            continue;
        }

        if (ret == coke::FUTURE_STATE_READY) {
            std::cout << "wait success " << f.get() << std::endl;
        }
        else if (ret == coke::FUTURE_STATE_EXCEPTION) {
            std::cout << "wait exception" << std::endl;
        }
        else if (ret == coke::FUTURE_STATE_BROKEN) {
            std::cout << "wait broken" << std::endl;
        }
        else {
            std::cout << "wait unknown error " << ret << std::endl;
        }
        break;
    }
}

int main() {
    coke::sync_wait(wait_future(SET_VALUE));
    coke::sync_wait(wait_future(SET_EXCEPTION));
    coke::sync_wait(wait_future(SET_NOTHING));
    return 0;
}
```


## 辅助函数

- 创建一个`Future`并立即启动任务

    从`coke::Task<T>`创建一个`coke::Future<T>`，任务将在正在运行的任务流`series`上立即启动。

    ```cpp
    template<Cokeable T>
    coke::Future<T> create_future(Task<T> &&task, SeriesWork *series);
    ```

- 创建一个`Future`并立即启动任务（自定义任务流创建器）

    从`coke::Task<T>`创建一个`coke::Future<T>`，任务将在新创建的任务流中立即启动。

    ```cpp
    template<Cokeable T, typename SeriesCreaterType>
    coke::Future<T> create_future(coke::Task<T> &&task, SeriesCreaterType &&creater);
    ```

- 创建一个`Future`并立即启动任务（默认创建器）

    从`coke::Task<T>`创建一个`coke::Future<T>`，任务将在默认的任务流创建器中立即启动。

    ```cpp
    template<Cokeable T>
    coke::Future<T> create_future(coke::Task<T> &&task);
    ```

- 等待至少`n`个`Future`完成

    注意参数使用引用传递，调用者应保证该协程结束前参数不会被修改或析构。

    ```cpp
    template<Cokeable T>
    coke::Task<void> wait_futures(std::vector<coke::Future<T>> &futs, std::size_t n);
    ```

- 等待至少`n`个`Future`完成或超时

    注意参数使用引用传递，调用者应保证该协程结束前参数不会被修改或析构。协程返回`int`类型整数，若等待成功则为`coke::TOP_SUCCESS`，若超时则为`coke::TOP_TIMEOUT`。

    ```cpp
    template<Cokeable T>
    coke::Task<int> wait_futures_for(std::vector<coke::Future<T>> &futs, std::size_t n,
                                     coke::NanoSec nsec);
    ```
