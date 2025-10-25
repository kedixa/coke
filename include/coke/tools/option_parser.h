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

#ifndef COKE_TOOLS_OPTION_PARSER_H
#define COKE_TOOLS_OPTION_PARSER_H

#include <algorithm>
#include <bit>
#include <cctype>
#include <cinttypes>
#include <climits>
#include <concepts>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <numeric>
#include <ostream>
#include <string>
#include <type_traits>
#include <vector>

namespace coke {

/**
 * @brief NULL_SHORT_NAME is used to indicate that the option doesn't has short
 *        name, its value is always char(0).
 */
constexpr const char NULL_SHORT_NAME = 0;

/**
 * @brief NULL_LONG_NAME is used to indicate that the option doesn't has long
 *        name, its value is always empty string.
 */
constexpr const char *NULL_LONG_NAME = "";

namespace detail {

/**
 * @brief Check whether `c` is an valid short option.
 * Valid short option is [0-9a-zA-Z].
 */
inline bool is_valid_option(char c);

/**
 * @brief Check whether `s` is an valid long option.
 * Valid long option is [0-9a-zA-z][-._0-9a-zA-Z]*.
 */
inline bool is_valid_option(const std::string &s);

/**
 * @brief Merge option name to "-n, --long-name" format.
 */
inline std::string merge_option_name(char c, const std::string &s);

/**
 * @brief Show option name in usage.
 * The usage format is like:
 * |  -a, --a-long       info for a
 * |  -b, --b-long-name  info for b
 * @param width Max width occupied by the name part, aligned with spaces.
 */
inline void option_show_name(std::ostream &os, std::size_t width,
                             char short_name, const std::string &long_name);

/**
 * @brief Parse signed integer from str.
 */
template<typename T>
    requires std::signed_integral<T>
bool parse_integer(const std::string &str, T &val);

/**
 * @brief Parse unsigned integer from str.
 */
template<typename T>
    requires std::unsigned_integral<T>
bool parse_integer(const std::string &str, T &val);

/**
 * @brief Parse floating point number from str, just call func like
 *        std::strtod.
 */
template<typename T>
    requires std::floating_point<T>
bool parse_floating(const std::string &str, T &val);

inline std::string quoted_string(const std::string &str);

/**
 * @brief Parse data unit from str, support B, KB, MB, GB, TB, PB, EB.
 *
 * Both uppercase and lowercase are acceptable. If the unit is not specified,
 * the default is B. Negative numbers are not allowed.
 */
inline bool parse_data_unit(const std::string &str, uintmax_t &val);

/**
 * @brief Convert data unit back to string.
 */
inline std::string data_unit_to_string(uintmax_t val);

} // namespace detail

/**
 * @brief Base Exception class for option errors.
 */
class OptionError : public std::exception {
public:
    OptionError() noexcept : OptionError("") {}

    OptionError(const std::string &err) noexcept
        : OptionError(NULL_SHORT_NAME, NULL_LONG_NAME, err)
    {
    }

    OptionError(char short_name, const std::string &err)
        : OptionError(short_name, NULL_LONG_NAME, err)
    {
    }

    OptionError(const std::string long_name, const std::string &err)
        : OptionError(NULL_SHORT_NAME, long_name, err)
    {
    }

    OptionError(char short_name, const std::string &long_name,
                const std::string &error) noexcept
        : short_name(short_name), long_name(long_name), error(error)
    {
    }

    OptionError(const OptionError &) = default;
    OptionError &operator=(const OptionError &) = default;

    virtual ~OptionError() = default;

    bool has_short_name() const noexcept
    {
        return short_name != NULL_SHORT_NAME;
    }

    bool has_long_name() const noexcept { return long_name != NULL_LONG_NAME; }

    char get_short_name() const noexcept { return short_name; }

    const std::string &get_long_name() const noexcept { return long_name; }

    std::string get_merged_name() const
    {
        return detail::merge_option_name(short_name, long_name);
    }

    virtual const char *what() const noexcept override { return error.c_str(); }

    virtual const std::string &what_str() const noexcept { return error; }

protected:
    char short_name;
    std::string long_name;
    std::string error;
};

/**
 * @brief Same option name add to one OptionParser.
 */
class DuplicateOptionError : public OptionError {
public:
    DuplicateOptionError(char c) noexcept
        : OptionError(c, "Duplicate option -" + std::string(1, c))
    {
    }

    DuplicateOptionError(const std::string &name) noexcept
        : OptionError(name, "Duplicate option --" + name)
    {
    }
};

/**
 * @brief The option name is invalid.
 */
class InvalidOptionError : public OptionError {
public:
    InvalidOptionError(char c) noexcept
        : OptionError(c, "Invalid option -" + std::string(1, c))
    {
    }

    InvalidOptionError(const std::string &name) noexcept
        : OptionError(name, "Invalid option --" + name)
    {
    }
};

/**
 * @brief The value cannot be converted to option value.
 */
class InvalidValueError : public OptionError {
public:
    InvalidValueError(char short_name, const std::string &type,
                      const std::string &value)
        : OptionError(short_name, "")
    {
        error.assign("Invalid value for -").push_back(short_name);
        error.append(", type is ").append(type);
        error.append(", value is ").append(value);
    }

