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

#include "coke/redis/value.h"

namespace coke {

static std::string escape_string(const std::string &s, bool quote = true)
{
    static constexpr char hex[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
    };

    std::string str;
    str.reserve(s.size() + 8);

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
            str.push_back(hex[uc >> 4]);
            str.push_back(hex[uc & 0xF]);
        }
    }

    if (quote)
        str.append("\"");

    return str;
}

static void debug_string_impl(const RedisValue &val, std::size_t indent,
                              std::string &output)
{
    int32_t type = val.get_type();
    std::size_t arr_size;

    switch (type) {
    case REDIS_TYPE_NULL:
        output.append("(nil)");
        break;

    case REDIS_TYPE_SIMPLE_STRING:
        output.append(val.get_string());
        break;

    case REDIS_TYPE_BULK_STRING:
    case REDIS_TYPE_VERBATIM_STRING:
        output.append(escape_string(val.get_string()));
        break;

    case REDIS_TYPE_SIMPLE_ERROR:
        output.append("(error) ").append(val.get_string());
        break;

    case REDIS_TYPE_BULK_ERROR:
        output.append("(bulk error) ").append(escape_string(val.get_string()));
        break;

    case REDIS_TYPE_BIG_NUMBER:
        output.append("(bignumber) ").append(val.get_string());
        break;

    case REDIS_TYPE_INTEGER:
        output.append("(integer) ").append(std::to_string(val.get_integer()));
        break;

    case REDIS_TYPE_DOUBLE:
        output.append("(double) ").append(std::to_string(val.get_double()));
        break;

    case REDIS_TYPE_BOOLEAN:
        output.append(val.get_boolean() ? "true" : "false");
        break;

    case REDIS_TYPE_ARRAY:
    case REDIS_TYPE_PUSH:
    case REDIS_TYPE_SET:
        if (type == REDIS_TYPE_ARRAY)
            output.append("(array) ");
        else if (type == REDIS_TYPE_PUSH)
            output.append("(push) ");
        else
            output.append("(set) ");

        arr_size = val.array_size();
        if (arr_size == 0)
            output.append("[]");
        else {
            const RedisArray &arr = val.get_array();

            output.append("[\n");
            indent += 4;

            for (std::size_t i = 0; i < arr_size; ++i) {
                output.append(indent, ' ');
                debug_string_impl(arr[i], indent, output);
                output.append(",\n");
            }

            indent -= 4;
            output.append(indent, ' ').append("]");
        }
        break;

    case REDIS_TYPE_MAP:
        arr_size = val.map_size();
        output.append("(map) ");

        if (arr_size == 0)
            output.append("{}");
        else {
            const RedisMap &map = val.get_map();

            output.append("{\n");
            indent += 4;

            for (const auto &pair : map) {
                output.append(indent, ' ');
                debug_string_impl(pair.key, indent, output);
                output.append(": ");
                debug_string_impl(pair.value, indent, output);
                output.append(",\n");
            }

            indent -= 4;
            output.append(indent, ' ').append("}");
        }
        break;

    default:
        output.append("(unknown type) ").append(std::to_string(type));
        break;
    }
}

std::string RedisValue::debug_string() const
{
    std::string str;
    std::size_t indent{0};

    debug_string_impl(*this, indent, str);
    return str;
}

} // namespace coke
