#ifndef COKE_DETAIL_CONCEPT_H
#define COKE_DETAIL_CONCEPT_H

#include <concepts>

namespace coke {

/**
 * This concept indicates which types can be use to instantiate
 * coke::Task template. Only SimpleType can be awaited.
*/
template<typename T>
concept SimpleType = (std::copyable<T> &&
                     std::is_same_v<T, std::remove_cvref_t<T>> &&
                     !std::is_array_v<T>) ||
                     std::is_void_v<T>;

/**
 * This concept is used to provide constraints on the parameters of
 * AwaiterBase::await_suspend. The current implementation of
 * coke::detail::Promise is strongly coupled with series and
 * cannot accept other type of std::coroutine_handle yet.
 * Future versions may change Promise's implementation.
*/
template<typename T>
concept PromiseType = requires(T t) {
    { t.get_series() } -> std::same_as<void *>;
    { t.set_series((void *)nullptr) } -> std::same_as<void>;
};

} // namespace coke

#endif // COKE_DETAIL_CONCEPT_H
