## Coke简介
协程是一种特殊类型的函数，可以在执行过程中挂起并在稍后恢复。`C++ 20`引入了对无栈协程的语法支持，但尚未提供协程相关的库，`Coke`便是基于`C++ 20`提供的协程语法实现的一个协程框架。`Coke`没有单独实现一套异步逻辑，而是站在巨人的肩膀上，基于[C++ Workflow](https://github.com/sogou/workflow)框架实现了协程的框架和一系列异步组件。


## Coke的目标
1. 可与`C++ Workflow`任务流体系灵活切换
2. 不影响`C++ Workflow`的性能
3. 提供便捷的异步组件，提高开发效率
4. 借助协程语义，无需因回调函数而拆散上下文联系紧密的代码逻辑

`Coke`框架适合熟悉`C++ Workflow`的用户使用，已有的基于任务流的业务逻辑无需改动，可以在某个任务的回调函数中切换到协程，并在协程执行结束时将流程切换到任务流当中。业务逻辑越复杂，使用`Coke`的收益就越明显，例如在回调模式中，若要在多个复杂任务之间传递上下文，需要动态分配对象并传递指针，或者使用`std::shared_ptr`等智能指针，但在协程环境中，可以简单地定义成函数中的局部变量，大大降低了代码的复杂度。使用`Coke`提供的异步锁、异步容器等组件，可以更方便地实现复杂的业务逻辑，大大提高开发效率。

`Coke`框架也适合不熟悉`C++ Workflow`的用户了解和使用，若拥有其他无栈协程（例如`Python3 asyncio`）框架的使用经验，即使没有使用过`Workflow`任务流体系也可以很快地掌握`Coke`的使用方法。`Coke`从`Workflow`中引入了`http`、`redis`、`mysql`网络协议，但如果需要使用其他网络协议，或者自定义协议，仍需要深入理解`Workflow`机制后才能进行开发。


## 从线程到协程
先从一个简单的代码中快速了解一下协程的相关概念和使用方法。假如有这样一个需求：先等待一秒钟，输出`"Hello "`，再等待一秒钟，输出`"World!\n"`。

- 使用线程实现

```cpp
#include <iostream>
#include <thread>
#include <chrono>

void thread_func() {
    using namespace std::chrono_literals;

    std::this_thread::sleep_for(1s);
    std::cout << "Hello " << std::flush;

    std::this_thread::sleep_for(1s);
    std::cout << "World!" << std::endl;
}

int main() {
    std::jthread th(thread_func);
    return 0;
}
```

- 使用协程实现

```cpp
#include <iostream>
#include <chrono>
#include "coke/wait.h"
#include "coke/sleep.h"

coke::Task<> coke_func() {
    using namespace std::chrono_literals;

    co_await coke::sleep(1s);
    std::cout << "Hello " << std::flush;

    co_await coke::sleep(1s);
    std::cout << "World!" << std::endl;
}

int main() {
    coke::sync_wait(coke_func());
    return 0;
}
```

如果只有一个这样的任务需要执行，那使用线程和协程方式实现的差距不那么明显，但如果每秒钟都有成千上万个这样的任务需要执行呢？线程的调度和频繁的上下文切换将会花费大量宝贵的CPU资源，这种开销在高性能低延迟的场景下是无法接受的。


## 什么是协程
正如一开始提到的，协程是一种特殊类型的函数，在`Coke`框架当中，将返回值类型是`coke::Task<T>`且函数体中包含`co_await`、`co_return`的函数称为协程，如无特别说明，后文提到协程时特指`Coke`框架中的协程。注：暂时不讨论基于`co_yield`的生成器模型。

```cpp
// 返回值类型是coke::Task<int>，函数体包含co_return，因此是一个协程
coke::Task<int> coroutine() {
    co_return 1;
}

// 返回值类型是coke::Task<int>，但函数体中既没有co_await也没有co_return，因此不是协程
// 有时会将多个函数重载转发到同一个协程函数，以简化实现，所以本文也可能将这种函数称为协程，但其本质是一个普通函数
coke::Task<int> not_coroutine() {
    return coroutine();
}

// 返回值类型是coke::Task<bool>，函数体包含co_await和co_return，因此是一个协程
coke::Task<bool> func() {
    int ret1 = co_await coroutine();
    // 虽然not_coroutine不是协程，但其返回值可以被co_await
    int ret2 = co_await not_coroutine();

    co_return (ret1 + ret2 == 2);
}
```

在协程当中，可以被`co_await`的对象称为可等待对象`awaitable object`。在`Coke`中，可等待对象基本都继承自`coke::AwaiterBase`，上文中实现休眠任务的`coke::sleep`函数的返回值类型是`coke::SleepAwaiter`，其中`co_await coke::sleep(1s)`实际上做了两件事

```cpp
coke::Task<void> func() {
    using namespace std::chrono_literals;
    // 1. 创建休眠任务
    coke::SleepAwaiter s = coke::sleep(1s);
    // 中间可以做其他的事情
    // 2. 执行休眠任务
    co_await std::move(s);
}
```

读者需要了解以下几点内容

1. 可等待对象创建后，可以立刻`co_await`，可以稍后`co_await`，也可以放弃执行
2. 可等待对象往往在内部关联了其他状态，至多可被执行一次，执行时需要转换成右值
3. 协程中需要显式使用`co_await`开始任务，若没有使用这个运算符，则表示仅创建可等待对象但不执行

在编写代码时，建议开启编译器警告，当将`co_await coke::sleep(1s);`误写为`coke::sleep(1s);`时可以得到类似`warning: ignoring returned value of type 'coke::SleepAwaiter', declared with attribute 'nodiscard'`的警告信息，但对于将创建和执行分开写的场景则无法收到警告，请特别注意。

`coke::Task<T>`也可以通过`co_await`运算符开始执行，但其并未继承自`coke::AwaiterBase`，实际上它通过`operator co_await`生成可等待对象，读者暂时无需深入了解这些概念。


## 协程内存占用
有栈协程一般会在启动时分配一块足够大小的栈空间，而无栈协程会在启动时分配一块大小恰当的协程帧。考虑下述简单的协程，在`GCC 12`编译环境及默认内存分配器情况下，其内存占用分析如下

1. 休眠任务`coke::detail::TimerTask`占用80字节，实际分配96字节
2. `Workflow`为休眠任务分配`struct __poller_node`占用104字节，实际分配112字节
3. 任务流`SeriesWork`占用168字节，实际分配176字节
4. 协程帧占用200字节，实际分配208字节，其中已经包括`coke::Task<void>`、`coke::detail::CoPromise<void>`、`coke::SleepAwaiter`等

该协程总计需要内存`80+104+168+200=552`字节，内存分配器共分配`96+112+176+208=592`字节，如果为协程增加参数或者局部变量，其内存随协程帧一同分配。

```cpp
coke::Task<> task() {
    co_await coke::sleep(1.0);
}
```
