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

#ifndef COKE_COMPATIBLE_TO_NUMERIC_H
#define COKE_COMPATIBLE_TO_NUMERIC_H

#include <charconv>
#include <string>
#include <stdexcept>

namespace coke::comp {

/**
 * @brief Convert string to numeric, used by mysql_utils.h.
 *
 * This function is implemented here for compatibility, because some versions of
 * clang do not yet implement std::from_chars with float and double.
*/
template<typename T>
std::errc to_numeric(const std::string_view &data, T &value) noexcept {
    const char *first = data.data();
    const char *last = first + data.size();
    auto r = std::from_chars(first, last, value);

    if (r.ec == std::errc() && r.ptr != last)
        return std::errc::invalid_argument;
    return r.ec;
}

#ifndef __cpp_lib_to_chars
template<typename T>
using strto_func_t = T(*)(const std::string &, std::size_t *);

template<typename T>
std::errc __to_numeric(const std::string &str, T &value, strto_func_t<T> func) noexcept {
    std::size_t pos = 0;

    try {
        value = func(str, &pos);
        if (pos != str.size())
            return std::errc::invalid_argument;
        return std::errc{};
    }
    catch (const std::invalid_argument &) {
        return std::errc::invalid_argument;
    }
    catch (const std::out_of_range &) {
        return std::errc::result_out_of_range;
    }
}

template<> inline
std::errc to_numeric(const std::string_view &data, float &value) noexcept {
    return __to_numeric(std::string(data), value, std::stof);
}

template<> inline
std::errc to_numeric(const std::string_view &data, double &value) noexcept {
    return __to_numeric(std::string(data), value, std::stod);
}
#endif

}

#endif // COKE_COMPATIBLE_TO_NUMERIC_H
