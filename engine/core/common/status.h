#pragma once

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace Fabrica::Core {

enum class ErrorCode {
  kOk = 0,
  kCancelled,
  kInvalidArgument,
  kNotFound,
  kAlreadyExists,
  kOutOfRange,
  kFailedPrecondition,
  kInternal,
  kUnavailable,
};

class Status {
 public:
  Status() = default;
  Status(ErrorCode code, std::string message);

  static Status Ok();
  static Status Cancelled(std::string message);
  static Status InvalidArgument(std::string message);
  static Status NotFound(std::string message);
  static Status Internal(std::string message);
  static Status Unavailable(std::string message);

  bool ok() const { return code_ == ErrorCode::kOk; }
  ErrorCode code() const { return code_; }
  const std::string& message() const { return message_; }

  std::string ToString() const;

 private:
  ErrorCode code_ = ErrorCode::kOk;
  std::string message_;
};

template <typename T>
class StatusOr {
 public:
  StatusOr(const T& value) : value_(value), status_(Status::Ok()) {}
  StatusOr(T&& value) : value_(std::move(value)), status_(Status::Ok()) {}
  StatusOr(Status status) : status_(std::move(status)) {}

  bool ok() const { return status_.ok(); }
  const Status& status() const { return status_; }

  const T& value() const& { return *value_; }
  T& value() & { return *value_; }
  T&& value() && { return std::move(*value_); }

  const T& operator*() const& { return *value_; }
  T& operator*() & { return *value_; }

 private:
  std::optional<T> value_;
  Status status_ = Status::Internal("Uninitialized StatusOr");
};

template <>
class StatusOr<void> {
 public:
  StatusOr() : status_(Status::Ok()) {}
  StatusOr(Status status) : status_(std::move(status)) {}

  bool ok() const { return status_.ok(); }
  const Status& status() const { return status_; }

 private:
  Status status_;
};

template <typename T>
struct IsStatusOr : std::false_type {};

template <typename T>
struct IsStatusOr<StatusOr<T>> : std::true_type {};

template <typename T>
inline constexpr bool IsStatusOrV = IsStatusOr<T>::value;

}  // namespace Fabrica::Core
