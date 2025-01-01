使用下述功能需要包含头文件`coke/tools/option_parser.h`。


## 常量

下述常量均位于`coke`命名空间。

```cpp
// 当不需要短选项时，将NULL_SHORT_NAME传递给short_name参数
constexpr const char NULL_SHORT_NAME = 0;

// 当不需要长选项时，将NULL_LONG_NAME传递给long_name参数
constexpr const char *NULL_LONG_NAME = "";
```


## coke::OptionParser

`coke::OptionParser`是一个命令行参数解析器，支持解析整数、浮点数、字符串等类型的参数，支持长选项和短选项，以及支持自动生成帮助页面。

合法的短选项为正则表达式`[0-9a-zA-Z]`表示的范围，合法的长选项为正则表达式`[0-9a-zA-z][-._0-9a-zA-Z]*`表示的范围。

### 添加选项

除用户自定义的选项外，其它添加选项的函数返回类型是类模板`coke::BasicOption`实例的引用，该引用的生命周期与当前`coke::OptionParser`一致，可使用该引用进一步设置其他属性。

- 添加整数选项

    其中`val`用于保存解析后的结果，`short_name`为短选项的名称，`long_name`为长选项的名称，`required`用于标记该选项是否必须指定，`desc`为该选项的简单描述。

    使用`add_integer`添加的选项，当多次指定同一个选项时，后指定的值将会覆盖先指定的值，而多值选项`add_multi_integer`会将多个值依次添加到结果`val`中。

    ```cpp
    template<typename T>
        requires std::integral<T>
    IntegerOption<T> &
    add_integer(T &val, char short_name, const std::string &long_name,
                bool required = false, const std::string &desc = "");

    template<typename T>
        requires std::integral<T>
    MIntegerOption<T> &
    add_multi_integer(std::vector<T> &val,
                      char short_name, const std::string &long_name,
                      bool required = false, const std::string &desc = "");
    ```

- 添加浮点数选项

    参数含义参考整数选项。

    ```cpp
    template<typename T>
        requires std::floating_point<T>
    FloatingOption<T> &
    add_floating(T &val, char short_name, const std::string &long_name,
                 bool required = false, const std::string &desc = "");

    template<typename T>
        requires std::floating_point<T>
    MFloatingOption<T> &
    add_multi_floating(std::vector<T> &val,
                       char short_name, const std::string &long_name,
                       bool required = false, const std::string &desc = "");
    ```

- 添加字符串选项

    参数含义参考整数选项。

    ```cpp
    StringOption &
    add_string(std::string &val, char short_name, const std::string &long_name,
               bool required = false, const std::string &desc = "");

    MStringOption &
    add_multi_string(std::vector<std::string> &val,
                     char short_name, const std::string &long_name,
                     bool required = false, const std::string &desc = "");
    ```

- 添加标记选项

    标记选项没有参数，当被解析的选项中包含该选项时，`val`被设置为`true`。`val`在添加选项时会被设置为`false`。

    计数标记选项`add_countable_flag`的值为整数，该选项每次出现会给该整数`val`加一，`val`在添加选项时会被设置为`0`。

    参数含义参考整数选项。

    ```cpp
    FlagOption &
    add_flag(bool &val, char short_name, const std::string &long_name,
             const std::string &desc = "");

    CountableOption &
    add_countable_flag(int &val, char short_name, const std::string &long_name,
                       const std::string &desc = "");
    ```

- 添加布尔选项

    布尔选项只有真和假两种值，当为该选项指定`true`、`yes`、`y`时，`val`将被置为真值，当为该选项指定`false`、`no`、`n`时，`val`将被置为假值。

    参数含义参考整数选项。

    ```cpp
    BoolOption &
    add_bool(bool &val, char short_name, const std::string &long_name,
             bool required = false, const std::string &desc = "");
    ```

- 添加数据单位选项

    数据单位是以`B`、`KB`、`MB`、`GB`、`TB`、`PB`、`EB`为单位的无符号整数类型，进制为1024，该类型的选项会被转换成以字节为单位的整数保存到`val`中。

    若未指定单位，则默认为字节`B`，单位中的`B`可省略；单位中的字符使用大小写均可；一个选项中可以出现多个单位，例如`1M2KB3B`。

    参数含义参考整数选项。

    ```cpp
    template<typename T>
        requires std::unsigned_integral<T>
    DataUnitOption<T> &
    add_data_unit(T &val, char short_name, const std::string &long_name,
                  bool required = false, const std::string &desc = "");
    ```

- 添加自定义选项

    当已有的选项类型无法满足需求时，可使用自定义选项类型。具体方法参考示例。

    参数含义参考整数选项。

    ```cpp
    template<typename OptionType>
        requires std::derived_from<OptionType, OptionBase>
    OptionType &
    add_user_defined(typename OptionType::ValueType &val,
                     char short_name, const std::string &long_name,
                     bool required = false, const std::string &desc = "");
    ```

### 其他成员函数

- 设置程序名称

    默认以`argv[0]`为程序名称，设置了非空字符串则以该字符串为程序名称。

    ```cpp
    void set_program(const std::string &s);
    ```

- 设置额外提示语

    在帮助页面增加额外的提示语，位于`extra_prompt`指示的位置，如`program_name [required options]... extra_prompt`。

    ```cpp
    void set_extra_prompt(const std::string &s)
    ```

- 设置更多帮助文档内容

    在帮助页面的末尾增加用户指定的内容，每个字符串一行。

    ```cpp
    void set_more_usage(const std::vector<std::string> &content);
    ```

