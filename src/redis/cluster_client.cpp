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

#include <array>
#include <cstddef>
#include <cstring>
#include <set>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "coke/detail/random.h"
#include "coke/redis/client_task.h"
#include "coke/redis/cluster_client_impl.h"
#include "workflow/StringUtil.h"

namespace coke {

/**
 * CRC-16-XMODEM to calculate redis key's slot.
 *
 * Poly                       : 1021
 * Initialization             : 0000
 * Reflect Input byte         : False
 * Reflect Output CRC         : False
 * Xor constant to output CRC : 0000
 * Output for "123456789"     : 31C3
 */

constexpr static auto create_crc16_xmodem_table()
{
    std::array<uint16_t, 256> table;

    for (uint16_t i = 0; i < 256; i++) {
        uint16_t crc = i << 8;

        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }

        table[i] = crc;
    }

    return table;
}

constexpr std::array<uint16_t, 256> crc16tbl = create_crc16_xmodem_table();
constexpr int16_t REDIS_CLUSTER_SLOTS = 16384;

static uint16_t redis_crc16(const char *key, std::size_t len)
{
    const auto *p = (const unsigned char *)key;
    uint16_t crc = 0;

    for (std::size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ crc16tbl[(crc >> 8) ^ p[i]];
    }

    return crc;
}

static void append_bool(std::string &info, const char *key, bool value)
{
    info.append(key).append("=");
    info.append(value ? "true" : "false").append("&");
}

static void append_int(std::string &info, const char *key, int value)
{
    info.append(key).append("=");
    info.append(std::to_string(value)).append("&");
}

static void append_string(std::string &info, const char *key,
                          const std::string &value)
{
    info.append(key).append("=");
    info.append(StringUtil::url_encode(value)).append("&");
}

static bool slot_node_to_addr(RedisSlotNode &node)
{
    int port = std::atoi(node.port.c_str());
    if (port <= 0 || port >= 65536)
        return false;

    std::memset(&node.addr_storage, 0, sizeof(node.addr_storage));

    const char *host = node.host.c_str();
    auto *addr = (sockaddr_in *)&(node.addr_storage);
    auto *addr6 = (sockaddr_in6 *)&(node.addr_storage);

    if (inet_pton(AF_INET, host, &addr->sin_addr) == 1) {
        addr->sin_family = AF_INET;
        addr->sin_port = htons(port);
        node.addr_len = sizeof(sockaddr_in);
        return true;
    }

    if (inet_pton(AF_INET6, host, &addr6->sin6_addr) == 1) {
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(port);
        node.addr_len = sizeof(sockaddr_in6);
        return true;
    }

    return false;
}

static std::unique_ptr<RedisSlotNode>
parse_redis_redirect(const std::string &err)
{
    auto v = StringUtil::split_filter_empty(err, ' ');
    std::unique_ptr<RedisSlotNode> p;

    if (v.size() == 3) {
        std::size_t pos = v[2].find(':');
        if (pos != std::string::npos) {
            p = std::make_unique<RedisSlotNode>();
            p->host = v[2].substr(0, pos);
            p->port = v[2].substr(pos + 1);
        }
    }

    return p;
}

static int16_t get_redis_key_slot(std::string_view key)
{
    constexpr auto npos = std::string_view::npos;

    std::size_t begin = key.find('{'), end;

    if (begin != npos) {
        ++begin;
        end = key.find('}', begin);
        if (end != npos && end != begin)
            key = key.substr(begin, end - begin);
    }

    uint16_t crc = redis_crc16(key.data(), key.size());
    return (int16_t)(crc % (uint16_t)REDIS_CLUSTER_SLOTS);
}

void RedisClusterClientImpl::init_client()
{
    cli_info.pipe_handshake = params.pipe_handshake;
    cli_info.read_replica = params.read_replica;

    cli_info.protover = params.protover;
    cli_info.database = 0;
    cli_info.username = params.username;
    cli_info.password = params.password;
    cli_info.client_name = params.client_name;
    cli_info.lib_name = params.lib_name;
    cli_info.lib_ver = params.lib_ver;

    cli_info.no_evict = params.no_evict;
    cli_info.no_touch = params.no_touch;

    cli_info.enable_tracking = false;
    cli_info.tracking_bcast = false;
    cli_info.tracking_optin = false;
    cli_info.tracking_optout = false;
    cli_info.tracking_noloop = false;
    cli_info.redirect_client_id.clear();
    cli_info.tracking_prefixes.clear();

    init_node.host = params.host;
    init_node.port = params.port;

    std::string info("coke:redis?");

    append_int(info, "protover", params.protover);
    append_int(info, "database", 0);
    append_string(info, "username", params.username);
    append_string(info, "password", params.password);
    append_string(info, "client_name", params.client_name);
    append_string(info, "lib_name", params.lib_name);
    append_string(info, "lib_ver", params.lib_ver);
    append_bool(info, "no_evict", params.no_evict);
    append_bool(info, "no_touch", params.no_touch);

    // Tracking is not supported now.
    append_bool(info, "enable_tracking", false);

    info.pop_back();
    cli_info.conn_info = ClientConnInfo::create_instance(info, false);
}

