#include "core/jobs/thread_pool_executor.h"

#include <algorithm>

#include "core/config/config.h"
#include "core/logging/logger.h"

namespace Fabrica::Core::Jobs {

ThreadPoolExecutor::ThreadPoolExecutor(Options options)
    : scheduler_(options.scheduler_options),
      foreground_executor_(options.foreground_executor) {
  const size_t worker_count = ComputeWorkerCount(options.worker_count);
  workers_.reserve(worker_count);
  for (size_t i = 0; i < worker_count; ++i) {
    workers_.emplace_back([this, i]() { WorkerLoop(i); });
  }
}

ThreadPoolExecutor::~ThreadPoolExecutor() { Shutdown(); }

TaskId ThreadPoolExecutor::ScheduleInvocable(Core::Invocable<void()> invocable,
                                             int task_priority) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (shutting_down_) {
    return kInvalidTaskId;
  }

  TaskId task_id = scheduler_.PushTask(std::move(invocable), task_priority);
  cv_.notify_one();
#if FABRICA_JOBS_VERBOSE_LOG
  FABRICA_LOG(Jobs, Debug) << "[Jobs][Scheduler] Task id=" << task_id.value
                           << " scheduled | priority=" << task_priority;
#endif
  return task_id;
}

TaskId ThreadPoolExecutor::ReserveTaskId() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (shutting_down_) {
    return kInvalidTaskId;
  }
  return scheduler_.ReserveTaskId();
}

bool ThreadPoolExecutor::ScheduleWithReservedTaskId(
    TaskId reserved_task_id, Core::Invocable<void()> invocable,
    int task_priority) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (shutting_down_) {
    return false;
  }
  const bool scheduled = scheduler_.PushWithReservedTaskId(
      reserved_task_id, std::move(invocable), task_priority);
  if (scheduled) {
    cv_.notify_one();
  }
  return scheduled;
}

Core::Status ThreadPoolExecutor::UpdateTaskPriority(TaskId task_id,
                                                    int task_priority) {
  std::lock_guard<std::mutex> lock(mutex_);
  return scheduler_.RescheduleTask(task_id, task_priority);
}

Core::StatusOr<int> ThreadPoolExecutor::GetTaskPriority(TaskId task_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  return scheduler_.GetTaskPriority(task_id);
}

void ThreadPoolExecutor::Shutdown() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (shutting_down_) {
      return;
    }
    shutting_down_ = true;
  }
  cv_.notify_all();

  for (auto& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  workers_.clear();
}

bool ThreadPoolExecutor::Pump(bool drain) {
  bool ran_any = false;

  while (true) {
    Core::Invocable<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (drain && scheduler_.IsEmpty() && active_tasks_ > 0) {
        drain_cv_.wait(lock, [this]() {
          return active_tasks_ == 0 || !scheduler_.IsEmpty();
        });
      }
      task = PopTaskLocked();
      if (task) {
        ++active_tasks_;
      }
    }

    if (!task) {
      if (!drain) {
        break;
      }
      std::unique_lock<std::mutex> lock(mutex_);
      if (scheduler_.IsEmpty() && active_tasks_ == 0) {
        break;
      }
      drain_cv_.wait(lock, [this]() {
        return active_tasks_ == 0 || !scheduler_.IsEmpty();
      });
      continue;
    }

    ran_any = true;
    RunTask(std::move(task));

    if (!drain) {
      break;
    }
  }
  return ran_any;
}

bool ThreadPoolExecutor::HasPendingTasks() {
  std::lock_guard<std::mutex> lock(mutex_);
  return !scheduler_.IsEmpty();
}

size_t ThreadPoolExecutor::ComputeWorkerCount(size_t configured) {
  if (configured != 0) {
    return configured;
  }
  const size_t hc = std::max<size_t>(std::thread::hardware_concurrency(), 2);
  const size_t candidate = (hc > 2) ? (hc - 2) : 2;
  return std::clamp<size_t>(candidate, 2, 6);
}

void ThreadPoolExecutor::WorkerLoop(size_t index) {
  SetCurrentExecutor(this);
  SetBackgroundExecutor(this);
  if (foreground_executor_ != nullptr) {
    SetForegroundExecutor(foreground_executor_);
  }

#if FABRICA_JOBS_VERBOSE_LOG
  FABRICA_LOG(Jobs, Debug) << "[Jobs][ThreadPool] Worker started | index=" << index
                           << " | thread_id="
                           << std::hash<std::thread::id>{}(std::this_thread::get_id());
#else
  (void)index;
#endif

  while (true) {
    Core::Invocable<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this]() {
        return shutting_down_ || !scheduler_.IsEmpty();
      });
      if (shutting_down_ && scheduler_.IsEmpty()) {
        break;
      }
      task = PopTaskLocked();
      if (task) {
        ++active_tasks_;
      }
    }
    if (task) {
      RunTask(std::move(task));
    }
  }
}

Core::Invocable<void()> ThreadPoolExecutor::PopTaskLocked() {
  return scheduler_.PopTask();
}

void ThreadPoolExecutor::RunTask(Core::Invocable<void()> task) {
  task();

  std::lock_guard<std::mutex> lock(mutex_);
  if (active_tasks_ > 0) {
    --active_tasks_;
  }
  if (active_tasks_ == 0 && scheduler_.IsEmpty()) {
    drain_cv_.notify_all();
  }
}

}  // namespace Fabrica::Core::Jobs
