`coke::NSPolicy`描述了实现名称服务策略需要的接口，可通过实现这些接口定制自己的名称服务策略，`coke`也提供了几个常见的策略。


## coke::WeightedPolicyBase
使用该功能需要包含头文件`coke/nspolicy/weighted_policy_base.h`。

类模板`coke::WeightedPolicyBase`为带权重的名称服务策略实现了公共部分，基于此可以更简洁且高效地实现一个支持权重的名称服务策略，要求子类实现以下类型特征和成员函数。

- 实现类型特征`coke::NSPolicyTrait`

    以`coke::WeightedRandomPolicy`为例介绍具体含义

    ```cpp
    template<>
    struct NSPolicyTrait<WeightedRandomPolicy> {
        // Policy 为名称服务策略的类型名称
        using Policy   = WeightedRandomPolicy;
        // 策略可自定义地址信息的类型，需继承自`coke::AddressInfo`
        using AddrInfo = WeightedRandomAddressInfo;

        // 名称服务策略使用的锁类型
        using PolicyMutex      = std::shared_mutex;
        // 指定更新策略时如何操作锁
        using PolicyModifyLock = std::unique_lock<PolicyMutex>;
        // 指定选择地址时如何操作锁，例如带权随机策略选择地址只需要加共享锁即可
        using PolicySelectLock = std::shared_lock<PolicyMutex>;

        // 指示该策略可通过PolicySelectLock更高效地进行地址选择
        using EfficientSelectTag = void;
        // 指示请求成功且地址状态为ADDR_STATE_GOOD时，该策略无需处理地址状态反馈
        using FastSuccessTag     = void;
        // 指示该策略无需处理addr_finish状态的反馈
        using NoNeedFinishTag    = void;
    };
    ```

- 实现下述成员函数

    ```cpp
    // 添加地址
    void add_to_policy(AddrInfo *addr);

    // 移除地址
    void remove_from_policy(AddrInfo *addr);

    // 选择地址
    AddrInfo *select_addr(const ParsedURI &uri, const SelectHistory &history);

    // 返回未被熔断的地址数量
    std::size_t policy_set_size() const;

    // 处理地址使用完毕的反馈
    void handle_finish(AddrInfo *);
    ```


## coke::WeightedLeastConnPolicy
使用该功能需要包含头文件`coke/nspolicy/weighted_least_conn_policy.h`。

`coke::WeightedLeastConnPolicy`实现了带权重的最小连接数策略，最小连接数策略是一种动态的负载均衡算法，用于将新请求分配给当前​​活跃连接数最少​​的服务器，避免某些服务器过载。

最小连接数策略适用于客户端到每个服务器的延迟相差不多，但每个请求的处理耗时差异较大的场景。如果某个服务器有较多的连接正在占用，可能正在处理非常耗时的请求，也可能该服务器达到了性能瓶颈导致请求堆积，此时该策略可以避免将新请求发送到这个服务器上。若客户端到服务器的延迟差异较大，该策略不能很好地估计服务器的负载情况，可以通过调整权重让请求更加均匀。

该策略采用红黑树维护，地址选取和反馈地址状态的复杂度均为`O(log n)`，其中`n`为当前未被熔断的地址数量。


## coke::WeightedRandomPolicy
使用该功能需要包含头文件`coke/nspolicy/weighted_random_policy.h`。

`coke::WeightedRandomPolicy`实现了带权重的随机选择策略，该策略不考虑服务器的负载情况，仅依据配置的权重随机地选择地址，权重越高的服务器被选中的概率越大。

随机选择策略适用于每个请求对服务器的资源消耗差异不大，且客户端对地址选择效率要求很高的场景。由于地址选择完全依赖随机数，当服务器数量较少时，有一定的概率连续地选中同一台服务器，导致其短时间负载较高；若不同请求间资源消耗较大，有概率将资源消耗大的请求发送到同一台服务器上。

该策略采用树状数组维护，地址选取的复杂度为`O(log n)`，反馈地址状态为失败且触发熔断时的复杂度为`O(log n)`，其他情况下复杂度为`O(1)`，其中`n`为该策略正在维护的地址总数。由于地址选择仅依赖随机数，地址选择仅需要加共享锁即可，多线程场景下更加高效。


## coke::WeightedRoundRobinPolicy
使用该功能需要包含头文件`coke/nspolicy/weighted_round_robin_policy.h`。

`coke::WeightedRoundRobinPolicy`实现了带权重的轮询策略，该策略不考虑服务器的负载情况，将所有地址排列成固定的顺序并依次选择，权重越高的服务器在每轮中出现的频率越高。

带权轮询策略适用于每个请求对服务器的资源消耗差异不大，且希望减少随机数影响的场景。在地址不发生变化的情况下，该策略选取出的地址顺序具有周期性，是可预测的结果，不会发生请求分布不均的情况。

该策略采用红黑树维护，地址选取的复杂度为`O(log n)`，反馈地址状态为失败且触发熔断时的复杂度为`O(log n)`，其他情况下复杂度为`O(1)`，其中`n`为当前未被熔断的地址数量。由于需要维护周期性的状态，地址选择需要加互斥锁，理论上效率低于带权随机策略。名称服务策略需要支持随时增加和减少地址（包括熔断和恢复），为了保证该步骤的复杂度不高于`O(log n)`，新增的地址在首个周期中出现的位置是由随机数确定的。
