#include "core/logging/logger.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>

namespace Fabrica::Core::Logging {

namespace {

constexpr const char* kAnsiReset = "\x1b[0m";

const char* ToString(LogLevel level) {
  switch (level) {
    case LogLevel::kDebug:
      return "DEBUG";
    case LogLevel::kInfo:
      return "INFO";
    case LogLevel::kWarning:
      return "WARNING";
    case LogLevel::kError:
      return "ERROR";
    case LogLevel::kFatal:
      return "FATAL";
  }
  return "UNKNOWN";
}

const char* ToString(LogChannel channel) {
  switch (channel) {
    case LogChannel::kCore:
      return "Core";
    case LogChannel::kAsync:
      return "Async";
    case LogChannel::kJobs:
      return "Jobs";
    case LogChannel::kAssets:
      return "Assets";
    case LogChannel::kRender:
      return "Render";
    case LogChannel::kECS:
      return "ECS";
    case LogChannel::kPAL:
      return "PAL";
    case LogChannel::kWindow:
      return "Window";
    case LogChannel::kMemory:
      return "Memory";
    case LogChannel::kGame:
      return "Game";
    case LogChannel::kCount:
      return "Invalid";
  }
  return "Invalid";
}

const char* LevelAnsiColor(LogLevel level) {
  switch (level) {
    case LogLevel::kDebug:
      return "\x1b[90m";
    case LogLevel::kInfo:
      return "\x1b[32m";
    case LogLevel::kWarning:
      return "\x1b[33m";
    case LogLevel::kError:
      return "\x1b[31m";
    case LogLevel::kFatal:
      return "\x1b[97;41m";
  }
  return "";
}

std::uint32_t AcquireThreadLogIndex() {
  static std::atomic<std::uint32_t> next_thread_index = 1;
  thread_local std::uint32_t cached_thread_index = 0;

  if (cached_thread_index == 0) {
    cached_thread_index =
        next_thread_index.fetch_add(1, std::memory_order_relaxed);
  }

  return cached_thread_index;
}

const char* ShortFileName(const char* path) {
  if (path == nullptr) {
    return "";
  }

  const char* short_name = path;
  for (const char* current = path; *current != '\0'; ++current) {
    if (*current == '/' || *current == '\\') {
      short_name = current + 1;
    }
  }

  return short_name;
}

std::string FormatTimestamp(std::chrono::system_clock::time_point timestamp) {
  using namespace std::chrono;
  const auto millis =
      duration_cast<milliseconds>(timestamp.time_since_epoch()) % 1000;
  const std::time_t time_value = system_clock::to_time_t(timestamp);

  std::tm local_time{};
#if defined(_WIN32)
  localtime_s(&local_time, &time_value);
#else
  localtime_r(&time_value, &local_time);
#endif

  std::ostringstream stream;
  stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") << "."
         << std::setw(3) << std::setfill('0') << millis.count();
  return stream.str();
}

constexpr size_t NextPowerOfTwo(size_t value) {
  size_t power = 1;
  while (power < value) {
    power <<= 1;
  }
  return power;
}

}  // namespace

struct Logger::LogRecord {
  std::chrono::system_clock::time_point timestamp;
  LogChannel channel = LogChannel::kCore;
  LogLevel level = LogLevel::kInfo;
  std::uint32_t thread_index = 0;
  int line = 0;
  std::uint64_t correlation_id = 0;
  std::array<char, 448> file{};
  std::array<char, 1024> message{};
};

class Logger::Queue {
 public:
  explicit Queue(size_t requested_capacity)
      : capacity_(NextPowerOfTwo(requested_capacity)),
        mask_(capacity_ - 1),
        cells_(capacity_) {
    for (size_t i = 0; i < capacity_; ++i) {
      cells_[i].sequence.store(i, std::memory_order_relaxed);
    }
  }

  bool Push(const LogRecord& record) {
    Cell* cell = nullptr;
    size_t position = enqueue_pos_.load(std::memory_order_relaxed);

    while (true) {
      cell = &cells_[position & mask_];
      const size_t sequence = cell->sequence.load(std::memory_order_acquire);
      const intptr_t difference =
          static_cast<intptr_t>(sequence) - static_cast<intptr_t>(position);

      if (difference == 0) {
        if (enqueue_pos_.compare_exchange_weak(position, position + 1,
                                               std::memory_order_relaxed)) {
          break;
        }
      } else if (difference < 0) {
        return false;
      } else {
        position = enqueue_pos_.load(std::memory_order_relaxed);
      }
    }

    cell->record = record;
    cell->sequence.store(position + 1, std::memory_order_release);
    return true;
  }

