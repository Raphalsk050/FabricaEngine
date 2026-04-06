#include "core/jobs/task_scheduler.h"

#include <algorithm>
#include <cstddef>
#include <limits>

namespace Fabrica::Core::Jobs {

TaskScheduler::TaskScheduler(TaskSchedulerOptions options)
    : options_(options) {}

TaskId TaskScheduler::PushTask(Core::Invocable<void()> invocable, int priority) {
  TaskId task_id = ReserveTaskId();
  if (!PushWithReservedTaskId(task_id, std::move(invocable), priority)) {
    return kInvalidTaskId;
  }
  return task_id;
}

TaskId TaskScheduler::ReserveTaskId() {
  TaskId reserved{next_task_id_++};
  reserved_task_ids_.insert(reserved.value);
  return reserved;
}

bool TaskScheduler::PushWithReservedTaskId(TaskId task_id,
                                           Core::Invocable<void()> invocable,
                                           int priority) {
  if (!invocable) {
    return false;
  }
  if (!task_id.IsValid() || task_id == kGenericTaskId) {
    return false;
  }

  if (!reserved_task_ids_.contains(task_id.value)) {
    return false;
  }
  reserved_task_ids_.erase(task_id.value);

  TaskEntry entry;
  entry.task_id = task_id;
  entry.priority = std::clamp(priority, kMinimumTaskPriority, kMaximumTaskPriority);
  entry.enqueued_at = std::chrono::steady_clock::now();
  entry.invocable = std::move(invocable);

  tasks_[task_id.value] = std::move(entry);
  queue_order_.push_back(task_id.value);
  return true;
}

Core::Invocable<void()> TaskScheduler::PopTask() {
  if (queue_order_.empty()) {
    return {};
  }

  const auto now = std::chrono::steady_clock::now();
  int best_priority = std::numeric_limits<int>::min();
  size_t best_index = queue_order_.size();

  for (size_t i = 0; i < queue_order_.size(); ++i) {
    const uint32_t key = queue_order_[i];
    auto entry_it = tasks_.find(key);
    if (entry_it == tasks_.end()) {
      continue;
    }

    const int candidate_priority = EffectivePriority(entry_it->second, now);
    if (candidate_priority > best_priority) {
      best_priority = candidate_priority;
      best_index = i;
    }
  }

  if (best_index == queue_order_.size()) {
    return {};
  }

  const uint32_t key = queue_order_[best_index];
  queue_order_.erase(queue_order_.begin() + static_cast<ptrdiff_t>(best_index));

  auto entry_it = tasks_.find(key);
  if (entry_it == tasks_.end()) {
    return {};
  }

  Core::Invocable<void()> invocable = std::move(entry_it->second.invocable);
  tasks_.erase(entry_it);
  return invocable;
}

Core::Status TaskScheduler::RescheduleTask(TaskId task_id, int task_priority) {
  auto task_it = tasks_.find(task_id.value);
  if (task_it == tasks_.end()) {
    return Core::Status::NotFound("Task not found");
  }
  task_it->second.priority =
      std::clamp(task_priority, kMinimumTaskPriority, kMaximumTaskPriority);
  return Core::Status::Ok();
}

Core::StatusOr<int> TaskScheduler::GetTaskPriority(TaskId task_id) const {
  auto task_it = tasks_.find(task_id.value);
  if (task_it == tasks_.end()) {
    return Core::Status::NotFound("Task not found");
  }
  return task_it->second.priority;
}

bool TaskScheduler::IsEmpty() const { return queue_order_.empty(); }

size_t TaskScheduler::Size() const { return queue_order_.size(); }

void TaskScheduler::Clear() {
  tasks_.clear();
  reserved_task_ids_.clear();
  queue_order_.clear();
}

int TaskScheduler::EffectivePriority(
    const TaskEntry& entry, std::chrono::steady_clock::time_point now) const {
  const auto age_rate_count = options_.task_age_rate.count();
  if (age_rate_count <= 0) {
    return entry.priority;
  }

  const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - entry.enqueued_at);
  const int age_bonus = static_cast<int>(waited.count() / age_rate_count);
  return entry.priority + age_bonus;
}

}  // namespace Fabrica::Core::Jobs
