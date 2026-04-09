#pragma once

#include <mutex>

#include "core/jobs/executor.h"
#include "core/jobs/task_scheduler.h"

namespace Fabrica::Core::Jobs {

/**
 * Runs prioritized tasks on the foreground thread via cooperative pumping.
 *
 * This executor pairs with the runtime loop, which calls `Pump()` each frame.
 * Scheduling and reprioritization are thread-safe.
 */
class SimpleForegroundExecutor final : public Executor {
 public:
  /// Create an executor with default scheduler options.
  SimpleForegroundExecutor();

  /// Create an executor with custom scheduler options.
  explicit SimpleForegroundExecutor(
      TaskScheduler::TaskSchedulerOptions options);

  TaskId ScheduleInvocable(Core::Invocable<void()> invocable,
                           int task_priority = kNormalTaskPriority) override;
  TaskId ReserveTaskId() override;
  bool ScheduleWithReservedTaskId(
      TaskId reserved_task_id, Core::Invocable<void()> invocable,
      int task_priority = kNormalTaskPriority) override;
  Core::Status UpdateTaskPriority(TaskId task_id, int task_priority) override;
  Core::StatusOr<int> GetTaskPriority(TaskId task_id) override;
  bool IsTaskReprioritizingSupported() override { return true; }

  /// Prevent new tasks and clear queued work.
  void Shutdown() override;

  /**
   * Execute one or more queued tasks.
   *
   * @param drain When true, run until queue is empty.
   * @return True when at least one task was executed.
   */
  bool Pump(bool drain) override;

  bool IsPumpingRequired() override { return true; }

  /// Return true when scheduler still has queued tasks.
  bool HasPendingTasks() override;

 private:
  mutable std::mutex mutex_;
  TaskScheduler scheduler_;
  bool shutting_down_ = false;
};

}  // namespace Fabrica::Core::Jobs
