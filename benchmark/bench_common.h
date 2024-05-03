#ifndef COKE_BENCH_COMMON_H
#define COKE_BENCH_COMMON_H

#include <chrono>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <vector>

inline long long current_msec() {
    auto dur = std::chrono::steady_clock::now().time_since_epoch();
    auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(dur);
    return msec.count();
}

inline long long current_usec() {
    auto dur = std::chrono::steady_clock::now().time_since_epoch();
    auto usec = std::chrono::duration_cast<std::chrono::microseconds>(dur);
    return usec.count();
}

inline void
data_distribution(const std::vector<long long> &data,
                  double &mean, double &stddev) {
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
        stddev = std::sqrt(stddev / (double)(data.size()-1));
    }
}

inline
void delimiter(std::ostream &os, const std::vector<int> &width, char c = ' ') {
    if (width.empty())
        return;

    os << "|";
    for (auto n : width)
        os << " " << std::string(n, c) << " |";
    os << std::endl;
}

template<typename T>
void print_helper(std::ostream &os, int width, T &&t) {
    os << " " << std::setw(width) << t << " |";
}

template<typename... ARGS>
void table_line(std::ostream &os, const std::vector<int> &width,
                ARGS&&... args) {
    os << "|";
    std::size_t i = 0;
    (print_helper(os, width[i++], args), ...);
    os << std::endl;
}

template<typename T>
int parse_args(T &args, int argc, char *argv[], bool *yes) {
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
