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

#ifndef COKE_FILEIO_H
#define COKE_FILEIO_H

#include <sys/uio.h>  // struct iovec

#include "coke/detail/awaiter_base.h"

namespace coke {

/**
 * @brief Return value type of file input and output operations.
 * When `state` is not coke::STATE_SUCCESS, `error` means system error errno.
 * When file io success, `nbytes` indicates the number of bytes read or written.
*/
struct FileResult {
    int state;
    int error;
    long nbytes;  // the number of bytes read or written
};

class FileAwaiter : public BasicAwaiter<FileResult> {
public:
    template<typename Task>
    explicit FileAwaiter(Task *task);
};

[[nodiscard]]
FileAwaiter pread(int fd, void *buf, std::size_t count, off_t offset);
[[nodiscard]]
FileAwaiter pwrite(int fd, const void *buf, std::size_t count, off_t offset);

[[nodiscard]]
FileAwaiter preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset);
[[nodiscard]]
FileAwaiter pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset);

[[nodiscard]]
FileAwaiter fsync(int fd);
[[nodiscard]]
FileAwaiter fdatasync(int fd);

} // namespace coke

#endif // COKE_FILEIO_H
