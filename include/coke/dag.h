/**
 * Copyright 2024 Coke Project (https://github.com/kedixa/coke)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Authors: kedixa (https://github.com/kedixa)
 */

#ifndef COKE_DAG_H
#define COKE_DAG_H

#include <atomic>
#include <functional>
#include <initializer_list>
#include <iosfwd>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "coke/global.h"
#include "coke/latch.h"
#include "coke/sleep.h"
#include "coke/task.h"

namespace coke {

using dag_index_t = uint32_t;

namespace detail {

template<typename T>
struct dag_node_func {
    using type = std::function<Task<>(T &)>;
};

template<>
struct dag_node_func<void> {
    using type = std::function<Task<>()>;
};

class DagCounter {
public:
    DagCounter() = default;
    DagCounter(const DagCounter &) = delete;

    void init(dag_index_t count, bool weak)
    {
        cnt.store(count + (weak == true), std::memory_order_relaxed);
        weak_flag.store(weak, std::memory_order_relaxed);
    }

    bool count(bool weak)
    {
        if (weak && !weak_flag.exchange(false, std::memory_order_acq_rel))
            return false;

        return cnt.fetch_sub(1, std::memory_order_acq_rel) == 1;
    }

private:
    std::atomic<dag_index_t> cnt;
    std::atomic<bool> weak_flag;
};

class DagContextBase {
public:
    DagContextBase(dag_index_t cnt) : lt(cnt), counts(new DagCounter[cnt]) {}

    DagContextBase(const DagContextBase &) = delete;
    ~DagContextBase() = default;

    auto wait() { return lt.wait(); }

    void count_down() { lt.count_down(); }

    DagCounter &operator[](std::size_t i) { return counts[i]; }

protected:
    Latch lt;
    std::unique_ptr<DagCounter[]> counts;
};

template<typename T>
class DagContext : public DagContextBase {
    using node_func_t = typename dag_node_func<T>::type;

public:
    DagContext(dag_index_t cnt, T &data) : DagContextBase(cnt), data(data) {}

    Task<> invoke(node_func_t &func) { return func(data); }

private:
    T &data;
};

template<>
class DagContext<void> : public DagContextBase {
    using node_func_t = typename dag_node_func<void>::type;

public:
    DagContext(dag_index_t cnt) : DagContextBase(cnt) {}

    Task<> invoke(node_func_t &func) { return func(); }
};

bool dag_check(const std::vector<std::vector<dag_index_t>> &outs,
               const std::vector<std::vector<dag_index_t>> &weak_outs,
               std::vector<dag_index_t> &counts, std::vector<bool> &weak_flags);

void dag_dump(std::ostream &os,
              const std::vector<std::vector<dag_index_t>> &outs,
              const std::vector<std::vector<dag_index_t>> &weak_outs,
              const std::vector<std::string> &names);

} // namespace detail

template<typename T>
using dag_node_func_t = typename detail::dag_node_func<T>::type;

template<typename T>
class DagBuilder;

template<typename T>
class DagGraphBase;

template<typename T>
class DagNodeRef;

template<typename T>
class DagGraphBase {
public:
    using node_func_t = dag_node_func_t<T>;

    DagGraphBase(const DagGraphBase &) = delete;
    ~DagGraphBase() = default;

    /**
     * @brief Get whether current `DagGraph` is valid or not.
     *
     * @return true if valid, else false.
     */
    bool valid() const { return is_valid; }

    /**
     * @brief Dump the current DAG to `os` in dot language.
     */
    void dump(std::ostream &os) const
    {
        detail::dag_dump(os, outs, weak_outs, names);
    }

protected:
    struct Node {
        template<typename FUNC>
        Node(FUNC &&func) : func(std::forward<FUNC>(func))
        {
        }

        node_func_t func;
    };

    using node_ptr_t = std::unique_ptr<Node>;

    DagGraphBase() : is_valid(false) { node(nullptr, "root"); }

    void start(detail::DagContext<T> &ctx)
    {
        std::size_t n = nodes.size();
        for (std::size_t i = 0; i < n; i++)
            ctx[i].init(counters[i], weak_flags[i]);

        invoke(ctx, 0).detach();
    }

