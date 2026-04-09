#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/async/future.h"

namespace Fabrica::PAL {

/**
 * Defines asynchronous file I/O operations for platform adapters.
 */
class IFileSystem {
 public:
  virtual ~IFileSystem() = default;

  /**
   * Read full file contents asynchronously.
   *
   * @param path File system path to read.
   * @param executor Executor used to run I/O task.
   * @return Future with file bytes or error status.
   */
  virtual Core::Async::Future<std::vector<std::byte>> ReadFileAsync(
      std::string path,
      Core::Jobs::Executor::Type executor =
          Core::Jobs::Executor::Type::kBackground) = 0;

  /**
   * Write full byte payload asynchronously.
   *
   * @param path Destination file path.
   * @param bytes Payload to persist.
   * @param executor Executor used to run I/O task.
   * @return Future resolving to write status.
   */
  virtual Core::Async::Future<Core::Status> WriteFileAsync(
      std::string path, std::vector<std::byte> bytes,
      Core::Jobs::Executor::Type executor =
          Core::Jobs::Executor::Type::kBackground) = 0;
};

/**
 * Implements file I/O against the local operating-system file system.
 */
class LocalFileSystem final : public IFileSystem {
 public:
  Core::Async::Future<std::vector<std::byte>> ReadFileAsync(
      std::string path,
      Core::Jobs::Executor::Type executor =
          Core::Jobs::Executor::Type::kBackground) override;

  Core::Async::Future<Core::Status> WriteFileAsync(
      std::string path, std::vector<std::byte> bytes,
      Core::Jobs::Executor::Type executor =
          Core::Jobs::Executor::Type::kBackground) override;

  /**
   * Normalize path separators and redundant segments for platform calls.
   */
  static std::string NormalizePath(std::string path);
};

}  // namespace Fabrica::PAL
