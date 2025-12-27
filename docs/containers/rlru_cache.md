使用下述功能需要包含头文件`coke/rlru_cache.h`。

## coke::RlruCache

`RlruCache`是一个类似`RlruCache`的并发安全的缓存容器，用于大量读取且极少写入的场景。该容器在需要淘汰一条数据时，不会从全局中进行选择，而是随机选择一些数据并淘汰其中上次使用时刻最久远的那条。

```cpp
template<typename K, typename V, typename H = std::hash<K>,
         typename E = std::equal_to<>>
class RlruCache;
```

`RlruCache`使用哈希表管理数据，默认使用`std::hash<K>`计算key的哈希值，使用`std::equal_to<void>`比较两个key是否等价。

### 成员类型

```cpp
using Key = K;
using Value = V;
using Hash = H;
using Equal = E;
using Entry = detail::RlruEntry<K, V>;
using Handle = detail::RlruHandle<K, V>;
using SizeType = std::size_t;
```

### 成员函数

- 构造函数

    创建指定容量的`RlruCache`。若`max_size`为0则转换为`SizeType(-1)`。

    当需要淘汰数据时，从至多`max_scan`个数据中进行选择。

    ```cpp
    explicit RlruCache(SizeType max_size = 0, SizeType max_scan = 5,
                       const Hash &hash = Hash(), const Equal &equal = Equal());
    ```

    `RlruCache`不可移动、不可复制。

    ```cpp
    RlruCache(RlruCache &&) = delete;
    RlruCache &operator=(RlruCache &&) = delete;

    RlruCache(const RlruCache &) = delete;
    RlruCache &operator=(const RlruCache &) = delete;
    ```

- 析构函数

    销毁`RlruCache`，应保证所有通过`get_or_create`返回的`handle`都不再处于等待状态。

    ```cpp
    ~RlruCache();
    ```

- 容量查询

    ```cpp
    // 获取当前缓存的条目数
    SizeType size() const;

    // 获取构造时传入的缓存容量
    SizeType capacity() const;
    ```

- 获取条目

    通过键查找条目，返回空`handle`表示未命中。命中时会自动将key升热。

    ```cpp
    template<typename U>
        requires HashEqualComparable<Hash, Equal, Key, U>
    Handle get(const U &key);
    ```

- 获取或创建条目

    返回`(handle, created)`对。当`created`为`true`时，调用者应负责填充值并唤醒等待者。

    ```cpp
    template<typename U>
        requires (HashEqualComparable<Hash, Equal, Key, U> &&
                  std::constructible_from<Key, const U &>)
    [[nodiscard]]
    std::pair<Handle, bool> get_or_create(const U &key);
    ```

- 插入条目

    直接插入条目，使用`args...`构造`value`，并返回对应`handle`。若key存在则会被覆盖，此前已经获取的与旧值关联的`handle`仍能访问旧的值。

    ```cpp
    template<typename U, typename... Args>
        requires (HashEqualComparable<Hash, Equal, Key, const U &> &&
                  std::constructible_from<Key, const U &> &&
                  std::constructible_from<Value, Args && ...>)
    Handle put(const U &key, Args &&...args);
    ```

- 删除条目

    支持通过键或`handle`删除。条目会立即被移除，但此前已经获取的`handle`仍可使用。

    ```cpp
    template<typename U>
        requires HashEqualComparable<Hash, Equal, Key, const U &>
    void remove(const U &key);

    void remove(const Handle &handle);
    ```

- 清空缓存

    ```cpp
    void clear();
    ```


## coke::detail::RlruHandle

- 构造、赋值、析构函数

    ```cpp
    // 创建一个空RlruHandle，不关联任何条目。
    RlruHandle() noexcept;

    // 复制构造，创建一个新的RlruHandle，关联同一个条目。
    RlruHandle(const RlruHandle &) noexcept;

    // 移动构造
    RlruHandle(RlruHandle &&) noexcept;

    // 复制赋值，关联同一个条目。
    RlruHandle &operator=(const RlruHandle &) noexcept;

    // 移动赋值
    RlruHandle &operator=(RlruHandle &&) noexcept;

    // 析构函数，自动释放关联的条目。
    ~RlruHandle();
    ```

- 状态检查

    ```cpp
    // 是否关联条目
    operator bool() const noexcept;

    // 是否正在等待值
    bool waiting() const noexcept;

    // 是否成功设置值
    bool success() const noexcept;

    // 是否标记为失败
    bool failed() const noexcept;
    ```

- 值操作

    填充值或标记失败状态后，均需要调用`notify_one`/`notify_all`唤醒等待者。

    ```cpp
    // 原地构造值
    template<typename... Args>
    void emplace_value(Args &&...args)

    // 通过回调函数设置值
    template<typename ValueCreater>
    void create_value(ValueCreater &&creater)

    // 标记为失败状态
    void set_failed();
    ```

- 异步等待

    协程返回一个`int`类型整数，有以下几种情况

    - `coke::TOP_SUCCESS`表示成功取出
    - `coke::TOP_TIMEOUT`表示等待超时，`wait_for`可能返回该值
    - `coke::TOP_ABORTED`表示进程正在退出
    - `coke::TOP_CLOSED`表示容器为空且已被关闭
    - 负数表示发生系统错误，该情况几乎不会发生

    ```cpp
    // 等待至状态变化
    Task<int> wait();

    // 带超时的等待
    Task<int> wait_for(coke::NanoSec nsec);
    ```

- 唤醒机制

    ```cpp
    // 唤醒单个等待协程
    void notify_one();

    // 唤醒所有等待协程
    void notify_all();
    ```

- 获取值

    ```cpp
    // 获取key
    const Key &key() const noexcept;

    // 获取value，要求此时处于成功状态
    const Value &value() const noexcept;

    // 获取可修改的value，要求此时处于成功状态。使用者保证后续对value的修改是线程安全的。
    Value &mutable_value() noexcept;
    ```

## 示例

该容器使用方式与`LruCache`一致，可参考`example/ex018-lru_cache.cpp`。
