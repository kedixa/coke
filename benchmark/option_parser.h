/**
 * @file option_parser.h
 *
 * ATTENTION: This file is currently only for writing sample code, it is not
 *            part of the coke library. It may be modified, moved, renamed, or
 *            even deleted at any time. It may also be added to the coke library
 *            in the future.
*/

#ifndef COKE_OPTION_PARSER_H
#define COKE_OPTION_PARSER_H

#include <algorithm>
#include <cctype>
#include <exception>
#include <memory>
#include <map>
#include <numeric>
#include <string>
#include <type_traits>
#include <vector>

class OptionError : public std::exception {
public:
    OptionError() { }
    OptionError(const std::string &m) : m(m) { }

    const char *what() const noexcept override { return m.c_str(); }

protected:
    std::string m;
};

class DuplicateOptionError : public OptionError {
public:
    DuplicateOptionError(char c)
        : OptionError(std::string("Duplicate option -") + std::string(1, c))
    { }

    DuplicateOptionError(const std::string &s)
        : OptionError(std::string("Duplicate option --").append(s))
    { }
};

class InvalidRangeError : public OptionError {
public:
    InvalidRangeError(const std::string &s)
        : OptionError(s + " value out of range")
    { }
};

class InvalidOptionName : public OptionError {
public:
    static bool check(char c, const std::string &s) {
        std::string error;
        return check(c, s, error);
    }

    static bool check(char c, const std::string &s, std::string &e) {
        if (c == 0 && s.empty()) {
            e.assign("There is neigher short nor long option name");
            return false;
        }

        if (c != 0 && !std::isgraph(c)) {
            e.assign("Invalid character ").append(std::to_string((int)c));
            return false;
        }

        for (char ch : s) {
            if (!std::isgraph(ch)) {
                e.assign("Invalid character ").append(std::to_string((int)ch));
                return false;
            }
        }

        return true;
    }

    InvalidOptionName(char c, const std::string &s)
        : OptionError()
    {
        check(c, s, m);
    }
};


namespace {

inline
void option_show_head(std::ostream &os, std::size_t width,
                      char sname, const std::string &lname) {
    if (sname == 0)
        os << "      ";
    else
        os << "  -" << sname << (lname.empty() ? "  " : ", ");

    if (lname.empty())
        os << std::string(width - 4, ' ');
    else {
        os << "--" << lname;
        if (lname.size() + 6 < width)
            os << std::string(width - lname.size() - 6, ' ');
    }
}

template<typename T>
struct OptionTypeName;

#define OPTION_TYPE_NAME(T) template<> struct OptionTypeName<T> {\
    static constexpr const char *name() { return  #T; } \
}

OPTION_TYPE_NAME(std::string);
OPTION_TYPE_NAME(short);
OPTION_TYPE_NAME(int);
OPTION_TYPE_NAME(long);
OPTION_TYPE_NAME(long long);

OPTION_TYPE_NAME(unsigned short);
OPTION_TYPE_NAME(unsigned int);
OPTION_TYPE_NAME(unsigned long);
OPTION_TYPE_NAME(unsigned long long);

OPTION_TYPE_NAME(float);
OPTION_TYPE_NAME(double);
OPTION_TYPE_NAME(long double);

#undef OPTION_TYPE_NAME


template<typename T>
std::string option_value_to_string(const T &t) { return std::to_string(t); }

inline
std::string option_value_to_string(const std::string &t) { return t; }


inline
void option_setter(std::string *arg, const char *value) { arg->assign(value); }

template<typename T>
typename std::enable_if<
    std::is_integral<T>::value && std::is_signed<T>::value,
    void
>::type
option_setter(T *arg, const char *value) {
    static_assert(sizeof (T) <= sizeof (long long));

    long long t = std::stoll(value);
    if (t < std::numeric_limits<T>::min() || t > std::numeric_limits<T>::max())
        throw InvalidRangeError(value);

    *arg = static_cast<T>(t);
}

template<typename T>
typename std::enable_if<
    std::is_integral<T>::value && std::is_unsigned<T>::value,
    void
>::type
option_setter(T *arg, const char *value) {
    static_assert(sizeof (T) <= sizeof (unsigned long long));

    unsigned long long t = std::stoull(value);
    if (t > std::numeric_limits<T>::max())
        throw InvalidRangeError(value);

    *arg = static_cast<T>(t);
}

inline
void option_setter(float *arg, const char *value) { *arg = std::stof(value); }

inline
void option_setter(double *arg, const char *value) { *arg = std::stod(value); }

inline
void option_setter(long double *arg, const char *value) {
    *arg = std::stold(value);
}

} // unnamed namespace


