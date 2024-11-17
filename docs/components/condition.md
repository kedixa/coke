使用下述功能需要包含头文件`coke/condition.h`。


## coke::Condition
`coke::Condition`用于在协程中实现条件变量的机制，支持阻塞当前协程直到满足条件或被唤醒。

### 成员函数

- 构造函数/析构函数

    只可默认构造，不可复制构造，不可移动构造。

    ```cpp
    Condition();

    Condition(const Condition &) = delete;

    ~Condition();
    ```

- 等待被唤醒

    阻塞当前协程直到被唤醒。协程返回一个`int`类型的整数，`coke::TOP_SUCCESS`表示被成功唤醒，`coke::TOP_ABORTED`表示进程退出，负数表示发生系统错误。可参考`全局配置`章节的相关内容。

    ```cpp
    coke::Task<int> wait(std::unique_lock<std::mutex> &lock);
    ```

- 等待直到谓词返回`true`

    阻塞当前协程直到被唤醒且`pred()`返回`true`。协程返回一个`int`类型的整数，`coke::TOP_SUCCESS`表示被成功唤醒，`coke::TOP_ABORTED`表示进程退出，负数表示发生系统错误。可参考`全局配置`章节的相关内容。

    ```cpp
    coke::Task<int> wait(std::unique_lock<std::mutex> &lock,
                         std::function<bool()> pred);
    ```

- 等待直到超时

    阻塞当前协程直到被唤醒或等待超时。协程返回一个`int`类型的整数，`coke::TOP_SUCCESS`表示获取成功，`coke::TOP_TIMEOUT`表示因超时而获取失败，`coke::TOP_ABORTED`表示进程退出，负数表示发生系统错误。可参考`全局配置`章节的相关内容。

    ```cpp
    Task<int> wait_for(std::unique_lock<std::mutex> &lock, NanoSec nsec);
    ```

- 等待直到谓词返回`true`或超时

    阻塞当前协程直到被唤醒且`pred()`返回`true`或等待超时。协程返回一个`int`类型的整数，`coke::TOP_SUCCESS`表示获取成功，`coke::TOP_TIMEOUT`表示因超时而获取失败，`coke::TOP_ABORTED`表示进程退出，负数表示发生系统错误。可参考`全局配置`章节的相关内容。

    ```cpp
    coke::Task<int> wait_for(std::unique_lock<std::mutex> &lock, coke::NanoSec nsec,
                             std::function<bool()> pred);
    ```

- 唤醒一个等待的协程

    如果有协程在等待，唤醒其中一个。

    ```cpp
    void notify_one();
    ```

- 唤醒指定数量的等待协程

    如果有协程在等待，唤醒最多`n`个协程。

    ```cpp
    void notify(std::size_t n);
    ```

- 唤醒所有等待的协程

    如果有协程在等待，唤醒所有协程。

    ```cpp
    void notify_all();
    ```

### 示例

```cpp
#include <iostream>
#include <mutex>

#include "coke/condition.h"
#include "coke/sleep.h"
#include "coke/wait.h"

using namespace std::chrono_literals;

std::mutex mtx;
coke::Condition cond;
bool has_data = false;

coke::Task<> worker(int wid, std::chrono::milliseconds ms) {
    int ret;
    std::unique_lock lk(mtx);

    // 等待直到数据设置完毕，或者超时
    ret = co_await cond.wait_for(lk, ms, [&]() { return has_data; });

    // 在占有锁的情况下处理数据
    if (ret == coke::TOP_SUCCESS) {
        std::cout << wid << " wait success\n";
    }
    else if (ret == coke::TOP_TIMEOUT) {
        std::cout << wid << " wait timeout\n";
    }

    // 及时解锁，注意不可在锁定状态下进行异步操作
    lk.unlock();

    // 其他无需占有锁的过程
}

coke::Task<> prepare_data() {
    // 模拟耗时的异步操作
    co_await coke::sleep(50ms);

    // 将数据设置给共享变量并通知其他协程
    std::unique_lock lk(mtx);
    has_data = true;
    lk.unlock();

    cond.notify_all();
}

int main() {
    coke::sync_wait(prepare_data(), worker(1, 20ms), worker(2, 60ms));
    return 0;
}
```
