使用下述功能需要包含头文件`coke/latch.h`。


## coke::Latch
`coke::Latch`提供了一种同步机制，允许协程等待直到内部计数器降到零，适用于需要在特定条件下协调多个协程的场景。

### 成员函数

- 构造函数/析构函数

    创建一个可以精确计数`n`次的`Latch`，其中`n >= 0`。该类型不可移动、不可复制。

    ```cpp
    explicit Latch(long n) noexcept;

    Latch(const Latch &) = delete;

    ~Latch();
    ```

- 尝试等待

    检查内部计数器是否已达到零。若已经到达则返回`true`，否则返回`false`。

    ```cpp
    bool try_wait() const;
    ```

- 等待计数器到达零

    等待直到内部计数器降到零。返回一个可等待的对象，调用者应立即使用`co_await`等待该对象完成。

    ```cpp
    LatchAwaiter wait();
    ```

- 等待计数器到达零或超时

    在指定的时间内等待计数器降到零。返回一个可等待的对象，调用者应立即使用`co_await`等待该对象完成，若等待超时，该对象返回`coke::LATCH_TIMEOUT`，若计数器到达零返回`coke::LATCH_SUCCESS`。

    ```cpp
    LatchAwaiter wait_for(NanoSec nsec);
    ```

- 计数并等待

    将内部计数器减少`n`，`n`应大于等于1，并等待计数器降到零。所有`arrive_and_wait`和`count_down`减少的计数总和，必须与构造该对象时期望的计数一致。返回一个可等待对象，调用者应立即使用`co_await`等待该对象完成。

    ```cpp
    LatchAwaiter arrive_and_wait(long n = 1);
    ```

- 减少计数

    将计数器减少`n`但不等待，其中`n >= 1`。

    ```cpp
    void count_down(long n = 1);
    ```

### 示例
```cpp
#include <iostream>
#include <chrono>

#include "coke/latch.h"
#include "coke/sleep.h"
#include "coke/wait.h"

using namespace std::chrono_literals;

coke::Task<> worker(std::chrono::milliseconds ms, coke::Latch &lt) {
    co_await coke::sleep(ms);

    // 每完成一项就减少一次计数
    lt.count_down();
}

coke::Task<> wait_all() {
    coke::Latch lt(3);

    // 启动多个任务
    coke::detach(worker(100ms, lt));
    coke::detach(worker(200ms, lt));
    coke::detach(worker(300ms, lt));

    // 等待所有任务完成
    while (true) {
        int ret = co_await lt.wait_for(160ms);
        if (ret == coke::LATCH_TIMEOUT) {
            std::cout << "wait timeout" << std::endl;
            continue;
        }

        if (ret == coke::LATCH_SUCCESS) {
            std::cout << "wait success" << std::endl;
        }
        else {
            std::cout << "wait error " << ret << std::endl;
        }
        break;
    }
}

coke::Task<> wait_together(std::chrono::milliseconds ms, coke::Latch &lt) {
    std::cout << "start" << std::endl;
    co_await coke::sleep(ms);
    std::cout << "after sleep" << std::endl;

    // 等待所有相关工作都完成后，再一同开始后续任务
    co_await lt.arrive_and_wait();
    std::cout << "after all done" << std::endl;
}

int main() {
    coke::sync_wait(wait_all());

    coke::Latch lt(3);
    coke::sync_wait(
        wait_together(100ms, lt),
        wait_together(200ms, lt),
        wait_together(300ms, lt));
    return 0;
}
```


## coke::SyncLatch
`coke::SyncLatch`是`coke::Latch`的同步版本。相比于`std::latch`，`coke::SyncLatch`可在协程中使用，当同步操作处于`handler`线程当中时，内部会再启动一个`handler`线程以避免该阻塞带来的影响。一般情况下不建议使用该类型。

### 成员函数

- 构造函数

    创建一个可以精确计数`n`次的`Latch`，其中`n >= 0`。该类型不可移动、不可复制。

    ```cpp
    explicit SyncLatch(long n);

    ~SyncLatch();
    ```

- 减少计数

    将`SyncLatch`的内部计数器减少`n`，其中`n >= 1`。

    ```cpp
    void count_down(long n = 1);
    ```

- 等待计数器到达零

    同步等待直到`SyncLatch`的内部计数器降到零。

    ```cpp
    void wait() const;
    ```
