使用下述功能需要包含头文件`coke/queue.h`。


## coke::QueueCommon
`coke::QueueCommon`是`coke::Queue`、`coke::Stack`、`coke::PriorityQueue`的基类，几乎所有的成员函数都在这里实现。

```cpp
template<typename Q, typename T>
class QueueCommon;
```

### 成员函数

- 观察容器的状态
    由于容器可能被多个线程同时使用，观察到的状态在函数返回后已经不再精确。

    `size()`的返回值可能因强行放入数据而超出`max_size()`的值，参考`force_push`等函数。

    ```cpp
    // 检测容器是否为空
    bool empty() const noexcept;

    // 检测容器是否已满
    bool full() const noexcept;

    // 检测容器是否已被关闭
    bool closed() const noexcept;

    // 获得容器中的数据的数量
    std::size_t size() const noexcept;

    // 获得容器的最大容量
    std::size_t max_size() const noexcept;
    ```

- 关闭容器
    当容器被关闭后，所有等待放入和取出数据的协程都将被唤醒。向已经关闭的容器放入数据时，会以`coke::TOP_CLOSED`状态失败；从已经关闭的容器取出数据时，若容器不为空则取出成功，否则以`coke::TOP_CLOSED`状态失败。

    ```cpp
    void close();
    ```

- 重新打开容器
    当容器处于关闭状态，且没有任何协程正在操作容器时，可以通过该操作重新打开容器，使其可以继续使用。

    ```cpp
    void reopen() noexcept;
    ```

- 以原位构造的方式将新数据放入容器
    当容器满或者容器已经关闭时，`try_emplace`拒绝操作并返回`false`，否则放入数据并返回`true`。
    `force_emplace`不检查容量，当容器已经关闭时拒绝操作并返回`false`，否则放入数据并返回`true`。

    ```cpp
    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    bool try_emplace(Args&&... args);

    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    bool force_emplace(Args&&... args);
    ```

- 以原位构造的方式将新数据放入容器，等待至容器有空余、超时或被关闭
    协程返回一个`int`类型整数，有以下几种情况
    - `coke::TOP_SUCCESS`表示成功放入
    - `coke::TOP_TIMEOUT`表示等待超时，`try_emplace_for`可能返回该值
    - `coke::TOP_ABORTED`表示进程正在退出
    - `coke::TOP_CLOSED`表示容器已被关闭
    - 负数表示发生系统错误，该情况几乎不会发生

    ```cpp
    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    coke::Task<int> emplace(Args&&... args);

    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    coke::Task<int> try_emplace_for(coke::NanoSec nsec, Args&&... args);
    ```

- 将数据放入容器
    将新数据复制或移动到容器中，返回值参考`emplace`相关描述。

    ```cpp
    template<typename U>
        requires std::assignable_from<T&, U&&>
    bool try_push(U &&u);

    template<typename U>
        requires std::assignable_from<T&, U&&>
    bool force_push(U &&u);
    ```

- 将数据放入容器中，等待至容器有空余、超时或被关闭
    返回值参考`emplace`相关描述。

    ```cpp
    template<typename U>
        requires std::assignable_from<T&, U&&>
    coke::Task<int> push(U &&u);

    template<typename U>
        requires std::assignable_from<T&, U&&>
    coke::Task<int> try_push_for(coke::NanoSec nsec, U &&u);
    ```

- 从容器中取出数据
    若容器为空，则取出失败，返回`fasle`，否则取出成功并返回`true`。

    ```cpp
    template<typename U>
        requires std::assignable_from<U&, T&&>
    bool try_pop(U &u);
    ```

- 从容器中取出数据，等待至有新数据被放入或超时
    协程返回一个`int`类型整数，有以下几种情况
    - `coke::TOP_SUCCESS`表示成功取出
    - `coke::TOP_TIMEOUT`表示等待超时，`try_pop_for`可能返回该值
    - `coke::TOP_ABORTED`表示进程正在退出
    - `coke::TOP_CLOSED`表示容器为空且已被关闭
    - 负数表示发生系统错误，该情况几乎不会发生

    ```cpp
    template<typename U>
        requires std::assignable_from<U&, T&&>
    coke::Task<int> pop(U &u);

    template<typename U>
        requires std::assignable_from<U&, T&&>
    coke::Task<int> try_pop_for(coke::NanoSec nsec, U &u);
    ```

- 批量放入数据
    尝试将`[first, last)`中的数据按顺序逐个放入容器，返回首个因容器满而未被放入的迭代器，若全部放入则返回`last`。若当前容器空间余量不足`size_hint`，则不放入任何数据，返回`first`。
    批量放入过程中发生异常时，当前实现的行为是：已放入的数据不会被取出，正在处理的数据状态取决于底层容器的异常保证，尚未被处理的数据不会被放入容器。

    ```cpp
    template<std::input_iterator Iter>
        requires std::assignable_from<T&, typename Iter::value_type>
    auto try_push_range(Iter first, Iter last, std::size_t size_hint = 0);
    ```

