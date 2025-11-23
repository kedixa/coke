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

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "coke/utils/hashtable.h"

namespace coke::detail {

// clang-format off

constexpr std::size_t prime_table[] = {
    2UL,          3UL,          5UL,          7UL,          11UL,
    13UL,         17UL,         23UL,         29UL,         37UL,
    43UL,         53UL,         61UL,         71UL,         83UL,
    97UL,         113UL,        131UL,        151UL,        179UL,
    211UL,        251UL,        293UL,        337UL,        389UL,
    449UL,        521UL,        601UL,        701UL,        809UL,
    937UL,        1087UL,       1259UL,       1451UL,       1669UL,
    1931UL,       2221UL,       2557UL,       2953UL,       3407UL,
    3919UL,       4507UL,       5189UL,       5981UL,       6883UL,
    7919UL,       9109UL,       10477UL,      12049UL,      13859UL,
    15959UL,      18353UL,      21107UL,      24281UL,      27941UL,
    32141UL,      36973UL,      42533UL,      48947UL,      56299UL,
    64747UL,      74471UL,      85643UL,      98491UL,      113279UL,
    130279UL,     149827UL,     172307UL,     198173UL,     227947UL,
    262147UL,     301471UL,     346699UL,     398711UL,     458531UL,
    527327UL,     606433UL,     697399UL,     802019UL,     922331UL,
    1060687UL,    1219793UL,    1402763UL,    1613179UL,    1855169UL,
    2133463UL,    2453483UL,    2821513UL,    3244741UL,    3731461UL,
    4291181UL,    4934863UL,    5675107UL,    6526379UL,    7505341UL,
    8631151UL,    9925837UL,    11414729UL,   13126957UL,   15096019UL,
    17360449UL,   19964533UL,   22959247UL,   26403187UL,   30363677UL,
    34918229UL,   40155977UL,   46179389UL,   53106299UL,   61072273UL,
    70233167UL,   80768159UL,   92883389UL,   106815949UL,  122838367UL,
    141264127UL,  162453793UL,  186821863UL,  214845143UL,  247071917UL,
    284132711UL,  326752619UL,  375765587UL,  432130427UL,  496950007UL,
    571492511UL,  657216407UL,  755798933UL,  869168803UL,  999544141UL,
    1149475763UL, 1321897151UL, 1520181739UL, 1748209003UL, 2010440381UL,
    2312006447UL, 2658807421UL, 3057628547UL, 3516272831UL, 4294967291UL,

#if (SIZE_MAX > 4294967295UL)
    5798205859UL,           7827577931UL,          10567230217UL,
    14265760793UL,          19258777081UL,         25999349099UL,
    35099121301UL,          47383813787UL,         63968148623UL,
    86357000651UL,          116581950893UL,        157385633719UL,
    212470605527UL,         286835317471UL,        387227678599UL,
    522757366121UL,         705722444291UL,        952725299819UL,
    1286179154779UL,        1736341858957UL,       2344061509597UL,
    3164483037967UL,        4272052101271UL,       5767270336757UL,
    7785814954633UL,        10510850188759UL,      14189647754827UL,
    19156024469027UL,       25860633033217UL,      34911854594849UL,
    47131003703059UL,       63626854999187UL,      85896254248997UL,
    115959943236161UL,      156545923368829UL,     211336996547939UL,
    285304945339759UL,      385161676208731UL,     519968262881789UL,
    701957154890429UL,      947642159102083UL,     1279316914787827UL,
    1727077834963571UL,     2331555077200829UL,    3147599354221219UL,
    4249259128198649UL,     5736499823068241UL,    7744274761142137UL,
    10454770927541911UL,    14113940752181591UL,   19053820015445177UL,
    25722657020851003UL,    34725586978148863UL,   46879542420500989UL,
    63287382267676417UL,    85437966061363183UL,   115341254182840333UL,
    155710693146834481UL,   210209435748226609UL,  283782738260105969UL,
    383106696651143117UL,   517194040479043211UL,  698211954646708391UL,
    942586138773056411UL,   1272491287343626243UL, 1717863237913895483UL,
    2319115371183758893UL,  3130805751098074631UL, 4226587763982401057UL,
    5705893481376241691UL,  7702956199857927193UL, 10398990869808202009UL,
    18446744073709551557UL,
#endif
};

// clang-format on

constexpr std::size_t PRIME_CNT = sizeof(prime_table) / sizeof(prime_table[0]);
constexpr std::size_t PRIME_MAX = prime_table[PRIME_CNT - 1];

static std::size_t find_prime(double factor, std::size_t cap)
{
    double n = std::ceil(cap / factor);
    if (n >= PRIME_MAX)
        return PRIME_MAX;

    const std::size_t *begin = prime_table;
    const std::size_t *end = begin + PRIME_CNT;
    const std::size_t target = static_cast<std::size_t>(n);
    const std::size_t *p = std::lower_bound(begin, end, target);
    return (p == end ? PRIME_MAX : *p);
}

HashtableBase::HashtableBase() noexcept : max_factor(0.80), next_resize(0)
{
    head.next = &head;
    head.prev = &head;
    head.hash_code = 0;
    head.node_index = 0;
}

HashtableBase::HashtableBase(HashtableBase &&other) noexcept
    : head(other.head), table(std::move(other.table)),
      nodes(std::move(other.nodes)), max_factor(other.max_factor),
      next_resize(other.next_resize)
{
    other.reinit();
}

HashtableBase &HashtableBase::operator=(HashtableBase &&other) noexcept
{
    if (this != &other) {
        this->reinit();
        head = other.head;
        table = std::move(other.table);
        nodes = std::move(other.nodes);
        max_factor = other.max_factor;
        next_resize = other.next_resize;
        other.reinit();
    }

    return *this;
}

HashtableBase::size_type HashtableBase::bucket_size(size_type bkt_index) const
{
    size_type tbl_size = bucket_count();
    Node *node = table[bkt_index];
    const Node *node_end = get_end_node();
    size_type cnt = 0;

    while (node != node_end && node->hash_code % tbl_size == bkt_index) {
        ++cnt;
        node = node->next;
    }

    return cnt;
}

double HashtableBase::load_factor() const noexcept
{
    if (empty())
        return 0.0;
    else
        return static_cast<double>(size()) / bucket_count();
}

void HashtableBase::max_load_factor(double m) noexcept
{
    double n = std::floor(m * bucket_count());
    size_type k = static_cast<size_type>(n);

    if (k < size())
        next_resize = k;

    max_factor = m;
}

void HashtableBase::insert_at(Node *node, size_type bkt_index,
                              Node *pos) noexcept
{
    node->node_index = nodes.size();
    nodes.push_back(node);

    Node *prev = pos->prev;
    Node *next = pos;

    prev->next = node;
    next->prev = node;
    node->prev = prev;
    node->next = next;

    if (table[bkt_index] == pos)
        table[bkt_index] = node;
}

void HashtableBase::insert_front(Node *node, size_type bkt_index) noexcept
{
    node->node_index = nodes.size();
    nodes.push_back(node);

    Node *prev = table[bkt_index]->prev;
    Node *next = table[bkt_index];

    prev->next = node;
    next->prev = node;
    node->prev = prev;
    node->next = next;
    table[bkt_index] = node;
}

void HashtableBase::erase_node(Node *node) noexcept
{
    size_type tbl_size = table.size();
    size_type bkt_index = node->hash_code % tbl_size;
    Node *prev = node->prev;
    Node *next = node->next;

    if (table[bkt_index] == node) {
        Node *node_end = &head;
        if (next != node_end && next->hash_code % tbl_size == bkt_index)
            table[bkt_index] = node->next;
        else
            table[bkt_index] = node_end;
    }

    prev->next = next;
    next->prev = prev;

    Node *back = nodes.back();
    nodes.pop_back();

    if (back != node) {
        back->node_index = node->node_index;
        nodes[back->node_index] = back;
    }
}

const HashtableBase::Node *
HashtableBase::find_by_hash(size_type hash_code,
                            size_type bkt_index) const noexcept
{
    const Node *node = table[bkt_index];
    const Node *node_end = &head;
    size_type tbl_size = table.size();

    while (node != node_end && node->hash_code % tbl_size == bkt_index) {
        if (node->hash_code == hash_code)
            return node;

        node = node->next;
    }

    return node_end;
}

void HashtableBase::expand(size_type cnt)
{
    size_type new_cap = nodes.size();

    if (new_cap == 0)
        new_cap = 3;

    if (new_cap < cnt)
        new_cap += cnt;
    else
        new_cap *= 2;

    if (new_cap > next_resize)
        reserve_impl(new_cap);
}

void HashtableBase::reserve_impl(size_type cap)
{
    size_type bkt_count = find_prime(max_factor, cap);
    double new_cap = std::floor(bkt_count * max_factor);

    if (new_cap > cap) {
        if (new_cap <= size_type(-1))
            cap = static_cast<size_type>(new_cap);
        else
            cap = size_type(-1);
    }

    if (nodes.capacity() < cap)
        nodes.reserve(cap);

    if (bkt_count <= table.size()) {
        next_resize = cap;
        return;
    }

    Node *node_end = &head;
    Node *node = nullptr;
    std::vector<Node *> tbl(bkt_count, node_end);

    // No exceptions after this line

    if (head.next != node_end) {
        node = head.next;
        head.prev->next = nullptr;
        head.next = head.prev = node_end;
    }

    size_type bkt_index;
    Node *prev, *next, *tmp;

    while (node) {
        tmp = node->next;

        bkt_index = node->hash_code % bkt_count;
        prev = tbl[bkt_index]->prev;
        next = tbl[bkt_index];

        prev->next = node;
        next->prev = node;
        node->prev = prev;
        node->next = next;
        tbl[bkt_index] = node;

        node = tmp;
    }

    table.swap(tbl);
    next_resize = cap;
}

} // namespace coke::detail
