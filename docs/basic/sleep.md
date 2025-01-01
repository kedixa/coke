使用下述功能需要包含头文件`coke/sleep.h`。


## 辅助类型与常量
定义类型`coke::NanoSec`为与休眠任务相关的时间单位，它是`std::chrono::nanoseconds`的别名，其他常见的时间单位可以直接用于构造`std::chrono::nanoseconds`对象，因此在使用时不必显式转换成纳秒。

定义类型`coke::InfiniteDuration`为无限制的休眠时间，并定义常量`coke::inf_dur`为该类型的对象，这种类型的休眠任务只会因被取消而唤醒。

定义下述常量表示休眠任务的返回值

```cpp
// 休眠任务因时间到期而被唤醒
constexpr int SLEEP_SUCCESS = 0;
// 休眠任务因被取消而被唤醒
constexpr int SLEEP_CANCELED = 1;
// 休眠任务因进程退出而被唤醒
constexpr int SLEEP_ABORTED = 2;
// 若返回值为负数，则表示遇到了系统错误，实际上几乎不会遇到这种情况
```

用户可以认为下述约束恒成立

```cpp
static_assert(SLEEP_SUCCESS == TOP_SUCCESS);
static_assert(SLEEP_ABORTED == TOP_ABORTED);
```


## SleepAwaiter
### 构造函数
- 默认构造函数，该任务不会休眠，直接返回`coke::SLEEP_SUCCESS`

    ```cpp
    SleepAwaiter();
    ```

- 使用`std::chrono`时间类型构造，休眠指定的时间

    ```cpp
    SleepAwaiter(coke::NanoSec nsec);
    ```

- 使用`double`表示的秒数构造，需保证该值的范围是一个合理的时间长度

    ```cpp
    SleepAwaiter(double sec);
    ```

- 基于全局唯一整数标识的休眠任务
    - 使用该方法创建任务，需要从`coke::get_unique_id()`获取一个整数标识(`id`)，通过这种方法创建的休眠任务，除了可以因时间到期而唤醒外，还可以通过`coke::cancel_sleep_by_id`来取消。
    - 使用同一个`id`创建的任务，会被放置在同一个队列当中，后创建的任务默认放到队列尾。当执行取消操作时，优先从队列头取出并取消任务，若`insert_head`参数设置为`true`则将该任务放置到队列头部。

    ```cpp
    SleepAwaiter(uint64_t id, coke::NanoSec nsec, bool insert_head);
    SleepAwaiter(uint64_t id, double sec, bool insert_head);
    SleepAwaiter(uint64_t id, coke::InfiniteDuration, bool insert_head);
    ```

- 基于全局唯一地址的休眠任务
    - 若某一组休眠任务的生命周期严格限制于某个对象的生命周期内，可以使用该对象的地址来作为休眠任务的唯一标识，这得益于内存地址都是全局唯一的。由于生命周期有严格限制，当该对象释放内存并被其他对象使用时，该内存地址还可以由下一组休眠任务复用，因此绝不会被耗尽。`coke::Condition`等组件使用了该机制实现，该机制应谨慎用于具有`[[no_unique_address]]`属性的类型，确保地址的唯一性。
    - 使用该方法创建的任务，除了可以因时间到期而唤醒外，还可以通过`coke::cancel_sleep_by_addr`来取消。
    - 使用同一个`addr`创建的任务，会被放置在同一个队列当中，后创建的任务默认放到队列尾。当执行取消操作时，优先从队列头取出并取消任务，若`insert_head`参数设置为`true`则将该任务放置到队列头部。

    ```cpp
    SleepAwaiter(const void *addr, coke::NanoSec nsec, bool insert_head);
    SleepAwaiter(const void *addr, double sec, bool insert_head);
    SleepAwaiter(const void *addr, coke::InfiniteDuration, bool insert_head);
    ```


## WFSleepAwaiter
`Workflow`中通过名称创建可取消的休眠任务的辅助类型，可使用下述构造函数创建任务。

```cpp
WFSleepAwaiter(const std::string &name, coke::NanoSec nsec);
```


## 辅助函数
使用休眠任务时，建议直接使用下述辅助函数创建可等待对象，其含义与上文描述一致。

```cpp
SleepAwaiter sleep(coke::NanoSec nsec);
SleepAwaiter sleep(double sec);

SleepAwaiter sleep(uint64_t id, coke::NanoSec nsec, bool insert_head = false);
SleepAwaiter sleep(uint64_t id, double sec, bool insert_head = false);
SleepAwaiter sleep(uint64_t id, coke::InfiniteDuration x, bool insert_head = false);

SleepAwaiter sleep(const void *addr, coke::NanoSec nsec, bool insert_head = false);
SleepAwaiter sleep(const void *addr, double sec, bool insert_head = false);
SleepAwaiter sleep(const void *addr, coke::InfiniteDuration x, bool insert_head = false);

WFSleepAwaiter sleep(const std::string &name, coke::NanoSec nsec);

// 其效果与时长为零的休眠任务等价，但效率更高。
SleepAwaiter yield();
```

用于取消与指定标识(`id`、`addr`、`name`)关联的休眠任务的函数。其中`max`参数表示至多取消多少个任务，没有`max`参数的重载函数表示取消所有关联的休眠任务。函数返回值表示实际取消了多少个休眠任务。

```cpp
std::size_t cancel_sleep_by_id(uint64_t id, std::size_t max);
std::size_t cancel_sleep_by_id(uint64_t id);

std::size_t cancel_sleep_by_addr(const void *addr, std::size_t max);
std::size_t cancel_sleep_by_addr(const void *addr);

int cancel_sleep_by_name(const std::string &name, std::size_t max);
int cancel_sleep_by_name(const std::string &name);
```


## 示例
### 简单休眠任务
```cpp
#include <iostream>

#include "coke/sleep.h"
#include "coke/wait.h"

coke::Task<> helloworld() {
    co_await coke::sleep(0.5);
    std::cout << "Hello World" << std::endl;

    co_await coke::sleep(std::chrono::milliseconds(500));
    std::cout << "Hello World" << std::endl;
}

int main() {
    coke::sync_wait(helloworld());
    return 0;
}
```

### 可取消的休眠任务
这里展示可取消的休眠任务的使用方法，这是许多异步组件实现的基础，在实际应用中，可直接使用封装好的异步组件，这些内容在其他章节进一步介绍。

```cpp
#include <iostream>

#include "coke/global.h"
#include "coke/sleep.h"
#include "coke/wait.h"

coke::Task<> another_task(uint64_t uid) {
    std::cout << "开始工作" << std::endl;

    // 模拟耗时的工作
    co_await coke::sleep(1.0);

    std::cout << "唤醒正在等待的协程" << std::endl;
    coke::cancel_sleep_by_id(uid);
}

coke::Task<> sleep_example() {
    uint64_t uid = coke::get_unique_id();
    coke::SleepAwaiter s = coke::sleep(uid, coke::inf_dur);

    std::cout << "分离一个协程去做其他工作" << std::endl;
    coke::detach(another_task(uid));

    std::cout << "继续执行当前协程" << std::endl;

    // 等待被分离的协程取消休眠任务
    int ret = co_await std::move(s);
    std::cout << "取消成功：" << std::boolalpha << (ret == coke::SLEEP_CANCELED)
        << std::endl;
}

int main() {
    coke::sync_wait(sleep_example());
    return 0;
}
```
