#include "pal/file_system.h"

#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

#include "core/common/test/test_framework.h"
#include "core/jobs/executor.h"
#include "core/jobs/simple_foreground_executor.h"
#include "core/jobs/thread_pool_executor.h"

namespace {

FABRICA_TEST(LocalFileSystemNormalizesPathSeparators) {
  const std::string normalized =
      Fabrica::PAL::LocalFileSystem::NormalizePath("a\\b\\c.txt");
  FABRICA_EXPECT_EQ(normalized, std::string("a/b/c.txt"));
}

FABRICA_TEST(LocalFileSystemReadsAndWritesAsync) {
  Fabrica::Core::Jobs::SimpleForegroundExecutor foreground;
  Fabrica::Core::Jobs::ThreadPoolExecutor background(
      {.worker_count = 2, .foreground_executor = &foreground});
  Fabrica::Core::Jobs::Executor::SetForegroundExecutor(&foreground);
  Fabrica::Core::Jobs::Executor::SetBackgroundExecutor(&background);

  Fabrica::PAL::LocalFileSystem file_system;

  const auto temp_path =
      std::filesystem::temp_directory_path() / "fabrica_pal_file_test.bin";
  std::vector<std::byte> bytes = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};

  auto write_future = file_system.WriteFileAsync(temp_path.string(), bytes);
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!write_future.Ready() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  FABRICA_EXPECT_TRUE(write_future.Ready());
  FABRICA_EXPECT_TRUE(write_future.Get().ok());

  auto read_future = file_system.ReadFileAsync(temp_path.string());
  while (!read_future.Ready() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  FABRICA_EXPECT_TRUE(read_future.Ready());
  FABRICA_EXPECT_TRUE(read_future.Get().ok());
  FABRICA_EXPECT_EQ(read_future.Get().value().size(), bytes.size());

  background.Shutdown();
}

}  // namespace

