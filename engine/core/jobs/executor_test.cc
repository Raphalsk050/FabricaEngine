#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "core/common/test/test_framework.h"
#include "core/jobs/simple_foreground_executor.h"
#include "core/jobs/thread_pool_executor.h"

namespace {

using Fabrica::Core::Jobs::SimpleForegroundExecutor;
using Fabrica::Core::Jobs::ThreadPoolExecutor;
using Fabrica::Core::Jobs::kHighTaskPriority;
using Fabrica::Core::Jobs::kLowTaskPriority;
using Fabrica::Core::Jobs::kNormalTaskPriority;

FABRICA_TEST(SimpleForegroundExecutorRunsOnPump) {
  SimpleForegroundExecutor executor;
  std::vector<int> order;

  executor.ScheduleInvocable(
      Fabrica::Core::Invocable<void()>([&order]() { order.push_back(1); }),
      kLowTaskPriority);
  executor.ScheduleInvocable(
      Fabrica::Core::Invocable<void()>([&order]() { order.push_back(2); }),
      kHighTaskPriority);

  const bool ran = executor.Pump(true);
  FABRICA_EXPECT_TRUE(ran);
  FABRICA_EXPECT_EQ(order.size(), 2u);
  FABRICA_EXPECT_EQ(order[0], 2);
}

FABRICA_TEST(SimpleForegroundExecutorSupportsReservedTaskReprioritization) {
  SimpleForegroundExecutor executor;
  std::vector<int> order;

  auto task_a = executor.ReserveTaskId();
  auto task_b = executor.ReserveTaskId();
  executor.ScheduleWithReservedTaskId(
      task_a, Fabrica::Core::Invocable<void()>([&order]() { order.push_back(1); }),
      kNormalTaskPriority);
  executor.ScheduleWithReservedTaskId(
      task_b, Fabrica::Core::Invocable<void()>([&order]() { order.push_back(2); }),
      kNormalTaskPriority);

  executor.UpdateTaskPriority(task_a, kHighTaskPriority);
  executor.Pump(true);
  FABRICA_EXPECT_EQ(order[0], 1);
}

FABRICA_TEST(ThreadPoolExecutorExecutesTasksInBackground) {
  ThreadPoolExecutor executor({.worker_count = 2});
  std::atomic<int> counter = 0;

  for (int i = 0; i < 8; ++i) {
    executor.ScheduleInvocable(Fabrica::Core::Invocable<void()>(
        [&counter]() { counter.fetch_add(1, std::memory_order_relaxed); }));
  }

  const auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (counter.load(std::memory_order_relaxed) < 8 &&
         std::chrono::steady_clock::now() < timeout) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  executor.Shutdown();
  FABRICA_EXPECT_EQ(counter.load(std::memory_order_relaxed), 8);
}

FABRICA_TEST(ThreadPoolExecutorPumpDrainWaitsUntilAllTasksComplete) {
  ThreadPoolExecutor executor({.worker_count = 2});
  std::atomic<int> counter = 0;

  for (int i = 0; i < 6; ++i) {
    executor.ScheduleInvocable(Fabrica::Core::Invocable<void()>([&counter]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      counter.fetch_add(1, std::memory_order_relaxed);
    }));
  }

  executor.Pump(true);
  FABRICA_EXPECT_EQ(counter.load(std::memory_order_relaxed), 6);
  executor.Shutdown();
}

}  // namespace
