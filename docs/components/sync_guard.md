使用下述功能需要包含头文件`coke/sync_guard.h`。


## coke::SyncGuard
`coke`非常不推荐在协程中使用同步阻塞的操作，若必须如此，应采取措施避免将`handler`线程全部阻塞住。

`coke::SyncGuard`是对`WFGlobal::sync_operation_begin()`和`WFGlobal::sync_operation_end()`的封装。合理使用这些功能，`Workflow`会动态调整`handler`线程数，使新任务总会有线程处理，避免完全阻塞。

### 成员函数
- 构造函数、析构函数
    若`auto_sync_begin`为`true`，构造函数会自动调用`sync_operation_begin`。
    `SyncGuard`不可拷贝，不可移动。
    若析构时`in_guard()`返回`true`，则会自动调用`sync_operation_end`。

    ```cpp
    SyncGuard(bool auto_sync_begin);

    SyncGuard(const SyncGuard &) = delete;
    SyncGuard &operator= (const SyncGuard &) = delete;

    ~SyncGuard();
    ```

- 开始同步操作
    调用底层的`WFGlobal::sync_operation_begin`，但同一个线程内递归地多次使用`SyncGuard`，实现保证仅调用一次底层接口。
    同一个`coke::SyncGuard`对象的所有`sync_operation_begin`和`sync_operation_end`都必须在同一个线程内调用，包括在构造函数和析构函数中的调用，否则为错误行为。

    ```cpp
    void sync_operation_begin();
    ```

- 结束同步操作
    调用底层的`WFGlobal::sync_operation_end`，仅在当前线程所有`SyncGuard`都调用`sync_operation_end`后才会调用底层接口。

    ```cpp
    void sync_operation_end();
    ```

- 查看是否正在进行同步操作
    若已执行`sync_operation_begin`但尚未执行`sync_operation_end`则返回`true`，否则返回`false`。

    ```cpp
    bool in_guard() const noexcept;
    ```
