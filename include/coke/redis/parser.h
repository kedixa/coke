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

#ifndef COKE_REDIS_PARSER_H
#define COKE_REDIS_PARSER_H

#include "coke/redis/value.h"

namespace coke {

class RedisParser {
public:
    RedisParser() { reset(); }

    RedisParser(const RedisParser &) = delete;
    RedisParser(RedisParser &&) = delete;

    ~RedisParser() = default;

    bool parse_success() const;

    RedisValue &get_value() & { return value; }

    const RedisValue &get_value() const & { return value; }

    void reset();

    int append(const char *data, std::size_t *dsize);

private:
    bool stack_empty();
    int parse_size(std::size_t &size);

    int parse_str(const char *&cur, const char *end);
    int parse_type(const char *&cur, const char *end);

    int parse_simple_string(int32_t type, RedisValue *val);
    int parse_integer(RedisValue *val);
    int parse_string(int32_t type, RedisValue *val);
    int parse_double(RedisValue *val);
    int parse_boolean(RedisValue *val);
    int parse_array(int32_t type, RedisValue *val);
    int parse_map(int32_t type, RedisValue *val);
    int parse_inline_command(RedisValue *val);

private:
    int state;
    const char *first;
    const char *last;
    std::string buf;
    RedisValue value;
    std::vector<RedisValue *> stack;
    std::vector<std::size_t> sizes;
};

} // namespace coke

#endif // COKE_REDIS_PARSER_H