- 设置帮助选项

    若设置了帮助选项名称，且被解析的选项中包含帮助选项，`has_help_flag`函数会返回`true`，是否展示帮助页面由用户决定，不设置帮助选项不影响帮助页面的生成。

    若被解析的选项中包含帮助选项，数据校验过程将不会被执行，因此在使用解析结果前必须先检查是否有帮助选项。

    ```cpp
    void set_help_flag(char short_name, const std::string &long_name);
    ```

- 解析选项

    解析`argv`数组中的选项。

    返回值为`0`表示解析成功，返回值为`1`表示包含帮助选项，返回值为`-1`表示解析失败。

    对于没有`err`参数的函数，解析失败时会抛出异常；有`err`参数的函数，解析失败时会将失败原因保存在`err`中。

    ```cpp
    int parse(int argc, const char * const argv[], std::string &err);
    int parse(int argc, const char * const argv[]);
    int parse(const std::vector<std::string> &argv, std::string &err);
    int parse(const std::vector<std::string> &argv);
    ```

- 展示帮助页面

    将帮助页面格式化输出到`os`中。

    ```cpp
    void usage(std::ostream &os);
    ```

- 展示解析结果

    将解析结果格式化输出到`os`中，若`is_set`为`true`，不展示未在选项中指定的参数。

    ```cpp
    void show_values(std::ostream &os, bool is_set = false);
    ```


## coke::BasicOption

类模板`coke::BasicOption`用于保存一个选项的属性，`T`为选项值的类型，`Trait`用于指示如何在选项值和命令行参数间进行转换。

```cpp
template<typename T, typename Trait>
class BasicOption;
```

### 类型别名

```cpp
using ValueType = T;
using TraitType = Trait;
using ValidatorType = std::function<bool(ValueType &, std::string &)>;
```

### 成员函数

- 设置默认值

    若命令行参数未设置该选项，则使用默认值。

    ```cpp
    BasicOption &set_default(const T &t);
    ```

- 设置选项描述

    选项描述展示在帮助页面，长描述可包含多行文本，在短描述后展示。

    ```cpp
    BasicOption &set_description(const std::string &desc);
    BasicOption &set_long_descriptions(const std::vector<std::string> &descs);
    ```

- 设置是否必须在命令行参数中指定该选项

    ```cpp
    BasicOption &set_required(bool required = true);
    ```

- 设置选项值验证器

    验证器的第一个参数为选项值的引用，第二个参数用于保存验证发现的错误信息。

    选项值验证器用于提供灵活的检查机制，若该函数返回`false`则解析失败，此时应设置错误信息。

    ```cpp
    BasicOption &set_validator(ValidatorType validator);
    ```


## 异常

在添加选项或者解析选项时，若发现错误行为会抛出下列异常。其中`coke::OptionError`为其它异常类型的基类，其子类有

- DuplicateOptionError

    添加的选项与已有选项名称冲突

- InvalidOptionError

    选项名称不符合要求

- InvalidValueError

    指定的命令行参数无法转换成选项的值

- NoSuchOptionError

    命令行参数中出现了未知的选项

- UnexpectedValueError

    选项需要指定值，但未在命令行参数中指定；或者选项不需要值，但在命令行参数中指定了值

- OptionNotSetError

    选项要求必须被指定（设置了`required`属性），但未在命令行参数中指定

- ValidateError

    使用校验器对选项的值进行校验时返回了`false`

### coke::OptionError

```cpp
// 检查发生异常的选项是否有短选项
bool has_short_name() const noexcept;
// 检查发生异常的选项是否有长选项
bool has_long_name() const noexcept;

// 获取发生异常的短选项
char get_short_name() const noexcept;
// 获取发生异常的长选项
const std::string &get_long_name() const noexcept;

// 获取异常的字符串描述
const char *what() const noexcept;
```


## 示例

### 基本用法

基本用法参考示例`ex014-option_parser.cpp`。

### 自定义选项

实现一个自定义选项，选项值仅接受字母，并将小写字母转换为大写字母。

```cpp
#include <cctype>
#include <string>
#include <iostream>
#include "coke/tools/option_parser.h"

// 自定义选项的类型特征
struct UpperStringTrait {
    // 定义类型别名，表示这个选项使用该类型保存值
    using ValueType = std::string;

    // 返回类型名称，用于在帮助页面展示该选项需要什么类型的值
    static std::string get_type_str() { return "upper_string"; }

    // 将选项的值转换为字符串表示，用于生成结果展示页面
    static std::string to_string(const std::string &val) { return val; }

    // 将选项的字符串表示转换为值，用于解析选项
    static bool from_string(const std::string &str, std::string &val, bool) {
        val = str;

        for (char &c : val) {
            if (!std::isalpha(c))
                return false;
            c = std::toupper(c);
        }
        return true;
    }
};

using UpperStringOption = coke::BasicOption<std::string, UpperStringTrait>;

int main(int argc, char *argv[]) {
    coke::OptionParser args;
    std::string str;

    // 向参数解析器增加自定义选项，使用链式调用设置更多属性
    args.add_user_defined<UpperStringOption>(str, 's', "UpperString")
        .set_default("UPPERSTRING")
        .set_description("THIS OPTION ACCEPTS ONLY LETTERS AND CONVERTS TO UPPERCASE");

    args.set_help_flag('h', "help");

    try {
        args.parse(argc, argv);
    }
    catch (const coke::OptionError &e) {
        std::cerr << e.what() << std::endl;
        std::cerr << "use -h option to show help message" << std::endl;
        return 1;
    }

    if (args.has_help_flag()) {
        args.usage(std::cout);
        return 0;
    }
    else {
        args.show_values(std::cout, false);
        std::cout << std::string(32, '-') << std::endl;
        std::cout << "The value in str: " << str << std::endl;
    }

    return 0;
}
```
