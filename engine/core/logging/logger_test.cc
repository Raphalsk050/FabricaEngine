#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include "core/common/test/test_framework.h"
#include "core/logging/logger.h"

namespace {

using Fabrica::Core::Logging::LogChannel;
using Fabrica::Core::Logging::LogLevel;
using Fabrica::Core::Logging::Logger;
using Fabrica::Core::Logging::LoggerConfig;

std::string ReadAll(const std::filesystem::path& path) {
  std::ifstream file(path);
  std::ostringstream stream;
  stream << file.rdbuf();
  return stream.str();
}

FABRICA_TEST(LoggerWritesMessageToDisk) {
  const auto path = std::filesystem::temp_directory_path() / "fabrica_logger_test.log";

  Logger& logger = Logger::Instance();
  logger.Shutdown();
  LoggerConfig config;
  config.file_path = path.string();
  config.min_level = LogLevel::kDebug;
  logger.Initialize(config);

  logger.Submit(LogChannel::kCore, LogLevel::kInfo, __FILE__, __LINE__,
                "logger write test");
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  logger.Shutdown();

  const std::string contents = ReadAll(path);
  FABRICA_EXPECT_TRUE(contents.find("logger write test") != std::string::npos);
}

FABRICA_TEST(LoggerFiltersDisabledChannel) {
  const auto path =
      std::filesystem::temp_directory_path() / "fabrica_logger_filter_test.log";

  Logger& logger = Logger::Instance();
  logger.Shutdown();
  LoggerConfig config;
  config.file_path = path.string();
  config.min_level = LogLevel::kDebug;
  logger.Initialize(config);

  logger.SetChannelEnabled(LogChannel::kAssets, false);
  logger.Submit(LogChannel::kAssets, LogLevel::kInfo, __FILE__, __LINE__,
                "must not be written");
  logger.Submit(LogChannel::kCore, LogLevel::kInfo, __FILE__, __LINE__,
                "must be written");
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  logger.Shutdown();

  const std::string contents = ReadAll(path);
  FABRICA_EXPECT_TRUE(contents.find("must not be written") == std::string::npos);
  FABRICA_EXPECT_TRUE(contents.find("must be written") != std::string::npos);
}

}  // namespace
