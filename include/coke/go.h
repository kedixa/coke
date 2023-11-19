#ifndef COKE_GO_H
#define COKE_GO_H

#include <optional>
#include <functional>
#include <string>
#include <string_view>

#include "coke/detail/basic_concept.h"
#include "coke/detail/awaiter_base.h"

namespace coke {

namespace detail {

// Some SSO std::string has 15 chars locally, so use a short and meaningful name
inline static constexpr std::string_view DFT_QUEUE{"coke:dft-que"};

SubTask *create_go_task(const std::string &queue, std::function<void()> &&func);

} // namespace detail

template<SimpleType R>
class GoAwaiter : public BasicAwaiter<R> {
public:
    GoAwaiter(const std::string &queue, std::function<R()> func) {
        AwaiterInfo<R> *info = this->get_info();

        auto go = [info, func = std::move(func)] () {
            auto *awaiter = info->template get_awaiter<GoAwaiter<R>>();

            if constexpr (!std::is_same_v<R, void>)
                awaiter->emplace_result(func());
            else
                func();

            awaiter->done();
        };

        this->set_task(detail::create_go_task(queue, std::move(go)));
    }
};


template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
auto go(const std::string &queue, FUNC &&func, ARGS&& ...args) {
    using result_t = std::remove_cvref_t<std::invoke_result_t<FUNC, ARGS...>>;
    auto &&f = std::bind(std::forward<FUNC>(func), std::forward<ARGS>(args)...);

    return GoAwaiter<result_t>(queue, std::move(f));
}

template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
auto go(FUNC &&func, ARGS&& ...args) {
    return go(std::string(detail::DFT_QUEUE), std::forward<FUNC>(func), std::forward<ARGS>(args)...);
}

// Switch to a thread in the go thread pool with `name`
inline auto switch_go_thread(const std::string &name) {
    return go(name, []{});
}

// Switch to a thread in the go thread pool with default name
inline auto switch_go_thread() {
    return go(std::string(detail::DFT_QUEUE), []{});
}

} // namespace coke

#endif // COKE_GO_H
