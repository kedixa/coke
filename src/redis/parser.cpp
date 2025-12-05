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

#include <cerrno>

#include "coke/compatible/to_numeric.h"
#include "coke/redis/parser.h"

namespace coke {

enum : int {
    REDIS_PARSE_TYPE = 0,
    REDIS_PARSE_STR = 1,
    REDIS_PARSE_FINISH = 2,
};

constexpr std::size_t REDIS_STRING_INIT_SIZE_HINT = 256 * 1024;
constexpr std::size_t REDIS_ARRAY_INIT_SIZE_HINT = 16;
constexpr std::size_t REDIS_INLINE_COMMAND_SIZE_HINT = 8;

static const char *find_lf(const char *ptr, const char *end)
{
    while (ptr != end && *ptr != '\n')
        ++ptr;
    return ptr;
}

bool RedisParser::parse_success() const
{
    return state == REDIS_PARSE_FINISH;
}

void RedisParser::reset()
{
    state = REDIS_PARSE_TYPE;
    first = last = nullptr;
    buf.clear();
    value.clear();
    stack.assign(1, &value);
    sizes.clear();
}

int RedisParser::append(const char *data, std::size_t *dsize)
{
    const char *cur = data;
    const char *end = data + *dsize;
    int ret;

    while (state != REDIS_PARSE_FINISH && cur != end) {
        if (state == REDIS_PARSE_STR) {
            ret = parse_str(cur, end);
            if (ret < 0)
                return ret;
            else if (ret > 0)
                break;
        }
        else {
            ret = parse_type(cur, end);
            if (ret < 0)
                return ret;
            else if (ret > 0)
                break;

            // ret == 0 means parse type line finished, clear the line buffer
            buf.clear();
        }

        if (stack_empty()) {
            state = REDIS_PARSE_FINISH;
            break;
        }
    }

    *dsize = cur - data;
    return (state == REDIS_PARSE_FINISH) ? 1 : 0;
}

bool RedisParser::stack_empty()
{
    while (!stack.empty()) {
        RedisValue *val = stack.back();

        if (val->is_array_like()) {
            auto &arr = val->get_array();

            if (arr.size() == sizes.back()) {
                stack.pop_back();
                sizes.pop_back();
                continue;
            }

            arr.emplace_back();
            stack.push_back(&arr.back());
            break;
        }
        else if (val->is_map() || val->type == REDIS_TYPE_ATTRIBUTE) {
            auto &map = val->get_map();

            if (map.size() == sizes.back()) {
                if (val->type == REDIS_TYPE_ATTRIBUTE) {
                    val->set_attribute(std::move(map));
                    val->set_null();
                    sizes.pop_back();
                    break;
                }
                else {
                    stack.pop_back();
                    sizes.pop_back();
                    continue;
                }
            }

            map.emplace_back();
            stack.push_back(&map.back().value);
            stack.push_back(&map.back().key);
            break;
        }
        else
            break;
    }

    return stack.empty();
}

int RedisParser::parse_size(std::size_t &size)
{
    if (*first != '-') {
        auto r = std::from_chars(first, last, size);
        return (r.ec == std::errc{} && r.ptr == last) ? 0 : -1;
    }

    auto r = std::from_chars(first + 1, last, size);
    return (r.ec == std::errc{} && r.ptr == last && size == 1) ? 1 : -1;
}

int RedisParser::parse_str(const char *&cur, const char *end)
{
    RedisValue *val = stack.back();
    std::size_t size = sizes.back();
    std::string &str = val->get_string();
    std::size_t nleft = end - cur;
    bool finish = false;

    if (nleft >= size + 2 - str.size()) {
        finish = true;
        nleft = size + 2 - str.size();
    }

    str.append(cur, nleft);
    cur += nleft;

    if (!finish)
        return 1;

    if (!str.ends_with("\r\n")) {
        errno = EBADMSG;
        return -1;
    }

    str.resize(size);
    stack.pop_back();
    sizes.pop_back();

    state = REDIS_PARSE_TYPE;
    return 0;
}

int RedisParser::parse_type(const char *&cur, const char *end)
{
    const char *plf = find_lf(cur, end);
    bool finish = (plf != end);

    if (finish)
        ++plf;

    buf.append(cur, plf);
    cur = plf;

    if (!finish)
        return 1;

    if (buf.size() < 3 || !buf.ends_with("\r\n")) {
        errno = EBADMSG;
        return -1;
    }

    RedisValue *val;

    val = stack.back();
    stack.pop_back();
    first = buf.data() + 1;
    last = buf.data() + buf.size() - 2;

    // see https://redis.io/docs/latest/develop/reference/protocol-spec/

    switch (buf[0]) {
    case '$':
        return parse_string(REDIS_TYPE_BULK_STRING, val);

    case '+':
        return parse_simple_string(REDIS_TYPE_SIMPLE_STRING, val);

    case '-':
        return parse_simple_string(REDIS_TYPE_SIMPLE_ERROR, val);

    case ':':
        return parse_integer(val);

    case '_':
        if (first == last) {
            val->set_null();
            return 0;
        }
        else {
            errno = EBADMSG;
            return -1;
        }

    case ',':
        return parse_double(val);

    case '#':
        return parse_boolean(val);

    case '!':
        return parse_string(REDIS_TYPE_BULK_ERROR, val);

    case '=':
        return parse_string(REDIS_TYPE_VERBATIM_STRING, val);

    case '(':
        return parse_simple_string(REDIS_TYPE_BIG_NUMBER, val);

    case '*':
        return parse_array(REDIS_TYPE_ARRAY, val);

    case '~':
        return parse_array(REDIS_TYPE_SET, val);

    case '>':
        return parse_array(REDIS_TYPE_PUSH, val);

    case '%':
        return parse_map(REDIS_TYPE_MAP, val);

    case '|':
        return parse_map(REDIS_TYPE_ATTRIBUTE, val);

    default:
        if (std::isspace(buf[0]) || std::isalnum(buf[0])) {
            // inline command starts from first byte.
            first = buf.data();
            return parse_inline_command(val);
        }
        else {
            errno = EBADMSG;
            return -1;
        }
    }
}

int RedisParser::parse_simple_string(int32_t type, RedisValue *val)
{
    val->type = type;
    val->var.emplace<std::string>(first, last);
    return 0;
}

int RedisParser::parse_integer(RedisValue *val)
{
    int64_t num;
    auto r = std::from_chars(first, last, num);

    if (r.ec == std::errc{} && r.ptr == last) {
        val->type = REDIS_TYPE_INTEGER;
        val->var.emplace<int64_t>(num);
        return 0;
    }

    errno = EBADMSG;
    return -1;
}

int RedisParser::parse_string(int32_t type, RedisValue *val)
{
    std::size_t size;
    int ret = parse_size(size);

    if (ret == 0) {
        std::size_t hint = std::min(size, REDIS_STRING_INIT_SIZE_HINT);
        val->type = type;
        val->var.emplace<std::string>();
        val->get_string().reserve(hint);

        state = REDIS_PARSE_STR;
        stack.push_back(val);
        sizes.push_back(size);
        return 0;
    }
    else if (ret > 0) {
        val->type = REDIS_TYPE_NULL;
        val->var.emplace<RedisNull>();
        return 0;
    }

    errno = EBADMSG;
    return ret;
}

int RedisParser::parse_double(RedisValue *val)
{
    double d;
    std::errc ec;

    // clang 15 not support std::from_chars with double
    ec = comp::to_numeric(std::string_view(first, last), d);

    if (ec == std::errc{}) {
        val->type = REDIS_TYPE_DOUBLE;
        val->var.emplace<double>(d);
        return 0;
    }

    errno = EBADMSG;
    return -1;
}

int RedisParser::parse_boolean(RedisValue *val)
{
    if (first + 1 == last) {
        if (*first == 't') {
            val->type = REDIS_TYPE_BOOLEAN;
            val->var.emplace<bool>(true);
            return 0;
        }
        else if (*first == 'f') {
            val->type = REDIS_TYPE_BOOLEAN;
            val->var.emplace<bool>(false);
            return 0;
        }
    }

    errno = EBADMSG;
    return -1;
}

int RedisParser::parse_array(int32_t type, RedisValue *val)
{
    std::size_t size;
    int ret = parse_size(size);

    if (ret == 0) {
        std::size_t hint = std::min(size, REDIS_ARRAY_INIT_SIZE_HINT);
        val->create_array();
        // force reset type
        val->type = type;
        val->get_array().reserve(hint);

        stack.push_back(val);
        sizes.push_back(size);

        return 0;
    }
    else if (ret > 0) {
        val->type = REDIS_TYPE_NULL;
        val->var.emplace<RedisNull>();
        return 0;
    }

    errno = EBADMSG;
    return ret;
}

int RedisParser::parse_map(int32_t type, RedisValue *val)
{
    std::size_t size;
    int ret = parse_size(size);

    if (ret == 0) {
        std::size_t hint = std::min(size, REDIS_ARRAY_INIT_SIZE_HINT);
        val->create_map();
        // force reset type
        val->type = type;
        val->get_map().reserve(hint);

        stack.push_back(val);
        sizes.push_back(size);

        return 0;
    }
    else if (ret > 0) {
        val->type = REDIS_TYPE_NULL;
        val->var.emplace<RedisNull>();
        return 0;
    }

    errno = EBADMSG;
    return ret;
}

int RedisParser::parse_inline_command(RedisValue *val)
{
    if (!val->is_null()) {
        errno = EBADMSG;
        return -1;
    }

    std::vector<std::string_view> vs;
    const char *cur = first;

    vs.reserve(REDIS_INLINE_COMMAND_SIZE_HINT);

    while (cur != last) {
        while (cur != last && std::isspace(*cur))
            ++cur;

        if (cur == last)
            break;

        const char *tmp = cur;
        while (cur != last && !std::isspace(*cur))
            ++cur;

        vs.emplace_back(tmp, cur);
    }

    if (vs.size() == 0) {
        stack.push_back(val);
        return 0;
    }

    val->create_array();

    RedisArray &arr = val->get_array();
    arr.resize(vs.size());

    for (std::size_t i = 0; i < vs.size(); i++)
        arr[i].set_bulk_string(std::string(vs[i]));

    return 0;
}

} // namespace coke
