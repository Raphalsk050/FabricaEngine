#include "core/async/future.h"
#include "core/common/status.h"
#include "core/jobs/executor.h"
#include "core/jobs/simple_foreground_executor.h"
#include "core/jobs/thread_pool_executor.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <tuple>

namespace {

using Fabrica::Core::Async::Future;
using Fabrica::Core::Jobs::Executor;
using Fabrica::Core::Jobs::SimpleForegroundExecutor;
using Fabrica::Core::Jobs::ThreadPoolExecutor;
using Fabrica::Core::Status;

/**
 * Pump the foreground executor until a future becomes ready or timeout expires.
 *
 * @tparam T Future value type.
 * @param future Future to wait for.
 * @param foreground Foreground executor used by continuation chains.
 * @param timeout Maximum wait duration.
 * @return `Status::Ok()` when future is ready before timeout.
 */
template <typename T>
Status WaitForFuture(const Future<T>& future,
                     SimpleForegroundExecutor* foreground,
                     std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!future.Ready() && std::chrono::steady_clock::now() < deadline) {
    if (foreground != nullptr) {
      foreground->Pump(true);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  if (!future.Ready()) {
    return Status::Unavailable("Future did not complete before timeout");
  }

  return Status::Ok();
}

/**
 * Demonstrate scheduling, chaining, and merge fan-in operations.
 *
 * @return `Status::Ok()` when async contracts behave as expected.
 */
Status RunAsyncDemo() {
  SimpleForegroundExecutor foreground;
  ThreadPoolExecutor background({.worker_count = 2, .foreground_executor = &foreground});
  Executor::SetForegroundExecutor(&foreground);
  Executor::SetBackgroundExecutor(&background);

  auto background_future = Future<int>::Schedule(
      []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return 21;
      },
      Executor::Type::kBackground);

  auto chained = background_future.Then(
      [](int value) { return value * 2; }, Executor::Type::kForeground);

  auto a = Future<int>::Schedule([]() { return 3; }, Executor::Type::kForeground);
  auto b = Future<int>::Schedule([]() { return 5; }, Executor::Type::kForeground);
  auto merged = a.Merge(b);

  Status wait_status = WaitForFuture(chained, &foreground, std::chrono::milliseconds(2000));
  if (!wait_status.ok()) {
    background.Shutdown();
    Executor::SetBackgroundExecutor(nullptr);
    Executor::SetForegroundExecutor(nullptr);
    return wait_status;
  }

  wait_status = WaitForFuture(merged, &foreground, std::chrono::milliseconds(2000));
  if (!wait_status.ok()) {
    background.Shutdown();
    Executor::SetBackgroundExecutor(nullptr);
    Executor::SetForegroundExecutor(nullptr);
    return wait_status;
  }

  if (!chained.Get().ok() || chained.Get().value() != 42) {
    background.Shutdown();
    Executor::SetBackgroundExecutor(nullptr);
    Executor::SetForegroundExecutor(nullptr);
    return Status::Internal("Unexpected chained future value");
  }

  if (!merged.Get().ok()) {
    background.Shutdown();
    Executor::SetBackgroundExecutor(nullptr);
    Executor::SetForegroundExecutor(nullptr);
    return merged.Get().status();
  }

  const auto& tuple_values = merged.Get().value();
  if (std::get<0>(tuple_values) != 3 || std::get<1>(tuple_values) != 5) {
    background.Shutdown();
    Executor::SetBackgroundExecutor(nullptr);
    Executor::SetForegroundExecutor(nullptr);
    return Status::Internal("Unexpected merge tuple values");
  }

  background.Shutdown();
  Executor::SetBackgroundExecutor(nullptr);
  Executor::SetForegroundExecutor(nullptr);
  return Status::Ok();
}

}  // namespace

int main() {
  const Status status = RunAsyncDemo();
  if (!status.ok()) {
    std::cerr << "[async_sample] Failed: " << status.ToString() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "[async_sample] Future scheduling and chaining completed successfully\n";
  return EXIT_SUCCESS;
}
