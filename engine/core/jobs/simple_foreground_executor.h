#pragma once

#include <mutex>

#include "core/jobs/executor.h"
#include "core/jobs/task_scheduler.h"

namespace Fabrica::Core::Jobs {

class SimpleForegroundExecutor final : public Executor {
 public:
  explicit SimpleForegroundExecutor(
      TaskScheduler::TaskSchedulerOptions options = {});

  TaskId ScheduleInvocable(Core::Invocable<void()> invocable,
                           int task_priority = kNormalTaskPriority) override;
  TaskId ReserveTaskId() override;
  bool ScheduleWithReservedTaskId(
      TaskId reserved_task_id, Core::Invocable<void()> invocable,
      int task_priority = kNormalTaskPriority) override;
  Core::Status UpdateTaskPriority(TaskId task_id, int task_priority) override;
  Core::StatusOr<int> GetTaskPriority(TaskId task_id) override;
  bool IsTaskReprioritizingSupported() override { return true; }

  void Shutdown() override;
  bool Pump(bool drain) override;
  bool IsPumpingRequired() override { return true; }
  bool HasPendingTasks() override;

 private:
  mutable std::mutex mutex_;
  TaskScheduler scheduler_;
  bool shutting_down_ = false;
};

}  // namespace Fabrica::Core::Jobs

