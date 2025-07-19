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

#ifndef COKE_REDIS_MESSAGE_H
#define COKE_REDIS_MESSAGE_H

#include <cstddef>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "coke/redis/parser.h"
#include "coke/utils/str_packer.h"
#include "workflow/ProtocolMessage.h"

namespace coke {

class RedisMessage : public protocol::ProtocolMessage {
public:
    RedisMessage() = default;

    RedisMessage(RedisMessage &&) = default;
    RedisMessage(const RedisMessage &) = delete;

    RedisMessage &operator=(RedisMessage &&) = default;
    RedisMessage &operator=(const RedisMessage &) = delete;

    virtual ~RedisMessage() = default;

protected:
    int append(const void *buf, std::size_t *size) override;

protected:
    std::unique_ptr<RedisParser> parser;
    std::unique_ptr<StrPacker> packer;
    std::size_t cur_size{0};
};

class RedisRequest : public RedisMessage {
public:
    using CommandHolder = std::variant<const StrHolderVec *, StrHolderVec>;

    RedisRequest() = default;

    bool has_command() const { return !commands.empty(); }

    bool use_pipeline() const { return pipeline; }

    // Single command

    void set_command(StrHolderVec command)
    {
        pipeline = false;
        commands.clear();
        commands.push_back(std::move(command));
    }

    void set_command_nocopy(const StrHolderVec &command) noexcept
    {
        pipeline = false;
        commands.clear();
        commands.push_back(&command);
    }

    const StrHolderVec &get_command() const
    {
        if (commands.empty()) {
            return {};
        }
        else {
            if (std::holds_alternative<StrHolderVec>(commands[0]))
                return std::get<StrHolderVec>(commands[0]);
            else
                return *std::get<const StrHolderVec *>(commands[0]);
        }
    }

    // Multiple commands

    void reserve_commands(std::size_t size) { commands.reserve(size); }

    void clear_commands() { commands.clear(); }

    void add_command(StrHolderVec command)
    {
        pipeline = true;
        commands.emplace_back(std::move(command));
    }

    void add_command_nocopy(const StrHolderVec &command)
    {
        pipeline = true;
        commands.emplace_back(&command);
    }

    std::size_t commands_size() const { return commands.size(); }

    std::vector<CommandHolder> &get_commands() & { return commands; }

    const std::vector<CommandHolder> &get_commands() const &
    {
        return commands;
    }

    void reset()
    {
        if (parser)
            parser->reset();

        if (packer)
            packer->clear();

        cur_size = 0;
        pipeline = false;
        commands.clear();
    }

protected:
    int encode(struct iovec vectors[], int max) override;

    int append(const void *, std::size_t *) override;

    bool extract_command_from_value(RedisValue &);

private:
    bool pipeline{false};
    std::vector<CommandHolder> commands;
};

class RedisResponse : public RedisMessage {
public:
    RedisResponse() = default;

    void set_value(RedisValue val) { value = std::move(val); }

    RedisValue &get_value() & { return value; }

    const RedisValue &get_value() const & { return value; }

    void reset()
    {
        if (parser)
            parser->reset();

        cur_size = 0;
        value.clear();
    }

    int prepare_pipeline(std::size_t cnt);

protected:
    int append(const void *buf, std::size_t *size) override;

private:
    RedisValue value;
};

} // namespace coke

#endif // COKE_REDIS_MESSAGE_H
