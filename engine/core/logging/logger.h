#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#include "core/common/status.h"
#include "core/config/config.h"
#include "core/logging/log_channel.h"
#include "core/logging/log_level.h"

namespace Fabrica::Core::Logging {

/**
 * Configures logger sinks, queue capacity, and filtering behavior.
 */
struct LoggerConfig {
  std::string file_path = "fabrica.log";
  ///< Destination path when file sink is enabled.

  size_t queue_capacity = 2048;
  ///< Maximum number of buffered log records.

  LogLevel min_level = LogLevel::kDebug;
  ///< Minimum severity accepted at runtime.

  bool enable_file_sink = true;
  ///< Writes records to file when true.

  bool enable_console_sink = true;
  ///< Writes records to stdout/stderr when true.

  bool enable_console_colors = true;
  ///< Enables ANSI coloring for console sink.
};

/**
 * Provides asynchronous structured logging with channel-level filtering.
 *
 * The logger uses a producer/consumer queue and a dedicated worker thread to
 * keep call-site overhead low. It is exposed as a singleton to guarantee a
 * single output stream ordering across systems.
 *
 * Thread safety: Public methods are thread-safe.
 *
 * @note Call `Initialize()` before first production use.
 * @see LogChannel, LogLevel, LogStream
 */
class Logger {
 public:
  /// Return the process-wide logger instance.
  static Logger& Instance();

  /**
   * Initialize sinks, queue, and worker thread.
   *
   * @param config Logger runtime configuration.
   * @return `Status::Ok()` on success, error status otherwise.
   */
  Status Initialize(const LoggerConfig& config = {});

  /// Flush pending records and stop worker thread.
  void Shutdown();

  /// Set runtime minimum severity threshold.
  void SetMinLevel(LogLevel min_level);

  /// Enable or disable one channel at runtime.
  void SetChannelEnabled(LogChannel channel, bool enabled);

  /**
   * Check if channel and severity pass current filters.
   */
  bool IsEnabled(LogChannel channel, LogLevel level) const;

  /**
   * Submit one log record to the async queue.
   *
   * @param channel Logical source channel.
   * @param level Severity level.
   * @param file Source file associated with the record.
   * @param line Source line associated with the record.
   * @param message Rendered log message.
   * @param correlation_id Optional request or trace correlation identifier.
   */
  void Submit(LogChannel channel, LogLevel level, const char* file, int line,
              std::string_view message, std::uint64_t correlation_id = 0);

  /// Block until all queued records are written.
  void Flush();

 private:
  Logger();
  ~Logger();

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  /// Worker thread loop that drains queued records to sinks.
  void WorkerLoop();

  /**
   * Format and write one record to active sinks.
   */
  void WriteLine(std::chrono::system_clock::time_point timestamp,
                 LogChannel channel, LogLevel level, std::uint32_t thread_index,
                 const char* file, int line, std::string_view message,
                 std::uint64_t correlation_id);

  struct LogRecord;
  class Queue;

  std::unique_ptr<Queue> queue_;
  std::atomic<bool> initialized_ = false;
  std::atomic<bool> running_ = false;
  std::atomic<int> min_level_{static_cast<int>(LogLevel::kDebug)};
  std::atomic<bool> console_sink_enabled_{true};
  std::atomic<bool> console_colors_enabled_{true};
  std::atomic<std::uint64_t> dropped_records_ = 0;
  std::array<std::atomic<bool>, kLogChannelCount> enabled_channels_{};
  std::uint32_t main_thread_index_ = 0;

  std::thread worker_;
  std::ofstream* file_ = nullptr;
  mutable std::mutex mutex_;
  std::condition_variable wake_up_;
};

/**
 * RAII stream that accumulates message text and submits on destruction.
 *
 * This type underpins the `FABRICA_LOG` macro to keep call sites concise.
 */
class LogStream {
 public:
  LogStream(LogChannel channel, LogLevel level, const char* file, int line)
      : channel_(channel), level_(level), file_(file), line_(line) {}

  ~LogStream() {
    Logger::Instance().Submit(channel_, level_, file_, line_, stream_.str());
  }

  /**
   * Append one value to the buffered message stream.
   */
  template <typename T>
  LogStream& operator<<(const T& value) {
    stream_ << value;
    return *this;
  }

 private:
  LogChannel channel_;
  LogLevel level_;
  const char* file_;
  int line_;
  std::ostringstream stream_;
};

/**
 * Evaluate compile-time log filtering based on `FABRICA_LOG_MIN_LEVEL`.
 */
constexpr bool IsCompileTimeEnabled(LogLevel level) {
  return static_cast<int>(level) >= FABRICA_LOG_MIN_LEVEL;
}

/**
 * Emit a log stream when compile-time filter allows the selected level.
 */
#define FABRICA_LOG(Channel, Level)                                             \
  if constexpr (!::Fabrica::Core::Logging::IsCompileTimeEnabled(               \
                    ::Fabrica::Core::Logging::LogLevel::k##Level))             \
    ;                                                                           \
  else                                                                          \
    ::Fabrica::Core::Logging::LogStream(                                        \
        ::Fabrica::Core::Logging::LogChannel::k##Channel,                       \
        ::Fabrica::Core::Logging::LogLevel::k##Level, __FILE__, __LINE__)

}  // namespace Fabrica::Core::Logging