    InvalidValueError(const std::string &long_name, const std::string &type,
                      const std::string &value)
        : OptionError(long_name, "")
    {
        error.assign("Invalid value for --").append(long_name);
        error.append(", type is ").append(type);
        error.append(", value is ").append(value);
    }
};

/**
 * @brief This option name is not registered in OptionParser, but appears in
 *        argv.
 */
class NoSuchOptionError : public OptionError {
public:
    NoSuchOptionError(char short_name) : OptionError(short_name, "")
    {
        error.assign("No short option named -").push_back(short_name);
    }

    NoSuchOptionError(const std::string &long_name) : OptionError(long_name, "")
    {
        error.assign("No long option named --").append(long_name);
    }
};

/**
 * @brief The option requires value but not set, or not requires value but set.
 */
class UnexpectedValueError : public OptionError {
public:
    UnexpectedValueError(char short_name, bool has_value)
        : OptionError(short_name, "")
    {
        if (has_value)
            error.assign("Value not required but set for -");
        else
            error.assign("Value required but not set for -");

        error.push_back(short_name);
    }

    UnexpectedValueError(const std::string &long_name, bool has_value)
        : OptionError(long_name, "")
    {
        if (has_value)
            error.assign("Value not required but set for --");
        else
            error.assign("Value required but not set for --");

        error.append(long_name);
    }
};

/**
 * @brief The option is required but not set.
 */
class OptionNotSetError : public OptionError {
public:
    OptionNotSetError(char short_name) : OptionError(short_name, "")
    {
        error.assign("Option -").push_back(short_name);
        error.append(" is required but not set");
    }

    OptionNotSetError(const std::string &long_name) : OptionError(long_name, "")
    {
        error.assign("Option --").append(long_name);
        error.append(" is required but not set");
    }
};

/**
 * @brief The validator return an error.
 */
class ValidateError : public OptionError {
public:
    ValidateError(char short_name, const std::string &err)
        : OptionError(short_name, "")
    {
        error.assign("Option -").push_back(short_name);
        error.append(" validate error: ").append(err);
    }

    ValidateError(const std::string &long_name, const std::string &err)
        : OptionError(long_name, "")
    {
        error.assign("Option --").append(long_name);
        error.append(" validate error: ").append(err);
    }
};

/**
 * @brief Option trait for integers, such as short, int, long, long long and
 *        its unsigned types. For convenience, their names will be displayed as
 *        intXX_t or uintXX_t.
 */
template<typename T>
    requires std::integral<T>
struct IntegerOptionTrait {
    using ValueType = T;

    static std::string get_type_str()
    {
        constexpr bool is_signed = std::is_signed<T>::value;

        std::string tstr;
        tstr.assign(is_signed ? "int" : "uint")
            .append(std::to_string(sizeof(T) * CHAR_BIT))
            .append("_t");
        return tstr;
    }

    static std::string to_string(const T &val) { return std::to_string(val); }

    static bool from_string(const std::string &str, T &val, bool)
    {
        return detail::parse_integer(str, val);
    }
};

/**
 * @brief Option trait for float, double, long double. The fixed width floating
 *        point types are not supported now.
 */
template<typename T>
    requires std::floating_point<T>
struct FloatingOptionTrait {
    using ValueType = T;

    static std::string get_type_str()
    {
        if constexpr (std::is_same_v<T, float>)
            return "float";
        else if constexpr (std::is_same_v<T, double>)
            return "double";
        else if constexpr (std::is_same_v<T, long double>)
            return "long double";
        else
            return "unknown";
    }

    static std::string to_string(const T &val) { return std::to_string(val); }

    static bool from_string(const std::string &str, T &val, bool)
    {
        return detail::parse_floating(str, val);
    }
};

/**
 * @brief Option trait for flags, the flag option has no value, just set flag to
 *        true when option is specified.
 */
struct FlagOptionTrait {
    using ValueType = bool;

    static std::string get_type_str() { return "Flag"; }

    static std::string to_string(const bool &val)
    {
        return val ? "true" : "false";
    }

    static bool from_string(const std::string &, bool &val, bool)
    {
        val = true;
        return true;
    }
};

/**
 * @brief Option trait for countable flags, unlike FlagOption, CountableOption
 *        increment inner value by one for each time the option is specified.
 */
struct CountableOptionTrait {
    using ValueType = int;

    static std::string get_type_str() { return "CountableFlag"; }

    static std::string to_string(const int &val)
    {
        return "CountableFlag " + std::to_string(val);
    }

    static bool from_string(const std::string &, int &val, bool)
    {
        ++val;
        return true;
    }
};

/**
 * @brief Option trait for bool, unlike FlagOption, BoolOption should use
 *        "true", "yes", "y" or "false", "no", "n" to specify the value.
 */
struct BoolOptionTrait {
    using ValueType = bool;

    static std::string get_type_str() { return "bool"; }

