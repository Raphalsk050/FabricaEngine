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

struct LoggerConfig {
  std::string file_path = "fabrica.log";
  size_t queue_capacity = 2048;
  LogLevel min_level = LogLevel::kDebug;
};

class Logger {
 public:
  static Logger& Instance();

  Status Initialize(const LoggerConfig& config = {});
  void Shutdown();

  void SetMinLevel(LogLevel min_level);
  void SetChannelEnabled(LogChannel channel, bool enabled);
  bool IsEnabled(LogChannel channel, LogLevel level) const;

  void Submit(LogChannel channel, LogLevel level, const char* file, int line,
              std::string_view message, std::uint64_t correlation_id = 0);
  void Flush();

 private:
  Logger();
  ~Logger();

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  void WorkerLoop();
  void WriteLine(std::chrono::system_clock::time_point timestamp,
                 LogChannel channel, LogLevel level, std::thread::id thread_id,
                 const char* file, int line, std::string_view message,
                 std::uint64_t correlation_id);

  struct LogRecord;
  class Queue;

  std::unique_ptr<Queue> queue_;
  std::atomic<bool> initialized_ = false;
  std::atomic<bool> running_ = false;
  std::atomic<int> min_level_{static_cast<int>(LogLevel::kDebug)};
  std::atomic<std::uint64_t> dropped_records_ = 0;
  std::array<std::atomic<bool>, kLogChannelCount> enabled_channels_{};

  std::thread worker_;
  std::ofstream* file_ = nullptr;
  mutable std::mutex mutex_;
  std::condition_variable wake_up_;
};

class LogStream {
 public:
  LogStream(LogChannel channel, LogLevel level, const char* file, int line)
      : channel_(channel), level_(level), file_(file), line_(line) {}

  ~LogStream() {
    Logger::Instance().Submit(channel_, level_, file_, line_, stream_.str());
  }

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

constexpr bool IsCompileTimeEnabled(LogLevel level) {
  return static_cast<int>(level) >= FABRICA_LOG_MIN_LEVEL;
}

#define FABRICA_LOG(Channel, Level)                                             \
  if constexpr (!::Fabrica::Core::Logging::IsCompileTimeEnabled(               \
                    ::Fabrica::Core::Logging::LogLevel::k##Level))             \
    ;                                                                           \
  else                                                                          \
    ::Fabrica::Core::Logging::LogStream(                                        \
        ::Fabrica::Core::Logging::LogChannel::k##Channel,                       \
        ::Fabrica::Core::Logging::LogLevel::k##Level, __FILE__, __LINE__)

}  // namespace Fabrica::Core::Logging