  bool Pop(LogRecord* out_record) {
    Cell* cell = nullptr;
    size_t position = dequeue_pos_.load(std::memory_order_relaxed);

    while (true) {
      cell = &cells_[position & mask_];
      const size_t sequence = cell->sequence.load(std::memory_order_acquire);
      const intptr_t difference =
          static_cast<intptr_t>(sequence) - static_cast<intptr_t>(position + 1);

      if (difference == 0) {
        if (dequeue_pos_.compare_exchange_weak(position, position + 1,
                                               std::memory_order_relaxed)) {
          break;
        }
      } else if (difference < 0) {
        return false;
      } else {
        position = dequeue_pos_.load(std::memory_order_relaxed);
      }
    }

    *out_record = cell->record;
    cell->sequence.store(position + capacity_, std::memory_order_release);
    return true;
  }

 private:
  struct Cell {
    std::atomic<size_t> sequence = 0;
    LogRecord record;
  };

  const size_t capacity_;
  const size_t mask_;
  std::vector<Cell> cells_;
  std::atomic<size_t> enqueue_pos_ = 0;
  std::atomic<size_t> dequeue_pos_ = 0;
};

Logger& Logger::Instance() {
  static Logger logger;
  return logger;
}

Logger::Logger() {
  for (auto& channel : enabled_channels_) {
    channel.store(true, std::memory_order_relaxed);
  }
}

Logger::~Logger() { Shutdown(); }

Status Logger::Initialize(const LoggerConfig& config) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (initialized_.load(std::memory_order_acquire)) {
    return Status::Ok();
  }

  console_sink_enabled_.store(config.enable_console_sink,
                              std::memory_order_release);
  console_colors_enabled_.store(config.enable_console_colors,
                                std::memory_order_release);
  main_thread_index_ = AcquireThreadLogIndex();

  if (config.enable_file_sink) {
    auto* file =
        new std::ofstream(config.file_path, std::ios::out | std::ios::trunc);
    if (!file->is_open()) {
      delete file;
      file_ = nullptr;
      if (!config.enable_console_sink) {
        return Status::Internal("Failed to open log file");
      }
    } else {
      file_ = file;
    }
  } else {
    file_ = nullptr;
  }

  if (file_ == nullptr && !config.enable_console_sink) {
    return Status::Internal("Logger has no active sinks");
  }

  queue_ = std::make_unique<Queue>(config.queue_capacity);
  min_level_.store(static_cast<int>(config.min_level), std::memory_order_release);
  running_.store(true, std::memory_order_release);
  worker_ = std::thread([this]() { WorkerLoop(); });
  initialized_.store(true, std::memory_order_release);
  return Status::Ok();
}