    static std::string to_string(const bool &val)
    {
        return val ? "true" : "false";
    }

    static bool from_string(const std::string &str, bool &val, bool)
    {
        std::string tmp = str;

        for (char &c : tmp)
            c = std::tolower(c);

        if (tmp == "true" || tmp == "yes" || tmp == "y") {
            val = true;
            return true;
        }
        else if (tmp == "false" || tmp == "no" || tmp == "n") {
            val = false;
            return true;
        }

        return false;
    }
};

/**
 * @brief Option trait for std::string.
 */
struct StringOptionTrait {
    using ValueType = std::string;

    static std::string get_type_str() { return "std::string"; }

    static std::string to_string(const std::string &val)
    {
        return detail::quoted_string(val);
    }

    static bool from_string(const std::string &str, std::string &val, bool)
    {
        val = str;
        return true;
    }
};

/**
 * @brief Option trait for data unit, the format of data unit is 16M, 1MB2K3B,
 *        128kb, etc.
 * @tparam T Only unsigned integer type supported.
 */
template<typename T>
    requires std::unsigned_integral<T>
struct DataUnitTrait {
    using ValueType = T;

    static std::string get_type_str()
    {
        return "DataUnit<" + IntegerOptionTrait<T>::get_type_str() + ">";
    }

    static std::string to_string(const T &val)
    {
        return detail::data_unit_to_string((uintmax_t)val);
    }

    static bool from_string(const std::string &str, T &val, bool)
    {
        constexpr auto TMAX = std::numeric_limits<T>::max();
        uintmax_t u;

        if (detail::parse_data_unit(str, u) && u <= TMAX) {
            val = static_cast<T>(u);
            return true;
        }

        return false;
    }
};

/**
 * @brief Option trait for multi values, unlike single trait, this trait
 *        supports store multiple specified options into a std::vector.
 * @tparam SingleTrait The trait for single option.
 */
template<typename SingleTrait>
struct MultiOptionTrait {
    using SingleTraitType = SingleTrait;
    using ValueType = std::vector<typename SingleTraitType::ValueType>;

    static std::string get_type_str()
    {
        return "std::vector<" + SingleTraitType::get_type_str() + ">";
    }

    static std::string to_string(const ValueType &val)
    {
        std::string str("{");
        for (std::size_t i = 0; i < val.size(); i++) {
            if (i > 0)
                str.append(", ");
            str.append(SingleTraitType::to_string(val[i]));
        }
        str.append("}");
        return str;
    }

    /**
     * @brief Convert option str to val.
     * @param is_first true if this is the first time to call `from_string`
     *        with this val.
     */
    static bool from_string(const std::string &str, ValueType &val,
                            bool is_first)
    {
        if (is_first)
            val.clear();

        typename SingleTraitType::ValueType new_value;
        if (!SingleTraitType::from_string(str, new_value, is_first))
            return false;

        val.push_back(std::move(new_value));
        return true;
    }
};

/**
 * @brief Base class for option types.
 */
class OptionBase {
public:
    virtual ~OptionBase() = default;

    bool is_required() const { return required; }
    bool is_option_set() const { return opt_set; }
    bool has_default_value() const { return has_default; }
    bool option_has_value() const { return has_value; }
    bool has_short_name() const { return short_name != NULL_SHORT_NAME; }
    bool has_long_name() const { return long_name != NULL_LONG_NAME; }

    char get_short_name() const { return short_name; }
    const std::string &get_long_name() const { return long_name; }

    const std::string &get_description() const { return desc; }

    std::string merged_option_name() const
    {
        return detail::merge_option_name(short_name, long_name);
    }

    const std::vector<std::string> &get_long_descriptions() const
    {
        return long_desc;
    }

    std::size_t head_width()
    {
        if (has_short_name() && has_long_name())
            return get_long_name().size() + 6;
        else if (has_short_name())
            return 2;
        else
            return get_long_name().size() + 2;
    }

    void show_usage(std::ostream &os, std::size_t width);

    void show_value(std::ostream &os, std::size_t width);

    virtual std::string get_type_str() = 0;
    virtual std::string get_default_str() = 0;
    virtual std::string get_value_str() = 0;
    virtual bool set_value(const std::string &) = 0;
    virtual bool validate(std::string &err) = 0;

protected:
    OptionBase(char short_name, const std::string &long_name, bool required,
               const std::string &desc)
        : required(required), opt_set(false), has_default(false),
          has_value(true), short_name(short_name), long_name(long_name),
          desc(desc)
    {
    }

    void set_has_value(bool has) { has_value = has; }

    friend class OptionParser;

protected:
    bool required;
    bool opt_set;
    bool has_default;
    bool has_value;
    char short_name;
    std::string long_name;
    std::string desc;
    std::vector<std::string> long_desc;
};

/**
 * @brief Basic implementation of option types.
 */
template<typename T, typename Trait>
class BasicOption : public OptionBase {
public:
    using ValueType = T;
    using TraitType = Trait;

    /**
     * @brief Type of validator, which receive reference of value and error
     *        string. Return whether the value is a valid option value, if
     *        false is returned, write the error info into error string.
     */
    using ValidatorType = std::function<bool(ValueType &, std::string &)>;

