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

#include <cstdint>

#include "workflow/WFGlobal.h"
#include "coke/sync_guard.h"

namespace coke {

namespace detail {

struct SyncHelper {
    int cookie = 0;
    std::size_t counter = 0;
};

SyncHelper *get_sync_helper() {
    static thread_local SyncHelper helper;
    return &helper;
}

} // namespace detail


void SyncGuard::sync_operation_begin() {
    if (!guarded) {
        detail::SyncHelper *p = detail::get_sync_helper();

        if (p->counter++ == 0)
            p->cookie = WFGlobal::sync_operation_begin();

        guarded = true;
    }
}

void SyncGuard::sync_operation_end() {
    if (guarded) {
        detail::SyncHelper *p = detail::get_sync_helper();

        if (--p->counter == 0)
            WFGlobal::sync_operation_end(p->cookie);

        guarded = false;
    }
}

} // namespace coke
