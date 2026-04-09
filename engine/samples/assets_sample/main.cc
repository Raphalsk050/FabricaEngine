#include "core/assets/asset_path.h"
#include "core/assets/resource_manager.h"
#include "core/common/status.h"
#include "core/jobs/executor.h"
#include "core/jobs/simple_foreground_executor.h"
#include "core/jobs/thread_pool_executor.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace {

using Fabrica::Core::Assets::AssetPath;
using Fabrica::Core::Assets::LoadPriority;
using Fabrica::Core::Assets::ResourceManager;
using Fabrica::Core::Jobs::Executor;
using Fabrica::Core::Jobs::SimpleForegroundExecutor;
using Fabrica::Core::Jobs::ThreadPoolExecutor;
using Fabrica::Core::Status;

/**
 * Pump the foreground queue until a future is ready or timeout is reached.
 *
 * @tparam T Future value type.
 * @param future Future to observe.
 * @param foreground Foreground executor that runs upload continuations.
 * @return `Status::Ok()` when the future completed in time.
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
    return Status::Unavailable("Asset future did not complete before timeout");
  }

  return Status::Ok();
}

/**
 * Demonstrate in-flight deduplication for repeated texture requests.
 *
 * @return `Status::Ok()` when both requests resolve to the same handle id.
 */
Status RunAssetsDemo() {
  const std::filesystem::path asset_path =
      std::filesystem::temp_directory_path() / "fabrica_assets_sample.bin";
  {
    std::ofstream file(asset_path, std::ios::binary | std::ios::trunc);
    const char payload[] = {'A', 'S', 'S', 'E', 'T'};
    file.write(payload, sizeof(payload));
  }

  SimpleForegroundExecutor foreground;
  ThreadPoolExecutor background({.worker_count = 2, .foreground_executor = &foreground});
  Executor::SetForegroundExecutor(&foreground);
  Executor::SetBackgroundExecutor(&background);

  const auto path_or = AssetPath::Create(asset_path.string());
  if (!path_or.ok()) {
    background.Shutdown();
    Executor::SetBackgroundExecutor(nullptr);
    Executor::SetForegroundExecutor(nullptr);
    std::filesystem::remove(asset_path);
    return path_or.status();
  }

  ResourceManager manager;
  auto texture_a = manager.LoadTexture(path_or.value(), LoadPriority::kHigh);
  auto texture_b = manager.LoadTexture(path_or.value(), LoadPriority::kHigh);

  Status wait_status = WaitForFuture(texture_a, &foreground);
  if (!wait_status.ok()) {
    background.Shutdown();
    Executor::SetBackgroundExecutor(nullptr);
    Executor::SetForegroundExecutor(nullptr);
    std::filesystem::remove(asset_path);
    return wait_status;
  }

  wait_status = WaitForFuture(texture_b, &foreground);
  if (!wait_status.ok()) {
    background.Shutdown();
    Executor::SetBackgroundExecutor(nullptr);
    Executor::SetForegroundExecutor(nullptr);
    std::filesystem::remove(asset_path);
    return wait_status;
  }

  if (!texture_a.Get().ok() || !texture_b.Get().ok()) {
    background.Shutdown();
    Executor::SetBackgroundExecutor(nullptr);
    Executor::SetForegroundExecutor(nullptr);
    std::filesystem::remove(asset_path);
    return Status::Internal("Texture loads should resolve successfully");
  }

  if (texture_a.Get().value().id != texture_b.Get().value().id) {
    background.Shutdown();
    Executor::SetBackgroundExecutor(nullptr);
    Executor::SetForegroundExecutor(nullptr);
    std::filesystem::remove(asset_path);
    return Status::Internal("Expected deduplicated texture handle id");
  }

  background.Shutdown();
  Executor::SetBackgroundExecutor(nullptr);
  Executor::SetForegroundExecutor(nullptr);
  std::filesystem::remove(asset_path);
  return Status::Ok();
}

}  // namespace

int main() {
  const Status status = RunAssetsDemo();
  if (!status.ok()) {
    std::cerr << "[assets_sample] Failed: " << status.ToString() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "[assets_sample] Deduplicated texture loading completed successfully\n";
  return EXIT_SUCCESS;
}
