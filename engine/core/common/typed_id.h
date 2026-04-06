#pragma once

#include <cstdint>
#include <functional>

namespace Fabrica::Core {

template <typename Tag, typename UnderlyingT = std::uint32_t>
class TypedId {
 public:
  using UnderlyingType = UnderlyingT;

  constexpr TypedId() = default;
  explicit constexpr TypedId(UnderlyingT value) : value_(value) {}

  static constexpr TypedId Invalid() { return TypedId(); }

  constexpr bool IsValid() const { return value_ != 0; }
  constexpr UnderlyingT Value() const { return value_; }

  constexpr friend bool operator==(TypedId lhs, TypedId rhs) {
    return lhs.value_ == rhs.value_;
  }
  constexpr friend bool operator!=(TypedId lhs, TypedId rhs) {
    return !(lhs == rhs);
  }
  constexpr friend bool operator<(TypedId lhs, TypedId rhs) {
    return lhs.value_ < rhs.value_;
  }

 private:
  UnderlyingT value_ = 0;
};

}  // namespace Fabrica::Core

namespace std {

template <typename Tag, typename UnderlyingT>
struct hash<Fabrica::Core::TypedId<Tag, UnderlyingT>> {
  size_t operator()(
      const Fabrica::Core::TypedId<Tag, UnderlyingT>& typed_id) const noexcept {
    return std::hash<UnderlyingT>{}(typed_id.Value());
  }
};

}  // namespace std

