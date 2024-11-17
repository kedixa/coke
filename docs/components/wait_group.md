使用下述功能需要包含头文件`coke/wait_group.h`。


## 常量
`coke::WaitGroup`内部使用休眠任务等待其他协程结束，其成功状态等价于休眠任务被取消，使用下述常量以表达该语义。

```cpp
constexpr int WAIT_GROUP_SUCCESS = SLEEP_CANCELED;
``` 


## coke::WaitGroup
`WaitGroup`用于等待一组并发操作完成。它允许用户在多个任务并行执行的情况下，等待所有任务完成。通过增加计数和通知完成，可以有效管理并发操作的结束。

### 成员函数
- 构造函数/析构函数

    默认构造函数创建一个内部计数为零的`WaitGroup`，不可移动，不可复制。

    ```cpp
    WaitGroup() noexcept;

    WaitGroup(const WaitGroup &) = delete;
    WaitGroup &operator= (const WaitGroup &) = delete;

    ~WaitGroup();
    ```

- 增加计数

    增加计数，其中`n > 0`。调用`done`的次数必须与`add`中增加的计数相匹配。

    ```cpp
    void add(long n) noexcept;
    ```

- 减少计数

    将内部计数减少一。当计数减少到零后，再增加计数是错误行为。

    ```cpp
    void done();
    ```

- 等待内部计数减少到零

    阻塞当前协程，直到内部计数减少到零。

    ```cpp
    WaitGroupAwaiter wait();
    ```

### 示例
参考`example/ex016-wait_group.cpp`。
