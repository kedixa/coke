使用下述功能需要包含头文件`coke/semaphore.h`。


## coke::Semaphore
信号量提供一种同步语义，用于约束对共享资源访问的并发程度。信号量支持两种操作，`acquire`用于获取访问许可，`release`用于释放访问许可，当并发程度小于指定值时，可以访问共享资源，当并发程度达到上限时，`acquire`操作会阻塞该操作，直到有其他协程释放该信号量。

### 成员函数
- 构造函数/析构函数

    指定初始计数`n`，至多允许`n`个协程同时获取到信号量。`Semaphore`不可复制构造、不可移动构造。

    ```cpp
    explicit Semaphore(uint32_t n);

    Semaphore(const Semaphore &) = delete;

    ~Semaphore();
    ```

- 尝试获取一个内部计数

    尝试获取但不阻塞，若获取成功则返回`true`，否则返回`false`。

    ```cpp
    bool try_acquire();
    ```

- 尝试获取一个内部计数，直到超时

    协程返回一个`int`类型的整数，`coke::TOP_SUCCESS`表示获取成功，`coke::TOP_TIMEOUT`表示因超时而获取失败，`coke::TOP_ABORTED`表示进程退出，负数表示发生系统错误。可参考`全局配置`章节的相关内容。

    ```cpp
    Task<int> try_acquire_for(NanoSec nsec);
    ```

- 获取一个内部计数

    协程返回一个`int`类型的整数，`coke::TOP_SUCCESS`表示获取成功，`coke::TOP_ABORTED`表示进程退出，负数表示发生系统错误。可参考`全局配置`章节的相关内容。

    ```cpp
    Task<int> acquire();
    ```

- 释放内部计数

    参数`cnt`表示释放的内部计数个数，默认值为1。如果需要在构造后增加信号量的值，可以通过`cnt`指定要增加的个数。用户应保证同一个信号量内部计数个数不超过`uint32_t`的最大值。

    ```cpp
    void release(uint32_t cnt = 1);
    ```

### 示例
```cpp
#include <iostream>

#include "coke/semaphore.h"
#include "coke/sleep.h"
#include "coke/wait.h"

coke::Task<> worker(int id, coke::Semaphore &sem) {
    for (int i = 0; i < 2; i++) {
        co_await sem.acquire();

        std::cout << id << " start\n";
        co_await coke::sleep(0.2);
        std::cout << id << " finish\n";

        sem.release();
    }
}

int main () {
    // 使用最大计数为2的信号量，控制至多同时有两个worker在运行
    coke::Semaphore sem(2);
    coke::sync_wait(worker(1, sem), worker(2, sem), worker(3, sem));
    return 0;
}
```
