#include "core/jobs/simple_foreground_executor.h"

#include "core/config/config.h"
#include "core/logging/logger.h"

namespace Fabrica::Core::Jobs {

SimpleForegroundExecutor::SimpleForegroundExecutor(
    TaskScheduler::TaskSchedulerOptions options)
    : scheduler_(options) {}

TaskId SimpleForegroundExecutor::ScheduleInvocable(Core::Invocable<void()> invocable,
                                                   int task_priority) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (shutting_down_) {
    return kInvalidTaskId;
  }
  TaskId task_id = scheduler_.PushTask(std::move(invocable), task_priority);
#if FABRICA_JOBS_VERBOSE_LOG
  FABRICA_LOG(Jobs, Debug) << "[Jobs][Scheduler] Task id=" << task_id.value
                           << " scheduled | priority=" << task_priority;
#endif
  return task_id;
}

TaskId SimpleForegroundExecutor::ReserveTaskId() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (shutting_down_) {
    return kInvalidTaskId;
  }
  return scheduler_.ReserveTaskId();
}

bool SimpleForegroundExecutor::ScheduleWithReservedTaskId(
    TaskId reserved_task_id, Core::Invocable<void()> invocable,
    int task_priority) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (shutting_down_) {
    return false;
  }
  return scheduler_.PushWithReservedTaskId(reserved_task_id, std::move(invocable),
                                           task_priority);
}

Core::Status SimpleForegroundExecutor::UpdateTaskPriority(TaskId task_id,
                                                          int task_priority) {
  std::lock_guard<std::mutex> lock(mutex_);
  return scheduler_.RescheduleTask(task_id, task_priority);
}

Core::StatusOr<int> SimpleForegroundExecutor::GetTaskPriority(TaskId task_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  return scheduler_.GetTaskPriority(task_id);
}

void SimpleForegroundExecutor::Shutdown() {
  std::lock_guard<std::mutex> lock(mutex_);
  shutting_down_ = true;
  scheduler_.Clear();
}

bool SimpleForegroundExecutor::Pump(bool drain) {
  bool ran_any_task = false;

  SetCurrentExecutor(this);
  while (true) {
    Core::Invocable<void()> task;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      task = scheduler_.PopTask();
    }

    if (!task) {
      break;
    }

    ran_any_task = true;
    task();

    if (!drain) {
      break;
    }
  }

  SetCurrentExecutor(nullptr);
  return ran_any_task;
}

bool SimpleForegroundExecutor::HasPendingTasks() {
  std::lock_guard<std::mutex> lock(mutex_);
  return !scheduler_.IsEmpty();
}

}  // namespace Fabrica::Core::Jobs

