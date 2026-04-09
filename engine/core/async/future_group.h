#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>

#include "core/jobs/task_priority.h"

namespace Fabrica::Core::Async::Internal {
class FutureImpl;
}

namespace Fabrica::Core::Async {

/**
 * Shares task-priority updates across a set of related futures.
 *
 * Futures can join a group so a caller can raise or lower urgency for all
 * members using one operation.
 */
class FutureGroup {
 public:
  /// Create an empty group with an initial cached priority.
  explicit FutureGroup(int task_priority = Jobs::kNormalTaskPriority);

  /**
   * Add a future implementation to the group.
   */
  void AddFuture(std::shared_ptr<Internal::FutureImpl> future);

  /**
   * Update priority for current and future group members.
   */
  void UpdatePriority(int task_priority);

  /// Return current cached group priority.
  int GetTaskPriority() const;

 private:
  struct FutureGroupImpl {
    mutable std::mutex mutex;
    int cached_priority = Jobs::kNormalTaskPriority;
    std::unordered_map<int, std::weak_ptr<Internal::FutureImpl>> futures;
    int next_future_id = 0;
  };

  std::shared_ptr<FutureGroupImpl> impl_;
};

}  // namespace Fabrica::Core::Async
