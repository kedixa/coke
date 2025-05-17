使用下述功能需要包含头文件`coke/nspolicy/nspolicy.h`。


## 辅助类型和常量
这些内容均位于`coke`命名空间下。

`ADDR_STATE_*`用于表示`AddressInfo`的内部状态，可以通过`AddressInfo::get_state`获得。

`ADDRESS_WEIGHT_MAX`是`AddressParams::weight`可设置的最大值。

`AddressParams`表示与一组`(host, port)`关联的参数，在向`NSPolicy`添加地址时一同指定。

`AddressPack`和`HostPortPack`用于`NSPolicy`类的批量操作接口，例如`get_all_address`等。

```cpp
enum {
    ADDR_STATE_GOOD     = 0,
    ADDR_STATE_FAILING  = 1,
    ADDR_STATE_DISABLED = 2,
    ADDR_STATE_REMOVED  = 3,
};

constexpr static uint16_t ADDRESS_WEIGHT_MAX = 1000;

struct AddressParams {
    EndpointParams endpoint_params = ENDPOINT_PARAMS_DEFAULT;
    unsigned dns_ttl_default{3600};
    unsigned dns_ttl_min{60};

    uint16_t weight{100};
};

struct AddressPack {
    int state{0};
    std::string host;
    std::string port;
    AddressParams params;
};

struct HostPortPack {
    std::string host;
    std::string port;
};
```


## coke::AddressInfo

`coke::AddressInfo`用于在`NSPolicy`中表示一个网络地址信息，该类型不可复制、不可移动，也不应直接构造。使用时通过`NSPolicy::add_address`添加网络地址，并通过`NSPolicy::get_address`或`NSPolicy::select_address`等方法获取并使用。`AddressInfo`由内部引用计数维护生命周期，使用完毕后应通过`AddressInfo::dec_ref`主动释放资源，参考`NSpolicy`相关接口的要求。

获取`AddressInfo`相关信息的接口

```cpp
const std::string &get_host() const &;
const std::string &get_port() const &;
const AddressParams &get_addr_params() const &;
uint16_t get_weight() const;
int get_state() const;
```


## coke::NSPolicyParams

`coke::NSPolicyParams`是创建`NSPolicy`时指定的参数，`NSPolicy`通过这些参数决定如何维护`AddressInfo`。该类型有如下选项

- bool enable_auto_break_recover

    若该选项为`false`，则`NSPolicy`不再自动实施熔断和恢复策略，默认值为`true`。

- bool fast_recover

    若该选项为`true`，在所有地址均被熔断后，当有任意地址应该被恢复时，同时启用所有地址，默认值为`true`。

- bool try_another_addr

    若该选项为`true`，在`select_address`时将尽量避免选则到与`history`最后一个相同的地址，具体策略可选择实现为尽量避免选择到`history`中出现的所有地址，默认值为`true`。

- uint32_t min_available_percent

    当可用的地址权重占所有地址权重的比例小于该百分比时，部分`select_address`请求会直接返回`nullptr`，该值应介于`[0, 100]`，默认值为`0`。

- uint32_t max_fail_marks

    当`AddressInfo`的失败标记达到该值时，该地址将被熔断，默认值为`100`。

- uint32_t max_fail_ms

    当`AddressInfo`连续失败指定的时间后，该地址将被熔断，默认值为`10'000`毫秒。

- uint32_t success_dec_marks

    当`AddressInfo`使用成功时，其失败标记会被减去该值，默认值为`1`。

- uint32_t fail_inc_marks

    当`AddressInfo`使用失败时，其失败标记会被增加该值，默认值为`1`。

- uint32_t break_timeout_ms

    当`AddressInfo`被熔断后，将在该时间后将其恢复为可用状态，其他选项可能将恢复时间提前，默认值为`60'000`毫秒。


## coke::NSPolicy

`coke::NSPolicy`是名称服务策略的抽象基类，它提供了一组接口，用于与具体的策略进行交互。`NSPolicy`继承自`WFNSPolicy`，可以顺利应用于`Workflow`的名称服务体系。

该类有如下接口

- 获取地址的数量

    ```cpp
    // 获取所有地址的数量
    std::size_t address_count() const;

    // 获取当前可用地址的数量
    std::size_t available_address_count() const;
    ```

- 检查地址是否存在

    ```cpp
    bool has_address(const std::string &host, const std::string &port) const;
    ```

