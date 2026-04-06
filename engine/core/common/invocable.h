#pragma once

#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

namespace Fabrica::Core {

template <typename Fn, typename R, typename... Args>
using EnableIfInvocableMatches = std::enable_if_t<
    std::is_invocable_v<Fn, Args...> &&
        std::is_same_v<R, std::invoke_result_t<Fn, Args...>>,
    int>;

template <typename Signature>
class Invocable;

template <typename R, typename... Args>
class Invocable<R(Args...)> {
 public:
  Invocable() : invocable_(nullptr, nullptr) {}

  template <typename Fn, EnableIfInvocableMatches<Fn, R, Args...> = 0>
  Invocable(Fn&& fn)
      : invocable_(
            new std::decay_t<Fn>(std::forward<Fn>(fn)),
            +[](void const* held_ptr) {
              const auto* typed_ptr =
                  static_cast<const std::decay_t<Fn>*>(held_ptr);
              delete typed_ptr;
            }),
        invoker_(
            +[](const ErasedInvocable& erased_invocable, Args... args) -> R {
              auto* typed_invocable =
                  static_cast<std::decay_t<Fn>*>(erased_invocable.get());
              return (*typed_invocable)(std::forward<Args>(args)...);
            }) {}

  Invocable(const Invocable&) = delete;
  Invocable(Invocable&&) noexcept = default;
  Invocable& operator=(const Invocable&) = delete;
  Invocable& operator=(Invocable&&) noexcept = default;

  template <typename... OperatorArgs>
  R operator()(OperatorArgs&&... args) {
    assert(invoker_ && invocable_);
    return invoker_(invocable_, std::forward<OperatorArgs>(args)...);
  }

  template <typename... OperatorArgs>
  R operator()(OperatorArgs&&... args) const {
    assert(invoker_ && invocable_);
    return invoker_(invocable_, std::forward<OperatorArgs>(args)...);
  }

  explicit operator bool() const noexcept {
    return invoker_ != nullptr && invocable_ != nullptr;
  }

 private:
  using ErasedInvocable = std::unique_ptr<void, void (*)(void const*)>;

  ErasedInvocable invocable_;
  R (*invoker_)(const ErasedInvocable&, Args...) = nullptr;
};

}  // namespace Fabrica::Core
