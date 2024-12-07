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

#ifndef COKE_SYNC_GUARD_H
#define COKE_SYNC_GUARD_H

namespace coke {

class SyncGuard {
public:
    /**
     * @brief Create SyncGuard.
     * @param auto_sync_begin Call sync_operation_begin if it is true.
     *
     * @attention It is an error to call sync_operation_begin and
     *            sync_operation_end of the same SyncGuard object in different
     *            threads.
     */
    explicit SyncGuard(bool auto_sync_begin)
        : guarded(false)
    {
        if (auto_sync_begin)
            sync_operation_begin();
    }

    SyncGuard(const SyncGuard &) = delete;
    SyncGuard &operator= (const SyncGuard &) = delete;

    /**
     * @brief Destroy SyncGuard. If in_guard() is true, sync_operation_end will
     *        be called.
     */
    ~SyncGuard() {
        if (guarded)
            sync_operation_end();
    }

    /**
     * @brief Check whether *this is already in sync guard.
     */
    bool in_guard() const noexcept {
        return guarded;
    }

    /**
     * @brief Call WFGlobal::sync_operation_begin. If multiple SyncGuard objects
     *        are used recursively multiple times in the same thread, only one
     *        WFGlobal::sync_operation_begin will be called.
     */
    void sync_operation_begin();

    /**
     * @brief Call WFGlobal::sync_operation_end when the last
     *        SyncGuard::sync_operation_end is called in the same thread.
     */
    void sync_operation_end();

private:
    bool guarded;
};

} // namespace coke

#endif // COKE_SYNC_GUARD_H
