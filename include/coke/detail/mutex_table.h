#ifndef COKE_DETAIL_MUTEX_TABLE_H
#define COKE_DETAIL_MUTEX_TABLE_H

#include <mutex>

namespace coke::detail {

std::mutex &get_mutex(void *ptr);

} // namespace coke::detail

#endif // COKE_DETAIL_MUTEX_TABLE_H
