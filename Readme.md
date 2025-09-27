# Coke: Concurrent Operations, Keep Elegant

`Coke`是基于[C++ Workflow](https://github.com/sogou/workflow)和`C++ 20`实现的协程框架，专为高性能异步任务设计。受益于`C++ 20`提供的协程特性，`Coke`提供了简洁直观的异步编程接口，开发者可以使用同步的编码风格创造出异步的高性能应用。


## 核心特征

- ⚡️ ​​性能出众​​：继承`C++ Workflow`的高效架构，支持海量并发和高吞吐场景
- 🚀 ​​高效抽象​​：基于`C++20`协程，在极低开销下提供友好的开发体验
- 🔗 ​​无缝集成​​：完全兼容`C++ Workflow`生态，可在协程和任务流之间自由切换
- 📝 ​组件丰富​​：实现了异步锁、异步条件变量、Qps限制器等诸多基础组件
- 🌐 ​​易于扩展​​：已经为HTTP、Redis、MySQL协议提供支持，用户为`Workflow`扩展的协议也可方便地转为协程


## 示例

### 示例一：定时器

每隔一秒向标准输出打印一次`Hello World`。

```cpp
#include <iostream>
#include <chrono>

#include "coke/wait.h"
#include "coke/sleep.h"

coke::Task<> say_hello(std::size_t n) {
    std::chrono::seconds one_sec(1);

    for (std::size_t i = 0; i < n; i++) {
        if (i != 0)
            co_await coke::sleep(one_sec);

        std::cout << "Hello World" << std::endl;
    }
}

int main() {
    coke::sync_wait(say_hello(3));

    return 0;
}
```

### 示例二：使用Redis客户端

使用`coke::RedisClient`发起读写请求，并输出结果。

```cpp
#include <iostream>

#include "coke/coke.h"
#include "coke/redis/client.h"

void show_result(const coke::RedisResult &res)
{
    if (res.get_state() == coke::STATE_SUCCESS)
        std::cout << res.get_value().debug_string() << std::endl;
    else
        std::cout << "RedisFailed state:" << res.get_state()
                  << " error:" << res.get_error() << std::endl;
}

coke::Task<> redis_cli()
{
    coke::RedisClientParams params;
    params.host = "127.0.0.1";
    params.port = "6379";
    params.password = "your_password";

    coke::RedisClient cli(params);
    coke::RedisResult res;

    // setex key 100 value
    res = co_await cli.setex("key", 100, "value");
    show_result(res);

    // mget key nokey
    res = co_await cli.mget("key", "nokey");
    show_result(res);

    // del key
    res = co_await cli.del("key");
    show_result(res);
}

int main()
{
    coke::sync_wait(redis_cli());
    return 0;
}
```

### 示例三：并发执行多个异步任务

同时发起三个休眠任务，并等待执行完成。

```cpp
#include <iostream>
#include <vector>

#include "coke/coke.h"

coke::Task<> sleep()
{
    co_await coke::sleep(1.0);
    std::cout << "Sleep 1 second finished.\n";
}

int main()
{
    std::vector<coke::Task<>> tasks;

    for (int i = 0; i < 3; i++)
        tasks.emplace_back(sleep());

    coke::sync_wait(std::move(tasks));

    return 0;
}
```

### 更多示例

请前往[example](./example/)目录继续阅读。


## 如何构建

需要完整支持C++ 20 coroutine功能的编译器

- 若使用GCC编译器，建议`GCC >= 13`，至少`GCC >= 11`
- 若使用Clang编译器，建议`Clang >= 18`，至少`Clang >= 15`


### 一：安装依赖项

构建时依赖编译器和openssl，单元测试部分依赖gtest，根据实际情况按需安装依赖项。

- Ubuntu 24.04/22.04

    ```bash
    apt install gcc g++ libgtest-dev libssl-dev git cmake
    ```

- Fedora
    ```bash
    dnf install gcc gcc-c++ gtest-devel openssl-devel git cmake
    ```

- CentOS Stream 10/9

    ```bash
    dnf install gcc gcc-c++ openssl-devel git cmake
    ```

    可通过下述方式安装gtest

    ```bash
    dnf install epel-release
    dnf install gtest-devel
    ```

### 二：下载源代码

此处以`coke v0.6.0`和`workflow v0.11.11`为例，从github下载源代码

```bash
git clone --branch v0.6.0 https://github.com/kedixa/coke.git
cd coke
git clone --branch v0.11.11 https://github.com/sogou/workflow.git
```

或从gitee下载源代码

```bash
git clone --branch v0.6.0 https://gitee.com/kedixa/coke.git
cd coke
git clone --branch v0.11.11 https://gitee.com/sogou/workflow.git
```

### 三：编译并体验示例

- cmake参数`-j 8`表示编译时开启8个并发，如果有更多的cpu核心，可以指定一个更大的数。
- 选项`-D COKE_ENABLE_EXAMPLE=1`表示构建示例，选项`-D COKE_ENABLE_TEST=1`表示构建单元测试。

```bash
cmake -S workflow -B build.workflow -D CMAKE_CXX_STANDARD=20
cmake --build build.workflow -j 8

cmake -S . -B build.coke -D Workflow_DIR=workflow -D CMAKE_CXX_STANDARD=20 -D COKE_ENABLE_EXAMPLE=1 -D COKE_ENABLE_TEST=1
cmake --build build.coke -j 8

# 执行测试
ctest --test-dir build.coke/test/

# 体验示例
./build.coke/example/helloworld
```


## 注意事项
### 关于具名任务
`Workflow`中有些任务创建时可以指定名称，例如具名`WFTimerTask`可以根据名称被取消、具名`WFCounterTask`可以根据名称唤醒、具名`WFGoTask`可以根据名称区分计算队列等。`Coke`保留所有以`coke:`开始的名称，以满足自身使用。


## LICENSE
Apache 2.0
