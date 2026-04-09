#pragma once

#include <cstdint>
#include <functional>

namespace Fabrica::Core::Jobs {

/**
 * Identifies a scheduled task inside executor queues.
 */
struct TaskId {
  uint32_t value = 0;

  /// Return true when id is not the invalid sentinel.
  constexpr bool IsValid() const { return value != 0; }

  constexpr friend bool operator==(TaskId lhs, TaskId rhs) {
    return lhs.value == rhs.value;
  }
  constexpr friend bool operator!=(TaskId lhs, TaskId rhs) {
    return !(lhs == rhs);
  }
};

inline constexpr TaskId kInvalidTaskId{0};
///< Sentinel id used for invalid/unassigned tasks.

inline constexpr TaskId kGenericTaskId{1};
///< Fallback id used by executors without reservation support.

}  // namespace Fabrica::Core::Jobs

namespace std {

/**
 * Enables `TaskId` as key type in unordered containers.
 */
template <>
struct hash<Fabrica::Core::Jobs::TaskId> {
  size_t operator()(const Fabrica::Core::Jobs::TaskId& task_id) const noexcept {
    return std::hash<uint32_t>{}(task_id.value);
  }
};

}  // namespace std