class OptionBase {
public:
    virtual ~OptionBase() = default;

    bool required() { return is_required; }
    bool is_set() { return value_set; }
    bool has_default_value() { return has_default; }
    bool has_short_name() { return sname != 0; }
    bool has_long_name() { return !lname.empty(); }

    char short_name() { return sname; }
    std::string long_name() { return lname; }
    std::string description() { return desc; }
    std::string option_string() {
        std::string s;

        if (has_short_name())
            s.append("-").append(1, short_name());

        if (has_long_name()) {
            if (!s.empty())
                s.append(", ");
            s.append("--").append(long_name());
        }

        return s;
    }

    std::size_t head_width() { return lname.empty() ? 2 : lname.size() + 6; }

    void show_usage(std::ostream &os, std::size_t width) {
        option_show_head(os, width, sname, lname);

        os << "type: " << get_type_str();
        if (required())
            os << ", required";

        if (has_default_value())
            os << ", default: " << get_default_str();

        os << "\n";

        if (!desc.empty())
            os << std::string(width+2, ' ') << desc << "\n";
    }

    void show_values(std::ostream &os, std::size_t width) {
        option_show_head(os, width, sname, lname);
        os << get_value_str() << "\n";
    }

    // return whether this option should has a value
    virtual bool has_value() { return true; }

    virtual std::string get_type_str() = 0;
    virtual std::string get_default_str() = 0;
    virtual std::string get_value_str() = 0;
    virtual void set_value(const char *value) = 0;
    virtual void use_default() = 0;

protected:
    OptionBase(char sname, const std::string &lname,
               bool required, const std::string &desc)
        : is_required(required), value_set(false), has_default(false),
          sname(sname), lname(lname), desc(desc)
    { }

protected:
    bool is_required;
    bool value_set;
    bool has_default;
    char sname;
    std::string lname;
    std::string desc;
};

template<typename T, typename = void>
class OptionType : public OptionBase {
public:
    using ValueType = T;

    OptionType(ValueType *arg, char sname, const std::string &lname,
               bool is_required, const std::string &desc)
        : OptionBase(sname, lname, is_required, desc), arg(arg)
    { }

    std::string get_type_str() override {
        return OptionTypeName<ValueType>::name();
    }

    std::string get_default_str() override {
        return option_value_to_string(dft);
    }

    std::string get_value_str() override {
        return option_value_to_string(*arg);
    }

    void set_value(const char *value) override {
        option_setter(arg, value);
        value_set = true;
    }

    void use_default() override {
        *arg = dft;
        value_set = true;
    }

    void set_default(const T &t) {
        dft = t;
        has_default = true;
    }

private:
    ValueType *arg;
    ValueType dft;
};

class FlagOption : public OptionBase {
public:
    using ValueType = bool;

    FlagOption(ValueType *arg, char sname, const std::string &lname,
               bool /* unused */, const std::string &desc)
        : OptionBase(sname, lname, false, desc), arg(arg)
    {
        *arg = false;
    }

    bool has_value() override { return false; }

    std::string get_type_str() override { return "bool flag"; }

    std::string get_default_str() override { return ""; }

    std::string get_value_str() override { return *arg ? "true" : "false"; }

    void set_value(const char *) override {
        *arg = true;
        value_set = true;
    }

    void use_default() override { }

