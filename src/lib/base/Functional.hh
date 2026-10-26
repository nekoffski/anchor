#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

#include "Concepts.hh"
#include "Error.hh"
#include "Log.hh"
#include "Macros.hh"

namespace anchor {

template <typename... Steps>
    requires(std::same_as<std::invoke_result_t<Steps>, Result<void>> && ...)
Result<void> pipeline(Steps&&... steps) {
    Result<void> result;
    ((result = std::move(result).and_then(std::forward<Steps>(steps))), ...);
    return result;
}

template <typename Signature>
class MoveOnlyFunction;

template <typename ReturnType, typename... ArgumentTypes>
class MoveOnlyFunction<ReturnType(ArgumentTypes...)> {
   private:
    struct CallableBase {
        virtual ~CallableBase() = default;
        virtual ReturnType invoke(ArgumentTypes&&... arguments) = 0;
    };

    template <typename Callable>
    struct CallableModel final : CallableBase {
        template <typename F>
        explicit CallableModel(F&& callable)
            : callable(std::forward<F>(callable)) {}

        ReturnType invoke(ArgumentTypes&&... arguments) override {
            if constexpr (std::is_void_v<ReturnType>) {
                std::invoke(
                    callable, std::forward<ArgumentTypes>(arguments)...
                );
            } else {
                return std::invoke(
                    callable, std::forward<ArgumentTypes>(arguments)...
                );
            }
        }

        Callable callable;
    };

   public:
    MoveOnlyFunction() = default;
    MoveOnlyFunction(std::nullptr_t) {}

    template <typename Callable>
        requires(
            not std::same_as<std::remove_cvref_t<Callable>, MoveOnlyFunction> &&
            std::is_invocable_r_v<
                ReturnType, std::decay_t<Callable>&, ArgumentTypes...> &&
            std::is_constructible_v<std::decay_t<Callable>, Callable &&>
        )
    MoveOnlyFunction(Callable&& callable)
        : m_callable(
              std::make_unique<CallableModel<std::decay_t<Callable>>>(
                  std::forward<Callable>(callable)
              )
          ) {}

    MoveOnlyFunction(const MoveOnlyFunction&) = delete;
    MoveOnlyFunction& operator=(const MoveOnlyFunction&) = delete;
    MoveOnlyFunction(MoveOnlyFunction&&) noexcept = default;
    MoveOnlyFunction& operator=(MoveOnlyFunction&&) noexcept = default;

    MoveOnlyFunction& operator=(std::nullptr_t) noexcept {
        m_callable.reset();
        return *this;
    }

    template <typename Callable>
        requires(
            not std::same_as<std::remove_cvref_t<Callable>, MoveOnlyFunction> &&
            std::is_invocable_r_v<
                ReturnType, std::decay_t<Callable>&, ArgumentTypes...> &&
            std::is_constructible_v<std::decay_t<Callable>, Callable &&>
        )
    MoveOnlyFunction& operator=(Callable&& callable) {
        MoveOnlyFunction replacement(std::forward<Callable>(callable));
        *this = std::move(replacement);
        return *this;
    }

    explicit operator bool() const noexcept {
        return static_cast<bool>(m_callable);
    }

    ReturnType operator()(ArgumentTypes... arguments) const {
        log::expect(bool(m_callable), "Cannot invoke an empty function");

        if constexpr (std::is_void_v<ReturnType>) {
            m_callable->invoke(std::forward<ArgumentTypes>(arguments)...);
        } else {
            return m_callable->invoke(
                std::forward<ArgumentTypes>(arguments)...
            );
        }
    }

   private:
    std::unique_ptr<CallableBase> m_callable;
};

namespace details {

template <typename T>
struct ToImpl {};

template <typename T, std::ranges::range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, T>
std::vector<T> operator|(R&& r, ToImpl<T>) {
    std::vector<T> out;

    if constexpr (std::ranges::sized_range<decltype(r)>) {
        out.reserve(std::ranges::size(r));
    }

    std::ranges::copy(r, std::back_inserter(out));
    return out;
}

struct DeferCallEntry {};

}  // namespace details

template <typename T>
auto toVector() {
    return details::ToImpl<T>{};
}

template <typename... Ts>
struct Overloader : Ts... {
    using Ts::operator()...;
};

class DeferCall : public NonCopyable, public NonMovable {
   public:
    DeferCall() : m_callback([]() {}) {}

    template <typename Callback>
    DeferCall(Callback&& callback)
        : m_callback(std::forward<Callback>(callback)) {}

    ~DeferCall() {
        if (not m_dismissed) {
            m_callback();
        }
    }
    void dismiss() { m_dismissed = true; }

   private:
    bool m_dismissed{false};
    MoveOnlyFunction<void()> m_callback;
};

constexpr DeferCall operator+(
    details::DeferCallEntry, MoveOnlyFunction<void()> callback
) {
    return DeferCall{std::move(callback)};
}

template <typename F>
    requires std::is_invocable_v<F>
class LazyEvaluator {
    using Result = decltype(std::declval<const F>()());

   public:
    explicit LazyEvaluator(F&& evaluable) : m_evaluable(std::move(evaluable)) {}

    constexpr operator Result() const { return m_evaluable(); }

    template <typename T>
        requires(std::is_constructible_v<T, Result>)
    constexpr operator T() const {
        return T{m_evaluable()};
    }

   private:
    F m_evaluable;
};

template <typename F>
constexpr auto lazyEvaluate(F&& f) {
    return LazyEvaluator{std::forward<F>(f)};
}

#define LAZY_EVALUATE(expr) lazyEvaluate([&]() { return (expr); })

}  // namespace anchor

#define DEFER \
    auto ANONYMOUS_VAR(SCOPE_EXIT) = anchor::details::DeferCallEntry{} + [&]()

#define NAMED_DEFER(name) auto name = anchor::details::DeferCallEntry{} + [&]()
