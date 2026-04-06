#include "core/logging/logger.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <memory>
#include <vector>

namespace Fabrica::Core::Logging {

namespace {

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

std::string FormatTimestamp(std::chrono::system_clock::time_point timestamp) {
  using namespace std::chrono;
  const auto millis =
      duration_cast<milliseconds>(timestamp.time_since_epoch()) % 1000;
  const std::time_t time_value = system_clock::to_time_t(timestamp);

  std::tm utc_time{};
#if defined(_WIN32)
  gmtime_s(&utc_time, &time_value);
#else
  gmtime_r(&time_value, &utc_time);
#endif

  std::ostringstream stream;
  stream << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%S") << "."
         << std::setw(3) << std::setfill('0') << millis.count() << "Z";
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
  std::thread::id thread_id;
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

  auto* file = new std::ofstream(config.file_path, std::ios::out | std::ios::trunc);
  if (!file->is_open()) {
    delete file;
    return Status::Internal("Failed to open log file");
  }

  file_ = file;
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
  record.thread_id = std::this_thread::get_id();
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
    queue_->Push(record);
    wake_up_.notify_one();
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
    WriteLine(record.timestamp, record.channel, record.level, record.thread_id,
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
      WriteLine(record.timestamp, record.channel, record.level, record.thread_id,
                record.file.data(), record.line, record.message.data(),
                record.correlation_id);
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
                       std::thread::id thread_id, const char* file, int line,
                       std::string_view message, std::uint64_t correlation_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (file_ == nullptr) {
    return;
  }

  (*file_) << "[" << FormatTimestamp(timestamp) << "]"
           << "[" << ToString(level) << "]"
           << "[" << ToString(channel) << "]"
           << "[Thread:" << std::hash<std::thread::id>{}(thread_id) << "]"
           << "[CID:" << correlation_id << "] "
           << message;

  if (file != nullptr && *file != '\0') {
    (*file_) << " (" << file << ":" << line << ")";
  }
  (*file_) << "\n";
}

}  // namespace Fabrica::Core::Logging