- 获取地址信息

    `get_address`返回与`(host, port)`关联的地址信息，若地址不存在则返回`nullptr`，若地址存在，应在使用完毕后通过`AddressInfo::dec_ref`接口释放。

    `get_all_address`返回所有地址的信息。

    ```cpp
    AddressInfo *get_address(const std::string &host,
                             const std::string &port) const;

    std::vector<AddressPack> get_all_address() const;
    ```

- 向名称服务策略添加地址

    `add_address`向名称服务策略添加地址`(host, port)`，返回该地址是否添加成功。若`replace`为`false`且该地址已经存在，则不会覆盖已有地址信息。

    `add_addresses`为批量添加地址的接口，默认实现为逐个调用`add_address`，具体策略实现可选择更高效的方式。

    ```cpp
    bool add_address(const std::string &host, const std::string &port,
                     const AddressParams &params, bool replace = false);

    std::vector<bool>
    add_addresses(const std::vector<AddressPack> &addrs, bool replace = false);
    ```

- 从名称服务策略移除地址

    `remove_address`从名称服务策略移除地址`(host, port)`，返回移除操作前该地址是否存在。

    `remove_addresses`为批量移除的接口，默认实现为逐个调用`remove_address`，具体策略实现可选择更高效的方式。

    ```cpp
    bool remove_address(const std::string &host, const std::string &port);

    std::vector<bool> remove_addresses(const std::vector<HostPortPack> &addrs);

    std::vector<bool> remove_addresses(const std::vector<AddressPack> &addrs);
    ```

- 熔断、恢复地址

    通常情况下通过名称服务策略的参数指定熔断策略，若需要主动介入，可使用这些函数。

    `break_address`用于将地址设置为熔断状态`ADDR_STATE_DISABLED`，返回该地址是否存在。

    `recover_address`用于将地址设置为正常状态`ADDR_STATE_GOOD`，返回该地址是否存在。

    ```cpp
    bool break_address(const std::string &host, const std::string &port);

    bool recover_address(const std::string &host, const std::string &port);
    ```

- 选择一个地址

    通常情况下将`NSPolicy`的实例注册到`WFNameService`上使用即可，不需要手动选取地址。

    `select_address`用来选择一个地址发起网络请求，部分策略可能需要依赖网络请求的`uri`用于地址选取，`history`为同一个请求之前尝试过但请求失败的地址列表。选取失败时返回`nullptr`，若选取成功，在使用完成后必须通过调用`addr_success`、`addr_failed`或`addr_finish`中的一个将地址的状态反馈给`NSPolicy`，在此之后再通过`AddressInfo::dec_ref`释放地址。

    ```cpp
    AddressInfo *select_address(const ParsedURI &uri, const SelectHistory &history);
    ```

- 反馈地址状态

    通过`select_address`选取成功的地址必须进行请求状态反馈。若请求成功则调用`addr_success`，若请求失败则调用`addr_failed`，若不希望本次的状态影响熔断策略，则调用`addr_finished`进行反馈。

    ```cpp
    void addr_success(AddressInfo *addr);

    void addr_failed(AddressInfo *addr);

    void addr_finish(AddressInfo *addr);
    ```


## 示例
以`coke::WeightedRandomPolicy`为例，将名称服务策略注册到`WFNameService`中，通过`WFNetworkTask`发起的请求会自动使用对应的策略。

```cpp
#include "coke/wait.h"
#include "coke/nspolicy/weighted_random_policy.h"
#include "workflow/WFGlobal.h"

coke::Task<> use_policy() {
    // 使用my.host作为域名发起请求，此处省略
    co_return;
}

int main() {
    coke::NSPolicyParams policy_params;
    coke::WeightedRandomPolicy wr(policy_params);
    coke::AddressParams addr_params;

    // 向策略中添加地址
    addr_params.weight = 100;
    wr.add_address("127.0.0.1", "8080", addr_params);

    addr_params.weight = 200;
    wr.add_address("127.0.0.1", "8081", addr_params);

    // 将策略添加到名称服务中
    WFNameService *ns = WFGlobal::get_name_service();
    ns->add_policy("my.host", &wr);

    // 使用策略完成一系列请求
    coke::sync_wait(use_policy());

    // 从名称服务中删除策略，确保此时已经没有请求在使用该策略
    ns->del_policy("my.host");

    return 0;
}
```

