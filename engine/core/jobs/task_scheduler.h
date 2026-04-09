#pragma once

#include <chrono>
#include <deque>
#include <unordered_map>
#include <unordered_set>

#include "core/common/invocable.h"
#include "core/common/status.h"
#include "core/jobs/task_id.h"
#include "core/jobs/task_priority.h"

namespace Fabrica::Core::Jobs {

/**
 * Maintains a reprioritizable queue of pending tasks.
 *
 * The scheduler uses aging to prevent starvation: older tasks gradually gain
 * effective priority based on `task_age_rate`.
 */
class TaskScheduler {
 public:
  /**
   * Configures queue aging behavior.
   */
  struct TaskSchedulerOptions {
    std::chrono::milliseconds task_age_rate{100};
    ///< Time window used to increase effective priority by one step.
  };

  TaskScheduler();
  explicit TaskScheduler(TaskSchedulerOptions options);

  /**
   * Enqueue a task and return its generated id.
   */
  TaskId PushTask(Core::Invocable<void()> invocable,
                  int priority = kNormalTaskPriority);

  /**
   * Reserve an id for deferred enqueue operations.
   */
  TaskId ReserveTaskId();

  /**
   * Enqueue a task using a previously reserved id.
   *
   * @return True when enqueue succeeds.
   */
  bool PushWithReservedTaskId(TaskId task_id, Core::Invocable<void()> invocable,
                              int priority = kNormalTaskPriority);

  /**
   * Pop the highest effective-priority task.
   *
   * Returns an empty invocable when queue is empty.
   */
  [[nodiscard]] Core::Invocable<void()> PopTask();

  /// Update queued task priority.
  Core::Status RescheduleTask(TaskId task_id, int task_priority);

  /// Query queued task priority.
  Core::StatusOr<int> GetTaskPriority(TaskId task_id) const;

  bool IsEmpty() const;
  size_t Size() const;

  /// Remove all queued and reserved task state.
  void Clear();

 private:
  struct TaskEntry {
    TaskId task_id = kInvalidTaskId;
    int priority = kNormalTaskPriority;
    std::chrono::steady_clock::time_point enqueued_at;
    Core::Invocable<void()> invocable;
  };

  /**
   * Compute dynamic priority including aging contribution.
   */
  int EffectivePriority(const TaskEntry& entry,
                        std::chrono::steady_clock::time_point now) const;

  TaskSchedulerOptions options_;
  uint32_t next_task_id_ = kGenericTaskId.value + 1;
  std::unordered_map<uint32_t, TaskEntry> tasks_;
  std::unordered_set<uint32_t> reserved_task_ids_;
  std::deque<uint32_t> queue_order_;
};

}  // namespace Fabrica::Core::Jobs
