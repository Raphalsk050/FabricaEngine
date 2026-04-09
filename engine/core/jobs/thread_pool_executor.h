#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "core/jobs/executor.h"
#include "core/jobs/task_scheduler.h"

namespace Fabrica::Core::Jobs {

/**
 * Executes prioritized tasks on a worker thread pool.
 *
 * The pool supports dynamic reprioritization and optional foreground handoff.
 * It uses condition variables for wake-up and drain synchronization.
 */
class ThreadPoolExecutor final : public Executor {
 public:
  /**
   * Configures worker topology and scheduler behavior.
   */
  struct Options {
    TaskScheduler::TaskSchedulerOptions scheduler_options{};
    ///< Aging settings for queued work.

    size_t worker_count = 0;
    ///< Explicit worker count. Zero selects hardware-based default.

    Executor* foreground_executor = nullptr;
    ///< Optional executor used for foreground fallback dispatch.
  };

  ThreadPoolExecutor();
  explicit ThreadPoolExecutor(Options options);
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

  /// Stop workers, reject new work, and drain pending tasks.
  void Shutdown() override;

  /**
   * Execute pending work from caller thread.
   *
   * @param drain When true, execute until queue reaches idle state.
   * @return True when at least one task was executed.
   */
  bool Pump(bool drain) override;

  bool IsPumpingRequired() override { return false; }

  /// Return true when scheduler or workers still hold pending work.
  bool HasPendingTasks() override;

 private:
  /// Compute default worker count from platform capabilities.
  static size_t ComputeWorkerCount(size_t configured);

  /// Execute one task and update executor context/liveness counters.
  void RunTask(Core::Invocable<void()> task);

  /// Main worker loop for one thread index.
  void WorkerLoop(size_t index);

  /// Pop one task while holding executor synchronization.
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