    BasicOption(ValueType *value, char short_name, const std::string &long_name,
                bool required, const std::string &desc)
        : OptionBase(short_name, long_name, required, desc), value(value)
    {
    }

    /**
     * @brief Get the option type as string.
     */
    std::string get_type_str() override { return TraitType::get_type_str(); }

    /**
     * @brief Get the option's default value as string.
     */
    std::string get_default_str() override
    {
        if (has_default_value())
            return TraitType::to_string(dft);
        else
            return "";
    }

    /**
     * @brief Get the option's value as string.
     */
    std::string get_value_str() override
    {
        return TraitType::to_string(*value);
    }

    /**
     * @brief Set the default value for this option, if the option not set by
     *        command line argv, the default value is used.
     */
    BasicOption &set_default(const T &t) &
    {
        dft = t;
        *value = t;
        has_default = true;
        return *this;
    }

    /**
     * @brief Set short description for this option, make it simple and can be
     *        displayed in one line.
     */
    BasicOption &set_description(const std::string &desc)
    {
        this->desc = desc;
        return *this;
    }

    /**
     * @brief Set long description lines. These lines will print after short
     *        description.
     */
    BasicOption &set_long_descriptions(const std::vector<std::string> &descs)
    {
        this->long_desc = descs;
        return *this;
    }

    /**
     * @brief Set whether this option is required or not.
     */
    BasicOption &set_required(bool required = true)
    {
        this->required = required;
        return *this;
    }

    /**
     * @brief Set the validator to check whether the value of this option is
     *        valid.
     * @param validator See ValidatorType.
     */
    BasicOption &set_validator(ValidatorType validator)
    {
        this->validator = std::move(validator);
        return *this;
    }

    /**
     * @brief Parse and set value to this option from str.
     * This function is called by class OptionParser when the name and value
     * of this option is specified in command line argv.
     */
    bool set_value(const std::string &str) override
    {
        opt_set = TraitType::from_string(str, *value, !opt_set);
        return opt_set;
    }

    /**
     * @brief Check whether the value is valid use validator.
     * @retval true If value is valid.
     * @retval false If value is invalid and the reason is in err.
     */
    bool validate(std::string &err) override
    {
        if (!validator)
            return true;

        return validator(*value, err);
    }

private:
    ValueType *value;
    ValueType dft;
    std::function<bool(ValueType &, std::string &)> validator;
};

template<typename T>
    requires std::integral<T>
using IntegerOption = BasicOption<T, IntegerOptionTrait<T>>;

template<typename T>
    requires std::integral<T>
using MIntegerOption =
    BasicOption<std::vector<T>, MultiOptionTrait<IntegerOptionTrait<T>>>;

template<typename T>
    requires std::floating_point<T>
using FloatingOption = BasicOption<T, FloatingOptionTrait<T>>;

template<typename T>
    requires std::floating_point<T>
using MFloatingOption =
    BasicOption<std::vector<T>, MultiOptionTrait<FloatingOptionTrait<T>>>;

using FlagOption = BasicOption<bool, FlagOptionTrait>;

using CountableOption = BasicOption<int, CountableOptionTrait>;

using BoolOption = BasicOption<bool, BoolOptionTrait>;

using StringOption = BasicOption<std::string, StringOptionTrait>;

using MStringOption =
    BasicOption<std::vector<std::string>, MultiOptionTrait<StringOptionTrait>>;

template<typename T>
    requires std::unsigned_integral<T>
using DataUnitOption = BasicOption<T, DataUnitTrait<T>>;

class OptionParser {
public:
    OptionParser() = default;
    OptionParser(const OptionParser &) = delete;

    std::vector<std::string> get_extra_args() { return extra_args; }

    /**
     * @brief Set program name.
     * If non empty program name is set by this function, the argv[0] will
     * be skipped when parse; else the argv[0] will be treated as program name.
     */
    void set_program(const std::string &s) { program = s; }

    /**
     * @brief Set extra prompt on the usage line.
     * An usage line would be like
     * program_name [required options]... extra_prompt
     */
    void set_extra_prompt(const std::string &s) { extra_prompt = s; }

    /**
     * @brief Set more content on usage page.
     */
    void set_more_usage(const std::vector<std::string> &content)
    {
        more_usage = content;
    }

    /**
     * @brief Set help option names to OptionParser.
     * If help flag is set and appears in argv, the call to `parse` will return
     * 1, you can also check `has_help_flag` to get whether there is help flag.
     */
    void set_help_flag(char short_name, const std::string &long_name)
    {
        short_help = short_name;
        long_help = long_name;

        add_flag(help_flag, short_name, long_name, "Show this help message.");
    }