Task<RedisResult> RedisClusterClientImpl::_execute(StrHolderVec command,
                                                   RedisExecuteOption opt)
{
    auto table_ptr = get_slots_table();
    if (!table_ptr || table_ptr->state != 0 || table_ptr->outdated) {
        uint64_t ver = (table_ptr ? table_ptr->version : 0);
        table_ptr = co_await update_slots_table(ver);
    }

    RedisResult result;

    if (table_ptr->state != 0) {
        result.set_state(table_ptr->state);
        result.set_error(table_ptr->error);
        co_return result;
    }

    int slot = opt.slot;
    if (slot == REDIS_AUTO_SLOT) {
        // Auto slot not supported now.
        result.set_state(WFT_STATE_TASK_ERROR);
        result.set_error(REDIS_ERR_INVALID_SLOT);
        co_return result;
    }

    if (slot < 0) {
        int key_pos = -slot;
        if (key_pos >= (int)command.size()) {
            result.set_state(WFT_STATE_TASK_ERROR);
            result.set_error(REDIS_ERR_INVALID_SLOT);
            co_return result;
        }

        slot = get_redis_key_slot(command[key_pos].as_view());
    }

    if (slot == REDIS_ANY_PRIMARY) {
        // Randomly choose a slot.
        slot = rand_u64() % (uint64_t)REDIS_CLUSTER_SLOTS;
    }
    else if (slot >= REDIS_CLUSTER_SLOTS) {
        // TODO handle other slots types.
        result.set_state(WFT_STATE_TASK_ERROR);
        result.set_error(REDIS_ERR_INVALID_SLOT);
        co_return result;
    }

    // Handle normal slot.

    int nodes_index = table_ptr->slot_index[slot];
    if (nodes_index < 0) {
        result.set_state(WFT_STATE_TASK_ERROR);
        result.set_error(REDIS_ERR_INCOMPLETE_SLOT);
        co_return result;
    }

    const RedisSlotNodes &nodes = table_ptr->nodes_vec[nodes_index];
    bool read_only = (opt.flags & REDIS_FLAG_READONLY);

    if (read_only && params.read_replica) {
        std::size_t replica_cnt = nodes.size();
        std::size_t try_from = choice_cnt++ % replica_cnt;

        for (int i = 0; i <= params.retry_max; i++) {
            const RedisSlotNode &node = nodes[(try_from + i) % replica_cnt];
            result = co_await execute_impl(table_ptr, node, 0, command, opt);
            if (result.get_state() != WFT_STATE_SYS_ERROR)
                break;
        }
    }
    else {
        result = co_await execute_impl(table_ptr, nodes[0], params.retry_max,
                                       command, opt);
    }

    co_return result;
}

