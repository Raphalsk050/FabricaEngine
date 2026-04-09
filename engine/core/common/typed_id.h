#pragma once

#include <cstdint>
#include <functional>

namespace Fabrica::Core {

/**
 * Provides strongly typed identifiers over primitive integer values.
 *
 * This value type prevents accidental mixing of ids from unrelated domains
 * while preserving compact storage and cheap comparisons.
 *
 * @tparam Tag Distinct phantom type that defines id domain.
 * @tparam UnderlyingT Integer storage type.
 */
template <typename Tag, typename UnderlyingT = std::uint32_t>
class TypedId {
 public:
  using UnderlyingType = UnderlyingT;

  constexpr TypedId() = default;
  explicit constexpr TypedId(UnderlyingT value) : value_(value) {}

  /// Return the default invalid id value.
  static constexpr TypedId Invalid() { return TypedId(); }

  /// Return true when id value is not the invalid sentinel.
  constexpr bool IsValid() const { return value_ != 0; }

  /// Return the underlying integer representation.
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

/**
 * Enables `TypedId` usage as a key in unordered containers.
 */
template <typename Tag, typename UnderlyingT>
struct hash<Fabrica::Core::TypedId<Tag, UnderlyingT>> {
  size_t operator()(
      const Fabrica::Core::TypedId<Tag, UnderlyingT>& typed_id) const noexcept {
    return std::hash<UnderlyingT>{}(typed_id.Value());
  }
};

}  // namespace std
