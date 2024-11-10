本节用于记录未在其他文档中提到，但又不足以单独组成一节的内容。


## coke.h
`coke/coke.h`中引入了大部分基础组件的头文件，在编写示例代码时可直接包含这个头文件，但在正式项目中还是仅包含实际需要的那些头文件为好。


## 使用有状态的函数对象创建协程
如果协程是由有状态的函数对象创建的，且协程的生命周期长于函数对象时，会导致在函数对象被销毁后再访问其成员的问题。`Coke`提供了函数`coke::make_task`，对于可移动的函数对象，可以通过延长其生命周期，保证在协程结束前不会被销毁。该函数主要是为了在特殊情况下提供一种可行的方法，正常情况下不建议使用。

使用该函数需要包含头文件`coke/make_task.h`。

```cpp
template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
auto make_task(FUNC &&func, ARGS && ... args);
```

### 示例
```cpp
#include <iostream>
#include <string>

#include "coke/sleep.h"
#include "coke/make_task.h"
#include "coke/wait.h"

coke::Task<std::string> create_task() {
    std::string s("hello");

    auto &&f = [=](std::string t) -> coke::Task<std::string> {
        co_await coke::sleep(0.1);
        co_return s + " " + t;
    };

    // 1. 错误，当前函数返回后，函数f已经被销毁
    //return f("world");

    // 2. 使用其他方式延长函数f的生命周期，可行
    return coke::make_task(std::move(f), "world");
}

coke::Task<> make_task() {
    std::string r = co_await create_task();
    std::cout << r << std::endl;
}

int main() {
    coke::sync_wait(make_task());
    return 0;
}
```
