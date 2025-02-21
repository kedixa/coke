全局配置位于头文件`coke/global.h`中，包含以下内容。


## 版本号
`Coke`定义了一个宏`COKE_VERSION_NUMBER`，以数值常量的形式表示当前代码的版本号，例如当版本号为`v1.2.3`时，该常量的值为`0x010203L`，便于在预处理阶段判断版本号。

同时在`coke`命名空间定义了以下几个常量，例如当版本号为`v1.2.3`时

```cpp
constexpr int COKE_MAJOR_VERSION = 1;
constexpr int COKE_MINOR_VERSION = 2;
constexpr int COKE_PATCH_VERSION = 3;
```

以及字符串常量`constexpr const char COKE_VERSION_STR[] = "1.2.3";`。


## 任务状态码
定义了以下常量

```cpp
// state constant from workflow, see WFTask.h
constexpr int STATE_UNDEFINED = -1;
constexpr int STATE_SUCCESS = 0;
constexpr int STATE_TOREPLY = 3;
constexpr int STATE_NOREPLY = 4;
constexpr int STATE_SYS_ERROR = 1;
constexpr int STATE_SSL_ERROR = 65;
constexpr int STATE_DNS_ERROR = 66;
constexpr int STATE_TASK_ERROR = 67;
constexpr int STATE_ABORTED = 2;
```

以及与超时原因相关的状态码

```cpp
// timeout reason constant from workflow, see CommRequest.h
constexpr int CTOR_NOT_TIMEOUT = 0;
constexpr int CTOR_WAIT_TIMEOUT = 1;
constexpr int CTOR_CONNECT_TIMEOUT = 2;
constexpr int CTOR_TRANSMIT_TIMEOUT = 3;
```

`Coke`应保证上述状态码与`Workflow`中的值完全一致，用户可自由选择使用哪种方式。


## 带超时时间的操作的状态码
`Coke`中提供了一系列异步组件，例如`coke::Mutex`、`coke::Queue`等，其中`coke::Mutex::try_lock_for`、`coke::Queue<T>::try_push_for`等需要指定超时时间的接口，这些协程返回一个`int`类型的整数，其含义如下所示。对于`coke::Mutex::lock`等接口，虽未显式指定超时时间，但可以看做是超时时间无限的`try_lock_for`调用，因此返回值含义也一致。其中`TOP`理解为`Timed OPeration`、`This OPeration`或`The OPeration`均可。

```cpp
// 该操作成功完成。
constexpr int TOP_SUCCESS = 0;
// 该操作未能成功，且已经到达了超时时间。
constexpr int TOP_TIMEOUT = 1;
// 进程正在退出，但仍有协程没有执行完成，其中的休眠任务以该状态失败。Coke强列建议在主线程退出前
// 等待所有协程退出，以免丢失重要数据。
constexpr int TOP_ABORTED = 2;
// 异步容器关闭后，若 1. 再向其中添加数据 或2. 容器为空后再尝试取数据时，返回该状态码。
constexpr int TOP_CLOSED = 3;
// 若返回值为负数，表示在异步等待过程中遇到了系统错误，通常不会出现。
```


## 全局配置
`coke::GlobalSettings`与`Workflow`中的`WFGlobalSettings`含义一致。`coke::EndpointParams`是`::EndpointParams`的一个别名。

```cpp
using EndpointParams = ::EndpointParams;

struct GlobalSettings {
    EndpointParams endpoint_params;
    EndpointParams dns_server_params;
    unsigned int dns_ttl_default        = 3600;
    unsigned int dns_ttl_min            = 60;
    int dns_threads                     = 4;
    int poller_threads                  = 4;
    int handler_threads                 = 20;
    int compute_threads                 = -1;
    int fio_max_events                  = 4096;
    const char *resolv_conf_path        = "/etc/resolv.conf";
    const char *hosts_path              = "/etc/hosts";
};
```


## 辅助函数
- 全局初始化函数，含义与`WORKFLOW_library_init`一致

    ```cpp
    void library_init(const GlobalSettings &s);
    ```

- 获取错误码对应的文本描述，与`WFGlobal::get_error_string`一致

    ```cpp
    const char *get_error_string(int state, int error);
    ```

- 获取一个当前进程全局唯一的`uint64_t`类型的标记
    - 该函数用于以`id`为标记的休眠任务，会在相关章节中展示具体用法。若每纳秒调用一次该函数，则所有可用id会在持续运行约583年后耗尽，此后不再保证正确性，建议在此之前重启一次进程。
    - 常量`INVALID_UNIQUE_ID`用于表示这不是一个从该函数获取到的id。

    ```cpp
    uint64_t get_unique_id();

    constexpr uint64_t INVALID_UNIQUE_ID = 0;
    ```

- 判断是否应尝试切换线程
    无栈协程调用下一个协程时会复用线程栈，在某些极端场景下会因递归调用而导致栈溢出，用户一般不会遇到这种场景。当确实遇到这类问题时，每次递归都切换一次线程会有较大的开销，此时可以调用这个函数，并保证当该函数返回`true`时切换一次线程，例如使用`co_await coke::yield();`。

    ```cpp
    bool prevent_recursive_stack(bool clear = false);
    ```