    /**
     * @brief Add an integer option.
     *
     * @param val OptionParser store parse result into this value.
     * @param short_name Single character name, or NULL_SHORT_NAME for no short
     *        name. Valid short name is [0-9a-zA-Z].
     * @param long_name Long option name or NULL_LONG_NAME for no long name.
     *        Valid long name is [0-9a-zA-z][-._0-9a-zA-Z]*.
     * @param required Whether this option must be specified in argv.
     * @param desc The short descrition in usage page.
     * @return Reference to the added option, user can set other attributes
     *         with this reference.
     * @exception InvalidOptionError if short_name or long_name is not valid;
     *            DuplicateOptionError if same option name is added.
     */
    template<typename T>
        requires std::integral<T>
    IntegerOption<T> &
    add_integer(T &val, char short_name, const std::string &long_name,
                bool required = false, const std::string &desc = "")
    {
        return add<IntegerOption<T>>(&val, short_name, long_name, required,
                                     desc);
    }

    /**
     * @brief Add multi-integer option.
     * Same as add_integer, but store all values into std::vector for each
     * time the option is specified.
     */
    template<typename T>
        requires std::integral<T>
    MIntegerOption<T> &add_multi_integer(std::vector<T> &val, char short_name,
                                         const std::string &long_name,
                                         bool required = false,
                                         const std::string &desc = "")
    {
        return add<MIntegerOption<T>>(&val, short_name, long_name, required,
                                      desc);
    }

    /**
     * @brief Add floating option. See add_integer.
     */
    template<typename T>
        requires std::floating_point<T>
    FloatingOption<T> &
    add_floating(T &val, char short_name, const std::string &long_name,
                 bool required = false, const std::string &desc = "")
    {
        return add<FloatingOption<T>>(&val, short_name, long_name, required,
                                      desc);
    }

    /**
     * @brief Add multi-floating option.
     * Same as add_floating, but store all values into std::vector for each
     * time the option is specified.
     */
    template<typename T>
        requires std::floating_point<T>
    MFloatingOption<T> &add_multi_floating(std::vector<T> &val, char short_name,
                                           const std::string &long_name,
                                           bool required = false,
                                           const std::string &desc = "")
    {
        return add<MFloatingOption<T>>(&val, short_name, long_name, required,
                                       desc);
    }

    /**
     * @brief Add flag option. See add_integer.
     */
    FlagOption &add_flag(bool &val, char short_name,
                         const std::string &long_name,
                         const std::string &desc = "")
    {
        auto &opt = add<FlagOption>(&val, short_name, long_name, false, desc);
        opt.set_has_value(false);
        val = false;
        return opt;
    }

    /**
     * @brief Add countable flag option. See add_integer.
     */
    CountableOption &add_countable_flag(int &val, char short_name,
                                        const std::string &long_name,
                                        const std::string &desc = "")
    {
        auto &opt = add<CountableOption>(&val, short_name, long_name, false,
                                         desc);
        opt.set_has_value(false);
        val = 0;
        return opt;
    }

    /**
     * @brief Add bool option. See add_integer.
     */
    BoolOption &add_bool(bool &val, char short_name,
                         const std::string &long_name, bool required = false,
                         const std::string &desc = "")
    {
        return add<BoolOption>(&val, short_name, long_name, required, desc);
    }

    /**
     * @brief Add string option. See add_integer.
     */
    StringOption &add_string(std::string &val, char short_name,
                             const std::string &long_name,
                             bool required = false,
                             const std::string &desc = "")
    {
        return add<StringOption>(&val, short_name, long_name, required, desc);
    }

    /**
     * @brief Add multi-string option.
     * Same as add_string, but store all values into std::vector for each
     * time the option is specified.
     */
    MStringOption &add_multi_string(std::vector<std::string> &val,
                                    char short_name,
                                    const std::string &long_name,
                                    bool required = false,
                                    const std::string &desc = "")
    {
        return add<MStringOption>(&val, short_name, long_name, required, desc);
    }

    /**
     * @brief Add data unit option, support B, KB, MB, GB, TB, PB, EB, the base
     *        is 1024, which means 1KB = 1024B. Multi unit can join togegher,
     *        for example, 1M2KB3B.
     *
     * Both uppercase and lowercase are acceptable. If the unit is not
     * specified, the default is B. Negative numbers are not allowed.
     */
    template<typename T>
        requires std::unsigned_integral<T>
    DataUnitOption<T> &
    add_data_unit(T &val, char short_name, const std::string &long_name,
                  bool required = false, const std::string &desc = "")
    {
        return add<DataUnitOption<T>>(&val, short_name, long_name, required,
                                      desc);
    }

    /**
     * @brief Add user defined option.
     * @tparam OptionType A subclass type of OptionBase, usually BasicOption
     *         with user-defined option traits.
     * @return A reference to an OptionType object, through which the user can
     *         modify its properties, especially those that cannot be passed as
     *         parameters to this function.
     */
    template<typename OptionType>
        requires std::derived_from<OptionType, OptionBase>
    OptionType &add_user_defined(typename OptionType::ValueType &val,
                                 char short_name, const std::string &long_name,
                                 bool required = false,
                                 const std::string &desc = "")
    {
        return add<OptionType>(&val, short_name, long_name, required, desc);
    }

