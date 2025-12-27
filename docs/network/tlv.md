## Tlv协议
Tlv协议是一种由类型、长度、内容组成的简单消息结构，`Workflow`提供了一种`Tlv`协议的实现，`Coke`封装了该协议的客户端和服务器，方便使用者快速实现一个自定义的网络服务。


```cpp
namespace coke {

// 目前消息类型为Workflow消息的别名
using TlvRequest  = protocol::TLVRequest;
using TlvResponse = protocol::TLVResponse;

// 客户端错误类型，当state为coke::STATE_TASK_ERROR时，error为这些错误
enum : int {
    // 未正确设置TlvClientInfo，使用TlvClient时不会发生该错误
    TLV_ERR_CLI_INFO = 1,
    // 密码认证失败
    TLV_ERR_AUTH     = 2,
};

}
```


## coke::TlvResult
`coke::TlvResult`是`coke::TlvClient::request`的返回值类型，表示一次请求的结果。

- 构造和赋值

    可默认构造，可复制，可移动。

    ```cpp
    TlvResult() = default;

    TlvResult(const TlvResult &) = default;
    TlvResult(TlvResult &&)      = default;

    TlvResult &operator=(const TlvResult &) = default;
    TlvResult &operator=(TlvResult &&)      = default;
    ```

- 获取请求状态

    这与Workflow网络任务中的`state`和`errro`含义相同。

    ```cpp
    int get_state() const;
    int get_error() const;
    ```

- 获取结果类型和值

    ```cpp
    int32_t get_type() const;

    const std::string &get_value() const &;
    std::string get_value() &&;
    ```


## Tlv客户端
使用下述功能需要包含头文件`coke/tlv/client.h`。

### 客户端参数

`coke::TlvClientParams`有以下参数

```cpp
// 最大重试次数
int retry_max{0};

// 任务超时时间，单位为毫秒
int send_timeout{-1};
int receive_timeout{-1};
int keep_alive_timeout{60 * 1000};
int watch_timeout{0};

// 网络类型，默认为TCP
TransportType transport_type{TT_TCP};
// 当使用TT_TCP_SSL类型时，使用该SSL CTX，若未指定则使用默认的SSL CTX
std::shared_ptr<SSL_CTX> ssl_ctx{nullptr};

// 服务器的host和port
std::string host;
std::string port;

// 若未指定host，则使用该地址为服务器地址
sockaddr_storage addr_storage{};
socklen_t addr_len{0};

// 用于支持简单的认证，使用auth_type, auth_value发起认证，若返回消息的type为
// auth_success_type则为认证成功
bool enable_auth{false};
int32_t auth_type{0};
int32_t auth_success_type{0};
std::string auth_value;

// 返回消息的长度限制，默认不限制长度
std::size_t response_size_limit{(std::size_t)-1};
```

### coke::TlvClient
`coke::TlvClient`是一个并发安全的Tlv协议客户端，可同时在多个协程当中使用同一个客户端发起请求。

- 构造函数

    使用`TlvClientParams`构造客户端对象，客户端不可复制、不可移动。

    ```cpp
    TlvClient(const TlvClientParams &params);

    TlvClient(const TlvClient &) = delete;
    TlvClient(TlvClient &&)      = delete;
    ```

- 发起请求

    ```cpp
    Task<TlvResult> request(int32_t type, StrHolder value);
    ```

### coke::TlvConnectionClient
`coke::TlvConnectionClient`不是并发安全的，使用者需保证在同一时刻至多发起一个请求，该客户端尝试使用同一个网络连接发起下一个请求，可用于实现事务的功能。当连接意外断开时，请求会以state为`coke::STATE_SYS_ERROR`、errro为`ECONNRESET`的状态失败，下一次请求时则会重建连接。

该客户端的成员函数与`TlvClient`基本相同，但增加了一个关闭连接的函数，客户端使用完毕后建议通过该函数主动关闭连接，否则该连接会持续到`keep_alive_timeout`到期，或者服务端主动关闭连接。

- 关闭连接

    ```cpp
    Task<TlvResult> disconnect();
    ```


## Tlv服务器

### 服务器参数
`coke::TlvServerParams`有以下参数，含义与`Workflow`中的`WFServerParams`一致。

```cpp
TransportType transport_type{TT_TCP};
size_t max_connections{2000};
int peer_response_timeout{10 * 1000};
int receive_timeout{-1};
int keep_alive_timeout{60 * 1000};
size_t request_size_limit{-1};
int ssl_accept_timeout{5000};
```

### coke::TlvServer

- 构造函数

    ```cpp
    TlvServer(const TlvServerParams &params, ProcessorType co_proc);

    TlvServer(ProcessorType co_proc);
    ```

## 示例

参考`ex022-tlv_server.cpp`、`ex023-tlv_client.cpp`、`ex024-tlv_uds.cpp`。
