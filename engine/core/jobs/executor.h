#pragma once

#include <functional>
#include <string>

#include "core/common/invocable.h"
#include "core/common/status.h"
#include "core/jobs/task_id.h"
#include "core/jobs/task_priority.h"

namespace Fabrica::Core::Jobs {

/**
 * Defines the scheduling interface used by async and runtime systems.
 *
 * Executors abstract where and when tasks run (foreground, background, or
 * immediate). Concrete implementations may support reprioritization and pumping
 * semantics depending on threading model.
 */
class Executor {
 public:
  virtual ~Executor();

  /**
   * Identifies well-known executor slots configured by the runtime.
   */
  enum class Type {
    kImmediate,
    ///< Executes inline on caller thread.
    kForeground,
    ///< Executes on the foreground main-thread scheduler.
    kBackground,
    ///< Executes on the background worker pool.
    kCurrent,
    ///< Resolves to executor currently running the task.
  };

  /// Convert executor type to debug-friendly text.
  static std::string ToString(Type type);

  /**
   * Schedule a std::function task with optional priority.
   */
  TaskId Schedule(std::function<void()> fn,
                  int task_priority = kNormalTaskPriority);

  /**
   * Schedule a move-only invocable task.
   */
  virtual TaskId ScheduleInvocable(Core::Invocable<void()> invocable,
                                   int task_priority = kNormalTaskPriority) = 0;

  /**
   * Reserve a task id for deferred scheduling.
   *
   * Executors that do not support reservation may return `kGenericTaskId`.
   */
  [[nodiscard]] virtual TaskId ReserveTaskId();

  /**
   * Schedule work using a previously reserved id.
   *
   * @return True when the task is accepted.
   */
  virtual bool ScheduleWithReservedTaskId(
      TaskId reserved_task_id, Core::Invocable<void()> invocable,
      int task_priority = kNormalTaskPriority);

  /**
   * Update priority of an already scheduled task.
   */
  virtual Core::Status UpdateTaskPriority(TaskId task_id, int task_priority);

  /**
   * Query current priority of a scheduled task.
   */
  virtual Core::StatusOr<int> GetTaskPriority(TaskId task_id);

  /// Return true when reprioritization APIs are supported.
  virtual bool IsTaskReprioritizingSupported() { return false; }

  /// Stop accepting work and release executor-owned resources.
  virtual void Shutdown() = 0;

  /**
   * Execute pending work when executor requires cooperative pumping.
   *
   * @param drain When true, run until queue is empty.
   * @return True when at least one task was executed.
   */
  virtual bool Pump(bool /*drain*/) { return false; }

  /// Return true when callers must periodically invoke `Pump()`.
  virtual bool IsPumpingRequired() { return true; }

  /// Return true when there is still pending work.
  virtual bool HasPendingTasks() { return false; }

  /// Resolve one of the globally configured executor slots.
  static Executor* Get(Type type);

  /// Return the configured background executor.
  static Executor* BackgroundExecutor();

  /// Set the global background executor pointer.
  static void SetBackgroundExecutor(Executor* executor);

  /// Return the configured foreground executor.
  static Executor* ForegroundExecutor();

  /// Set the global foreground executor pointer.
  static void SetForegroundExecutor(Executor* executor);

  /// Return executor bound to the current worker context.
  static Executor* CurrentExecutor();

 protected:
  /// Bind current-thread executor context during task execution.
  static void SetCurrentExecutor(Executor* executor);
};

/**
 * Executes tasks immediately on the caller thread.
 *
 * This strategy is used as a safe fallback when no threaded executor is
 * configured.
 */
class ImmediateExecutor final : public Executor {
 public:
  TaskId ScheduleInvocable(Core::Invocable<void()> invocable,
                           int task_priority = kNormalTaskPriority) override;
  void Shutdown() override {}
  bool Pump(bool /*drain*/) override { return false; }
  bool IsPumpingRequired() override { return false; }
  bool HasPendingTasks() override { return false; }
};

}  // namespace Fabrica::Core::Jobs
