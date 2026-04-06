#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "core/jobs/executor.h"
#include "core/jobs/task_scheduler.h"

namespace Fabrica::Core::Jobs {

class ThreadPoolExecutor final : public Executor {
 public:
  struct Options {
    TaskScheduler::TaskSchedulerOptions scheduler_options{};
    size_t worker_count = 0;
    Executor* foreground_executor = nullptr;
  };

  explicit ThreadPoolExecutor(Options options = {});
  ~ThreadPoolExecutor() override;

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
  bool IsPumpingRequired() override { return false; }
  bool HasPendingTasks() override;

 private:
  static size_t ComputeWorkerCount(size_t configured);
  void RunTask(Core::Invocable<void()> task);
  void WorkerLoop(size_t index);
  Core::Invocable<void()> PopTaskLocked();

  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::condition_variable drain_cv_;
  TaskScheduler scheduler_;
  std::vector<std::thread> workers_;
  Executor* foreground_executor_ = nullptr;
  size_t active_tasks_ = 0;
  bool shutting_down_ = false;
};

}  // namespace Fabrica::Core::Jobs