void Logger::Shutdown() {
  if (!initialized_.load(std::memory_order_acquire)) {
    return;
  }

  running_.store(false, std::memory_order_release);
  wake_up_.notify_all();
  if (worker_.joinable()) {
    worker_.join();
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (file_ != nullptr) {
    file_->flush();
    file_->close();
    delete file_;
    file_ = nullptr;
  }
  queue_.reset();
  main_thread_index_ = 0;
  initialized_.store(false, std::memory_order_release);
}

void Logger::SetMinLevel(LogLevel min_level) {
  min_level_.store(static_cast<int>(min_level), std::memory_order_release);
}

void Logger::SetChannelEnabled(LogChannel channel, bool enabled) {
  const size_t index = static_cast<size_t>(channel);
  if (index >= kLogChannelCount) {
    return;
  }
  enabled_channels_[index].store(enabled, std::memory_order_release);
}

bool Logger::IsEnabled(LogChannel channel, LogLevel level) const {
  const size_t index = static_cast<size_t>(channel);
  if (index >= kLogChannelCount) {
    return false;
  }
  const bool channel_enabled =
      enabled_channels_[index].load(std::memory_order_acquire);
  const bool level_enabled =
      static_cast<int>(level) >= min_level_.load(std::memory_order_acquire);
  return channel_enabled && level_enabled;
}

void Logger::Submit(LogChannel channel, LogLevel level, const char* file, int line,
                    std::string_view message, std::uint64_t correlation_id) {
  if (!initialized_.load(std::memory_order_acquire)) {
    LoggerConfig config;
    if (!Initialize(config).ok()) {
      return;
    }
  }

  if (!IsEnabled(channel, level)) {
    return;
  }

  LogRecord record;
  record.timestamp = std::chrono::system_clock::now();
  record.channel = channel;
  record.level = level;
  record.thread_index = AcquireThreadLogIndex();
  record.line = line;
  record.correlation_id = correlation_id;

  if (file != nullptr) {
    const size_t file_length = std::min(std::strlen(file), record.file.size() - 1);
    std::memcpy(record.file.data(), file, file_length);
    record.file[record.file.size() - 1] = '\0';
  }

  const size_t message_length =
      std::min(message.size(), record.message.size() - 1);
  std::memcpy(record.message.data(), message.data(), message_length);
  record.message[record.message.size() - 1] = '\0';

  if (queue_ != nullptr) {
    const bool pushed = queue_->Push(record);
    if (pushed) {
      wake_up_.notify_one();
    } else {
      dropped_records_.fetch_add(1, std::memory_order_relaxed);
      if (level >= LogLevel::kError) {
        WriteLine(record.timestamp, record.channel, record.level,
                  record.thread_index, record.file.data(), record.line,
                  record.message.data(), record.correlation_id);
      }
    }
  }

  if (level == LogLevel::kFatal) {
    Flush();
    std::terminate();
  }
}

void Logger::Flush() {
  if (!initialized_.load(std::memory_order_acquire)) {
    return;
  }

  if (queue_ == nullptr) {
    return;
  }

  LogRecord record;
  while (queue_->Pop(&record)) {
    WriteLine(record.timestamp, record.channel, record.level, record.thread_index,
              record.file.data(), record.line, record.message.data(),
              record.correlation_id);
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (file_ != nullptr) {
    file_->flush();
  }
}

void Logger::WorkerLoop() {
  while (running_.load(std::memory_order_acquire) || queue_ != nullptr) {
    bool consumed_any = false;
    LogRecord record;
    while (queue_ != nullptr && queue_->Pop(&record)) {
      consumed_any = true;
      WriteLine(record.timestamp, record.channel, record.level,
                record.thread_index, record.file.data(), record.line,
                record.message.data(), record.correlation_id);
    }

    if (!running_.load(std::memory_order_acquire)) {
      break;
    }

    if (!consumed_any) {
      std::unique_lock<std::mutex> lock(mutex_);
      wake_up_.wait_for(lock, std::chrono::milliseconds(2));
    }
  }

  Flush();
}

void Logger::WriteLine(std::chrono::system_clock::time_point timestamp,
                       LogChannel channel, LogLevel level,
                       std::uint32_t thread_index, const char* file, int line,
                       std::string_view message, std::uint64_t correlation_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (file_ == nullptr &&
      !console_sink_enabled_.load(std::memory_order_acquire)) {
    return;
  }

  const std::string timestamp_text = FormatTimestamp(timestamp);
  const char* level_text = ToString(level);
  const char* short_file_name = ShortFileName(file);

  std::ostringstream suffix_stream;
  suffix_stream << " [" << ToString(channel) << "]"
                << " [" << (thread_index == main_thread_index_ ? "main#" : "thr#")
                << std::setw(2) << std::setfill('0') << thread_index << "]";

  if (correlation_id != 0) {
    suffix_stream << " [cid:" << correlation_id << "]";
  }

  suffix_stream << " " << message;

  if (short_file_name != nullptr && *short_file_name != '\0') {
    suffix_stream << " (" << short_file_name << ":" << line << ")";
  }

  const std::string suffix_text = suffix_stream.str();

  std::ostringstream plain_stream;
  plain_stream << timestamp_text << " [" << level_text << "]" << suffix_text;
  const std::string plain_line = plain_stream.str();

  if (file_ != nullptr) {
    (*file_) << plain_line << "\n";
  }

  if (console_sink_enabled_.load(std::memory_order_acquire)) {
    std::ostream& console =
        (level >= LogLevel::kError) ? static_cast<std::ostream&>(std::cerr)
                                    : static_cast<std::ostream&>(std::cout);

    if (console_colors_enabled_.load(std::memory_order_acquire)) {
      console << timestamp_text << " " << LevelAnsiColor(level) << "["
              << level_text << "]" << kAnsiReset << suffix_text << "\n";
    } else {
      console << plain_line << "\n";
    }
  }
}

}  // namespace Fabrica::Core::Logging
