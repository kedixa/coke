#ifndef COKE_DETAIL_CONCEPT_H
#define COKE_DETAIL_CONCEPT_H

#include <concepts>
#include <type_traits>

#include "workflow/SubTask.h"

namespace coke {

/**
 * This concept indicates which types can be use to instantiate
 * coke::Task template. Only SimpleType can be awaited.
*/
template<typename T>
concept SimpleType = (std::movable<T> &&
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


/**
 * This concept check whether T is likely WFNetworkTask type. In order to
 * keep headers as few as possible, we didn't want to include WFTask.h in
 * network.h, the concept does basic checks on T to avoid incorrect usage.
*/
template<typename T>
concept NetworkTaskType = requires (T *t) {
    requires std::derived_from<T, SubTask>;
    requires std::same_as<T, std::remove_cvref_t<T>>;
    { t->get_state() } -> std::same_as<int>;
    { t->get_error() } -> std::same_as<int>;
    { t->get_task_seq() } -> std::same_as<long long>;
    { t->get_req() };
    { t->get_resp() };
};

/**
 * Helper type trait for NetworkTaskType.
 * ReqType: same as type *(T{}.get_req())
 * RespType: same as type *(T{}.get_resp())
*/
template<NetworkTaskType T>
struct NetworkTaskTrait {
    using ReqType = std::remove_pointer_t<std::invoke_result_t<decltype(&T::get_req), T *>>;
    using RespType = std::remove_pointer_t<std::invoke_result_t<decltype(&T::get_resp), T *>>;
};

} // namespace coke

#endif // COKE_DETAIL_CONCEPT_H
