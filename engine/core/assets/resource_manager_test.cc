#include "core/assets/resource_manager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "core/common/test/test_framework.h"
#include "core/jobs/executor.h"
#include "core/jobs/simple_foreground_executor.h"
#include "core/jobs/thread_pool_executor.h"

namespace {

FABRICA_TEST(ResourceManagerDeduplicatesConcurrentTextureLoads) {
  Fabrica::Core::Jobs::SimpleForegroundExecutor foreground;
  Fabrica::Core::Jobs::ThreadPoolExecutor background(
      {.worker_count = 2, .foreground_executor = &foreground});
  Fabrica::Core::Jobs::Executor::SetForegroundExecutor(&foreground);
  Fabrica::Core::Jobs::Executor::SetBackgroundExecutor(&background);

  const auto file_path =
      std::filesystem::temp_directory_path() / "fabrica_asset_test_texture.bin";
  {
    std::ofstream file(file_path, std::ios::binary | std::ios::trunc);
    const char bytes[] = {'T', 'E', 'X', '0'};
    file.write(bytes, sizeof(bytes));
  }

  auto path_result =
      Fabrica::Core::Assets::AssetPath::Create(file_path.string());
  FABRICA_EXPECT_TRUE(path_result.ok());

  Fabrica::Core::Assets::ResourceManager manager;
  auto future_a = manager.LoadTexture(path_result.value(),
                                      Fabrica::Core::Assets::LoadPriority::kHigh);
  auto future_b = manager.LoadTexture(path_result.value(),
                                      Fabrica::Core::Assets::LoadPriority::kHigh);

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((!future_a.Ready() || !future_b.Ready()) &&
         std::chrono::steady_clock::now() < deadline) {
    foreground.Pump(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  FABRICA_EXPECT_TRUE(future_a.Ready());
  FABRICA_EXPECT_TRUE(future_b.Ready());
  FABRICA_EXPECT_TRUE(future_a.Get().ok());
  FABRICA_EXPECT_TRUE(future_b.Get().ok());
  FABRICA_EXPECT_EQ(future_a.Get().value().id, future_b.Get().value().id);

  background.Shutdown();
}

}  // namespace

