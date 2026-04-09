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

FABRICA_TEST(LoggerUsesReadableFormatForFileSink) {
  const auto path =
      std::filesystem::temp_directory_path() / "fabrica_logger_format_test.log";

  Logger& logger = Logger::Instance();
  logger.Shutdown();

  LoggerConfig config;
  config.file_path = path.string();
  config.enable_console_sink = false;
  config.enable_console_colors = false;

  logger.Initialize(config);
  logger.Submit(LogChannel::kCore, LogLevel::kInfo, __FILE__, __LINE__,
                "format test message");
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  logger.Shutdown();

  const std::string contents = ReadAll(path);
  FABRICA_EXPECT_TRUE(contents.find(" [INFO] [Core] [main#") != std::string::npos);
  FABRICA_EXPECT_TRUE(contents.find("logger_test.cc:") != std::string::npos);
}

FABRICA_TEST(LoggerFallsBackToConsoleWhenFileSinkFails) {
  const auto missing_directory =
      std::filesystem::temp_directory_path() / "fabrica_logger_missing_dir";
  const auto file_path = missing_directory / "fabrica_logger_console_fallback.log";
  std::filesystem::remove_all(missing_directory);

  Logger& logger = Logger::Instance();
  logger.Shutdown();

  LoggerConfig config;
  config.file_path = file_path.string();
  config.enable_file_sink = true;
  config.enable_console_sink = true;

  const Fabrica::Core::Status status = logger.Initialize(config);
  FABRICA_EXPECT_TRUE(status.ok());

  logger.Submit(LogChannel::kGame, LogLevel::kError, __FILE__, __LINE__,
                "console fallback message");
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  logger.Shutdown();

  FABRICA_EXPECT_TRUE(!std::filesystem::exists(file_path));
}

FABRICA_TEST(LoggerFailsWhenNoSinkIsEnabled) {
  Logger& logger = Logger::Instance();
  logger.Shutdown();

  LoggerConfig config;
  config.enable_file_sink = false;
  config.enable_console_sink = false;

  const Fabrica::Core::Status status = logger.Initialize(config);
  FABRICA_EXPECT_TRUE(!status.ok());
  logger.Shutdown();
}

}  // namespace
