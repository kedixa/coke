# Coke
本项目由[C++ Workflow](https://github.com/sogou/workflow)强力驱动。

`Coke`是`C++ Workflow`的协程版本，在不失其高效性的前提下，`Coke`致力于实现一套简洁的协程接口，让我们多一种体验`Workflow`的方式。

## 快速入门
- 向标准输出打印`Hello World`，间隔500毫秒

```cpp
#include <iostream>
#include <chrono>
#include "coke/coke.h"
#include "coke/sleep.h"

coke::Task<> helloworld(size_t n, std::chrono::milliseconds ms) {
    for (size_t i = 0; i < n; i++) {
        if (i != 0)
            co_await coke::sleep(ms);

        std::cout << "Hello World" << std::endl;
    }
}

int main() {
    coke::sync_wait(helloworld(3, std::chrono::milliseconds(500)));

    return 0;
}

```

- 使用`coke::HttpClient`发起一个`GET`请求

```cpp
#include <iostream>
#include <string>
#include "coke/http_client.h"

coke::Task<> http_get(const std::string &url) {
    coke::HttpClient cli;
    coke::HttpResult res = co_await cli.request(url);
    coke::HttpResponse &resp = res.resp;

    if (res.state == 0) {
        std::cout << resp.get_http_version() << ' '
                  << resp.get_status_code() << ' '
                  << resp.get_reason_phrase() << std::endl;
    }
    else {
        std::cout << "ERROR: state: " << res.state
                  << " error: " << res.error << std::endl;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " URL" << std::endl;
        return 1;
    }

    coke::sync_wait(http_get(std::string(argv[1])));
    return 0;
}

```

- 更多示例请到[example](./example/)查看


## 构建
需要完整支持C++ 20 coroutine功能的编译器

- GCC >= 11
- Clang >= 15

### Ubuntu 22.04
```bash
apt install -y gcc g++ libgtest-dev libssl-dev git cmake
git clone https://github.com/kedixa/coke.git && cd coke
git clone https://github.com/sogou/workflow.git
make -C workflow -j 8
cmake -D Workflow_DIR=workflow .
make -j 8
```

### Ubuntu 20.04
```bash
# 添加源以使用gcc 11
add-apt-repository ppa:ubuntu-toolchain-r/test
apt install -y gcc-11 g++-11 libgtest-dev libssl-dev git cmake
git clone https://github.com/kedixa/coke.git && cd coke
git clone https://github.com/sogou/workflow.git
export CC=gcc-11 CXX=g++-11
make -C workflow -j 8
cmake -D Workflow_DIR=workflow .
make -j 8
```

### CentOS Stream 8
```bash
yum install -y gcc-toolset-12-gcc gcc-toolset-12-gcc-c++ openssl-devel git cmake
git clone https://github.com/kedixa/coke.git && cd coke
git clone https://github.com/sogou/workflow.git
scl enable gcc-toolset-12 "make -C workflow -j 8"
scl enable gcc-toolset-12 "cmake -D Workflow_DIR=workflow ."
scl enable gcc-toolset-12 "make -j 8"
```

## 注意事项
### 关于具名任务
`Workflow`中有些任务创建时可以指定名称，例如具名`WFTimerTask`可以根据名称被取消、具名`WFCounterTask`可以根据名称唤醒、具名`WFGoTask`可以根据名称区分计算队列等。`Coke`保留所有以`coke:`开始的名称，以满足自身使用。


## LICENSE
Apache 2.0