    Task<> invoke(detail::DagContext<T> &ctx, dag_index_t id)
    {
        // TODO yield when recursive

        auto &func = nodes[id]->func;
        if (func)
            co_await ctx.invoke(func);

        for (auto next : outs[id])
            if (ctx[next].count(false))
                invoke(ctx, next).detach();

        for (auto next : weak_outs[id])
            if (ctx[next].count(true))
                invoke(ctx, next).detach();

        ctx.count_down();
    }

    template<typename FUNC>
    dag_index_t node(FUNC &&func, const std::string &name)
    {
        dag_index_t id = nodes.size();
        node_ptr_t ptr(new Node(std::forward<FUNC>(func)));
        nodes.push_back(std::move(ptr));
        outs.push_back({});
        weak_outs.push_back({});
        names.push_back(name);
        return id;
    }

    void build()
    {
        is_valid = detail::dag_check(outs, weak_outs, counters, weak_flags);
    }

protected:
    bool is_valid;
    std::vector<dag_index_t> counters;
    std::vector<bool> weak_flags;

    std::vector<node_ptr_t> nodes;
    std::vector<std::vector<dag_index_t>> outs;
    std::vector<std::vector<dag_index_t>> weak_outs;
    std::vector<std::string> names;

    friend DagBuilder<T>;
    friend DagNodeRef<T>;
};

template<typename T>
class DagGraph : public DagGraphBase<T> {
    using Base = DagGraphBase<T>;

public:
    /**
     * @brief DagGraph is not copy or move constructible.
     */
    DagGraph(const DagGraph &) = delete;

    ~DagGraph() = default;

    /**
     * @brief Run the DAG with `data`. DagGraph::run can be called concurrently
     *        in multiple coroutines, but the destruction of DagGraph should be
     *        after DagGraph::run ends.
     *
     * @pre Current DagGraph is valid.
     */
    Task<> run(T &data)
    {
        detail::DagContext<T> ctx(Base::nodes.size(), data);
        Base::start(ctx);
        co_await ctx.wait();
    }

private:
    /**
     * @brief DagGraph cannot be constructed directly and must be created
     *        through DagBuilder.
     */
    DagGraph() : Base() {}

    friend class DagBuilder<T>;
};

template<>
class DagGraph<void> : public DagGraphBase<void> {
    using Base = DagGraphBase<void>;

public:
    DagGraph(const DagGraph &) = delete;
    ~DagGraph() = default;

    Task<> run()
    {
        detail::DagContext<void> ctx(Base::nodes.size());
        Base::start(ctx);
        co_await ctx.wait();
    }

private:
    DagGraph() : Base() {}

    friend class DagBuilder<void>;
};

/**
 * DagNodeRef
 */
template<typename T>
class DagNodeRef {
public:
    DagNodeRef(const DagNodeRef &) = default;
    DagNodeRef &operator=(const DagNodeRef &) = default;
    ~DagNodeRef() = default;

    template<typename FUNC>
        requires std::constructible_from<dag_node_func_t<T>, FUNC &&>
    DagNodeRef then(FUNC &&func) const
    {
        auto r = builder->node(std::forward<FUNC>(func));
        return then(r);
    }

    template<typename FUNC>
        requires std::constructible_from<dag_node_func_t<T>, FUNC &&>
    DagNodeRef weak_then(FUNC &&func) const
    {
        auto r = builder->node(std::forward<FUNC>(func));
        return weak_then(r);
    }

    DagNodeRef then(DagNodeRef r) const { return builder->connect(*this, r); }

    DagNodeRef weak_then(DagNodeRef r) const
    {
        return builder->weak_connect(*this, r);
    }

    using NodeGroup = std::initializer_list<DagNodeRef<T>>;
    using NodeVector = std::vector<DagNodeRef<T>>;

    const NodeGroup &then(const NodeGroup &group) const
    {
        for (const auto &r : group)
            then(r);
        return group;
    }

    const NodeGroup &weak_then(const NodeGroup &group) const
    {
        for (const auto &r : group)
            weak_then(r);
        return group;
    }

    const NodeVector &then(const NodeVector &vec) const
    {
        for (const auto &r : vec)
            then(r);
        return vec;
    }