    // do nothing, just for template
    void set_default(const ValueType &) { }

private:
    ValueType *arg;
};

class OptionParser {
    using StrOption = OptionType<std::string>;
    using OptionPtr = std::shared_ptr<OptionBase>;

public:
    OptionParser() = default;
    OptionParser(const OptionParser &) = delete;

    // set program name
    void set_program(const std::string &s) { program = s; }

    // set prompt for extra command line arguments
    void set_extra_prompt(const std::string &s) { extra_prompt = s; }

    void set_help_flag(char sname, const std::string &lname) {
        short_help = sname;
        long_help = lname;

        add_flag(help_flag, sname, lname, "show this help message");
    }

    void add_string(std::string &arg, char sname, const std::string &lname,
                    bool required, const std::string &desc) {
        return add<StrOption>(&arg, sname, lname, required, desc, nullptr);
    }

    void add_string(std::string &arg, char sname, const std::string &lname,
                    bool required, const std::string &desc,
                    const std::string &dft) {
        return add<StrOption>(&arg, sname, lname, required, desc, &dft);
    }

    template<typename T>
    typename std::enable_if<std::is_integral<T>::value, void>::type
    add_integral(T &arg, char sname, const std::string &lname,
                 bool required, const std::string &desc) {
        return add<OptionType<T>>(&arg, sname, lname, required, desc, nullptr);
    }

    template<typename T>
    typename std::enable_if<std::is_integral<T>::value, void>::type
    add_integral(T &arg, char sname, const std::string &lname,
                 bool required, const std::string &desc,
                 const T &dft) {
        return add<OptionType<T>>(&arg, sname, lname, required, desc, &dft);
    }

    template<typename T>
    typename std::enable_if<std::is_floating_point<T>::value, void>::type
    add_floating(T &arg, char sname, const std::string &lname,
                 bool required, const std::string &desc) {
        return add<OptionType<T>>(&arg, sname, lname, required, desc, nullptr);
    }

    template<typename T>
    typename std::enable_if<std::is_floating_point<T>::value, void>::type
    add_floating(T &arg, char sname, const std::string &lname,
                 bool required, const std::string &desc,
                 const T &dft) {
        return add<OptionType<T>>(&arg, sname, lname, required, desc, &dft);
    }

    void add_flag(bool &arg, char sname, const std::string &lname,
                  const std::string &desc) {
        return add<FlagOption>(&arg, sname, lname, false, desc, nullptr);
    }

    int parse(int argc, char *argv[], std::string &err) {
        std::vector<std::string> v;

        for (int i = 0; i < argc; i++)
            if (argv[i])
                v.push_back(argv[i]);

        return parse(v, err);
    }

    int parse(const std::vector<std::string> &argv, std::string &err) {
        std::size_t idx = 0, total = argv.size();
        bool has_help = false;
        extra_args.clear();

        if (program.empty()) {
            if (total == 0) {
                err.assign("Program name not set");
                return -1;
            }

            program = argv[idx++];
        }

        while (idx < total) {
            std::string key = argv[idx++], opt;

            // ignore options after "--" or "-"
            if (key == "--" || key == "-")
                break;

            if (is_long_opt(key)) {
                opt = key.substr(2);

                if (opt == long_help) {
                    has_help = true;
                    continue;
                }

                auto it = long_opts.find(opt);
                if (it == long_opts.end()) {
                    err.assign("No long option named --").append(opt);
                    return -1;
                }

                auto arg_ptr = it->second;
                if (arg_ptr->has_value()) {
                    if (idx >= total) {
                        err.assign("No value for --").append(opt);
                        return -1;
                    }

                    arg_ptr->set_value(argv[idx++].c_str());
                }
            }
            else if (is_short_opt(key)) {
                for (std::size_t i = 1; i < key.size(); i++) {
                    char ch = key[i];

                    if (ch == short_help) {
                        has_help = 1;
                        continue;
                    }

                    auto it = short_opts.find(ch);
                    if (it == short_opts.end()) {
                        err.assign("No short option named -").push_back(ch);
                        return -1;
                    }

                    auto arg_ptr = it->second;
                    if (arg_ptr->has_value()) {
                        if (i + 1 != key.size()) {
                            err.assign("Only last short option can have value");
                            return -1;
                        }

                        if (idx >= total) {
                            err.assign("No value for -").push_back(ch);
                            return -1;
                        }

                        arg_ptr->set_value(argv[idx++].c_str());
                    }
                    else
                        arg_ptr->set_value(nullptr);
                }
            }
            else {
                extra_args.push_back(key);
            }
        }

        while (idx < total)
            extra_args.push_back(argv[idx++]);

        for (auto &ptr : opts) {
            if (!ptr->is_set() && ptr->has_default_value())
                ptr->use_default();
            else if (ptr->required() && !ptr->is_set()) {
                err.assign("Option ").append(ptr->option_string())
                   .append(" requires value but not set");
                return -1;
            }
        }

        return has_help ? 1 : 0;
    }