    /**
     * @brief Parse argv to options.
     * @retval 0 If parse success.
     * @retval 1 If there are help flag, required check and validate will be
     *         skipped in this case.
     * @retval -1 If an error occurs, error info is stored in err.
     */
    int parse(int argc, const char *const argv[], std::string &err)
    {
        try {
            return parse(argc, argv);
        }
        catch (const OptionError &e) {
            err = e.what_str();
            return -1;
        }
    }

    /**
     * @brief Parse argv to options, but throw exception on error.
     * @exception NoSuchOptionError, UnexpectedValueError, InvalidValueError,
     *            OptionNotSetError.
     */
    int parse(int argc, const char *const argv[])
    {
        args.clear();

        for (int i = 0; i < argc; i++)
            if (argv[i])
                args.push_back(argv[i]);

        return do_parse();
    }

    /**
     * @brief Parse argv to options.
     * @retval 0 If parse success.
     * @retval 1 If there are help flag, required check and validate will be
     *         skipped in this case.
     * @retval -1 If an error occurs, error info is stored in err.
     */
    int parse(const std::vector<std::string> &argv, std::string &err)
    {
        try {
            return parse(argv);
        }
        catch (const OptionError &e) {
            err = e.what_str();
            return -1;
        }
    }

    /**
     * @brief Parse argv to options, but throw exception on error.
     * @exception NoSuchOptionError, UnexpectedValueError, InvalidValueError,
     *            OptionNotSetError.
     */
    int parse(const std::vector<std::string> &argv)
    {
        args = argv;
        return do_parse();
    }

    /**
     * @brief Show formatted usage page to ostream.
     */
    void usage(std::ostream &os);

    /**
     * @brief Show parsed values to ostream.
     * @param is_set If true, do not show values not set in argv.
     */
    void show_values(std::ostream &os, bool is_set = false);

    /**
     * @brief Get whether the parsing result contains help flag.
     * This function should be called after parse.
     */
    bool has_help_flag() const { return help_flag; }

private:
    int do_parse();

    bool try_parse_long(const std::string &opt);

    void try_parse_short_flag(char name);

    bool try_parse_short(const std::string &opt);

    template<typename Opt>
    auto add(typename Opt::ValueType *val, char short_name,
             const std::string &long_name, bool required,
             const std::string &desc) -> Opt &
    {
        if (short_name != NULL_SHORT_NAME) {
            if (!detail::is_valid_option(short_name))
                throw InvalidOptionError(short_name);

            if (short_opts.find(short_name) != short_opts.end())
                throw DuplicateOptionError(short_name);
        }

        if (long_name != NULL_LONG_NAME) {
            if (!detail::is_valid_option(long_name))
                throw InvalidOptionError(long_name);

            if (long_opts.find(long_name) != long_opts.end())
                throw DuplicateOptionError(long_name);
        }

        auto uptr = std::unique_ptr<Opt>(
            new Opt(val, short_name, long_name, required, desc));
        auto ptr = uptr.get();
        opts.push_back(std::move(uptr));

        if (short_name != NULL_SHORT_NAME)
            short_opts[short_name] = ptr;

        if (long_name != NULL_LONG_NAME)
            long_opts[long_name] = ptr;

        return *ptr;
    }

private:
    std::vector<std::unique_ptr<OptionBase>> opts;
    std::map<char, OptionBase *> short_opts;
    std::map<std::string, OptionBase *> long_opts;

    bool help_flag = false;
    char short_help = 0;
    std::string long_help;

    std::string program;
    std::string extra_prompt;
    std::vector<std::string> more_usage;

