/**
 * Copyright 2025 Coke Project (https://github.com/kedixa/coke)
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

#ifndef COKE_DETAIL_REF_COUNTED_H
#define COKE_DETAIL_REF_COUNTED_H

#include <atomic>
#include <concepts>
#include <cstdint>
#include <memory>

namespace coke::detail {

template<typename T, typename Deleter = std::default_delete<T>>
    requires std::invocable<Deleter, T *>
class RefCounted {
public:
    explicit RefCounted(Deleter deleter = Deleter{})
        : deleter(std::move(deleter))
    { }

    void inc_ref() const noexcept {
        ref_count.fetch_add(1, std::memory_order_relaxed);
    }

    void dec_ref() const noexcept {
        if (ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            Deleter local_deleter(std::move(deleter));
            T *ptr = static_cast<T *>(const_cast<RefCounted *>(this));
            local_deleter(ptr);
        }
    }

protected:
    ~RefCounted() = default;

private:
    mutable std::atomic<std::size_t> ref_count{1};
    [[no_unique_address]] mutable Deleter deleter;
};

} // namespace coke::detail

#endif // COKE_DETAIL_REF_COUNTED_H
