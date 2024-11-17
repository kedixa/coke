使用下述功能需要包含头文件`coke/shared_mutex.h`。


## coke::SharedMutex
`coke::SharedMutex`允许多个协程同时持有共享锁或一个协程持有独占锁。

在当前协程未锁定互斥锁或共享锁的情况下执行解锁操作是错误行为，该类不会检测这种行为。

### 成员函数

- 构造函数/析构函数

    只可默认构造，不可复制构造，不可移动构造。

    ```cpp
    SharedMutex() noexcept;

    SharedMutex(const SharedMutex &) = delete;
    SharedMutex &operator= (const SharedMutex &) = delete;

    ~SharedMutex();
    ```

- 尝试独占锁定

    尝试获得独占锁但不阻塞，若获取成功则返回`true`，否则返回`false`。

    ```cpp
    bool try_lock();
    ```

- 尝试共享锁定

    尝试获得共享锁但不阻塞，若获取成功则返回`true`，否则返回`false`。

    ```cpp
    bool try_lock_shared();
    ```

- 尝试从共享锁升级为独占锁

    若升级成功，返回`true`并获得独占锁，否则返回`false`，且保持共享锁。

    ```cpp
    bool try_upgrade();
    ```

- 解除锁

    释放当前持有的锁。

    ```cpp
    void unlock();
    ```

- 解除共享锁

    释放当前持有的共享锁，等价于`unlock`。

    ```cpp
    void unlock_shared();
    ```

- 独占锁定，直到超时

    协程返回一个`int`类型的整数，`coke::TOP_SUCCESS`表示获取成功，`coke::TOP_TIMEOUT`表示因超时而获取失败，`coke::TOP_ABORTED`表示进程退出，负数表示发生系统错误。可参考`全局配置`章节的相关内容。

    ```cpp
    coke::Task<int> try_lock_for(const NanoSec &nsec);
    ```

- 独占锁定

    协程返回一个`int`类型的整数，`coke::TOP_SUCCESS`表示获取成功，`coke::TOP_ABORTED`表示进程退出，负数表示发生系统错误。可参考`全局配置`章节的相关内容。

    ```cpp
    coke::Task<int> lock();
    ```

- 共享锁定，直到超时

    协程返回一个`int`类型的整数，`coke::TOP_SUCCESS`表示获取成功，`coke::TOP_TIMEOUT`表示因超时而获取失败，`coke::TOP_ABORTED`表示进程退出，负数表示发生系统错误。可参考`全局配置`章节的相关内容。

    ```cpp
    coke::Task<int> try_lock_shared_for(const coke::NanoSec &nsec);
    ```

- 共享锁定

    协程返回一个`int`类型的整数，`coke::TOP_SUCCESS`表示获取成功，`coke::TOP_ABORTED`表示进程退出，负数表示发生系统错误。可参考`全局配置`章节的相关内容。

    ```cpp
    coke::Task<int> lock_shared();
    ```

### 示例
```cpp
#include <iostream>
#include <chrono>

#include "coke/mutex.h"
#include "coke/shared_mutex.h"
#include "coke/sleep.h"
#include "coke/wait.h"

using namespace std::chrono_literals;

coke::Task<> reader(coke::SharedMutex &m) {
    coke::SharedLock lk(m);

    for (int i = 0; i < 3; i++) {
        co_await lk.lock();

        // 多个读者可以同时进入临界区
        std::cout << "reader start\n";
        co_await coke::sleep(100ms);
        std::cout << "reader finish\n";

        lk.unlock();
    }
}

coke::Task<> writer(coke::SharedMutex &m) {
    coke::UniqueLock lk(m);

    for (int i = 0; i < 3; i++) {
        co_await lk.lock();

        // 只有一个写者可以进入临界区
        std::cout << "writer start\n";
        co_await coke::sleep(100ms);
        std::cout << "writer finish\n";

        lk.unlock();

        // 共享锁为写优先锁，如果频繁加写锁可能导致读者无法获得锁
        co_await coke::sleep(110ms);
    }
}

int main() {
    coke::SharedMutex m;
    coke::sync_wait(
        reader(m), reader(m),
        writer(m), writer(m)
    );
    return 0;
}
```


## coke::SharedLock

`coke::SharedLock`是一个共享互斥锁的包装器，支持尝试锁定、锁定、带超时的锁定、以及析构时自动解锁。

```cpp
template<typename MutexType>
class SharedLock;
```

### 成员函数

- 构造函数/析构函数

    默认构造时不关联任何共享锁。使用共享锁对象构造时，通过参数`is_locked`来指定是否已经被锁定。若析构时当前对象关联共享锁且处于锁定状态，则自动解锁。

    ```cpp
    SharedLock() noexcept;

    explicit SharedLock(MutexType &m, bool is_locked = false) noexcept;

    SharedLock(SharedLock &&) noexcept;

    ~SharedLock();
    ```

- 检查是否关联共享锁且处于锁定状态

    ```cpp
    bool owns_lock() const;
    ```

- 与另一`coke::SharedLock`交换状态

    ```cpp
    void swap(SharedLock &other);
    ```

- 与共享锁解除关联，但不解锁它

    ```cpp
    MutexType *release();
    ```

- 尝试锁定

    尝试获得锁但不阻塞，若获取成功则返回`true`，否则返回`false`。若共享锁已经被锁定，抛出以`std::errc::resource_deadlock_would_occur`为错误码的`std::system_error`。

    ```cpp
    bool try_lock();
    ```

- 尝试锁定，直到超时

    协程返回一个`int`类型的整数，`coke::TOP_SUCCESS`表示获取成功，`coke::TOP_TIMEOUT`表示因超时而获取失败，`coke::TOP_ABORTED`表示进程退出，负数表示发生系统错误。可参考`全局配置`章节的相关内容。若共享锁已经被锁定，抛出以`std::errc::resource_deadlock_would_occur`为错误码的`std::system_error`。

    ```cpp
    coke::Task<int> try_lock_for(coke::NanoSec nsec);
    ```

- 锁定

    协程返回一个`int`类型的整数，`coke::TOP_SUCCESS`表示获取成功，`coke::TOP_ABORTED`表示进程退出，负数表示发生系统错误。可参考`全局配置`章节的相关内容。若共享锁已经被锁定，抛出以`std::errc::resource_deadlock_would_occur`为错误码的`std::system_error`。

    ```cpp
    coke::Task<int> lock();
    ```

- 解锁

    若当前对象未关联共享锁，或者未处于锁定状态，抛出以`std::errc::operation_not_permitted`为错误码的`std::system_error`。

    ```cpp
    void unlock();
    ```