    // return extra arguments that cannot recognize or after '--'
    std::vector<std::string> get_extra_args() { return extra_args; }

    void usage(std::ostream &os) {
        std::size_t max_width = 0;

        for (auto &ptr : opts)
            max_width = std::max(max_width, ptr->head_width());

        max_width += (max_width % 2 == 0) ? 2 : 1;

        os << program;

        for (auto &ptr : opts) {
            if (ptr->required()) {
                if (ptr->has_short_name())
                    os << " -" << ptr->short_name();
                else
                    os << " --" << ptr->long_name();
                os << " [" << ptr->get_type_str() << "]";
            }
        }

        os << " [Options]...";
        if (!extra_prompt.empty())
            os << " " << extra_prompt;
        os << "\nOptions:\n";

        for (auto &ptr : opts)
            ptr->show_usage(os, max_width);

        if (!usage_footer.empty())
            os << "\n" << usage_footer << "\n";
    }

    void show_values(std::ostream &os) {
        std::size_t max_width = 0;
        for (auto &ptr : opts)
            max_width = std::max(max_width, ptr->head_width());

        max_width += (max_width % 2 == 0) ? 2 : 1;

        for (auto &ptr : opts) {
            // skip help option
            if (is_help_opt(ptr))
                continue;

            ptr->show_values(os, max_width);
        }
    }

private:
    template<typename Arg>
    void add(typename Arg::ValueType *arg, char sname, const std::string &lname,
             bool required, const std::string &desc,
             const typename Arg::ValueType *dft) {
        if (!InvalidOptionName::check(sname, lname))
            throw InvalidOptionName(sname, lname);

        if (sname != 0 && short_opts.find(sname) != short_opts.end())
            throw DuplicateOptionError(sname);

        if (!lname.empty() && long_opts.find(lname) != long_opts.end())
            throw DuplicateOptionError(lname);

        auto ptr = std::make_shared<Arg>(arg, sname, lname, required, desc);
        if (dft)
            ptr->set_default(*dft);

        opts.push_back(ptr);
        if (sname != 0)
            short_opts[sname] = ptr;

        if (!lname.empty())
            long_opts[lname] = ptr;
    }

    bool is_long_opt(const std::string &s) {
        return s.size() >= 3 && s[0] == '-' && s[1] == '-';
    }

    // after check long
    bool is_short_opt(const std::string &s) {
        return s.size() >= 2 && s[0] == '-';
    }

    bool is_help_opt(OptionPtr &ptr) {
        if (short_help && ptr->short_name() == short_help)
            return true;
        if (!long_help.empty() && ptr->long_name() == long_help)
            return true;
        return false;
    }

private:
    std::vector<OptionPtr> opts;
    std::map<char, OptionPtr> short_opts;
    std::map<std::string, OptionPtr> long_opts;

    bool help_flag = false;
    char short_help = 0;
    std::string long_help;

    std::string program;
    std::string extra_prompt;
    std::string usage_footer;
    std::vector<std::string> extra_args;
};

#endif // COKE_OPTION_PARSER_H
