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

#ifndef COKE_DETAIL_RANDOM_H
#define COKE_DETAIL_RANDOM_H

#include <cstdint>
#include <random>

namespace coke {

namespace detail {

uint64_t rand_seed();

} // namespace detail

/**
 * @brief Thread safe 64 bit random integer generator.
 */
inline uint64_t rand_u64()
{
    thread_local std::mt19937_64 mt64(detail::rand_seed());
    return mt64();
}

} // namespace coke

#endif // COKE_DETAIL_RANDOM_H