- 批量取出数据
    `try_pop_range`尝试从容器中按顺序取出数据并保存到`[first, last)`，返回首个因容器已空而未放入数据的迭代器，若全部放入则返回`last`。若当前容器的数据量不足`size_hint`，则不取出任何数据，返回`first`。
    `try_pop_n`尝试从容器中按顺序取出数据并赋值给`iter`，返回取出数据的数量。`max_pop`表示至多取出多少个数据。
    批量取出过程中发生异常时，当前实现的行为是：已取出的数据不会被销毁，正在处理的数据状态取决于底层容器的异常保证。

    ```cpp
    template<typename Iter>
        requires std::is_assignable_v<decltype(*std::declval<Iter>()), T&&>
                 && std::output_iterator<Iter, T>
    auto try_pop_range(Iter first, Iter last, std::size_t size_hint = 0);

    template<typename Iter>
        requires std::is_assignable_v<decltype(*std::declval<Iter>()), T&&>
                 && std::output_iterator<Iter, T>
    std::size_t try_pop_n(Iter iter, std::size_t max_pop);
    ```

### 惯用法
容器提供了`try_push`、`try_pop`等接口，如果大部分情况下容器都是非满或非空的，先用这些接口尝试放入或取出，相比于直接使用协程`push`、`pop`接口更高效，这是因为创建协程有一定的开销。如下所示，若大部分情况下容器是非空的，则方法二比方法一更高效，用户可按实际场景选择合适的方法。

```cpp
coke::Task<> func(coke::Queut<int> &que) {
    int ret;
    int value;

    // 方法一
    ret = co_await que.pop(value);

    // 方法二
    if (que.try_pop(value))
        ret = coke::TOP_SUCCESS;
    else
        ret = co_await que.pop(value);

    // 判断ret，使用value
}
```


## coke::Queue
类模板`coke::Queue`是一种容器，它提供队列的功能，只允许从尾部放入数据，从头部取出数据，即先进先出。模板参数`T`表示容器中的数据类型；模板参数`Container`表示使用的底层容器，默认为`std::deque<T>`。

`coke::Queue`是`coke::QueueCommon`的派生类，可使用其所有公有成员函数。

```cpp
template<Queueable T, typename Container = std::deque<T>>
class Queue;
```

### 成员函数

- 构造函数
    构造队列容器时应指定一个最大容量`max_size`，但注意强行放入数据的接口不检测容量限制。最大容量不可指定为`0`。
    参数`alloc`用于指定底层容器的内存分配器，会直接传递给底层容器的构造函数。

    ```cpp
    explicit Queue(std::size_t max_size);

    template<typename Alloc>
        requires std::uses_allocator_v<QueueType, Alloc>
    Queue(std::size_t max_size, const Alloc &alloc);
    ```


## coke::PriorityQueue
类模板`coke::PriorityQueue`是一种容器，它提供优先队列的功能，放入数据时自动调整其位置，取出数据时只可取出当前最大的数据(基于`Compare`比较，默认为`std::less`)，即大顶堆。模板参数`T`表示容器中数据类型；模板参数`Container`表示使用的底层容器，默认为`std::vector<T>`；`Compare`为比较数据时使用的函数对象类型，默认为`std::less<typename Container::value_type>`。

`coke::PriorityQueue`是`coke::QueueCommon`的派生类，可使用其所有公有成员函数。该类型基于`std::priority_queue`实现，由于`std::priority_queue::top()`返回数据的常量引用，为了保证优先队列结构不因取出数据时发生异常而破坏，在使用类型为`U`的变量取出数据时，`coke::PriorityQueue`额外要求数据类型`T`和`U`，要么满足`std::is_nothrow_assignable_v<U &, T &&>`，要么满足`std::is_assignable_v<U &, const T &>`，否则会编译失败。

```cpp
template<
    Queueable T,
    typename Container = std::vector<T>,
    typename Compare = std::less<typename Container::value_type>
>
class PriorityQueue;
```

### 成员函数

- 构造函数
    构造优先队列容器时应指定一个最大容量`max_size`，但注意强行放入数据的接口不检测容量限制。最大容量不可指定为`0`。
    参数`alloc`用于指定底层容器的内存分配器，会直接传递给底层容器的构造函数。
    参数`comp`用于指定比较函数对象，会直接传递给底层容器的构造函数。

    ```cpp
    explicit PriorityQueue(std::size_t max_size);

    template<typename Alloc>
        requires std::uses_allocator_v<QueueType, Alloc>
    PriorityQueue(std::size_t max_size, const Alloc &alloc);

    PriorityQueue(std::size_t max_size, const CompareType &comp);

    template<typename Alloc>
        requires std::uses_allocator_v<QueueType, Alloc>
    PriorityQueue(std::size_t max_size, const CompareType &comp, const Alloc &alloc);
    ```


## coke::Stack
类模板`coke::Stack`是一种容器，它提供栈的功能，只允许从尾部放入数据，从尾部取出数据，即后进先出。模板参数`T`表示容器中的数据类型；模板参数`Container`表示使用的底层容器，默认为`std::deque<T>`。

`coke::PriorityQueue`是`coke::QueueCommon`的派生类，可使用其所有公有成员函数。

```cpp
template<Queueable T, typename Container = std::deque<T>>
class Stack;
```

### 成员函数

- 构造函数
    构造栈容器时应指定一个最大容量`max_size`，但注意强行放入数据的接口不检测容量限制。最大容量不可指定为`0`。
    参数`alloc`用于指定底层容器的内存分配器，会直接传递给底层容器的构造函数。

    ```cpp
    explicit Stack(std::size_t max_size);

    template<typename Alloc>
        requires std::uses_allocator_v<QueueType, Alloc>
    Stack(std::size_t max_size, const Alloc &alloc);
    ```


## 示例
参考`example/ex015-async_queue.cpp`，队列、栈、优先队列的使用方式一致。
