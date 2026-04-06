#pragma once

#include <functional>
#include <string>

#include "core/common/invocable.h"
#include "core/common/status.h"
#include "core/jobs/task_id.h"
#include "core/jobs/task_priority.h"

namespace Fabrica::Core::Jobs {

class Executor {
 public:
  virtual ~Executor();

  enum class Type { kImmediate, kForeground, kBackground, kCurrent };

  static std::string ToString(Type type);

  TaskId Schedule(std::function<void()> fn,
                  int task_priority = kNormalTaskPriority);
  virtual TaskId ScheduleInvocable(Core::Invocable<void()> invocable,
                                   int task_priority = kNormalTaskPriority) = 0;

  [[nodiscard]] virtual TaskId ReserveTaskId();
  virtual bool ScheduleWithReservedTaskId(
      TaskId reserved_task_id, Core::Invocable<void()> invocable,
      int task_priority = kNormalTaskPriority);

  virtual Core::Status UpdateTaskPriority(TaskId task_id, int task_priority);
  virtual Core::StatusOr<int> GetTaskPriority(TaskId task_id);
  virtual bool IsTaskReprioritizingSupported() { return false; }

  virtual void Shutdown() = 0;
  virtual bool Pump(bool /*drain*/) { return false; }
  virtual bool IsPumpingRequired() { return true; }
  virtual bool HasPendingTasks() { return false; }

  static Executor* Get(Type type);
  static Executor* BackgroundExecutor();
  static void SetBackgroundExecutor(Executor* executor);
  static Executor* ForegroundExecutor();
  static void SetForegroundExecutor(Executor* executor);
  static Executor* CurrentExecutor();

 protected:
  static void SetCurrentExecutor(Executor* executor);
};

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