    const NodeVector &weak_then(const NodeVector &vec) const
    {
        for (const auto &r : vec)
            weak_then(r);
        return vec;
    }

private:
    DagNodeRef(DagBuilder<T> *builder, dag_index_t id)
        : builder(builder), id(id)
    {
    }

    DagBuilder<T> *builder;
    dag_index_t id;

    friend class DagBuilder<T>;
};

/**
 * DagBuilder is used to create DagGraph.
 */
template<typename T>
class DagBuilder {
public:
    DagBuilder() { new_dag(); }

    /**
     * @brief Get the DagNodeRef of root node.
     */
    DagNodeRef<T> root() { return DagNodeRef<T>(this, 0); }

    /**
     * @brief Create a new node.
     *
     * @param func Coroutine that runs on the node of DAG, `func` should be able
     *        to be used to construct dag_node_func_t<T>.
     * @param name The name of the created node, used in DagGraph<T>::dump.
     *
     * @return DagNodeRef of the created node.
     */
    template<typename FUNC>
        requires std::constructible_from<dag_node_func_t<T>, FUNC &&>
    DagNodeRef<T> node(FUNC &&func, const std::string &name = "")
    {
        dag_index_t id = graph->node(std::forward<FUNC>(func), name);
        return DagNodeRef(this, id);
    }

    /**
     * @brief Strongly connect l -> r.
     */
    DagNodeRef<T> connect(DagNodeRef<T> l, DagNodeRef<T> r) const
    {
        graph->outs[l.id].push_back(r.id);
        return r;
    }

    /**
     * @brief Weakly connect l -> r.
     */
    DagNodeRef<T> weak_connect(DagNodeRef<T> l, DagNodeRef<T> r) const
    {
        graph->weak_outs[l.id].push_back(r.id);
        return r;
    }

    /**
     * @brief Build the DagGraph.
     *
     * @return std::shared_ptr of DagGraph<T>, DagGraph cannot be copied or
     *         moved, but the shared_ptr can be copied to anywhere you want.
     *
     * @post DagBuilder and its DagGraph are no longer related in any way.
     *       Except for the destructor, any function of this DagBuilder and the
     *       DagNodeRef constructed from it can no longer be called.
     */
    std::shared_ptr<DagGraph<T>> build()
    {
        auto g = graph;
        graph.reset();
        g->build();
        return g;
    }

private:
    void new_dag() { graph.reset(new DagGraph<T>()); }

    std::shared_ptr<DagGraph<T>> graph;
};

/**
 * Dag Operators, `operator >` means strong connect, `operator >=` means weak
 * connect.
 */

template<typename T>
using DagNodeGroup = typename DagNodeRef<T>::NodeGroup;

template<typename T>
using DagNodeVector = typename DagNodeRef<T>::NodeVector;

template<typename T, typename U>
    requires requires(const DagNodeRef<T> &l, const U &r) { l.then(r); }
const U &operator>(const DagNodeRef<T> &l, const U &r)
{
    l.then(r);
    return r;
}

template<typename T>
decltype(auto) operator>(const DagNodeGroup<T> &l, const DagNodeRef<T> &r)
{
    for (const auto &x : l)
        x.then(r);
    return (r);
}

template<typename T>
decltype(auto) operator>(const DagNodeVector<T> &l, const DagNodeRef<T> &r)
{
    for (const auto &x : l)
        x.then(r);
    return (r);
}

template<typename T, typename U>
    requires requires(const DagNodeRef<T> &l, const U &r) { l.weak_then(r); }
const U &operator>=(const DagNodeRef<T> &l, const U &r)
{
    l.weak_then(r);
    return r;
}

template<typename T>
decltype(auto) operator>=(const DagNodeGroup<T> &l, const DagNodeRef<T> &r)
{
    for (const auto &x : l)
        x.weak_then(r);
    return (r);
}

template<typename T>
decltype(auto) operator>=(const DagNodeVector<T> &l, const DagNodeRef<T> &r)
{
    for (const auto &x : l)
        x.weak_then(r);
    return (r);
}

} // namespace coke

#endif // COKE_DAG_H
