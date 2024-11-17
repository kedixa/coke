使用下述功能需要包含头文件`coke/qps_pool.h`。


## coke::QpsPool
`coke::QpsPool`提供了一种请求速率限制机制，让请求以指定的频率均匀地发出。在被请求方处理能力有限的情况下，该机制可以避免其被过大的请求速率击垮。

使用`coke::QpsPool`应设置合理的请求速率，例如每两秒一个请求、每秒五十万请求等。不合理的速率，如每天一个请求，每秒`9223372036854775807`个请求等。过低的请求速率会让协程等待极长的时间，由于目前的实现尚不支持取消等待，想正常结束进程也会因此而无法结束。过高的请求速率会有计时精度问题，同时一个进程也无法承载这些请求，因此没有实际意义。

### 成员函数

- 构造函数

    创建`QpsPool`，指定在`seconds`秒内允许的`query`请求数。`query`必须大于等于0，`seconds`必须大于等于1。该类型不可移动、不可复制。

    ```cpp
    QpsPool(long query, long seconds = 1);
    ```

- 重置速率限制

    即使正在使用中，也可以重置速率限制，但新的限制将在下次调用`get`时生效，重置前正在等待的请求仍使用原来的速率。参数的含义与约束参考构造函数。

    ```cpp
    void reset_qps(long query, long seconds = 1);
    ```

- 获取许可

    从`QpsPool`获取`count`个请求许可，默认获取一个。返回一个可等待的对象，调用者应立即使用`co_await`等待该对象完成，该对象将在合适的时机被唤醒。

    ```cpp
    AwaiterType get(unsigned count = 1);
    ```

- 有条件地获取许可

    如果当前已经有很多协程正在等待请求许可，后续的请求会等待更长的时间才能发起。返回一个可等待对象，调用者应立即使用`co_await`等待该对象完成，若无法在`nsec`时间内获取到`count`个请求许可，该对象会立即结束并返回`coke::SLEEP_CANCELED`，否则将在合适的时机被唤醒，并返回`coke::SLEEP_SUCCESS`，表示获取请求许可成功。

    ```cpp
    AwaiterType get_if(unsigned count, coke::NanoSec nsec);
    ```

### 示例
```cpp
#include <iostream>

#include "coke/qps_pool.h"
#include "coke/wait.h"

coke::Task<> worker(coke::QpsPool &pool) {
    for (int i = 0; i < 5; i++) {
        co_await pool.get();
        std::cout << "hello\n";
    }
}

int main() {
    coke::QpsPool pool(3);

    coke::sync_wait(worker(pool), worker(pool));
    return 0;
}
```
