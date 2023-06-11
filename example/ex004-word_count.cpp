#include <iostream>
#include <string>
#include <cerrno>
#include <cctype>
#include <unistd.h>
#include <fcntl.h>

#include "coke/coke.h"

constexpr size_t BUF_SIZE = 8ULL * 1024 * 1024;
unsigned char buf[BUF_SIZE];

/**
 * This example shows how to read data from file, count its words and lines,
 * the file maybe very large, so we read up to `BUF_SIZE` bytes at a time,
 * and read until FileResult::nbytes == 0 (which means EOF).
*/

coke::Task<> word_count(const std::string &fn) {
    coke::FileResult res;
    int fd;
    off_t offset = 0;

    fd = open(fn.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "Open " << fn << " errno:" << errno << std::endl;
        co_return;
    }

    size_t lines = 0, words = 0, chars = 0;
    bool in_word = false;

    while (true) {
        res = co_await coke::pread(fd, buf, BUF_SIZE, offset);

        if (res.state != coke::STATE_SUCCESS || res.nbytes == 0)
            break;

        const unsigned char *end = buf + res.nbytes;
        chars += res.nbytes;
        for (const unsigned char *p = buf; p != end; p++) {
            lines += (*p == '\n');

            if (std::isgraph(*p)) {
                words += !in_word;
                in_word = true;
            }
            else
                in_word = false;
        }

        offset += res.nbytes;
    }

    if (res.state == coke::STATE_SUCCESS) {
        std::cout << "Chars: " << chars << "\n"
                  << "Words: " << words << "\n"
                  << "Lines: " << lines << std::endl;
    }
    else {
        std::cerr << "ERROR: state:" << res.state << " error:" << res.error << std::endl;
        std::cerr << coke::get_error_string(res.state, res.error) << std::endl;
    }

    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 2 || argv[1] == nullptr) {
        std::cerr << "Usage: " << argv[0] << " file.txt" << std::endl;
        return 1;
    }

    coke::sync_wait(word_count(argv[1]));
    return 0;
}
