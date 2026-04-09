#include "core/common/status.h"
#include "core/jobs/executor.h"
#include "core/jobs/simple_foreground_executor.h"
#include "core/jobs/thread_pool_executor.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

namespace {

using Fabrica::Core::Status;
using Fabrica::Core::Jobs::Executor;
using Fabrica::Core::Jobs::SimpleForegroundExecutor;
using Fabrica::Core::Jobs::ThreadPoolExecutor;
using Fabrica::Core::Jobs::kHighTaskPriority;
using Fabrica::Core::Jobs::kLowTaskPriority;

/**
 * Wait for a counter to reach an expected value within a timeout budget.
 *
 * @param counter Shared completion counter.
 * @param expected Target counter value.
 * @param timeout Maximum wait time.
 * @return `Status::Ok()` when target is reached in time.
 */
Status WaitForCounter(const std::atomic<int>& counter, int expected,
                      std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (counter.load(std::memory_order_relaxed) < expected &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  if (counter.load(std::memory_order_relaxed) < expected) {
    return Status::Unavailable("Timed out while waiting for scheduled jobs");
  }
  return Status::Ok();
}

/**
 * Run foreground and background executor scheduling demonstrations.
 *
 * @return `Status::Ok()` when priority ordering and completion contracts hold.
 */
Status RunJobsDemo() {
  SimpleForegroundExecutor foreground;

  std::vector<int> foreground_order;
  foreground.ScheduleInvocable(
      Fabrica::Core::Invocable<void()>(
          [&foreground_order]() { foreground_order.push_back(1); }),
      kLowTaskPriority);
  foreground.ScheduleInvocable(
      Fabrica::Core::Invocable<void()>(
          [&foreground_order]() { foreground_order.push_back(2); }),
      kHighTaskPriority);

  foreground.Pump(true);
  if (foreground_order.size() != 2 || foreground_order[0] != 2) {
    return Status::Internal("Foreground executor did not honor task priority");
  }

  ThreadPoolExecutor background({.worker_count = 2, .foreground_executor = &foreground});
  Executor::SetForegroundExecutor(&foreground);
  Executor::SetBackgroundExecutor(&background);

  std::atomic<int> completed = 0;
  for (int index = 0; index < 8; ++index) {
    Executor::BackgroundExecutor()->ScheduleInvocable(Fabrica::Core::Invocable<void()>(
        [&completed]() { completed.fetch_add(1, std::memory_order_relaxed); }));
  }

  const Status wait_status =
      WaitForCounter(completed, 8, std::chrono::milliseconds(2000));

  background.Shutdown();
  Executor::SetBackgroundExecutor(nullptr);
  Executor::SetForegroundExecutor(nullptr);

  return wait_status;
}

}  // namespace

int main() {
  const Status status = RunJobsDemo();
  if (!status.ok()) {
    std::cerr << "[jobs_sample] Failed: " << status.ToString() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "[jobs_sample] Foreground and thread-pool executors ran successfully\n";
  return EXIT_SUCCESS;
}
