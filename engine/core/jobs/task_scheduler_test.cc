#include <chrono>
#include <thread>
#include <vector>

#include "core/common/test/test_framework.h"
#include "core/jobs/task_scheduler.h"

namespace {

using Fabrica::Core::Jobs::TaskScheduler;
using Fabrica::Core::Jobs::kHighTaskPriority;
using Fabrica::Core::Jobs::kLowTaskPriority;
using Fabrica::Core::Jobs::kNormalTaskPriority;

FABRICA_TEST(TaskSchedulerPopsHigherPriorityFirst) {
  TaskScheduler scheduler;
  std::vector<int> order;

  scheduler.PushTask(Fabrica::Core::Invocable<void()>([&order]() { order.push_back(1); }),
                     kLowTaskPriority);
  scheduler.PushTask(Fabrica::Core::Invocable<void()>([&order]() { order.push_back(2); }),
                     kHighTaskPriority);

  auto task1 = scheduler.PopTask();
  auto task2 = scheduler.PopTask();
  task1();
  task2();

  FABRICA_EXPECT_EQ(order.size(), 2u);
  FABRICA_EXPECT_EQ(order[0], 2);
  FABRICA_EXPECT_EQ(order[1], 1);
}

FABRICA_TEST(TaskSchedulerAppliesPriorityAging) {
  TaskScheduler scheduler({.task_age_rate = std::chrono::milliseconds(10)});
  std::vector<int> order;

  scheduler.PushTask(Fabrica::Core::Invocable<void()>([&order]() { order.push_back(1); }),
                     kNormalTaskPriority);
  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  scheduler.PushTask(Fabrica::Core::Invocable<void()>([&order]() { order.push_back(2); }),
                     kHighTaskPriority);

  auto task1 = scheduler.PopTask();
  auto task2 = scheduler.PopTask();
  task1();
  task2();

  FABRICA_EXPECT_EQ(order[0], 1);
  FABRICA_EXPECT_EQ(order[1], 2);
}

FABRICA_TEST(TaskSchedulerSupportsReservedTaskId) {
  TaskScheduler scheduler;
  bool ran = false;
  auto reserved = scheduler.ReserveTaskId();
  const bool scheduled = scheduler.PushWithReservedTaskId(
      reserved, Fabrica::Core::Invocable<void()>([&ran]() { ran = true; }),
      kNormalTaskPriority);

  FABRICA_EXPECT_TRUE(scheduled);
  auto task = scheduler.PopTask();
  FABRICA_EXPECT_TRUE(static_cast<bool>(task));
  task();
  FABRICA_EXPECT_TRUE(ran);
}

FABRICA_TEST(TaskSchedulerHandlesZeroAgeRateWithoutCrashing) {
  TaskScheduler scheduler({.task_age_rate = std::chrono::milliseconds(0)});
  bool ran = false;
  scheduler.PushTask(Fabrica::Core::Invocable<void()>([&ran]() { ran = true; }),
                     kNormalTaskPriority);

  auto task = scheduler.PopTask();
  FABRICA_EXPECT_TRUE(static_cast<bool>(task));
  task();
  FABRICA_EXPECT_TRUE(ran);
}

}  // namespace