    std::size_t cur_idx;
    std::size_t total_args;
    std::vector<std::string> extra_args;
    std::vector<std::string> args;
};

inline void OptionBase::show_usage(std::ostream &os, std::size_t width)
{
    detail::option_show_name(os, width, get_short_name(), get_long_name());

    os << "type " << get_type_str();

    if (is_required())
        os << ", required";

    if (has_default_value())
        os << ", default " << get_default_str();

    os << ".";

    const auto &d = get_description();
    if (!d.empty())
        os << " " << d;

    os << "\n";

    const auto &ds = get_long_descriptions();
    if (!ds.empty()) {
        for (const auto &s : ds)
            os << std::string(width + 2, ' ') << s << "\n";
        os << "\n";
    }
}

inline void OptionBase::show_value(std::ostream &os, std::size_t width)
{
    detail::option_show_name(os, width, get_short_name(), get_long_name());
    os << get_value_str() << "\n";
}

inline int OptionParser::do_parse()
{
    cur_idx = 1;
    total_args = args.size();
    help_flag = false;
    extra_args.clear();

    if (program.empty() && total_args > 0)
        program = args[0];

    while (cur_idx < total_args) {
        std::string option = args[cur_idx++];

        // ignore options after "--" or "-"
        if (option == "--" || option == "-")
            break;

        if (!try_parse_long(option) && !try_parse_short(option))
            extra_args.push_back(option);
    }

    while (cur_idx < total_args)
        extra_args.push_back(args[cur_idx++]);

    // When there are help flag, required check and validate is skipped
    if (help_flag)
        return 1;

    std::string validate_error;
    for (auto &ptr : opts) {
        if (ptr->is_required() && !ptr->is_option_set()) {
            if (ptr->has_short_name())
                throw OptionNotSetError(ptr->get_short_name());
            else
                throw OptionNotSetError(ptr->get_long_name());
        }

        validate_error.clear();
        if (!ptr->validate(validate_error)) {
            if (ptr->has_short_name())
                throw ValidateError(ptr->get_short_name(), validate_error);
            else
                throw ValidateError(ptr->get_long_name(), validate_error);
        }
    }

    return 0;
}

inline bool OptionParser::try_parse_long(const std::string &opt)
{
    std::string name, value;
    std::size_t pos;
    bool value_in_name = false;

    if (opt.size() < 3 || opt[0] != '-' || opt[1] != '-')
        return false;

    pos = opt.find('=');
    if (pos != std::string::npos) {
        name = opt.substr(2, pos - 2);
        value = opt.substr(pos + 1);
        value_in_name = true;
    }
    else {
        name = opt.substr(2);
    }

    auto it = long_opts.find(name);
    if (it == long_opts.end())
        throw NoSuchOptionError(name);

    auto opt_ptr = it->second;
    if (!opt_ptr->option_has_value()) {
        if (value_in_name)
            throw UnexpectedValueError(name, true);
    }
    else if (!value_in_name) {
        if (cur_idx >= total_args)
            throw UnexpectedValueError(name, false);
        else
            value = args[cur_idx++];
    }

    if (!opt_ptr->set_value(value))
        throw InvalidValueError(name, opt_ptr->get_type_str(), value);

    return true;
}

inline void OptionParser::try_parse_short_flag(char name)
{
    auto it = short_opts.find(name);
    if (it == short_opts.end())
        throw NoSuchOptionError(name);

    auto opt_ptr = it->second;
    if (opt_ptr->option_has_value())
        throw UnexpectedValueError(name, false);

    opt_ptr->set_value("");
}

inline bool OptionParser::try_parse_short(const std::string &opt)
{
    std::string value;
    std::size_t pos;
    bool value_in_name = false;

    if (opt.size() < 2 || opt[0] != '-' || opt[1] == '=')
        return false;

    pos = opt.find('=');
    if (pos != std::string::npos) {
        value = opt.substr(pos + 1);
        value_in_name = true;
    }
    else
        pos = opt.size();

    for (std::size_t i = 1; i + 1 < pos; i++)
        try_parse_short_flag(opt[i]);

    // last short option
    char name = opt[pos - 1];
    auto it = short_opts.find(name);
    if (it == short_opts.end())
        throw NoSuchOptionError(name);

    auto opt_ptr = it->second;
    if (!opt_ptr->option_has_value()) {
        if (value_in_name)
            throw UnexpectedValueError(name, true);
    }
    else if (!value_in_name) {
        if (cur_idx >= total_args)
            throw UnexpectedValueError(name, false);
        else
            value = args[cur_idx++];
    }

    if (!opt_ptr->set_value(value))
        throw InvalidValueError(name, opt_ptr->get_type_str(), value);

    return true;
}

inline void OptionParser::usage(std::ostream &os)
{
    std::size_t max_width = 0;
    bool has_not_required = false;

    for (auto &ptr : opts)
        max_width = std::max(max_width, ptr->head_width());

    max_width += (max_width % 2 == 0) ? 2 : 1;

    os << program;
    for (auto &ptr : opts) {
        if (ptr->is_required()) {
            if (ptr->has_short_name())
                os << " -" << ptr->get_short_name();
            else
                os << " --" << ptr->get_long_name();
            os << " <" << ptr->get_type_str() << ">";
        }
        else
            has_not_required = true;
    }

    if (has_not_required)
        os << " [Options]...";
    if (!extra_prompt.empty())
        os << " " << extra_prompt;

    os << "\n\nOPTIONS:\n";

    for (auto &ptr : opts)
        ptr->show_usage(os, max_width);

    if (!more_usage.empty()) {
        os << "\n";
        for (const auto &footer : more_usage)
            os << footer << "\n";
    }
}

inline void OptionParser::show_values(std::ostream &os, bool is_set)
{
    std::size_t max_width = 0;

    for (auto &ptr : opts)
        max_width = std::max(max_width, ptr->head_width());

    max_width += (max_width % 2 == 0) ? 2 : 1;

    if (is_set) {
        for (auto &ptr : opts)
            if (ptr->is_option_set())
                ptr->show_value(os, max_width);
    }
    else {
        for (auto &ptr : opts)
            ptr->show_value(os, max_width);
    }
}

namespace detail {

inline bool is_valid_option(char c)
{
    return std::isalnum(c);
}

inline bool is_valid_option(const std::string &s)
{
    if (s.empty() || !is_valid_option(s[0]))
        return false;

    // extra valid chars in long option
    const std::string ext("-._");

    for (std::size_t i = 1; i < s.size(); i++)
        if (!is_valid_option(s[i]) && ext.find(s[i]) == std::string::npos)
            return false;

    return true;
}

inline std::string merge_option_name(char c, const std::string &s)
{
    std::string name;

    if (c != NULL_SHORT_NAME)
        name.append("-").push_back(c);

    if (s != NULL_LONG_NAME) {
        if (!name.empty())
            name.append(", ");
        name.append("--").append(s);
    }

    return name;
}

inline void option_show_name(std::ostream &os, std::size_t width,
                             char short_name, const std::string &long_name)
{
    std::size_t w = 0;
    bool has_long_name = (long_name != NULL_LONG_NAME);
    os << "  ";

    if (short_name != NULL_SHORT_NAME) {
        os << "-" << short_name << (has_long_name ? ", " : "  ");
        w += 4;
    }

    if (has_long_name) {
        os << "--" << long_name;
        w += 2 + long_name.size();
    }

    if (w < width)
        os << std::string(width - w, ' ');
}

template<typename T>
    requires std::signed_integral<T>
bool parse_integer(const std::string &str, T &val)
{
    constexpr T TMIN = std::numeric_limits<T>::min();
    constexpr T TMAX = std::numeric_limits<T>::max();

    const char *first = str.c_str();
    const char *last = str.c_str() + str.size();
    char *end = nullptr;
    errno = 0;

    intmax_t tmp = std::strtoimax(first, &end, 10);
    if (errno == ERANGE || end != last)
        return false;

    if (tmp < TMIN || tmp > TMAX)
        return false;

    val = tmp;
    return true;
}

template<typename T>
    requires std::unsigned_integral<T>
bool parse_integer(const std::string &str, T &val)
{
    constexpr T TMAX = std::numeric_limits<T>::max();

    const char *first = str.c_str();
    const char *last = str.c_str() + str.size();
    char *end = nullptr;
    errno = 0;

    uintmax_t tmp = std::strtoumax(first, &end, 10);
    if (errno == ERANGE || end != last)
        return false;

    if (tmp > TMAX)
        return false;

    val = tmp;
    return true;
}

template<typename T>
    requires std::floating_point<T>
bool parse_floating(const std::string &str, T &val)
{
    const char *first = str.c_str();
    const char *last = str.c_str() + str.size();
    char *end = nullptr;
    errno = 0;

    T v;
    if constexpr (std::is_same_v<T, float>)
        v = std::strtof(first, &end);
    else if constexpr (std::is_same_v<T, double>)
        v = std::strtod(first, &end);
    else if constexpr (std::is_same_v<T, long double>)
        v = std::strtold(first, &end);
    else
        static_assert(!std::is_same_v<T, T>,
                      "unsupported floating point number");

    if (errno == ERANGE || end != last)
        return false;

    val = v;
    return true;
}

inline std::string quoted_string(const std::string &str)
{
    std::string res("\"");

    for (char c : str) {
        if (c == '"' || c == '\\')
            res.push_back('\\');
        res.push_back(c);
    }
    res.push_back('"');

    return res;
}

inline int get_data_unit_shift(char c)
{
    switch (std::tolower(c)) {
    case 'b':
        return 0;
    case 'k':
        return 10;
    case 'm':
        return 20;
    case 'g':
        return 30;
    case 't':
        return 40;
    case 'p':
        return 50;
    case 'e':
        return 60;
    default:
        return -1;
    }
}

inline bool parse_data_unit(const std::string &str, uintmax_t &val)
{
    const char *first = str.c_str();
    const char *last = str.c_str() + str.size();
    uintmax_t total = 0, tmp;
    char *end;
    int sft, lft;

    if (first == last)
        return false;

    while (first != last) {
        end = nullptr;
        errno = 0;

        tmp = std::strtoumax(first, &end, 10);
        if (errno == ERANGE || first == end)
            return false;

        first = end;
        if (first == last)
            sft = 0;
        else {
            sft = get_data_unit_shift(*first);
            ++first;
        }

        lft = std::countl_zero(tmp);
        if (sft < 0 || sft > lft)
            return false;
        else if (tmp > 0)
            tmp <<= sft;

        if (sft > 0 && (*first == 'b' || *first == 'B'))
            ++first;

        // overflow
        if (total + tmp < total)
            return false;
        total += tmp;
    }

    val = total;
    return true;
}

inline std::string data_unit_to_string(uintmax_t val)
{
    constexpr int N = 7;
    constexpr int sfts[N] = {0, 10, 20, 30, 40, 50, 60};
    constexpr char units[N] = {'B', 'K', 'M', 'G', 'T', 'P', 'E'};

    std::string str;
    uintmax_t u, t;
    int sft;

    for (int i = 6; i >= 0 && val > 0; i--) {
        sft = sfts[i];
        if (sft >= std::numeric_limits<uintmax_t>::digits)
            continue;

        u = (uintmax_t)1 << sft;
        if (val >= u) {
            t = val / u;
            val %= u;

            str.append(std::to_string(t)).push_back(units[i]);
        }
    }

    if (str.empty())
        str.assign("0B");

    return str;
}

} // namespace detail

} // namespace coke

#endif // COKE_TOOLS_OPTION_PARSER_H
