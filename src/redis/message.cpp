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
#include <string>
#include <string_view>

namespace coke {

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
            command.emplace_back(val.get_string());
        else
            return false;
    }

    commands.emplace_back(std::move(command));
    return true;
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
