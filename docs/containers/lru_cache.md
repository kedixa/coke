使用下述功能需要包含头文件`coke/lru_cache.h`。

## coke::LruCache
`coke::LruCache`是一个基于LRU淘汰策略的并发安全的缓存容器，支持异步等待缓存就绪。适用于需要缓存热点数据且需要协调多个协程访问同一资源的场景。

```cpp
template<typename Key, typename Value, typename Compare = std::less<>>
class LruCache;
```

`LruCache::Handle`为`LruHandle<Key, Value>`的别名。

### 成员函数

- 构造函数

    创建指定容量和比较器的`coke::LruCache`。默认使用`std::less<>`比较键。当`max_size`为0时，表示容量无限制。

    `coke::LruCache`不可移动、不可复制。

    ```cpp
    explicit LruCache(SizeType max_size);
    LruCache(SizeType max_size, const Compare &cmp);
    ```

- 析构函数

    销毁`LruCache`，应保证所有通过`get_or_create`返回的句柄都不再处于等待状态。

    ```cpp
    ~LruCache();
    ```

- 容量查询

    ```cpp
    SizeType size() const;    // 当前缓存条目数
    SizeType capacity() const; // 最大容量
    ```

- 获取条目

    通过键查找条目，返回空句柄表示未命中。命中时会自动将key升热。

    ```cpp
    template<typename U>
    Handle get(const U &key);
    ```

- 获取或创建条目

    返回`(handle, created)`对。当`created`为`true`时，调用者应负责填充值并唤醒等待者。

    ```cpp
    template<typename U>
    std::pair<Handle, bool> get_or_create(const U &key);
    ```

- 插入条目

    直接插入条目，使用`args...`构造`value`，并返回对应句柄。

    ```cpp
    template<typename U, typename... Args>
    Handle put(const U &key, Args &&... args);
    ```

- 删除条目

    支持通过键或句柄删除。条目会立即被移除，但已有句柄仍可访问已存在的值。

    ```cpp
    template<typename U>
    void remove(const U &key);

    void remove(const Handle &handle);
    ```

- 清空缓存

    ```cpp
    void clear();
    ```


## coke::LruHandle

- 构造/赋值/析构函数


    ```cpp
    // 创建一个空句柄，不关联任何条目。
    LruHandle();

    // 复制构造，创建一个新的句柄，关联同一个条目。
    LruHandle(const LruHandle &);

    // 移动构造
    LruHandle(LruHandle &&);

    // 复制赋值，关联同一个条目。
    LruHandle &operator=(const LruHandle &);

    // 移动赋值
    LruHandle &operator=(LruHandle &&);

    // 析构函数，自动释放关联的条目。
    ~LruHandle();
    ```

- 状态检查

    ```cpp
    // 是否关联条目
    operator bool() const;

    // 是否正在等待值
    bool waiting() const;

    // 是否成功设置值
    bool success() const;

    // 是否标记为失败
    bool failed() const;
    ```

- 值操作

    填充值或标记失败状态后，均需要调用`notify_one`/`notify_all`唤醒等待者。

    ```cpp
    // 原地构造值
    void emplace_value(Args&&... args);

    // 通过回调函数设置值
    void create_value(ValueCreater &&cb);

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
    coke::Task<int> wait();

    // 带超时的等待
    coke::Task<int> wait_for(coke::NanoSec nsec);
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
    const Key &key() const;

    // 获取value，要求此时处于成功状态
    const Value &value() const;

    // 获取可修改的value，要求此时处于成功状态，使用者保证修改是线程安全的
    Value &mutable_value();
    ```

## 示例

参考`example/ex018-lru_cache.cpp`。
