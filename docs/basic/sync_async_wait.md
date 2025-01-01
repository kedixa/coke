使用下述功能需要包含头文件`coke/wait.h`。


## 同步等待
同步等待函数用于启动协程或可等待对象，等待其执行完成，并返回结果。协程内产生的异常必须在内部处理完毕，否则是错误行为，进程会直接终止。若需要等待可能抛出异常的协程，可参考`coke::Future`组件。

- 一个协程

    ```cpp
    template<Cokeable T>
    T sync_wait(coke::Task<T> &&task);
    ```

- 一组协程
    - 当`T`是`void`时，返回类型是`void`，否则返回类型是`std::vector<T>`。

    ```cpp
    template<Cokeable T>
    auto sync_wait(std::vector<coke::Task<T>> &&tasks);
    ```

- 编译期确定数量的一组协程
    - 与等待`std::vector<coke::Task<T>>`效果等价，该语法糖在协程数量少且固定时可简化代码

    ```cpp
    template<Cokeable T, Cokeable... Ts>
    auto sync_wait(coke::Task<T> &&first, coke::Task<Ts> &&... others);
    ```

- 一个可等待对象
    - 返回可等待对象产生的返回值

    ```cpp
    template<AwaitableType A>
    auto sync_wait(A &&a) -> AwaiterResult<A>;
    ```

- 一组可等待对象
    - 令`T`为可等待对象`A`的返回值类型。当`T`是`void`时，返回类型是`void`，否则返回类型是`std::vector<T>`。

    ```cpp
    template<AwaitableType A>
    auto sync_wait(std::vector<A> &&);
    ```

- 确定数量的一组可等待对象，要求返回值类型相同
    - 与等待`std::vector<A>`效果等价，该语法糖在可等待对象数量少且固定时可简化代码
    - 该方式不要求可等待对象本身类型相同，只要返回值相同，可等待对象即可被包装成同类型的`coke::Task<T>`对象

    ```cpp
    template<AwaitableType A, AwaitableType... As>
    auto sync_wait(A &&first, As&&... others);
    ```

### 同步等待示例
```cpp
#include <iostream>
#include <vector>

#include "coke/sleep.h"
#include "coke/go.h"
#include "coke/wait.h"

coke::Task<int> task_int() {
    co_await coke::sleep(0.1);
    co_return 1;
}

int go_func() {
    return 1;
}

std::ostream &operator<<(std::ostream &out, const std::vector<int> &v) {
    out << "{";
    for (std::size_t i = 0; i < v.size(); i++) {
        if (i != 0)
            out << ", ";
        out << v[i];
    }
    out << "}";

    return out;
}

void sync_example() {
    int ret;
    std::vector<int> rets;

    // 1. 一个协程
    ret = coke::sync_wait(task_int());
    std::cout << "1. " << ret << std::endl;

    // 2. 一组协程
    std::vector<coke::Task<int>> tasks;
    tasks.reserve(3);
    tasks.emplace_back(task_int());
    tasks.emplace_back(task_int());
    tasks.emplace_back(task_int());
    rets = coke::sync_wait(std::move(tasks));
    std::cout << "2. " << rets << std::endl;

    // 3. 确定数量的一组协程
    rets = coke::sync_wait(task_int(), task_int());
    std::cout << "3. " << rets << std::endl;

    // 4. 一个可等待对象
    ret = coke::sync_wait(coke::sleep(0.1));
    std::cout << "4. " << ret << std::endl;

    // 5. 一组可等待对象
    std::vector<coke::SleepAwaiter> sleeps;
    sleeps.reserve(3);
    sleeps.emplace_back(coke::sleep(0.1));
    sleeps.emplace_back(coke::sleep(0.2));
    sleeps.emplace_back(coke::sleep(0.3));
    rets = coke::sync_wait(std::move(sleeps));
    std::cout << "5. " << rets << std::endl;

    // 6. 确定数量的一组可等待对象
    rets = coke::sync_wait(
        coke::sleep(0.1),
        coke::go(go_func)
    );
    std::cout << "6. " << rets << std::endl;
}

int main() {
    sync_example();
    return 0;
}
```


## 异步等待
异步等待函数用于启动协程或可等待对象，等待其执行完成，并返回结果。协程内产生的异常必须在内部处理完毕，否则是错误行为，进程会直接终止。若需要等待可能抛出异常的协程，可参考`coke::Future`组件。

- 一组协程
    - 当`T`是`void`时，返回类型是`coke::Task<void>`，否则返回类型是`coke::Task<std::vector<T>>`。

    ```cpp
    template<Cokeable T>
    auto async_wait(std::vector<coke::Task<T>> &&tasks);
    ```

- 编译期确定数量的一组协程
    - 与等待`std::vector<coke::Task<T>>`效果等价，该语法糖在协程数量少且固定时可简化代码

    ```cpp
    template<Cokeable T, Cokeable... Ts>
    auto async_wait(coke::Task<T> &&first, coke::Task<Ts> &&... others);
    ```

- 一组可等待对象
    - 令`T`为可等待对象`A`的返回值类型。当`T`是`void`时，返回类型是`coke::Task<void>`，否则返回类型是`coke::Task<std::vector<T>>`。

    ```cpp
    template<AwaitableType A>
    auto async_wait(std::vector<A> &&);
    ```

- 确定数量的一组可等待对象，要求返回值类型相同
    - 与等待`std::vector<A>`效果等价，该语法糖在可等待对象数量少且固定时可简化代码
    - 该方式不要求可等待对象本身类型相同，只要返回值相同，可等待对象即可被包装成同类型的`coke::Task<T>`对象

    ```cpp
    template<AwaitableType A, AwaitableType... As>
    auto async_wait(A &&first, As&&... others);
    ```

### 异步等待示例
```cpp
#include <iostream>
#include <vector>

#include "coke/sleep.h"
#include "coke/go.h"
#include "coke/wait.h"

coke::Task<int> task_int() {
    co_await coke::sleep(0.1);
    co_return 1;
}

int go_func() {
    return 1;
}

std::ostream &operator<<(std::ostream &out, const std::vector<int> &v) {
    out << "{";
    for (std::size_t i = 0; i < v.size(); i++) {
        if (i != 0)
            out << ", ";
        out << v[i];
    }
    out << "}";

    return out;
}

coke::Task<> async_example() {
    std::vector<int> rets;

    // 1. 一组协程
    std::vector<coke::Task<int>> tasks;
    tasks.reserve(3);
    tasks.emplace_back(task_int());
    tasks.emplace_back(task_int());
    tasks.emplace_back(task_int());
    rets = co_await coke::async_wait(std::move(tasks));
    std::cout << "1. " << rets << std::endl;

    // 2. 确定数量的一组协程
    rets = co_await coke::async_wait(task_int(), task_int());
    std::cout << "2. " << rets << std::endl;

    // 3. 一组可等待对象
    std::vector<coke::SleepAwaiter> sleeps;
    sleeps.reserve(3);
    sleeps.emplace_back(coke::sleep(0.1));
    sleeps.emplace_back(coke::sleep(0.2));
    sleeps.emplace_back(coke::sleep(0.3));
    rets = co_await coke::async_wait(std::move(sleeps));
    std::cout << "3. " << rets << std::endl;

    // 4. 确定数量的一组可等待对象
    rets = co_await coke::async_wait(
        coke::sleep(0.1),
        coke::go(go_func)
    );
    std::cout << "4. " << rets << std::endl;
}

int main() {
    coke::sync_wait(async_example());
    return 0;
}
```
