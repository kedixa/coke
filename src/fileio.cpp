#include "coke/fileio.h"

#include "workflow/WFTaskFactory.h"

namespace coke {

template<typename Task>
FileAwaiter::FileAwaiter(Task *task) {
    task->set_callback([info = this->get_info()] (Task *t) {
        auto *awaiter = info->get_awaiter<FileAwaiter>();
        awaiter->emplace_result(FileResult{
            t->get_state(), t->get_error(), t->get_retval()
        });

        awaiter->done();
    });

    set_task(task);
}

FileAwaiter pread(int fd, void *buf, size_t count, off_t offset) {
    auto *task = WFTaskFactory::create_pread_task(fd, buf, count, offset, nullptr);
    return FileAwaiter(task);
}

FileAwaiter pwrite(int fd, void *buf, size_t count, off_t offset) {
    auto *task = WFTaskFactory::create_pwrite_task(fd, buf, count, offset, nullptr);
    return FileAwaiter(task);
}

FileAwaiter preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset) {
    auto *task = WFTaskFactory::create_preadv_task(fd, iov, iovcnt, offset, nullptr);
    return FileAwaiter(task);
}

FileAwaiter pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset) {
    auto *task = WFTaskFactory::create_pwritev_task(fd, iov, iovcnt, offset, nullptr);
    return FileAwaiter(task);
}

FileAwaiter fsync(int fd) {
    auto *task = WFTaskFactory::create_fsync_task(fd, nullptr);
    return FileAwaiter(task);
}

FileAwaiter fdatasync(int fd) {
    auto *task = WFTaskFactory::create_fdsync_task(fd, nullptr);
    return FileAwaiter(task);
}

} // namespace coke
