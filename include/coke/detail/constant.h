#ifndef COKE_DETAIL_CONSTANT_H
#define COKE_DETAIL_CONSTANT_H

#include <new>

namespace coke::detail {

#ifdef __cpp_lib_hardware_interference_size
static constexpr std::size_t DESTRUCTIVE_ALIGN =
    std::hardware_destructive_interference_size;
#else
static constexpr std::size_t DESTRUCTIVE_ALIGN = 64;
#endif

#ifdef COKE_CANCELABLE_MAP_SIZE
static constexpr std::size_t CANCELABLE_MAP_SIZE = COKE_CANCELABLE_MAP_SIZE;
#else
static constexpr std::size_t CANCELABLE_MAP_SIZE = 16;
#endif

// Use prime numbers to avoid the effect of ptr is often aligned to a power of 2
#ifdef COKE_MUTEX_TABLE_SIZE
static constexpr std::size_t MUTEX_TABLE_SIZE = COKE_MUTEX_TABLE_SIZE;
#else
static constexpr std::size_t MUTEX_TABLE_SIZE = 61;
#endif

} // namespace coke::detail

#endif // COKE_DETAIL_CONSTANT_H
