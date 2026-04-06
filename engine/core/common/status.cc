#include "core/common/status.h"

#include <sstream>

namespace Fabrica::Core {

namespace {

const char* ToErrorCodeString(ErrorCode code) {
  switch (code) {
    case ErrorCode::kOk:
      return "OK";
    case ErrorCode::kCancelled:
      return "CANCELLED";
    case ErrorCode::kInvalidArgument:
      return "INVALID_ARGUMENT";
    case ErrorCode::kNotFound:
      return "NOT_FOUND";
    case ErrorCode::kAlreadyExists:
      return "ALREADY_EXISTS";
    case ErrorCode::kOutOfRange:
      return "OUT_OF_RANGE";
    case ErrorCode::kFailedPrecondition:
      return "FAILED_PRECONDITION";
    case ErrorCode::kInternal:
      return "INTERNAL";
    case ErrorCode::kUnavailable:
      return "UNAVAILABLE";
  }
  return "UNKNOWN";
}

}  // namespace

Status::Status(ErrorCode code, std::string message)
    : code_(code), message_(std::move(message)) {}

Status Status::Ok() { return Status(); }

Status Status::Cancelled(std::string message) {
  return Status(ErrorCode::kCancelled, std::move(message));
}

Status Status::InvalidArgument(std::string message) {
  return Status(ErrorCode::kInvalidArgument, std::move(message));
}

Status Status::NotFound(std::string message) {
  return Status(ErrorCode::kNotFound, std::move(message));
}

Status Status::Internal(std::string message) {
  return Status(ErrorCode::kInternal, std::move(message));
}

Status Status::Unavailable(std::string message) {
  return Status(ErrorCode::kUnavailable, std::move(message));
}

std::string Status::ToString() const {
  std::ostringstream stream;
  stream << ToErrorCodeString(code_);
  if (!message_.empty()) {
    stream << ": " << message_;
  }
  return stream.str();
}

}  // namespace Fabrica::Core
