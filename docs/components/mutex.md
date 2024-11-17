使用下述功能需要包含头文件`coke/mutex.h`。


## coke::Mutex
互斥锁用于保护共享数据的独占访问。`C++`标准库提供了`std::mutex`互斥锁机制，可用于在多线程访问共享数据时提供保护。在协程环境中，异步操作会在恰当的时候让出线程，让线程可以执行其他协程，这种情况下使用`std::mutex`会导致死锁等问题。`coke::Mutex`提供与标准库互斥锁相似的语义，并可以应用在协程中。

在当前协程未锁定互斥锁的情况下执行解锁操作是错误行为，该类不会检测这种行为。

### 成员函数
- 构造函数/析构函数

    只可默认构造，不可复制构造，不可移动构造

    ```cpp
    Mutex() noexcept;

    Mutex(const Mutex &) = delete;
    Mutex &operator= (const Mutex &) = delete;

    ~Mutex();
    ```

- 尝试锁定

    尝试获得锁但不阻塞，若获取成功则返回`true`，否则返回`false`。

    ```cpp
    bool try_lock();
    ```

- 尝试锁定，直到超时

    协程返回一个`int`类型的整数，`coke::TOP_SUCCESS`表示获取成功，`coke::TOP_TIMEOUT`表示因超时而获取失败，`coke::TOP_ABORTED`表示进程退出，负数表示发生系统错误。可参考`全局配置`章节的相关内容。

    ```cpp
    coke::Task<int> try_lock_for(coke::NanoSec nsec);
    ```

- 锁定

    协程返回一个`int`类型的整数，`coke::TOP_SUCCESS`表示获取成功，`coke::TOP_ABORTED`表示进程退出，负数表示发生系统错误。可参考`全局配置`章节的相关内容。

    ```cpp
    coke::Task<int> lock();
    ```

- 解锁

    ```cpp
    void unlock();
    ```


### 示例
```cpp
#include <iostream>
#include <chrono>

#include "coke/mutex.h"
#include "coke/sleep.h"
#include "coke/wait.h"

using namespace std::chrono_literals;

coke::Task<> worker(coke::Mutex &m) {
    coke::UniqueLock lk(m);

    // 获得锁
    co_await lk.lock();

    std::cout << "locked" << std::endl;
    // 执行需要保护的异步操作，例如写同一个文件
    co_await coke::sleep(100ms);

    std::cout << "finished" << std::endl;
    lk.unlock();

    // 不需要锁的其他操作
}

int main() {
    coke::Mutex m;
    coke::sync_wait(worker(m), worker(m));
    return 0;
}
```


## coke::UniqueLock
`coke::UniqueLock`是一种通用互斥包装器，支持尝试锁定、锁定、带超时的锁定、以及析构时自动解锁。

```cpp
template<typename MutexType>
class UniqueLock;
```

### 成员函数

- 构造函数/析构函数

    默认构造时不关联任何互斥锁。使用互斥锁对象构造时，通过参数`is_locked`来指定该互斥锁是否已经被锁定。该类型可以移动，但不可复制。若析构时当前对象关联互斥锁且处于锁定状态，则自动解锁。

    ```cpp
    UniqueLock() noexcept;

    explicit UniqueLock(MutexType &m, bool is_locked = false) noexcept;

    UniqueLock(UniqueLock &&) noexcept;

    ~UniqueLock();
    ```

- 检查是否关联互斥锁且处于锁定状态

    ```cpp
    bool owns_lock() const noexcept;
    ```

- 与另一`coke::UniqueLock`交换状态

    ```cpp
    void swap(UniqueLock &other) noexcept;
    ```

- 与互斥锁解除关联，但不解锁它

    ```cpp
    MutexType *release() noexcept;
    ```

- 尝试锁定

    尝试获得锁但不阻塞，若获取成功则返回`true`，否则返回`false`。若互斥锁已经被锁定，抛出以`std::errc::resource_deadlock_would_occur`为错误码的`std::system_error`。

    ```cpp
    bool try_lock();
    ```

- 尝试锁定，直到超时

    协程返回一个`int`类型的整数，`coke::TOP_SUCCESS`表示获取成功，`coke::TOP_TIMEOUT`表示因超时而获取失败，`coke::TOP_ABORTED`表示进程退出，负数表示发生系统错误。可参考`全局配置`章节的相关内容。若互斥锁已经被锁定，抛出以`std::errc::resource_deadlock_would_occur`为错误码的`std::system_error`。

    ```cpp
    coke::Task<int> try_lock_for(coke::NanoSec nsec);
    ```

- 锁定

    协程返回一个`int`类型的整数，`coke::TOP_SUCCESS`表示获取成功，`coke::TOP_ABORTED`表示进程退出，负数表示发生系统错误。可参考`全局配置`章节的相关内容。若互斥锁已经被锁定，抛出以`std::errc::resource_deadlock_would_occur`为错误码的`std::system_error`。

    ```cpp
    coke::Task<int> lock();
    ```

- 解锁

    若当前对象未关联互斥锁，或者未处于锁定状态，抛出以`std::errc::operation_not_permitted`为错误码的`std::system_error`。

    ```cpp
    void unlock();
    ```
