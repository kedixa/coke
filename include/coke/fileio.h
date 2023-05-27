#ifndef COKE_FILEIO_H
#define COKE_FILEIO_H

#include "sys/uio.h"  // struct iovec

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

class FileAwaiter : public AwaiterBase {
public:
    template<typename T>
    explicit FileAwaiter(T *task);

    FileResult await_resume() { return result; }

private:
    FileResult result;
};

FileAwaiter pread(int fd, void *buf, size_t count, off_t offset);
FileAwaiter pwrite(int fd, void *buf, size_t count, off_t offset);

FileAwaiter preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset);
FileAwaiter pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset);

FileAwaiter fsync(int fd);
FileAwaiter fdatasync(int fd);

} // namespace coke

#endif // COKE_FILEIO_H
