#include "core/common/status.h"
#include "core/jobs/executor.h"
#include "core/jobs/simple_foreground_executor.h"
#include "core/jobs/thread_pool_executor.h"
#include "pal/file_system.h"
#include "pal/threading.h"

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

using Fabrica::Core::Jobs::Executor;
using Fabrica::Core::Jobs::SimpleForegroundExecutor;
using Fabrica::Core::Jobs::ThreadPoolExecutor;
using Fabrica::Core::Status;
using Fabrica::PAL::LocalFileSystem;

/**
 * Wait for a future while the foreground executor drains continuation work.
 *
 * @tparam T Future value type.
 * @param future Future to await.
 * @param foreground Foreground executor used by continuation chains.
 * @return `Status::Ok()` when the future is ready before timeout.
 */
template <typename T>
Status WaitForFuture(const Fabrica::Core::Async::Future<T>& future,
                     SimpleForegroundExecutor* foreground) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!future.Ready() && std::chrono::steady_clock::now() < deadline) {
    foreground->Pump(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  if (!future.Ready()) {
    return Status::Unavailable("PAL async operation timed out");
  }

  return Status::Ok();
}

/**
 * Demonstrate PAL threading and filesystem adapters.
 *
 * @return `Status::Ok()` when thread and async file operations complete.
 */
Status RunPalDemo() {
  if (Fabrica::PAL::GetHardwareConcurrency() == 0) {
    return Status::Internal("Hardware concurrency must be greater than zero");
  }

  std::thread worker([]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  });
  Fabrica::PAL::SetThreadName(worker.native_handle(), "pal_sample_worker");
  const Status affinity_status =
      Fabrica::PAL::SetThreadAffinity(worker.native_handle(), 0x1);
  worker.join();

  if (!affinity_status.ok()) {
    std::cout << "[pal_sample] Affinity update not available: "
              << affinity_status.ToString() << '\n';
  }

  const std::string normalized =
      LocalFileSystem::NormalizePath("folder\\subfolder\\asset.bin");
  if (normalized.find('\\') != std::string::npos) {
    return Status::Internal("Path normalization should replace backslashes");
  }

  SimpleForegroundExecutor foreground;
  ThreadPoolExecutor background({.worker_count = 2, .foreground_executor = &foreground});
  Executor::SetForegroundExecutor(&foreground);
  Executor::SetBackgroundExecutor(&background);

  LocalFileSystem file_system;
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "fabrica_pal_sample.bin";
  const std::vector<std::byte> bytes = {
      std::byte{0x46}, std::byte{0x41}, std::byte{0x42}, std::byte{0x52}};

  auto write_future = file_system.WriteFileAsync(path.string(), bytes);
  Status status = WaitForFuture(write_future, &foreground);
  if (!status.ok()) {
    background.Shutdown();
    Executor::SetBackgroundExecutor(nullptr);
    Executor::SetForegroundExecutor(nullptr);
    std::filesystem::remove(path);
    return status;
  }

  if (!write_future.Get().ok()) {
    background.Shutdown();
    Executor::SetBackgroundExecutor(nullptr);
    Executor::SetForegroundExecutor(nullptr);
    std::filesystem::remove(path);
    return write_future.Get();
  }

  auto read_future = file_system.ReadFileAsync(path.string());
  status = WaitForFuture(read_future, &foreground);
  if (!status.ok()) {
    background.Shutdown();
    Executor::SetBackgroundExecutor(nullptr);
    Executor::SetForegroundExecutor(nullptr);
    std::filesystem::remove(path);
    return status;
  }

  if (!read_future.Get().ok()) {
    background.Shutdown();
    Executor::SetBackgroundExecutor(nullptr);
    Executor::SetForegroundExecutor(nullptr);
    std::filesystem::remove(path);
    return read_future.Get().status();
  }

  if (read_future.Get().value().size() != bytes.size()) {
    background.Shutdown();
    Executor::SetBackgroundExecutor(nullptr);
    Executor::SetForegroundExecutor(nullptr);
    std::filesystem::remove(path);
    return Status::Internal("Read byte count does not match written payload");
  }

  background.Shutdown();
  Executor::SetBackgroundExecutor(nullptr);
  Executor::SetForegroundExecutor(nullptr);
  std::filesystem::remove(path);
  return Status::Ok();
}

}  // namespace

int main() {
  const Status status = RunPalDemo();
  if (!status.ok()) {
    std::cerr << "[pal_sample] Failed: " << status.ToString() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "[pal_sample] Threading and filesystem adapters completed successfully\n";
  return EXIT_SUCCESS;
}