Task<RedisResult> RedisClusterClientImpl::execute_impl(
    RedisSlotsTablePtr &table, const RedisSlotNode &node, int retry_max,
    const StrHolderVec &command, const RedisExecuteOption &opt)
{
    constexpr int max_redirect = 2;

    std::unique_ptr<RedisSlotNode> redirect_node;
    const RedisSlotNode *pnode = &node;
    TransportType type = (params.use_ssl ? TT_TCP_SSL : TT_TCP);
    bool is_moved = false;
    bool is_ask = false;
    int watch_timeout = 0;
    RedisResult result;

    if (opt.block_ms == 0)
        watch_timeout = params.default_watch_timeout;
    else if (opt.block_ms > 0)
        watch_timeout = opt.block_ms + params.watch_extra_timeout;

    for (int redirect = 0; redirect <= max_redirect; redirect++) {
        auto *task = new RedisClientTask(retry_max, nullptr);
        task->set_client_info(&cli_info);
        task->set_ssl_ctx(params.ssl_ctx.get());

        if (pnode->addr_len > 0) {
            const sockaddr *addr = (const sockaddr *)&(pnode->addr_storage);
            const std::string &info = cli_info.conn_info.get_short_info();
            task->init(type, addr, pnode->addr_len, info);
        }
        else {
            ParsedURI uri;
            uri.host = strdup(pnode->host.c_str());
            uri.port = strdup(pnode->port.c_str());

            if (uri.host && uri.port)
                uri.state = URI_STATE_SUCCESS;
            else {
                uri.state = URI_STATE_ERROR;
                uri.error = errno;
            }

            task->set_transport_type(type);
            task->init(std::move(uri));
        }

        task->set_send_timeout(params.send_timeout);
        task->set_receive_timeout(params.receive_timeout);
        task->set_keep_alive(params.keep_alive_timeout);
        task->set_watch_timeout(watch_timeout);

        RedisRequest *req = task->get_req();
        if (is_ask) {
            // TODO maybe fail if ask response returns before socket write
            // finish, this may happens when command is very large.
            req->add_command(make_shv("ASKING"));
            req->add_command_nocopy(command);
        }
        else {
            req->set_command_nocopy(command);
        }

        task->get_resp()->set_size_limit(params.response_size_limit);

        // task is deleted before the next co_await.
        co_await RedisAwaiter(task);

        int state = task->get_state();
        int error = task->get_error();

        // Request failed
        if (state != WFT_STATE_SUCCESS) {
            result.set_state(state);
            result.set_error(error);
            break;
        }

        RedisValue &value = task->get_resp()->get_value();

        // Request with ask has two responses, the second is the real one.
        if (is_ask) {
            RedisArray &arr = value.get_array();
            if (arr[0].is_error()) {
                state = WFT_STATE_TASK_ERROR;
                error = REDIS_ERR_INVALID_REDIRECT;
                result.set_value(std::move(arr[0]));
                break;
            }

            RedisValue tmp = std::move(arr[1]);
            value = std::move(tmp);
        }

        // Too many redirect or is not redirect, return the last result.
        if (redirect >= max_redirect || !value.is_simple_error()) {
            result.set_state(state);
            result.set_error(error);
            result.set_value(value);
            break;
        }

        // Check if the response is a redirect.
        std::string err = value.get_string();
        std::unique_ptr<RedisSlotNode> redirect_to;

        if (err.starts_with("MOVED ")) {
            is_moved = true;
            is_ask = false;
            redirect_to = parse_redis_redirect(err);
        }
        else if (err.starts_with("ASK ")) {
            is_ask = true;
            is_moved = false;
            redirect_to = parse_redis_redirect(err);
        }

        // Is a redirect, continue request with the new node.
        if (redirect_to) {
            if (redirect_to->host.empty())
                redirect_to->host = pnode->host;

            // If moved, the table is outdated.
            if (is_moved && table) {
                table->outdated = true;
            }

            // Set redirect_node at last, because pnode may point to it.
            redirect_node = std::move(redirect_to);
            pnode = redirect_node.get();
            continue;
        }

        // Not a redirect, return the result.
        result.set_state(state);
        result.set_error(error);
        result.set_value(value);
        break;
    }

    co_return result;
}

Task<RedisSlotsTablePtr>
RedisClusterClientImpl::update_slots_table(uint64_t old_version)
{
    UniqueLock<Mutex> lk(co_table_mtx);
    RedisSlotsTablePtr cur_table;

    co_await lk.lock();

    {
        std::lock_guard lg(table_mtx);
        if (slots_table && slots_table->version > old_version)
            co_return slots_table;

        cur_table = slots_table;
    }

    RedisSlotsTablePtr new_table;
    int retry_max = params.retry_max;

    if (!cur_table || cur_table->state != 0) {
        new_table = co_await update_table_impl(init_node, retry_max);
    }
    else {
        const auto &nodes_vec = cur_table->nodes_vec;
        if (nodes_vec.size() > 1)
            retry_max = 0;

        for (const auto &nodes : nodes_vec) {
            new_table = co_await update_table_impl(nodes[0], retry_max);
            if (new_table->state == 0)
                break;
        }
    }

    new_table->version = old_version + 1;

    {
        std::lock_guard lg(table_mtx);
        slots_table = new_table;
    }

    co_return new_table;
}

