#pragma once

#include <cstdint>
#include <functional>

namespace Fabrica::Core::Jobs {

struct TaskId {
  uint32_t value = 0;

  constexpr bool IsValid() const { return value != 0; }

  constexpr friend bool operator==(TaskId lhs, TaskId rhs) {
    return lhs.value == rhs.value;
  }
  constexpr friend bool operator!=(TaskId lhs, TaskId rhs) {
    return !(lhs == rhs);
  }
};

inline constexpr TaskId kInvalidTaskId{0};
inline constexpr TaskId kGenericTaskId{1};

}  // namespace Fabrica::Core::Jobs

namespace std {

template <>
struct hash<Fabrica::Core::Jobs::TaskId> {
  size_t operator()(const Fabrica::Core::Jobs::TaskId& task_id) const noexcept {
    return std::hash<uint32_t>{}(task_id.value);
  }
};

}  // namespace std

