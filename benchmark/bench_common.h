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

#ifndef COKE_BENCH_COMMON_H
#define COKE_BENCH_COMMON_H

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

#include "coke/basic_awaiter.h"
#include "coke/tools/option_parser.h"
#include "workflow/WFTaskFactory.h"

class RepeaterAwaiter : public coke::BasicAwaiter<void> {
public:
    RepeaterAwaiter(WFRepeaterTask *task)
    {
        task->set_callback([info = this->get_info()](void *) {
            auto *awaiter = info->get_awaiter<RepeaterAwaiter>();
            awaiter->done();
        });

        this->set_task(task);
    }
};

inline long long current_msec()
{
    auto dur = std::chrono::steady_clock::now().time_since_epoch();
    auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(dur);
    return msec.count();
}

inline long long current_usec()
{
    auto dur = std::chrono::steady_clock::now().time_since_epoch();
    auto usec = std::chrono::duration_cast<std::chrono::microseconds>(dur);
    return usec.count();
}

inline void data_distribution(const std::vector<long long> &data, double &mean,
                              double &stddev)
{
    if (data.empty()) {
        mean = 0.0;
        stddev = 0.0;
        return;
    }

    double total = std::accumulate(data.begin(), data.end(), 0LL);
    mean = total / (double)data.size();

    stddev = 0.0;
    if (data.size() > 1) {
        for (const auto &d : data)
            stddev += (d - mean) * (d - mean);
        stddev = std::sqrt(stddev / (double)(data.size() - 1));
    }
}

inline void delimiter(std::ostream &os, const std::vector<int> &width,
                      char c = ' ')
{
    if (width.empty())
        return;

    os << "|";
    for (auto n : width)
        os << " " << std::string(n, c) << " |";
    os << std::endl;
}

template<typename T>
void print_helper(std::ostream &os, int width, T &&t)
{
    os << " " << std::setw(width) << t << " |";
}

template<typename... ARGS>
void table_line(std::ostream &os, const std::vector<int> &width, ARGS &&...args)
{
    os << "|";
    std::size_t i = 0;
    (print_helper(os, width[i++], args), ...);
    os << std::endl;
}

int parse_args(coke::OptionParser &args, int argc, char *argv[], bool *yes)
{
    std::string error;
    int ret = args.parse(argc, argv, error);
    if (ret < 0) {
        std::cerr << error << std::endl;
        return ret;
    }
    else if (ret > 0) {
        args.usage(std::cout);
        return 0;
    }

    if (!*yes) {
        args.show_values(std::cout);

        std::cout << "Continnue (y/N): ";
        std::cout.flush();

        if (std::cin.get() != 'y') {
            std::cout << "benchmark stopped" << std::endl;
            return 0;
        }
    }

    return 1;
}

#endif // COKE_BENCH_COMMON_H
