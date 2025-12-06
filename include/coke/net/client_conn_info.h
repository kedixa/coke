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

#ifndef COKE_NET_CLIENT_CONN_INFO_H
#define COKE_NET_CLIENT_CONN_INFO_H

#include <cstdint>
#include <memory>
#include <string>

namespace coke {

class ClientConnInfo;

inline constexpr std::size_t GENERIC_CLIENT_CONN_ID = 0;
inline constexpr std::size_t INVALID_CLIENT_INFO_ID = 0;

namespace detail {

class ConnInfoManager;

using ConnInfoManagerPtr = std::shared_ptr<ConnInfoManager>;

class ConnInfoImpl {
public:
    ConnInfoImpl(const ConnInfoManagerPtr &manager,
                 const std::string &full_info, bool unique_conn);

    ~ConnInfoImpl();

private:
    const std::string &get_full_info() const & noexcept { return full_info; }
    const std::string &get_short_info() const & noexcept { return short_info; }

    std::size_t get_info_id() const noexcept { return info_id; }
    std::size_t get_conn_id() const noexcept { return conn_id; }

private:
    ConnInfoManagerPtr manager;
    std::string full_info;
    std::string short_info;
    std::size_t info_id;
    std::size_t conn_id;

    friend ClientConnInfo;
};

} // namespace detail

/**
 * @brief This class is for internal use or for client developers.
 *
 * @details The Workflow framework manages network connections globally. This
 *          class maps the `full_info` composed of client parameters to the
 *          unique `short_info` within the process. Different client instances
 *          using the same parameters can share network connections.
 */
class ClientConnInfo {
public:
    static ClientConnInfo create_instance(const std::string &full_info,
                                          bool unique_conn);

    ClientConnInfo() = default;
    ~ClientConnInfo() = default;

    /**
     * @brief Check whether this instance is associated with the client
     *        connection information object.
     *
     * A default constructed instance is not valid, use create_instance to
     * get a valid one.
     */
    bool valid() const noexcept { return (bool)conn_info_ptr; }

    const std::string &get_full_info() const & noexcept
    {
        return conn_info_ptr->get_full_info();
    }

    const std::string &get_short_info() const & noexcept
    {
        return conn_info_ptr->get_short_info();
    }

    std::size_t get_info_id() const noexcept
    {
        return conn_info_ptr->get_info_id();
    }

    std::size_t get_conn_id() const noexcept
    {
        return conn_info_ptr->get_conn_id();
    }

private:
    using ConnInfoPtr = std::shared_ptr<detail::ConnInfoImpl>;

    explicit ClientConnInfo(const ConnInfoPtr &conn_info_ptr) noexcept
        : conn_info_ptr(conn_info_ptr)
    {
    }

private:
    ConnInfoPtr conn_info_ptr;
};

} // namespace coke

#endif // COKE_NET_CLIENT_CONN_INFO_H
