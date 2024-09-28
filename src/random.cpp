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

#include <mutex>
#include <chrono>

#include "coke/detail/random.h"

namespace coke::detail {

class LockedRandomU64 {
    static auto time_seed() {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        return (std::mt19937_64::result_type)now.count();
    }

public:
    LockedRandomU64() : mt64(time_seed()) { }

    uint64_t operator()() {
        std::lock_guard<std::mutex> lg(mtx);
        return mt64();
    }

private:
    std::mutex mtx;
    std::mt19937_64 mt64;
};

uint64_t rand_seed() {
    static LockedRandomU64 r;
    return r();
}

} // namespace coke::detail
