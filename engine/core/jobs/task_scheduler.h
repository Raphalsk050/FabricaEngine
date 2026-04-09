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

class TaskScheduler {
 public:
  struct TaskSchedulerOptions {
    std::chrono::milliseconds task_age_rate{100};
  };

  TaskScheduler();
  explicit TaskScheduler(TaskSchedulerOptions options);

  TaskId PushTask(Core::Invocable<void()> invocable,
                  int priority = kNormalTaskPriority);
  TaskId ReserveTaskId();
  bool PushWithReservedTaskId(TaskId task_id, Core::Invocable<void()> invocable,
                              int priority = kNormalTaskPriority);

  [[nodiscard]] Core::Invocable<void()> PopTask();
  Core::Status RescheduleTask(TaskId task_id, int task_priority);
  Core::StatusOr<int> GetTaskPriority(TaskId task_id) const;

  bool IsEmpty() const;
  size_t Size() const;
  void Clear();

 private:
  struct TaskEntry {
    TaskId task_id = kInvalidTaskId;
    int priority = kNormalTaskPriority;
    std::chrono::steady_clock::time_point enqueued_at;
    Core::Invocable<void()> invocable;
  };

  int EffectivePriority(const TaskEntry& entry,
                        std::chrono::steady_clock::time_point now) const;

  TaskSchedulerOptions options_;
  uint32_t next_task_id_ = kGenericTaskId.value + 1;
  std::unordered_map<uint32_t, TaskEntry> tasks_;
  std::unordered_set<uint32_t> reserved_task_ids_;
  std::deque<uint32_t> queue_order_;
};

}  // namespace Fabrica::Core::Jobs


