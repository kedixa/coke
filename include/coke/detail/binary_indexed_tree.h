/**
 * Copyright 2025 Coke Project (https://github.com/kedixa/coke)
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

#ifndef COKE_DETAIL_BINARY_INDEXED_TREE_H
#define COKE_DETAIL_BINARY_INDEXED_TREE_H

#include <concepts>
#include <cstdint>
#include <vector>

namespace coke::detail {

template<std::integral T>
class BinaryIndexedTree {
    using size_type = std::size_t;

    constexpr static T ZERO_OF_T{0};

    static size_type lowbit(size_type x) { return x & -x; }

public:
    BinaryIndexedTree() : tree_size(0), capacity(4), tree(5, ZERO_OF_T) {}

    ~BinaryIndexedTree() = default;

    size_type add_element(T x)
    {
        if (tree_size == capacity)
            expand();

        ++tree_size;
        increase(tree_size, x);
        return tree_size;
    }

    size_type size() const { return tree_size; }

    void remove_last_element()
    {
        T x = prefix_sum(tree_size) - prefix_sum(tree_size - 1);
        decrease(tree_size, x);
        --tree_size;
    }

    void increase(size_type pos, T x)
    {
        while (pos <= capacity) {
            tree[pos] += x;
            pos += lowbit(pos);
        }
    }

    void decrease(size_type pos, T x)
    {
        while (pos <= capacity) {
            tree[pos] -= x;
            pos += lowbit(pos);
        }
    }

    T prefix_sum(size_type pos) const
    {
        T sum = ZERO_OF_T;

        while (pos > 0) {
            sum += tree[pos];
            pos -= lowbit(pos);
        }

        return sum;
    }

    size_type find_pos(T x) const
    {
        size_type pos = 0;
        size_type p   = capacity >> 1;
        T cur         = ZERO_OF_T;

        while (p > 0) {
            if (cur + tree[pos + p] <= x) {
                pos += p;
                cur += tree[pos];
            }

            p >>= 1;
        }

        return pos + 1;
    }

    void shrink()
    {
        if (capacity >= 8 && tree_size <= capacity / 4) {
            capacity /= 2;
            tree.resize(capacity + 1);
        }
    }

private:
    void expand()
    {
        size_type new_cap = capacity * 2;

        tree.resize(new_cap + 1, ZERO_OF_T);
        tree[new_cap] = tree[capacity];
        capacity      = new_cap;
    }

private:
    size_type tree_size;
    size_type capacity;
    std::vector<T> tree;
};

} // namespace coke::detail

#endif // COKE_DETAIL_BINARY_INDEXED_TREE_H
