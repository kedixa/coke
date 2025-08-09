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

#include "coke/redis/message.h"

#include <cerrno>
#include <charconv>
#include <string>
#include <string_view>

namespace coke {

static std::string encode_double(double d)
{
    char buf[64];
    auto r = std::to_chars(buf, buf + 64, d);

    // Fall back to std::to_string on failure, but this should not occur.
    if (r.ec == std::errc())
        return std::string(buf, r.ptr);
    else
        return std::to_string(d);
}

static bool encode_value(StrPacker &pack, const RedisValue &val)
{
    if (val.has_attribute()) {
        const RedisMap &attr = val.get_attribute();
        pack.append("|").append(std::to_string(attr.size())).append("\r\n");

        for (const RedisPair &pair : attr) {
            if (!encode_value(pack, pair.key))
                return false;
            if (!encode_value(pack, pair.value))
                return false;
        }
    }

    auto type = val.get_type();

    switch (type) {
    case REDIS_TYPE_NULL:
        pack.append("$-1\r\n");
        return true;

    case REDIS_TYPE_SIMPLE_STRING:
        pack.append("+").append_nocopy(val.get_string()).append("\r\n");
        return true;

    case REDIS_TYPE_BULK_STRING:
        pack.append("$")
            .append(std::to_string(val.string_length()))
            .append("\r\n")
            .append_nocopy(val.get_string())
            .append("\r\n");
        return true;

    case REDIS_TYPE_VERBATIM_STRING:
        pack.append("=")
            .append(std::to_string(val.string_length()))
            .append("\r\n")
            .append_nocopy(val.get_string())
            .append("\r\n");
        return true;

    case REDIS_TYPE_SIMPLE_ERROR:
        pack.append("-").append_nocopy(val.get_string()).append("\r\n");
        return true;

    case REDIS_TYPE_BULK_ERROR:
        pack.append("!")
            .append(std::to_string(val.string_length()))
            .append("\r\n")
            .append_nocopy(val.get_string())
            .append("\r\n");
        return true;

    case REDIS_TYPE_BIG_NUMBER:
        pack.append("(").append_nocopy(val.get_string()).append("\r\n");
        return true;

    case REDIS_TYPE_INTEGER:
        pack.append(":")
            .append(std::to_string(val.get_integer()))
            .append("\r\n");
        return true;

    case REDIS_TYPE_DOUBLE:
        pack.append(",").append(encode_double(val.get_double())).append("\r\n");
        return true;

    case REDIS_TYPE_BOOLEAN:
        pack.append("#").append(val.get_boolean() ? "t" : "f").append("\r\n");
        return true;

    case REDIS_TYPE_ARRAY:
    case REDIS_TYPE_SET:
    case REDIS_TYPE_PUSH:
        if (type == REDIS_TYPE_ARRAY)
            pack.append("*");
        else if (type == REDIS_TYPE_SET)
            pack.append("~");
        else
            pack.append(">");

        pack.append(std::to_string(val.array_size())).append("\r\n");

        for (const RedisValue &v : val.get_array()) {
            if (!encode_value(pack, v))
                return false;
        }
        return true;

    case REDIS_TYPE_MAP:
        pack.append("%").append(std::to_string(val.map_size())).append("\r\n");

        for (const RedisPair &pair : val.get_map()) {
            if (!encode_value(pack, pair.key))
                return false;
            if (!encode_value(pack, pair.value))
                return false;
        }
        return true;

    default:
        // Unknown type.
        return false;
    }
}

int RedisMessage::append(const void *buf, std::size_t *size)
{
    if (!parser)
        parser = std::make_unique<RedisParser>();

    int ret = parser->append(static_cast<const char *>(buf), size);
    if (ret >= 0) {
        if (*size > this->size_limit - cur_size) {
            errno = EMSGSIZE;
            ret = -1;
        }
        else
            cur_size += *size;
    }

    return ret;
}

int RedisRequest::encode(struct iovec vectors[], int max)
{
    if (commands.empty()) {
        errno = EBADMSG;
        return -1;
    }

    if (!packer)
        packer = std::make_unique<StrPacker>();

    auto &pack = *packer;
    pack.clear();

    for (const CommandHolder &cmd : commands) {
        const StrHolderVec *cmd_ptr;

        if (std::holds_alternative<StrHolderVec>(cmd))
            cmd_ptr = &std::get<StrHolderVec>(cmd);
        else
            cmd_ptr = std::get<const StrHolderVec *>(cmd);

        pack.append("*").append(std::to_string(cmd_ptr->size())).append("\r\n");

        for (const StrHolder &arg : *cmd_ptr) {
            std::string_view sv = arg.as_view();
            pack.append("$")
                .append(std::to_string(sv.size()))
                .append("\r\n")
                .append_nocopy(sv)
                .append("\r\n");
        }
    }

    pack.merge(max);

    const StrHolderVec &shv = pack.get_strs();
    for (std::size_t i = 0; i < shv.size(); i++) {
        std::string_view sv = shv[i].as_view();
        vectors[i].iov_base = (void *)sv.data();
        vectors[i].iov_len = sv.size();
    }

    return (int)shv.size();
}

int RedisRequest::append(const void *buf, std::size_t *size)
{
    if (parser && parser->parse_success()) {
        *size = 0;
        return 1;
    }

    int ret = RedisMessage::append(buf, size);
    if (ret > 0) {
        RedisValue &v = parser->get_value();
        if (!extract_command_from_value(v)) {
            errno = EBADMSG;
            ret = -1;
        }
    }

    return ret;
}

bool RedisRequest::extract_command_from_value(RedisValue &value)
{
    if (!value.is_array() || value.array_size() == 0)
        return false;

    StrHolderVec command;
    RedisArray &array = value.get_array();

    command.reserve(array.size());
    for (RedisValue &val : array) {
        if (val.is_bulk_string())
            command.push_back(std::move(val.get_string()));
        else
            return false;
    }

    // Pipeline not supported for server now.
    set_command(std::move(command));
    return true;
}

int RedisResponse::encode(struct iovec vectors[], int max)
{
    if (!packer)
        packer = std::make_unique<StrPacker>();

    packer->clear();

    if (!encode_value(*packer, value)) {
        errno = EBADMSG;
        return -1;
    }

    packer->merge(max);

    const StrHolderVec &shv = packer->get_strs();
    for (std::size_t i = 0; i < shv.size(); i++) {
        std::string_view sv = shv[i].as_view();
        vectors[i].iov_base = (void *)sv.data();
        vectors[i].iov_len = sv.size();
    }

    return (int)shv.size();
}

int RedisResponse::append(const void *buf, std::size_t *size)
{
    if (parser && parser->parse_success()) {
        *size = 0;
        return 1;
    }

    int ret = RedisMessage::append(buf, size);
    if (ret > 0)
        value = std::move(parser->get_value());

    return ret;
}

int RedisResponse::prepare_pipeline(std::size_t cnt)
{
    std::string buf;
    std::size_t len;
    int ret;

    buf.append("*").append(std::to_string(cnt)).append("\r\n");
    len = buf.size();
    ret = append(buf.c_str(), &len);

    if (ret > 0 && len != buf.size()) {
        errno = EBADMSG;
        ret = -1;
    }

    return ret;
}

} // namespace coke
