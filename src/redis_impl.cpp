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

#include <string>
#include <cctype>

#include "coke/redis_client.h"
#include "coke/redis_utils.h"

#include "workflow/WFTaskFactory.h"
#include "workflow/StringUtil.h"

namespace coke {

RedisClient::RedisClient(const RedisClientParams &p)
    : params(p)
{
    std::string username, password, host;
    username = StringUtil::url_encode_component(params.username);
    password = StringUtil::url_encode_component(params.password);

    url.assign(params.use_ssl ? "rediss://" : "redis://");

    if (!(username.empty() && password.empty()))
        url.append(username).append(":").append(password).append("@");

    if (params.host.find(':') != std::string::npos &&
        params.host.front() != '[' && params.host.back() != ']')
    {
        // XXX we didn't encode % in ipv6 because URIParser doesn't decode it.
        host.reserve(params.host.size() + 2);
        host.append("[").append(params.host).append("]");
    }
    else
        host = params.host;

    url.append(host);

    if (params.port != 0)
        url.append(":").append(std::to_string(params.port));

    url.append("/").append(std::to_string(params.db));

    URIParser::parse(url, uri);
}

RedisClient::AwaiterType
RedisClient::request(const std::string &command,
                     const std::vector<std::string> &params) noexcept
{
    RedisRequest req;
    req.set_request(command, params);

    return create_task(&req);
}

RedisClient::AwaiterType RedisClient::create_task(ReqType *req) noexcept {
    WFRedisTask *task;

    task = WFTaskFactory::create_redis_task(uri, params.retry_max, nullptr);
    if (req)
        *(task->get_req()) = std::move(*req);

    task->set_send_timeout(params.send_timeout);
    task->set_receive_timeout(params.receive_timeout);
    task->set_keep_alive(params.keep_alive_timeout);

    return AwaiterType(task);
}


// redis_utils

static std::string escape_string(const std::string &s, bool quote = true) {
    std::string str;
    str.reserve(s.size() + 8);

    auto to_hex = [] (unsigned c) {
        return (c < 10) ? ('0' + c) : (c - 10 + 'a');
    };

    if (quote)
        str.append("\"");

    for (const char c : s) {
        if (quote && (c == '\\' || c == '\"')) {
            str.push_back('\\');
            str.push_back(c);
        }
        else if (std::isprint(c)) {
            str.push_back(c);
        }
        else {
            unsigned char uc = static_cast<unsigned char>(c);
            str.append("\\x");
            str.push_back(to_hex(uc >> 4));
            str.push_back(to_hex(uc & 0xF));
        }
    }

    if (quote)
        str.append("\"");

    return str;
}

static void to_string_impl(const RedisValue &value, const std::string &pre,
                           std::string &str) {
    int type = value.get_type();
    const std::string *view;
    std::size_t arr_sz;

    switch (type) {
    case REDIS_REPLY_TYPE_STRING:
        view = value.string_view();
        str.append(escape_string(*view)).append("\n");
        break;

    case REDIS_REPLY_TYPE_INTEGER:
        str.append("(integer) ")
           .append(std::to_string(value.int_value())).append("\n");
        break;

    case REDIS_REPLY_TYPE_NIL:
        str.append("(nil)\n");
        break;

    case REDIS_REPLY_TYPE_STATUS:
        view = value.string_view();
        str.append(escape_string(*view, false)).append("\n");
        break;

    case REDIS_REPLY_TYPE_ERROR:
        view = value.string_view();
        str.append("(error) ")
           .append(escape_string(*view, false)).append("\n");
        break;

    case REDIS_REPLY_TYPE_ARRAY:
        arr_sz = value.arr_size();
        for (std::size_t i = 0; i < arr_sz; i++) {
            if (i)
                str.append(pre);

            str.append(std::to_string(i+1)).append(") ");
            to_string_impl(value.arr_at(i), pre + "   ", str);
        }
        break;

    default:
        str.append("unknown type ")
           .append(std::to_string(type))
           .append("\n");
        break;
    }
}

std::string redis_value_to_string(const RedisValue &value) {
    std::string str;
    std::string prefix;

    to_string_impl(value, prefix, str);

    return str;
}

} // namespace coke
