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

#ifndef COKE_REDIS_VALUE_H
#define COKE_REDIS_VALUE_H

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace coke {

// clang-format off
enum : int32_t {
    REDIS_TYPE_NULL            = 0,
    REDIS_TYPE_SIMPLE_STRING   = 1,
    REDIS_TYPE_BULK_STRING     = 2,
    REDIS_TYPE_VERBATIM_STRING = 3,
    REDIS_TYPE_SIMPLE_ERROR    = 4,
    REDIS_TYPE_BULK_ERROR      = 5,
    REDIS_TYPE_BIG_NUMBER      = 6,
    REDIS_TYPE_INTEGER         = 7,
    REDIS_TYPE_DOUBLE          = 8,
    REDIS_TYPE_BOOLEAN         = 9,
    REDIS_TYPE_ARRAY           = 10,
    REDIS_TYPE_SET             = 11,
    REDIS_TYPE_PUSH            = 12,
    REDIS_TYPE_MAP             = 13,
    REDIS_TYPE_ATTRIBUTE       = 14,
};
// clang-format on

class RedisValue;

struct RedisNull {};

namespace detail {

template<typename T>
struct RedisPair {
    T key;
    T value;
};

} // namespace detail

using RedisPair = detail::RedisPair<RedisValue>;
using RedisArray = std::vector<RedisValue>;
using RedisMap = std::vector<RedisPair>;
using RedisVariant = std::variant<RedisNull, bool, int64_t, double, std::string,
                                  RedisArray, RedisMap>;

class RedisValue {
public:
    static RedisValue make_null() { return RedisValue(); }

    static RedisValue make_simple_string(std::string str)
    {
        RedisValue v;
        v.set_simple_string(std::move(str));
        return v;
    }

    static RedisValue make_bulk_string(std::string str)
    {
        RedisValue v;
        v.set_bulk_string(std::move(str));
        return v;
    }

    static RedisValue make_verbatim_string(std::string str)
    {
        RedisValue v;
        v.set_verbatim_string(std::move(str));
        return v;
    }

    static RedisValue make_simple_error(std::string str)
    {
        RedisValue v;
        v.set_simple_error(std::move(str));
        return v;
    }

    static RedisValue make_bulk_error(std::string str)
    {
        RedisValue v;
        v.set_bulk_error(std::move(str));
        return v;
    }

    static RedisValue make_big_number(std::string str)
    {
        RedisValue v;
        v.set_big_number(std::move(str));
        return v;
    }

    static RedisValue make_integer(int64_t n)
    {
        RedisValue v;
        v.set_integer(n);
        return v;
    }

    static RedisValue make_double(double d)
    {
        RedisValue v;
        v.set_double(d);
        return v;
    }

    static RedisValue make_boolean(bool b)
    {
        RedisValue v;
        v.set_boolean(b);
        return v;
    }

    static RedisValue make_array(RedisArray arr)
    {
        RedisValue v;
        v.set_array(std::move(arr));
        return v;
    }

    static RedisValue make_set(RedisArray set)
    {
        RedisValue v;
        v.set_set(std::move(set));
        return v;
    }

    static RedisValue make_push(RedisArray push)
    {
        RedisValue v;
        v.set_push(std::move(push));
        return v;
    }

    static RedisValue make_map(RedisMap map)
    {
        RedisValue v;
        v.set_map(std::move(map));
        return v;
    }

public:
    RedisValue() noexcept = default;

    ~RedisValue() = default;

    RedisValue(RedisValue &&o) noexcept
        : type(o.type), var(std::move(o.var)), attr(std::move(o.attr))
    {
        o.type = REDIS_TYPE_NULL;
        o.var = RedisNull{};
    }

    RedisValue &operator=(RedisValue &&o) noexcept
    {
        if (this != &o) {
            std::swap(this->type, o.type);
            std::swap(this->var, o.var);
            std::swap(this->attr, o.attr);
        }

        return *this;
    }

    RedisValue(const RedisValue &o) : type(o.get_type()), var(o.var)
    {
        if (o.attr)
            attr = std::make_unique<RedisMap>(*o.attr);
    }

    RedisValue &operator=(const RedisValue &o)
    {
        if (this != &o) {
            type = o.get_type();
            var = o.var;

            if (o.attr)
                attr = std::make_unique<RedisMap>(*o.attr);
            else
                attr.reset();
        }

        return *this;
    }

    void swap(RedisValue &o) noexcept
    {
        if (this != &o) {
            std::swap(this->type, o.type);
            std::swap(this->var, o.var);
            std::swap(this->attr, o.attr);
        }
    }

    bool is_null() const { return type == REDIS_TYPE_NULL; }

    bool is_simple_string() const { return type == REDIS_TYPE_SIMPLE_STRING; }
    bool is_bulk_string() const { return type == REDIS_TYPE_BULK_STRING; }
    bool is_verbatim_string() const
    {
        return type == REDIS_TYPE_VERBATIM_STRING;
    }

    bool is_simple_error() const { return type == REDIS_TYPE_SIMPLE_ERROR; }
    bool is_bulk_error() const { return type == REDIS_TYPE_BULK_ERROR; }

    bool is_big_number() const { return type == REDIS_TYPE_BIG_NUMBER; }
    bool is_integer() const { return type == REDIS_TYPE_INTEGER; }
    bool is_double() const { return type == REDIS_TYPE_DOUBLE; }
    bool is_boolean() const { return type == REDIS_TYPE_BOOLEAN; }

    bool is_array() const { return type == REDIS_TYPE_ARRAY; }
    bool is_set() const { return type == REDIS_TYPE_SET; }
    bool is_push() const { return type == REDIS_TYPE_PUSH; }
    bool is_map() const { return type == REDIS_TYPE_MAP; }

    bool is_error() const { return is_simple_error() || is_bulk_error(); }
    bool is_array_like() const { return is_array() || is_set() || is_push(); }
    bool has_attribute() const { return attr.get() != nullptr; }

    int32_t get_type() const { return type; }

    std::size_t array_size() const { return get_array().size(); }
    std::size_t map_size() const { return get_map().size(); }
    std::size_t string_length() const { return get_string().size(); }

    const std::string &get_string() const &
    {
        return std::get<std::string>(var);
    }

    std::string &get_string() & { return std::get<std::string>(var); }

    bool get_boolean() const { return std::get<bool>(var); }
    int64_t get_integer() const { return std::get<int64_t>(var); }
    double get_double() const { return std::get<double>(var); }

    const RedisArray &get_array() const & { return std::get<RedisArray>(var); }
    RedisArray &get_array() & { return std::get<RedisArray>(var); }

    const RedisMap &get_map() const & { return std::get<RedisMap>(var); }
    RedisMap &get_map() & { return std::get<RedisMap>(var); }

    const RedisMap &get_attribute() const & { return *attr; }
    RedisMap &get_attribute() & { return *attr; }

    void set_null()
    {
        type = REDIS_TYPE_NULL;
        var = RedisNull{};
    }

    void set_simple_string(std::string str)
    {
        type = REDIS_TYPE_SIMPLE_STRING;
        var.emplace<std::string>(std::move(str));
    }

    void set_bulk_string(std::string str)
    {
        type = REDIS_TYPE_BULK_STRING;
        var.emplace<std::string>(std::move(str));
    }

    void set_verbatim_string(std::string str)
    {
        type = REDIS_TYPE_VERBATIM_STRING;
        var.emplace<std::string>(std::move(str));
    }

    void set_simple_error(std::string str)
    {
        type = REDIS_TYPE_SIMPLE_ERROR;
        var.emplace<std::string>(std::move(str));
    }

    void set_bulk_error(std::string str)
    {
        type = REDIS_TYPE_BULK_ERROR;
        var.emplace<std::string>(std::move(str));
    }

    void set_big_number(std::string str)
    {
        type = REDIS_TYPE_BIG_NUMBER;
        var.emplace<std::string>(std::move(str));
    }

    void set_boolean(bool b)
    {
        type = REDIS_TYPE_BOOLEAN;
        var = b;
    }

    void set_integer(int64_t n)
    {
        type = REDIS_TYPE_INTEGER;
        var = n;
    }

    void set_double(double d)
    {
        type = REDIS_TYPE_DOUBLE;
        var = d;
    }

    void create_array(std::size_t size = 0)
    {
        type = REDIS_TYPE_ARRAY;
        var.emplace<RedisArray>(size);
    }

    void create_set(std::size_t size = 0)
    {
        type = REDIS_TYPE_SET;
        var.emplace<RedisArray>(size);
    }

    void create_map(std::size_t size = 0)
    {
        type = REDIS_TYPE_MAP;
        var.emplace<RedisMap>(size);
    }

    void set_array(RedisArray arr)
    {
        type = REDIS_TYPE_ARRAY;
        var = std::move(arr);
    }

    void set_set(RedisArray arr)
    {
        type = REDIS_TYPE_SET;
        var = std::move(arr);
    }

    void set_push(RedisArray arr)
    {
        type = REDIS_TYPE_PUSH;
        var = std::move(arr);
    }

    void set_map(RedisMap map)
    {
        type = REDIS_TYPE_MAP;
        var = std::move(map);
    }

    void set_attribute(RedisMap map)
    {
        attr = std::make_unique<RedisMap>(std::move(map));
    }

    void clear()
    {
        set_null();
        clear_attribute();
    }

    void clear_attribute() { attr.reset(); }

    std::string debug_string() const;

private:
    int32_t type{REDIS_TYPE_NULL};
    RedisVariant var{RedisNull{}};
    std::unique_ptr<RedisMap> attr;

    friend class RedisParser;
};

class RedisResult {
public:
    RedisResult() noexcept = default;
    RedisResult(RedisValue &&value) noexcept : value(value) {}
    RedisResult(const RedisValue &value) : value(value) {}

    RedisResult(RedisResult &&) noexcept = default;
    RedisResult(const RedisResult &) = default;

    RedisResult &operator=(RedisResult &&) noexcept = default;
    RedisResult &operator=(const RedisResult &) = default;

    void set_state(int state) { this->state = state; }
    void set_error(int error) { this->error = error; }

    int get_state() const { return state; }
    int get_error() const { return error; }

    void set_value(RedisValue &&value) { this->value = std::move(value); }
    void set_value(const RedisValue &value) { this->value = value; }

    const RedisValue &get_value() const & { return value; }
    RedisValue &get_value() & { return value; }
    RedisValue get_value() && { return std::move(value); }

private:
    int state{0};
    int error{0};
    RedisValue value;
};

} // namespace coke

#endif // COKE_REDIS_VALUE_H
