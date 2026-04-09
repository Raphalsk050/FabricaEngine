#pragma once

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace Fabrica::Core {

/**
 * Enumerates canonical engine-wide error categories.
 */
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

/**
 * Represents success or failure with structured code/message data.
 *
 * Use `Status` for APIs that only need to report operation outcome without
 * returning a value.
 */
class Status {
 public:
  Status() = default;

  /**
   * Build a non-ok status with explicit code and message.
   */
  Status(ErrorCode code, std::string message);

  /// Construct an ok status.
  static Status Ok();

  /// Construct a cancelled status.
  static Status Cancelled(std::string message);

  /// Construct an invalid-argument status.
  static Status InvalidArgument(std::string message);

  /// Construct a not-found status.
  static Status NotFound(std::string message);

  /// Construct an internal-error status.
  static Status Internal(std::string message);

  /// Construct an unavailable status.
  static Status Unavailable(std::string message);

  /// Return true when the status represents success.
  bool ok() const { return code_ == ErrorCode::kOk; }

  /// Return the structured error code.
  ErrorCode code() const { return code_; }

  /// Return the human-readable diagnostic message.
  const std::string& message() const { return message_; }

  /// Return a compact printable representation of the status.
  std::string ToString() const;

 private:
  ErrorCode code_ = ErrorCode::kOk;
  std::string message_;
};

/**
 * Carries either a value or a failure status.
 *
 * @tparam T Value type produced on success.
 */
template <typename T>
class StatusOr {
 public:
  StatusOr(const T& value) : value_(value), status_(Status::Ok()) {}
  StatusOr(T&& value) : value_(std::move(value)), status_(Status::Ok()) {}
  StatusOr(Status status) : status_(std::move(status)) {}

  /// Return true when a value is available.
  bool ok() const { return status_.ok(); }

  /// Return the current status.
  const Status& status() const { return status_; }

  /// Access the stored value.
  const T& value() const& { return *value_; }

  /// Access the stored value with mutable lvalue semantics.
  T& value() & { return *value_; }

  /// Move the stored value out.
  T&& value() && { return std::move(*value_); }

  const T& operator*() const& { return *value_; }
  T& operator*() & { return *value_; }

 private:
  std::optional<T> value_;
  Status status_ = Status::Internal("Uninitialized StatusOr");
};

/**
 * Specialization for status-only flows that have no payload.
 */
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

/**
 * Type trait that identifies `StatusOr<T>` specializations.
 */
template <typename T>
struct IsStatusOr : std::false_type {};

template <typename T>
struct IsStatusOr<StatusOr<T>> : std::true_type {};

template <typename T>
inline constexpr bool IsStatusOrV = IsStatusOr<T>::value;

}  // namespace Fabrica::Core
