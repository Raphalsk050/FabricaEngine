#include <chrono>
#include <thread>
#include <tuple>
#include <vector>

#include "core/async/future.h"
#include "core/common/test/test_framework.h"
#include "core/jobs/simple_foreground_executor.h"
#include "core/jobs/thread_pool_executor.h"

namespace {

using Fabrica::Core::Async::Future;
using Fabrica::Core::Jobs::Executor;
using Fabrica::Core::Jobs::SimpleForegroundExecutor;
using Fabrica::Core::Jobs::ThreadPoolExecutor;

FABRICA_TEST(FutureScheduleAndThenOnForeground) {
  SimpleForegroundExecutor foreground;
  Executor::SetForegroundExecutor(&foreground);
  Executor::SetBackgroundExecutor(&foreground);

  auto future = Future<int>::Schedule([]() { return 7; },
                                      Executor::Type::kForeground);
  auto chained = future.Then([](int value) { return value * 3; },
                             Executor::Type::kForeground);

  foreground.Pump(true);
  FABRICA_EXPECT_TRUE(chained.Ready());
  FABRICA_EXPECT_EQ(chained.Get().value(), 21);
}

FABRICA_TEST(FutureSupportsCancellation) {
  SimpleForegroundExecutor foreground;
  Executor::SetForegroundExecutor(&foreground);

  Future<int> future;
  future.Cancel();
  FABRICA_EXPECT_TRUE(future.Ready());
  FABRICA_EXPECT_TRUE(!future.Get().ok());
}

FABRICA_TEST(FutureCombineWaitForAllAndMerge) {
  SimpleForegroundExecutor foreground;
  Executor::SetForegroundExecutor(&foreground);
  Executor::SetBackgroundExecutor(&foreground);

  auto a = Future<int>::Schedule([]() { return 2; }, Executor::Type::kForeground);
  auto b = Future<int>::Schedule([]() { return 5; }, Executor::Type::kForeground);

  auto combined = a.CombineWaitForAll(b);
  auto merged = a.Merge(b);

  foreground.Pump(true);

  FABRICA_EXPECT_TRUE(combined.Ready());
  FABRICA_EXPECT_TRUE(combined.Get().ok());

  FABRICA_EXPECT_TRUE(merged.Ready());
  FABRICA_EXPECT_TRUE(merged.Get().ok());
  FABRICA_EXPECT_EQ(std::get<0>(merged.Get().value()), 2);
  FABRICA_EXPECT_EQ(std::get<1>(merged.Get().value()), 5);
}

FABRICA_TEST(FutureRunsInThreadPoolBackground) {
  SimpleForegroundExecutor foreground;
  ThreadPoolExecutor background({.worker_count = 2, .foreground_executor = &foreground});
  Executor::SetForegroundExecutor(&foreground);
  Executor::SetBackgroundExecutor(&background);

  auto background_future = Future<int>::Schedule(
      []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return 42;
      },
      Executor::Type::kBackground);

  auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!background_future.Ready() && std::chrono::steady_clock::now() < timeout) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  background.Shutdown();
  FABRICA_EXPECT_TRUE(background_future.Ready());
  FABRICA_EXPECT_EQ(background_future.Get().value(), 42);
}


FABRICA_TEST(FutureFallsBackToImmediateExecutorWhenNotConfigured) {
  Executor::SetForegroundExecutor(nullptr);
  Executor::SetBackgroundExecutor(nullptr);

  auto future = Future<int>::Schedule([]() { return 11; },
                                      Executor::Type::kForeground);
  FABRICA_EXPECT_TRUE(future.Ready());
  FABRICA_EXPECT_EQ(future.Get().value(), 11);

  auto chained = future.Then([](int value) { return value + 1; },
                             Executor::Type::kBackground);
  FABRICA_EXPECT_TRUE(chained.Ready());
  FABRICA_EXPECT_EQ(chained.Get().value(), 12);
}

}  // namespace
