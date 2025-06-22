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

#include <map>
#include <mutex>
#include <queue>

#include "coke/net/client_conn_info.h"

namespace coke {

namespace detail {

class ConnInfoData {
public:
    explicit ConnInfoData(std::size_t info_id) : info_id(info_id) {}

    std::size_t get_info_id() const noexcept { return info_id; }

    std::size_t acquire_conn_id()
    {
        std::size_t conn_id;

        if (!que.empty()) {
            conn_id = que.top();
            que.pop();
        }
        else {
            conn_id = top++;
        }

        return conn_id;
    }

    void release_conn_id(std::size_t conn_id)
    {
        if (conn_id + 1 == top)
            --top;
        else
            que.push(conn_id);
    }

private:
    std::size_t info_id;
    std::size_t top{1};
    std::priority_queue<std::size_t> que;
};

class ConnInfoManager {
public:
    static ConnInfoManagerPtr get_instance()
    {
        static auto manager_ptr = std::make_shared<ConnInfoManager>();
        return manager_ptr;
    }

    std::size_t get_info_id(const std::string &full_info)
    {
        std::lock_guard<std::mutex> lg(mtx);
        auto it = info_map.lower_bound(full_info);

        if (it == info_map.end() || it->first != full_info) {
            it = info_map.emplace_hint(it, full_info, next_info_id);
            next_info_id++;
        }

        return it->second.get_info_id();
    }

    std::size_t acquire_conn_id(const std::string &full_info)
    {
        std::lock_guard<std::mutex> lg(mtx);
        auto it = info_map.find(full_info);

        if (it == info_map.end())
            return 0;

        return it->second.acquire_conn_id();
    }

    void release_conn_id(const std::string &full_info, std::size_t conn_id)
    {
        std::lock_guard<std::mutex> lg(mtx);
        auto it = info_map.find(full_info);

        if (it != info_map.end())
            it->second.release_conn_id(conn_id);
    }

    ConnInfoManager() noexcept : next_info_id(1) {}
    ~ConnInfoManager() = default;

private:
    std::mutex mtx;
    std::size_t next_info_id;
    std::map<std::string, ConnInfoData> info_map;
};

ConnInfoImpl::ConnInfoImpl(const ConnInfoManagerPtr &manager,
                           const std::string &full_info, bool unique_conn)
    : manager(manager), full_info(full_info)
{
    info_id = manager->get_info_id(full_info);

    if (unique_conn)
        conn_id = manager->acquire_conn_id(full_info);
    else
        conn_id = GENERIC_CLIENT_CONN_ID;

    short_info.append("coke:")
        .append(std::to_string(info_id))
        .append(",")
        .append(std::to_string(conn_id));
}

ConnInfoImpl::~ConnInfoImpl()
{
    if (conn_id != GENERIC_CLIENT_CONN_ID)
        manager->release_conn_id(full_info, conn_id);
}

} // namespace detail

ClientConnInfo ClientConnInfo::create_instance(const std::string &full_info,
                                               bool unique_conn)
{
    auto manager = detail::ConnInfoManager::get_instance();
    auto ptr = std::make_shared<detail::ConnInfoImpl>(manager, full_info,
                                                      unique_conn);

    return ClientConnInfo(ptr);
}

} // namespace coke
