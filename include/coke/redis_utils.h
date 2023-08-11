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
