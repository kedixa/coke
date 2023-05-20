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

// TODO When use inline std::string here, gcc 12 -flto undefind reference
//inline static const std::string DFT_QUEUE{"coke_go"};
inline static constexpr std::string_view DFT_QUEUE{"coke_dft_queue"};

void *create_go_task(const std::string &queue_name, std::function<void()> &&func);

} // namespace detail

template<SimpleType R>
class GoAwaiter : public AwaiterBase {
    using return_t = std::remove_cvref_t<R>;
    using option_t = std::conditional_t<std::is_same_v<return_t, void>, bool, return_t>;

public:
    template<typename FUNC, typename... ARGS>
        requires std::invocable<FUNC, ARGS...>
    GoAwaiter(const std::string &queue_name, FUNC &&func, ARGS&&... args) {
        using result_t = std::remove_cvref_t<std::invoke_result_t<FUNC, ARGS...>>;
        static_assert(std::is_same_v<return_t, void> || std::is_same_v<result_t, return_t>,
                      "The result type of FUNC(ARGS...) must match return type");

        std::function<result_t()> f = std::bind(std::forward<FUNC>(func), std::forward<ARGS>(args)...);
        auto go = [this, f = std::move(f)] () mutable {
            if constexpr (std::is_same_v<return_t, void>)
                f();
            else
                ret.emplace(f());

            this->done();
        };

        set_task(detail::create_go_task(queue_name, std::move(go)));
    }

    return_t await_resume() {
        if constexpr (!std::is_same_v<return_t, void>) {
            assert(ret.has_value());
            return std::move(ret.value());
        }
    }

private:
    std::optional<option_t> ret;
};


template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
auto go(const std::string &queue_name, FUNC &&func, ARGS&& ...args) {
    using result_t = std::remove_cvref_t<std::invoke_result_t<FUNC, ARGS...>>;

    return GoAwaiter<result_t>(queue_name, std::forward<FUNC>(func), std::forward<ARGS>(args)...);
}

template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
auto go(FUNC &&func, ARGS&& ...args) {
    return go(std::string(detail::DFT_QUEUE), std::forward<FUNC>(func), std::forward<ARGS>(args)...);
}


// same as coke::go, but ignore return value
template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
auto go_void(const std::string &queue_name, FUNC &&func, ARGS&&... args) {
    return GoAwaiter<void>(queue_name, std::forward<FUNC>(func), std::forward<ARGS>(args)...);
}

// same as coke::go, but ignore return value
template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
auto go_void(FUNC &&func, ARGS&&... args) {
    return go_void(std::string(detail::DFT_QUEUE), std::forward<FUNC>(func), std::forward<ARGS>(args)...);
}


// switch to go thread pool's thread
inline auto switch_go_thread() {
    return go([]{});
}

} // namespace coke

#endif // COKE_GO_H
