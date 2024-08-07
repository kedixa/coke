
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "coke/tools/option_parser.h"

/**
 * This example shows how to use coke::OptionParser to parse command line
 * arguments.
*/

struct Config {
    bool flag;
    bool boolean{true};
    int verbose;

    short short_int = 0;
    unsigned unsigned_int = 1;
    long long_int;
    unsigned long long data_unit;

    double double_value = 0.0;
    std::string str;
    std::vector<float> floats_vector;
};

int main(int argc, char *argv[]) {
    coke::OptionParser args;
    Config cfg;

    try {
        args.add_flag(cfg.flag, 'f', "flag")
            .set_description("Enable flag feature");

        args.add_bool(cfg.boolean, 'b', "bool")
            .set_default(true)
            .set_description("Set boolean value to true or false");

        args.add_countable_flag(cfg.verbose, 'v', "verbose")
            .set_description("Show verbose output, set more times to increase verbose level");

        args.add_integer(cfg.short_int, 's', "short")
            .set_description("Set short integer")
            .set_validator([](short &val, std::string &err) {
                if (val % 2 != 0) {
                    err.assign("The value must be an integer multiple of 2");
                    return false;
                }
                return true;
            });

        args.add_integer(cfg.unsigned_int, 'u', coke::NULL_LONG_NAME, true)
            .set_description("Set unsigned integer, this option is required");

        args.add_integer(cfg.long_int, coke::NULL_SHORT_NAME, "long")
            .set_required(true)
            .set_description("Set long integer, this option is required");

        args.add_data_unit(cfg.data_unit, 'd', "data-unit", false, "Set data unit")
            .set_default(256 * 1024);

        args.add_floating(cfg.double_value, 'F', "double");

        args.add_string(cfg.str, 't', "string_type", true,
                        "Set string, this option is required");

        args.add_multi_floating(cfg.floats_vector, 'm', "multi.floats")
            .set_default({3.14})
            .set_description("Set multi float by -m 1.414 -m 2.718 ...")
            .set_long_descriptions({
                "This is an example of a long description.",
                "Since word segmentation is not supported,",
                "multiple strings need to be given, one per line.",
                "There is a blank line before and after the long description,",
                "and some indentation before the line."
            });

        args.set_help_flag('h', "help");
        args.set_program("option_parser_example");
        args.set_extra_prompt("[The extra prompt is here]");
        args.set_more_usage({
            "ANYTHING MORE",
            "    There are more content on usage page, you can write anything here,",
            "    and there is no extra indent, you should do it yourself.",
            "",
            "COPYRIGHT",
            "    Copyright (c) 2024 The Author's name.",
            "",
            "This page is generated by option parser. Thanks for using this software."
        });

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
        args.show_values(std::cout, true);

        const std::vector<std::string> &extra = args.get_extra_args();
        if (!extra.empty()) {
            std::cout << "Extra:";
            for (const std::string &arg : extra)
                std::cout << " " << arg;
            std::cout << std::endl;
        }
    }

    return 0;
}
