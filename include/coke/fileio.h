#ifndef COKE_FILEIO_H
#define COKE_FILEIO_H

#include <sys/uio.h>  // struct iovec

#include "coke/detail/awaiter_base.h"

namespace coke {

/**
 * Return value type of file input and output operations.
 * When `state` is not coke::STATE_SUCCESS, `error` means system error code errno.
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
