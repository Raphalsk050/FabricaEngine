#include "core/common/status.h"
#include "core/logging/logger.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace {

using Fabrica::Core::Status;
using Fabrica::Core::Logging::LogChannel;
using Fabrica::Core::Logging::LogLevel;
using Fabrica::Core::Logging::Logger;
using Fabrica::Core::Logging::LoggerConfig;

/**
 * Configure the asynchronous logger and emit representative records.
 *
 * The sample demonstrates runtime channel filtering plus `FABRICA_LOG` usage,
 * then flushes and shuts down explicitly to keep process teardown deterministic.
 *
 * @return `Status::Ok()` when logger initialization succeeds.
 */
Status RunLoggingDemo() {
  Logger& logger = Logger::Instance();
  logger.Shutdown();

  LoggerConfig config;
  config.file_path =
      (std::filesystem::temp_directory_path() / "fabrica_logging_sample.log")
          .string();
  config.queue_capacity = 128;
  config.min_level = LogLevel::kDebug;
  config.enable_file_sink = true;
  config.enable_console_sink = true;

  const Status init_status = logger.Initialize(config);
  if (!init_status.ok()) {
    return init_status;
  }

  logger.SetChannelEnabled(LogChannel::kAssets, false);

  FABRICA_LOG(Core, Info) << "[logging_sample] Logger configured";
  FABRICA_LOG(Game, Debug) << "[logging_sample] Debug entry from FABRICA_LOG macro";
  logger.Submit(LogChannel::kAssets, LogLevel::kInfo, __FILE__, __LINE__,
                "This entry is filtered by disabled channel");
  logger.Submit(LogChannel::kCore, LogLevel::kWarning, __FILE__, __LINE__,
                "Direct submit path remains available");

  logger.Flush();
  logger.Shutdown();
  return Status::Ok();
}

}  // namespace

int main() {
  const Status status = RunLoggingDemo();
  if (!status.ok()) {
    std::cerr << "[logging_sample] Failed: " << status.ToString() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "[logging_sample] Log records emitted successfully\n";
  return EXIT_SUCCESS;
}