Task<RedisSlotsTablePtr>
RedisClusterClientImpl::update_table_impl(const RedisSlotNode &node,
                                          int retry_max)
{
    StrHolderVec command = make_shv("CLUSTER"_sv, "SLOTS"_sv);
    RedisExecuteOption opt;
    RedisResult result;
    RedisSlotsTablePtr null_table;

    result = co_await execute_impl(null_table, node, retry_max, command, opt);
    if (result.get_state() != WFT_STATE_SUCCESS) {
        auto table = std::make_shared<RedisSlotsTable>();
        table->state = result.get_state();
        table->error = result.get_error();
        table->complete = false;
        table->version = 0;
        table->outdated = false;
        co_return table;
    }
    else {
        RedisValue &value = result.get_value();
        auto table = parse_slots(value);
        std::set<std::string> primary_ids;
        std::set<std::string> all_ids;

        for (auto &nodes : table->nodes_vec) {
            for (auto &n : nodes) {
                if (n.host.empty())
                    n.host = node.host;

                // If node_id is empty, use host:port as node_id
                if (n.node_id.empty()) {
                    if (n.host.find(':') == std::string::npos)
                        n.node_id = n.host + ":" + n.port;
                    else
                        n.node_id = "[" + n.host + "]:" + n.port;
                }

                slot_node_to_addr(n);

                // ? means unknown host
                if (n.host != "?" && !all_ids.contains(n.node_id)) {
                    table->all_nodes.push_back(n);
                    all_ids.insert(n.node_id);
                }
            }

            if (!nodes.empty()) {
                const auto &n = nodes[0];
                if (n.host != "?" && !primary_ids.contains(n.node_id)) {
                    table->all_primaries.push_back(n);
                    primary_ids.insert(n.node_id);
                }
            }
        }

        co_return table;
    }
}

RedisSlotsTablePtr RedisClusterClientImpl::parse_slots(const RedisValue &value)
{
    auto table = std::make_shared<RedisSlotsTable>();

    if (!value.is_array()) {
        table->state = WFT_STATE_TASK_ERROR;
        table->error = REDIS_ERR_GET_SLOT_FAILED;
        table->complete = false;
        table->version = 0;
        table->outdated = false;
        return table;
    }

    auto is_valid_result = [](const RedisValue &val) -> bool {
        if (!val.is_array())
            return false;

        const RedisArray &arr = val.get_array();
        if (arr.size() < 3 || !arr[0].is_integer() || !arr[1].is_integer())
            return false;

        int from = arr[0].get_integer();
        int to = arr[1].get_integer();

        if (from < 0 || from >= REDIS_CLUSTER_SLOTS)
            return false;

        if (to < 0 || to >= REDIS_CLUSTER_SLOTS)
            return false;

        for (std::size_t i = 2; i < arr.size(); i++) {
            if (!arr[i].is_array())
                return false;

            const RedisArray &node_info = arr[i].get_array();
            if (node_info.size() < 2)
                return false;

            // host(node_info[0]) maybe null, empty string, string, or ?,
            // ? means unknown host.
            if (!node_info[0].is_bulk_string() && !node_info[0].is_null())
                return false;

            if (!node_info[1].is_integer())
                return false;
        }

        return true;
    };

    const RedisArray &results = value.get_array();
    table->slot_index.assign(REDIS_CLUSTER_SLOTS, -1);

    for (const RedisValue &result : results) {
        if (!is_valid_result(result)) {
            table->state = WFT_STATE_TASK_ERROR;
            table->error = REDIS_ERR_GET_SLOT_FAILED;
            table->complete = false;
            table->version = 0;
            table->outdated = false;
            return table;
        }

        const RedisArray &slots_info = result.get_array();
        RedisSlotNodes nodes;

        for (std::size_t i = 2; i < slots_info.size(); i++) {
            const RedisArray &node_info = slots_info[i].get_array();
            RedisSlotNode node;

            // If node_info[0] is null, that is the same host it used to send
            // the current command to.
            if (node_info[0].is_bulk_string())
                node.host = node_info[0].get_string();

            node.port = std::to_string(node_info[1].get_integer());

            if (node_info.size() >= 3 && node_info[2].is_bulk_string())
                node.node_id = node_info[2].get_string();

            nodes.push_back(std::move(node));
        }

        if (nodes.empty())
            continue;

        int from = slots_info[0].get_integer();
        int to = slots_info[1].get_integer();
        int node_pos = (int)table->nodes_vec.size();

        table->nodes_vec.push_back(std::move(nodes));

        for (; from <= to; from++)
            table->slot_index[from] = node_pos;
    }

    table->state = 0;
    table->error = 0;
    table->complete = true;
    table->version = 0;
    table->outdated = false;

    for (int i = 0; i < REDIS_CLUSTER_SLOTS; i++) {
        if (table->slot_index[i] < 0) {
            table->complete = false;
            break;
        }
    }

    return table;
}

} // namespace coke
