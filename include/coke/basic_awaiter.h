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

#ifndef COKE_BASIC_AWAITER_H
#define COKE_BASIC_AWAITER_H

#include "coke/detail/awaiter_base.h"

namespace coke {

/**
 * @brief BasicAwaiter is a basic implementation of AwaiterBase.
 * It provides a way to save results, associate tasks and awaiters with the help
 * of AwaiterInfo. If we need to wrap a task(which returns an integer) into an
 * awaitable object, we can do it as following
 *
 * @code
 *  class SomeAwaiter : public BasicAwaiter<int> {
 *  public:
 *      explicit SomeAwaiter(WFSomeTask *task) {
 *          task->set_callback([info = this->get_info()] (WFSomeTask *t) {
 *              auto *awaiter = info->get_awaiter<SomeAwaiter>();
 *              int ret = get_return_value(t);
 *              awaiter->emplace_result(ret);
 *              awaiter->done();
 *          });
 *
 *          set_task(task);
 *      }
 *  };
 * @endcode
*/
template<Cokeable T>
class BasicAwaiter;

/**
 * @brief AwaiterInfo is a helper of BasicAwaiter.
*/
template<Cokeable T>
struct AwaiterInfo {
    AwaiterInfo(AwaiterBase *ptr) : ptr(ptr) { }
    AwaiterInfo(const AwaiterBase &) = delete;

    // The caller make sure that type(*ptr) is Awaiter A
    template<typename A = BasicAwaiter<T>>
    A *get_awaiter() {
        return static_cast<A *>(ptr);
    }

    friend class BasicAwaiter<T>;

private:
    struct Empty {};
    using OptType = std::conditional_t<std::is_void_v<T>, Empty, std::optional<T>>;

    AwaiterBase *ptr = nullptr;
    [[no_unique_address]] OptType opt;
};

template<Cokeable T>
class BasicAwaiter : public AwaiterBase {
public:
    BasicAwaiter() : info(new AwaiterInfo<T>(this)) { }
    ~BasicAwaiter() { delete info; }

    /**
     * @brief BasicAwaiter is not copyable.
     */
    BasicAwaiter(const BasicAwaiter &) = delete;

    /**
     * @brief Move constructor, AwaiterInfo associate task with the new
     *        BasicAwaiter after move.
    */
    BasicAwaiter(BasicAwaiter &&that)
        : AwaiterBase(std::move(that)), info(that.info)
    {
        that.info = nullptr;

        if (this->info)
            this->info->ptr = this;
    }

    /**
     * @brief Move assign operator, AwaiterInfo associate task with the new
     *        BasicAwaiter after move.
    */
    BasicAwaiter &operator= (BasicAwaiter &&that) {
        if (this != &that) {
            AwaiterBase::operator=(std::move(that));
            std::swap(this->info, that.info);

            if (this->info)
                this->info->ptr = this;
            if (that.info)
                that.info->ptr = &that;
        }

        return *this;
    }

    /**
     * @brief Get the AwaiterInfo on this BasicAwaiter. See code example of
     *        this class to find how to use it.
    */
    AwaiterInfo<T> *get_info() const {
        return info;
    }

    /**
     * @brief Emplace return value of workflow's task to this awaiter.
    */
    template<typename... ARGS>
    void emplace_result(ARGS&&... args) {
        if constexpr (!std::is_void_v<T>)
            info->opt.emplace(std::forward<ARGS>(args)...);
    }

    /**
     * @brief Get the co_await result of this awaiter, which is set
     *        by `emplace_result`.
    */
    T await_resume() {
        if constexpr (!std::is_void_v<T>)
            return std::move(info->opt.value());
    }

protected:
    AwaiterInfo<T> *info;
};

} // namespace coke

#endif // COKE_BASIC_AWAITER_H
