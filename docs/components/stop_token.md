使用下述功能需要包含头文件 `coke/stop_token.h`。


## coke::StopToken
`coke::StopToken`提供了一种机制，用于请求其他协程停止执行，并等待其执行完成。

### 成员函数

- 构造函数/析构函数

    创建一个`StopToken`，参数`cnt`表示当该对象恰好被`set_finished`这些次数后，会处于完成状态。该类型不可移动，不可复制。

    ```cpp
    explicit StopToken(std::size_t cnt = 1) noexcept;

    StopToken(const StopToken &) = delete;
    StopToken &operator= (const StopToken &) = delete;

    ~StopToken();
    ```

- 重置状态

    重置内部状态，以便为下一次使用做准备，在调用前确保前一个过程已正常结束。

    ```cpp
    void reset(std::size_t cnt) noexcept;
    ```

- 请求停止

    请求与此`StopToken`关联的协程停止运行。调用后，`stop_requested()`将返回 `true`，通过`wait_stop_for`等待的协程将被唤醒。

    ```cpp
    void request_stop();
    ```

- 检查是否已经请求停止

    ```cpp
    bool stop_requested() const noexcept;
    ```

- 设置一次完成事件

    将内部计数减少一次，当计数达到零时，唤醒等待在`wait_finish`和`wait_finish_for`的协程。

    ```cpp
    void set_finished();
    ```

- 检查内部计数是否已经达到零

    ```cpp
    bool finished() const;
    ```

- 等待完成

    等待直到内部计数达到零。协程返回`bool`类型，若等待成功返回`true`，失败返回`false`。

    ```cpp
    coke::Task<bool> wait_finish();
    ```

- 等待完成或超时

    等待直到内部计数达到零或超时。协程返回`bool`类型，若等待成功返回`true`，超时或失败返回`false`。

    ```cpp
    coke::Task<bool> wait_finish_for(coke::NanoSec nsec);
    ```

- 等待request_stop被调用或超时

    等待直到请求停止或超时。协程返回`bool`类型，若等待成功返回`true`，超时或失败返回`false`。

    ```cpp
    coke::Task<bool> wait_stop_for(coke::NanoSec nsec);
    ```

### 示例
```cpp
#include <iostream>
#include <chrono>

#include "coke/stop_token.h"
#include "coke/sleep.h"
#include "coke/wait.h"

using namespace std::chrono_literals;

coke::Task<> worker(coke::StopToken &st) {
    coke::StopToken::FinishGuard guard(&st);

    // 每次循环时检查是否被要求停止
    while (!st.stop_requested()) {
        bool need_stop = co_await st.wait_stop_for(100ms);
        if (need_stop)
            break;

        std::cout << "work once" << std::endl;
    }

    std::cout << "worker exit" << std::endl;
}

coke::Task<> stop_token() {
    coke::StopToken st;

    coke::detach(worker(st));

    co_await coke::sleep(410ms);

    // 要求worker协程停止
    std::cout << "request stop" << std::endl;
    st.request_stop();

    // 等待worker完全结束
    co_await st.wait_finish();
    std::cout << "wait finished" << std::endl;
}

int main() {
    coke::sync_wait(stop_token());
    return 0;
}
```


## coke::StopToken::FinishGuard
用于管理`StopToken`的完成状态，确保在超出作用域时调用`set_finished`。

### 成员函数

- 构造函数/析构函数

    使用`StopToken`对象的地址构造`FinishGuard`对象，当`FinishGuard`退出作用域时自动调用`StopToken::set_finished`。该对象不可移动、不可复制。

    ```cpp
    explicit FinishGuard(StopToken *ptr = nullptr) noexcept;

    FinishGuard(const FinishGuard &) = delete;
    FinishGuard &operator= (const FinishGuard &) = delete;

    ~FinishGuard();
    ```

- 重置内部状态

    使用新的`StopToken`地址重置内部状态，但不会对旧的`StopToken`调用`StopToken::set_finished`。

    ```cpp
    void reset(StopToken *ptr) noexcept;
    ```

- 释放内部状态

    等价于`reset(nullptr)`。

    ```cpp
    void release() noexcept;
    ```
