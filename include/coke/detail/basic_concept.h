#ifndef COKE_DETAIL_CONCEPT_H
#define COKE_DETAIL_CONCEPT_H

#include <concepts>
#include <type_traits>

#include "workflow/SubTask.h"

namespace coke {

/**
 * Cokeable concept is used to indicate type constraints in coke, such as
 * Task<T>, Future<T>, BasicAwaiter<T> ...
*/
template<typename T>
concept Cokeable = (
    std::is_object_v<T> &&
    (std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>) &&
    std::is_destructible_v<T> &&
    std::is_same_v<T, std::remove_cvref_t<T>> &&
    !std::is_array_v<T>
) || std::is_void_v<T>;

/**
 * This concept is used to provide constraints on the parameters of
 * AwaiterBase::await_suspend. The current implementation of
 * coke::detail::CoPromise is strongly coupled with series and
 * cannot accept other type of std::coroutine_handle yet.
 * Future versions may change CoPromise's implementation.
*/
template<typename T>
concept PromiseType = requires(T t) {
    { t.get_series() } -> std::same_as<void *>;
    { t.set_series((void *)nullptr) } -> std::same_as<void>;
};

/**
 * This concept is used to constrain the types of elements for coke containers,
 * for example coke::Queue.
*/
template<typename T>
concept Queueable = (
    std::is_object_v<T> &&
    (std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>) &&
    std::is_destructible_v<T> &&
    std::is_same_v<T, std::remove_cvref_t<T>> &&
    !std::is_array_v<T>
);

} // namespace coke

#endif // COKE_DETAIL_CONCEPT_H
