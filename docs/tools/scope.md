使用下述功能需要包含头文件`coke/tools/scope.h`。


## coke::ScopeExit

`coke::ScopeExit`提供一种简单但通用功能，它将在退出作用域时执行用户指定的函数，也可通过调用其`release`成员函数放弃执行。

```cpp
template<typename F>
    requires (std::is_nothrow_invocable_r_v<void, F>
              && std::is_nothrow_move_constructible_v<F>)
class ScopeExit;
```

### 成员函数

- 构造函数/析构函数

    只可通过指定函数或函数对象的方式构造。

    ```cpp
    explicit ScopeExit(F f);
    ScopeExit(const ScopeExit &) = delete;
    ~ScopeExit();
    ```

- 放弃执行

    在析构时不再执行构造时指定的函数。

    ```cpp
    void release() noexcept;
    ```

### 示例

参考`example/ex021-scope_guard.cpp`。
