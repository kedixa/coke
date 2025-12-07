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

#ifndef COKE_DETAIL_CONCEPT_H
#define COKE_DETAIL_CONCEPT_H

#include <concepts>
#include <type_traits>

namespace coke {

/**
 * @brief Cokeable concept is used to indicate type constraints in coke,
 *        such as Task<T>, Future<T>, BasicAwaiter<T>.
 */
template<typename T>
concept Cokeable =
    (std::is_object_v<T> &&
     (std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>) &&
     std::is_destructible_v<T> && std::is_same_v<T, std::remove_cvref_t<T>> &&
     !std::is_array_v<T>) ||
    std::is_void_v<T>;

/**
 * @brief This concept is used to constrain the types of elements for coke
 *        containers, for example coke::Queue.
 */
template<typename T>
concept Queueable =
    (std::is_object_v<T> &&
     (std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>) &&
     std::is_destructible_v<T> && std::is_same_v<T, std::remove_cvref_t<T>> &&
     !std::is_array_v<T>);

/**
 * @brief This concept is used to check whether T is coke::CoPromise<U>, used
 *        in AwaiterBase::await_suspend.
 */
template<typename T>
concept IsCokePromise = T::__is_coke_promise_type;

template<typename T>
constexpr bool is_coke_awaitable_v = false;

template<typename T>
concept IsCokeAwaitable = T::__is_coke_awaitable_type || is_coke_awaitable_v<T>;

} // namespace coke

#endif // COKE_DETAIL_CONCEPT_H
