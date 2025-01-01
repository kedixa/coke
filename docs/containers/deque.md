使用下述功能需要包含头文件`coke/deque.h`。

## coke::Deque

类模板`coke::Deque`是一种容器，它提供双端队列的功能，队列两端均可放入数据或者取出数据。内部使用`std::deque`保存数据，模板参数`T`表示容器中的数据类型；模板参数`Alloc`表示容器使用的内存分配器类型，默认为`std::allocator<T>`。

```cpp
template<Queueable T, typename Alloc=std::allocator<T>>
class Deque;
```

### 成员函数

- 构造函数和析构函数

    构造双端队列时应指定一个最大容量`max_size`，但注意强行放入数据的接口不检测容量限制。最大容量不可指定为`0`。

    参数`alloc`用于指定`std::deque`容器的内存分配器。

    ```cpp
    explicit Deque(std::size_t max_size)
    Deque(std::size_t max_size, const AllocatorType &alloc)

    Deque(const Deque &) = delete;
    Deque &operator= (const Deque &) = delete;

    ~Deque() = default;
    ```

- 观察容器的状态

    由于容器可能被多个线程同时使用，观察到的状态在函数返回后已经不再精确。

    `size()`的返回值可能因强行放入数据而超出`max_size()`的值，参考`force_push_back`等函数。

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

    当容器满或者容器已经关闭时，`try_emplace_*`拒绝操作并返回`false`，否则放入数据并返回`true`。

    `force_emplace_*`不检查容量，当容器已经关闭时拒绝操作并返回`false`，否则放入数据并返回`true`。

    ```cpp
    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    bool try_emplace_front(Args&&... args);

    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    bool try_emplace_back(Args&&... args);

    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    bool force_emplace_front(Args&&... args);

    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    bool force_emplace_back(Args&&... args);
    ```

- 以原位构造的方式将新数据放入容器，等待至容器有空余、超时或被关闭

    协程返回一个`int`类型整数，有以下几种情况

    - `coke::TOP_SUCCESS`表示成功放入
    - `coke::TOP_TIMEOUT`表示等待超时，`try_emplace_*_for`可能返回该值
    - `coke::TOP_ABORTED`表示进程正在退出
    - `coke::TOP_CLOSED`表示容器已被关闭
    - 负数表示发生系统错误，该情况几乎不会发生

    ```cpp
    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    coke::Task<int> emplace_front(Args&&... args);

    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    coke::Task<int> emplace_back(Args&&... args);

    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    coke::Task<int> try_emplace_front_for(coke::NanoSec nsec, Args&&... args);

    template<typename... Args>
        requires std::constructible_from<T, Args&&...>
    coke::Task<int> try_emplace_back_for(coke::NanoSec nsec, Args&&... args);
    ```

- 将数据放入容器

    将新数据复制或移动到容器中，返回值参考`emplace_*`相关描述。

    ```cpp
    template<typename U>
        requires std::assignable_from<T&, U&&>
    bool try_push_front(U &&u);

    template<typename U>
        requires std::assignable_from<T&, U&&>
    bool try_push_back(U &&u);

    template<typename U>
        requires std::assignable_from<T&, U&&>
    bool force_push_front(U &&u);

    template<typename U>
        requires std::assignable_from<T&, U&&>
    bool force_push_back(U &&u);
    ```

- 将数据放入容器中，等待至容器有空余、超时或被关闭

    返回值参考`emplace_*`相关描述。

    ```cpp
    template<typename U>
        requires std::assignable_from<T&, U&&>
    coke::Task<int> push_front(U &&u);

    template<typename U>
        requires std::assignable_from<T&, U&&>
    coke::Task<int> push_back(U &&u);

    template<typename U>
        requires std::assignable_from<T&, U&&>
    coke::Task<int> try_push_front_for(coke::NanoSec nsec, U &&u);

    template<typename U>
        requires std::assignable_from<T&, U&&>
    coke::Task<int> try_push_back_for(coke::NanoSec nsec, U &&u);
    ```

- 从容器中取出数据

    若容器为空，则取出失败，返回`fasle`，否则取出成功并返回`true`。

    ```cpp
    template<typename U>
        requires std::assignable_from<U&, T&&>
    bool try_pop_front(U &u);

    template<typename U>
        requires std::assignable_from<U&, T&&>
    bool try_pop_back(U &u);
    ```

- 从容器中取出数据，等待至有新数据被放入或超时

    协程返回一个`int`类型整数，有以下几种情况

    - `coke::TOP_SUCCESS`表示成功取出
    - `coke::TOP_TIMEOUT`表示等待超时，`try_pop_*_for`可能返回该值
    - `coke::TOP_ABORTED`表示进程正在退出
    - `coke::TOP_CLOSED`表示容器为空且已被关闭
    - 负数表示发生系统错误，该情况几乎不会发生

    ```cpp
    template<typename U>
        requires std::assignable_from<U&, T&&>
    coke::Task<int> pop_front(U &u);

    template<typename U>
        requires std::assignable_from<U&, T&&>
    coke::Task<int> pop_back(U &u);

    template<typename U>
        requires std::assignable_from<U&, T&&>
    coke::Task<int> try_pop_front_for(coke::NanoSec nsec, U &u);

    template<typename U>
        requires std::assignable_from<U&, T&&>
    coke::Task<int> try_pop_back_for(coke::NanoSec nsec, U &u);
    ```

- 批量放入数据

    尝试将`[first, last)`中的数据按顺序逐个放入容器，返回首个因容器满而未被放入的迭代器，若全部放入则返回`last`。若当前容器空间余量不足`size_hint`，则不放入任何数据，返回`first`。

    批量放入过程中发生异常时，当前实现的行为是：已放入的数据不会被取出，正在处理的数据状态取决于底层容器的异常保证，尚未被处理的数据不会被放入容器。

    ```cpp
    template<std::input_iterator Iter>
        requires std::assignable_from<T&, typename Iter::value_type>
    auto try_push_back_range(Iter first, Iter last, std::size_t size_hint = 0);
    ```

- 批量取出数据

    `try_pop_front_range`尝试从容器中按顺序取出数据并保存到`[first, last)`，返回首个因容器已空而未放入数据的迭代器，若全部放入则返回`last`。若当前容器的数据量不足`size_hint`，则不取出任何数据，返回`first`。

    `try_pop_front_n`尝试从容器中按顺序取出数据并赋值给`iter`，返回取出数据的数量。`max_pop`表示至多取出多少个数据。

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

容器提供了`try_push_front/try_push_back`、`try_pop_front/try_pop_back`等接口，如果大部分情况下容器都是非满或非空的，先用这些接口尝试放入或取出，相比于直接使用协程`push_front/push_back`、`pop_front/pop_back`接口更高效，这是因为创建协程有一定的开销。如下所示，若大部分情况下容器是非空的，则方法二比方法一更高效，用户可按实际场景选择合适的方法。

```cpp
coke::Task<> func(coke::Deque<int> &que) {
    int ret;
    int value;

    // 方法一
    ret = co_await que.pop_back(value);

    // 方法二
    if (que.try_pop_back(value))
        ret = coke::TOP_SUCCESS;
    else
        ret = co_await que.pop_back(value);

    // 判断ret，使用value
}
```

### 示例

参考`example/ex015-async_queue.cpp`，双端队列与队列的使用方式基本一致，只是接口上区分是从`front`还是`back`存取数据。
