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

#ifndef COKE_REDIS_UTILS_H
#define COKE_REDIS_UTILS_H

#include <string>

#include "workflow/RedisMessage.h"

namespace coke {

using RedisRequest = protocol::RedisRequest;
using RedisResponse = protocol::RedisResponse;
using RedisValue = protocol::RedisValue;

/**
 * Returns a human-readable string representation of RedisValue.
*/
std::string redis_value_to_string(const RedisValue &value);

} // namespace coke

#endif // COKE_REDIS_UTILS_H
