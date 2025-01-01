由于`coke`非常灵活，使用时可能会无意间写出容易引起故障的代码，本节对常见的场景进行汇总。

### 虚假的并行

下述示例中，`parallel_go`将一个大的计算任务分成三个小任务并行执行，如果此时忘记了切换计算线程，则这三个小任务将会在同一个线程内顺序执行，导致虚假的并行。

若要启动较多并发（例如几千）协程，每个协程开始都有一段耗时较短的准备工作要做，但又不想为此专门启动计算线程，也可使用`co_await coke::yield();`切换到`handler`线程中并行执行。

```cpp
coke::Task<void> go(int from, int to) {
    // 忘记切换计算线程
    // co_await coke::switch_go_thread();

    for (int i = from; i < to; i++) {
        // 模拟耗时的操作
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // 其他流程

    co_return;
}

coke::Task<void> parallel_go() {
    co_await coke::async_wait(
        go(   0, 1000),
        go(1000, 2000),
        go(2000, 3000)
    );
}
```

### 悬垂引用

下述示例中，`lvalue_ref`、`rvalue_ref`函数以引用方式接收参数，但调用方未能保证参数的生命周期长于协程，发生悬垂引用。

```cpp
coke::Task<void> lvalue_ref(const int &x) {
    co_await coke::sleep(1.0);
    std::cout << x << ' ' << &x << std::endl;
}

coke::Task<void> test_lvalue_ref() {
    std::vector<coke::Future<void>> futs;

    for (int i = 0; i < 5; i++) {
        // 传入局部变量的引用，但引用很快失效了
        futs.push_back(coke::create_future(lvalue_ref(i)));
    }

    co_await coke::wait_futures(futs, futs.size());
}

template<typename T>
coke::Task<void> rvalue_ref(T &&t) {
    co_await coke::sleep(1.0);
    std::cout << t << std::endl;
}

coke::Task<void> test_rvalue() {
    // 以右值引用的方式接收参数，但这个std::string很快就析构了
    coke::Task<void> task = rvalue_ref(std::string(20, 'a'));
    co_await std::move(task);

    // 但下述代码正确
    // co_await rvalue_ref(std::string(20, 'a'));
}
```

### 意外的内存占用

协程中局部变量占用的内存会在协程结束后释放，若有占用空间较大的局部变量，且该协程执行时间较长，则会长时间占用较大内存，即使该内存不会再被用到。该类场景可使用动态分配的方法，在使用完毕后及时释放。

```cpp
coke::Task<> cost_memory() {
    int big_array[1024*1024];
    co_await some_func1(big_array);
    co_await some_func2(big_array);

    // 虽然不再使用big_array，但内存会一直占用直到当前协程结束
    for (int i = 0; i < 100; i++) {
        co_await long_time_func();
    }
}
```
