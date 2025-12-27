# Coke: Concurrent Operations, Keep Elegant

`Coke`是基于[C++ Workflow](https://github.com/sogou/workflow)和`C++ 20`实现的协程框架，专为高性能异步任务设计。受益于`C++ 20`提供的协程特性，`Coke`提供了简洁直观的异步编程接口，开发者可以使用同步的编码风格创造出异步的高性能应用。

`Coke` is a coroutine framework based on [C++ Workflow](https://github.com/sogou/workflow) and `C++ 20`, designed for high-performance asynchronous tasks. Benefiting from the coroutine features provided by C++20, `Coke` offers a simple and clear asynchronous interface, helping developers to create high-performance asynchronous applications using a synchronous coding style.


## Features

- ⚡️ ​​性能出众​​：继承`C++ Workflow`的高效架构，支持海量并发和高吞吐场景
- 🚀 ​​高效抽象​​：基于`C++20`协程，在极低开销下提供友好的开发体验
- 🔗 ​​无缝集成​​：完全兼容`C++ Workflow`生态，可在协程和任务流之间自由切换
- 📝 ​组件丰富​​：实现了异步锁、异步条件变量、Qps限制器等诸多基础组件
- 🌐 ​​易于扩展​​：已经为HTTP、Redis、MySQL协议提供支持，用户为`Workflow`扩展的协议也可方便地转为协程

<br/>

- ⚡️ Outstanding Performance: Inherits the efficient architecture of `C++ Workflow`, supporting massive concurrency and high throughput scenarios.
- 🚀 Efficient Abstraction: Based on `C++20` coroutines, providing a user-friendly development experience with extremely low overhead.
- 🔗 Seamless Integration: Fully compatible with the `C++ Workflow` ecosystem, allowing free switching between coroutines and workflow's tasks.
- 📝 Rich Components: Implements many basic components such as asynchronous locks, asynchronous condition variables, and Qps limiters.
- 🌐 Easy Extensibility: Already supports HTTP, Redis, and MySQL protocols; protocols extended for `Workflow` can also be easily converted into coroutines.


## Examples

### Example 1: Timer

每隔一秒向标准输出打印一次`Hello World`。

Print `Hello World` to standard output once every second.

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

### Example 2: Redis Client

使用`coke::RedisClient`发起读写请求，并输出结果。

Use `coke::RedisClient` to initiate read and write requests, then show the results.

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

### Example 3: Executing multiple asynchronous tasks concurrently

同时发起三个休眠任务，并等待执行完成。

Start three sleep tasks, and wait for all of them to complete.

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

### More examples

请前往[example](./example/)目录继续阅读。

Please go to the [example](./example/) directory to continue reading.


## 构建和运行示例/Compile and run examples

需要完整支持C++ 20 coroutine功能的编译器。

- 若使用GCC编译器，建议`GCC >= 13`，至少`GCC >= 11`
- 若使用Clang编译器，建议`Clang >= 18`，至少`Clang >= 15`

Requires a compiler that fully supports C++20 coroutine.

- `GCC >= 13` recommended, `GCC >= 11` at least
- `Clang >= 18` recommended, `Clang >= 15` at least


### 安装依赖/Install dependencies

构建时依赖编译器和openssl。如果要执行单元测试需要额外安装gtest。

Build depends on the compiler and openssl. Gtest is needed when compile unit tests.

- Ubuntu 24.04/22.04

    ```bash
    apt install gcc g++ libssl-dev git cmake libgtest-dev
    ```

- Fedora
    ```bash
    dnf install gcc gcc-c++ openssl-devel git cmake gtest-devel
    ```

- macOS
    ```bash
    brew install openssl cmake googletest
    ```

- CentOS Stream 10/9

    ```bash
    dnf install gcc gcc-c++ openssl-devel git cmake
    ```

    Gtest can be installed in the following way.

    ```bash
    dnf install epel-release
    dnf install gtest-devel
    ```

### 下载代码/Download source code

此处以`coke v0.7.0`和`workflow v0.11.11`为例，从github下载源代

From github

```bash
git clone --branch v0.7.0 https://github.com/kedixa/coke.git
cd coke
git clone --branch v0.11.11 https://github.com/sogou/workflow.git
```

或从gitee下载源代码

Or gitee

```bash
git clone --branch v0.7.0 https://gitee.com/kedixa/coke.git
cd coke
git clone --branch v0.11.11 https://gitee.com/sogou/workflow.git
```

### 构建和运行/Compile and run

添加`-DCOKE_ENABLE_TEST=ON`以构建单元测试。

Add `-DCOKE_ENABLE_TEST=ON` to build unit tests.

```bash
cmake -Wno-dev -S . -B build.cmake -DCOKE_WORKFLOW_SRC_DIR=workflow \
    -DCMAKE_CXX_STANDARD=20 -DCOKE_ENABLE_EXAMPLE=ON

# use more jobs if the cpu has more core.
cmake --build build.cmake -j 8

./build.cmake/example/helloworld
```

## 在项目中使用coke/Use coke in project

### CMake with find package

若已经安装了workflow和coke，可使用`find_package`导入构建好的库。

If workflow and coke are already installed, you can import the built libraries using `find_package`.

```cmake
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(workflow REQUIRED)
find_package(coke REQUIRED)

add_executable(xxx xxx.cpp)

# link coke, workflow is linked implicitly
target_link_libraries(xxx PRIVATE coke::coke)
```

### CMake with source code

```cmake
# add workflow and coke
add_subdirectory(workflow)
add_subdirectory(coke)

add_executable(xxx xxx.cpp)

# link coke, workflow is linked implicitly
target_link_libraries(xxx PRIVATE coke::coke)
```

可进一步参考[cmake-example](https://github.com/coke-playground/cmake-example)。

Please refer to [cmake-example](https://github.com/coke-playground/cmake-example) for more information.

### Bazel

在`WORKSPACE`文件中声明依赖

Declare dependencies in the `WORKSPACE` file

```python
load('@bazel_tools//tools/build_defs/repo:git.bzl', 'git_repository')

git_repository(
    name = "workflow",
    remote = "https://github.com/sogou/workflow.git",
    commit = "choose a workflow commit id"
)

git_repository(
    name = "coke",
    remote = "https://github.com/kedixa/coke.git",
    commit = "choose a coke commit id"
)
```

在`BUILD`文件中指定依赖的目标

Specify the target of the dependency in the `BUILD` file

```python
cc_binary(
    name = "xxx",
    srcs = ["xxx.cpp"],
    deps = [
        "@coke//:common", # Basic library
        "@coke//:redis",  # Redis library
        # see BUILD.bazel for more libraries
    ],
)
```


## Attention

### 关于具名任务/About named tasks

`Workflow`中有些任务创建时可以指定名称，例如具名`WFTimerTask`可以根据名称被取消、具名`WFCounterTask`可以根据名称唤醒、具名`WFGoTask`可以根据名称区分计算队列等。`Coke`保留所有以`coke:`开始的名称，以满足自身使用。

In `Workflow`, some tasks can be named when created. For example, `WFTimerTask` can be canceled based on its name, `WFCounterTask` can be woken up based on its name, and `WFGoTask` can be distinguished by its name from the computation queue. `Coke` reserves all names that begin with `coke:` for its own use.


## LICENSE

Apache 2.0
