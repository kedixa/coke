使用下述功能需要包含头文件`coke/dag.h`。


## 类型别名
该别名位于`coke`命名空间，用于表示一组`Dag`节点。

```cpp
template<typename T>
using DagNodeGroup = std::initializer_list<DagNodeRef<T>>;

template<typename T>
using DagNodeVector = std::vector<DagNodeRef<T>>;
```


## coke::DagGraph
`DagGraph`是由协程构成的有向无环图`DAG`，用于定义节点和管理节点之间的依赖关系，仅当某个节点的前置依赖满足时才会开始执行。

除了有向无环图的概念外，`DagGraph`引入了强边和弱边的概念。其中强边用于表达强前置依赖，要求必须满足；弱边用于表达弱前置依赖，若某节点有多个弱前置依赖，其中任何一个满足时认为所有弱前置依赖均已经满足。

```cpp
template<typename T>
class DagGraph;
```

### 成员函数
- 构造函数/析构函数

    该类不可直接构造，需要借助`DagBuilder`创建，不可复制，不可移动。

    ```cpp
    DagGraph(const DagGraph &) = delete;

    ~DagGraph();
    ```

- 运行`DAG`

    使用给定的数据`data`运行`DAG`。同一个`DagGraph`实例可以在多个协程中并发调用`DagGraph::run`，若用于创建节点的协程未使用全局状态，则同一个`DagGraph`多次运行也会相互独立，`data`用于在每次运行时向协程传递数据以及在协程间传递数据，使用者应保证对数据操作的安全性。

    ```cpp
    coke::Task<> run(T &data);
    ```

- 运行`DAG`，`void`类型特化

    ```cpp
    coke::Task<> run();
    ```

- 有效性检查

    检查当前`DAG`是否有效。一个有效的`DagGraph`要保证根节点没有任何前置依赖，图中没有环，且所有节点均从根节点可达。

    ```cpp
    bool valid() const;
    ```

- 打印图的结构

    将当前`DAG`以`DOT`语言格式输出到指定的输出流。

    ```cpp
    void dump(std::ostream &os) const;
    ```


## coke::DagBuilder
`DagBuilder`类用于构建`DagGraph`，提供了创建和连接节点的功能。

```cpp
template<typename T>
class DagBuilder;
```

### 成员函数
- 构造函数

    创建一个新的`DAG`构建器。

    ```cpp
    DagBuilder();
    ```

- 获取根节点的引用

    ```cpp
    coke::DagNodeRef<T> root();
    ```

- 创建新节点

    使用协程函数创建一个新的节点，并返回其引用。其中节点名称`name`仅用于输出图结构，默认为其节点编号。

    ```cpp
    template<typename FUNC>
        requires std::constructible_from<coke::dag_node_func_t<T>, FUNC&&>
    coke::DagNodeRef<T> node(FUNC &&func, const std::string &name = "");
    ```

- 连接节点

    将`l`设置为`r`的前置依赖，返回`r`。

    ```cpp
    coke::DagNodeRef<T> connect(coke::DagNodeRef<T> l, coke::DagNodeRef<T> r) const;

    coke::DagNodeRef<T> weak_connect(coke::DagNodeRef<T> l, coke::DagNodeRef<T> r) const;
    ```

- 构建`DAG`

    构建`DAG`并返回一个共享指针，指向构建的`DagGraph`。

    ```cpp
    std::shared_ptr<coke::DagGraph<T>> build();
    ```


## coke::DagNodeRef
`DagNodeRef`提供对`DagGraph`节点的引用，用于在节点之间创建边。

```cpp
template<typename T>
class DagBuilder;
```

### 成员函数
- 复制构造和赋值

    支持复制构造和复制赋值。不可直接构造，必须从`DagBuilder`创建。

    ```cpp
    DagNodeRef(const DagNodeRef &) = default;

    DagNodeRef &operator= (const DagNodeRef &) = default;
    ```

- 连接节点

    将当前节点设为另一个节点的前置依赖。若参数为协程，则使用协程函数创建新节点。返回另一个节点。

    ```cpp
    template<typename FUNC>
        requires std::constructible_from<coke::dag_node_func_t<T>, FUNC&&>
    coke::DagNodeRef then(FUNC &&func) const;

    template<typename FUNC>
        requires std::constructible_from<coke::dag_node_func_t<T>, FUNC&&>
    coke::DagNodeRef weak_then(FUNC &&func) const;

    coke::DagNodeRef then(coke::DagNodeRef r) const;

    coke::DagNodeRef weak_then(coke::DagNodeRef r) const;
    ```

- 批量连接

    将当前节点设置为这一组节点的前置依赖，返回这个节点组的引用。

    ```cpp
    const coke::NodeGroup &then(const coke::NodeGroup &group);
    const coke::NodeGroup &weak_then(const coke::NodeGroup &group);

    const coke::NodeVector &then(const coke::NodeVector &vec);
    const coke::NodeVector &weak_then(const coke::NodeVector &vec);
    ```


## 操作符重载
提供强连接 (`>`) 和弱连接 (`>=`) 的操作符重载，简化节点间的连接语法。

```cpp
template<typename T>
coke::DagNodeRef<T> operator> (coke::DagNodeRef<T> l, coke::DagNodeRef<T> r);

template<typename T>
const coke::DagNodeGroup<T> &operator> (coke::DagNodeRef<T> l, const coke::DagNodeGroup<T> &r);

template<typename T>
const coke::DagNodeVector<T> &operator> (coke::DagNodeRef<T> l, const coke::DagNodeVector<T> &r);

template<typename T>
coke::DagNodeRef<T> operator> (const coke::DagNodeGroup<T> &l, coke::DagNodeRef<T> r);

template<typename T>
coke::DagNodeRef<T> operator> (const coke::DagNodeVector<T> &l, coke::DagNodeRef<T> r);
```

```cpp
template<typename T>
coke::DagNodeRef<T> operator>=(coke::DagNodeRef<T> l, coke::DagNodeRef<T> r);

template<typename T>
const coke::DagNodeGroup<T> &operator>=(coke::DagNodeRef<T> l, const coke::DagNodeGroup<T> &r);

template<typename T>
const coke::DagNodeVector<T> &operator>=(coke::DagNodeRef<T> l, const coke::DagNodeVector<T> &r);

template<typename T>
coke::DagNodeRef<T> operator>=(const coke::DagNodeGroup<T> &l, coke::DagNodeRef<T> r);

template<typename T>
coke::DagNodeRef<T> operator>=(const coke::DagNodeVector<T> &l, coke::DagNodeRef<T> r);
```

## 示例
参考`example/ex020-dag.cpp`。
