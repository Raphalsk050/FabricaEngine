#pragma once

#include <memory>
#include <type_traits>
#include <utility>

namespace Fabrica::Core {

class Holdable {
 public:
  Holdable() : held_(nullptr, nullptr) {}

  template <typename T>
  explicit Holdable(T held)
      : held_(
            new std::decay_t<T>(std::move(held)), +[](void const* held_ptr) {
              const auto* typed_ptr =
                  static_cast<const std::decay_t<T>*>(held_ptr);
              delete typed_ptr;
            }) {
    static_assert(!std::is_pointer_v<std::decay_t<T>>,
                  "Holdable cannot hold raw pointers");
  }

  Holdable(const Holdable&) = delete;
  Holdable& operator=(const Holdable&) = delete;
  Holdable(Holdable&&) noexcept = default;
  Holdable& operator=(Holdable&&) noexcept = default;

  explicit operator bool() const { return held_ != nullptr; }

 private:
  std::unique_ptr<void, void (*)(void const*)> held_;
};

}  // namespace Fabrica::Core

