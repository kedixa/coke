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

#include "coke/fileio.h"

#include "workflow/WFTaskFactory.h"

namespace coke {

template<typename Task>
FileAwaiter::FileAwaiter(Task *task)
{
    task->set_callback([info = this->get_info()](Task *t) {
        auto *awaiter = info->get_awaiter<FileAwaiter>();
        awaiter->emplace_result(
            FileResult{t->get_state(), t->get_error(), t->get_retval()});

        awaiter->done();
    });

    set_task(task);
}

FileAwaiter pread(int fd, void *buf, std::size_t count, off_t offset)
{
    auto *task = WFTaskFactory::create_pread_task(fd, buf, count, offset,
                                                  nullptr);
    return FileAwaiter(task);
}

FileAwaiter pwrite(int fd, const void *buf, std::size_t count, off_t offset)
{
    auto *task = WFTaskFactory::create_pwrite_task(fd, buf, count, offset,
                                                   nullptr);
    return FileAwaiter(task);
}

FileAwaiter preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    auto *task = WFTaskFactory::create_preadv_task(fd, iov, iovcnt, offset,
                                                   nullptr);
    return FileAwaiter(task);
}

FileAwaiter pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    auto *task = WFTaskFactory::create_pwritev_task(fd, iov, iovcnt, offset,
                                                    nullptr);
    return FileAwaiter(task);
}

FileAwaiter fsync(int fd)
{
    auto *task = WFTaskFactory::create_fsync_task(fd, nullptr);
    return FileAwaiter(task);
}

FileAwaiter fdatasync(int fd)
{
    auto *task = WFTaskFactory::create_fdsync_task(fd, nullptr);
    return FileAwaiter(task);
}

} // namespace coke
