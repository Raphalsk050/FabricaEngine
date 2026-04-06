#include "core/jobs/executor.h"

namespace Fabrica::Core::Jobs {

namespace {

thread_local Executor* g_foreground_executor = nullptr;
thread_local Executor* g_background_executor = nullptr;
thread_local Executor* g_current_executor = nullptr;

ImmediateExecutor g_immediate_executor;

}  // namespace

Executor::~Executor() = default;

std::string Executor::ToString(Type type) {
  switch (type) {
    case Type::kImmediate:
      return "Immediate";
    case Type::kForeground:
      return "Foreground";
    case Type::kBackground:
      return "Background";
    case Type::kCurrent:
      return "Current";
  }
  return "Unknown";
}

TaskId Executor::Schedule(std::function<void()> fn, int task_priority) {
  return ScheduleInvocable(Core::Invocable<void()>([function = std::move(fn)]() mutable {
    function();
  }), task_priority);
}

TaskId Executor::ReserveTaskId() { return kGenericTaskId; }

bool Executor::ScheduleWithReservedTaskId(TaskId reserved_task_id,
                                          Core::Invocable<void()> invocable,
                                          int task_priority) {
  if (reserved_task_id == kGenericTaskId) {
    return ScheduleInvocable(std::move(invocable), task_priority).IsValid();
  }
  return false;
}

Core::Status Executor::UpdateTaskPriority(TaskId, int) {
  return Core::Status::InvalidArgument("Task reprioritization is unsupported");
}

Core::StatusOr<int> Executor::GetTaskPriority(TaskId) {
  return Core::Status::InvalidArgument("Task reprioritization is unsupported");
}

Executor* Executor::Get(Type type) {
  switch (type) {
    case Type::kImmediate:
      return &g_immediate_executor;
    case Type::kForeground:
      return g_foreground_executor;
    case Type::kBackground:
      return g_background_executor;
    case Type::kCurrent:
      return g_current_executor;
  }
  return nullptr;
}

Executor* Executor::BackgroundExecutor() { return g_background_executor; }

void Executor::SetBackgroundExecutor(Executor* executor) {
  g_background_executor = executor;
}

Executor* Executor::ForegroundExecutor() { return g_foreground_executor; }

void Executor::SetForegroundExecutor(Executor* executor) {
  g_foreground_executor = executor;
}

Executor* Executor::CurrentExecutor() { return g_current_executor; }

void Executor::SetCurrentExecutor(Executor* executor) {
  g_current_executor = executor;
}

TaskId ImmediateExecutor::ScheduleInvocable(Core::Invocable<void()> invocable,
                                            int) {
  if (!invocable) {
    return kInvalidTaskId;
  }
  invocable();
  return kGenericTaskId;
}

}  // namespace Fabrica::Core::Jobs

