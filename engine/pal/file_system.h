#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/async/future.h"

namespace Fabrica::PAL {

class IFileSystem {
 public:
  virtual ~IFileSystem() = default;

  virtual Core::Async::Future<std::vector<std::byte>> ReadFileAsync(
      std::string path,
      Core::Jobs::Executor::Type executor =
          Core::Jobs::Executor::Type::kBackground) = 0;

  virtual Core::Async::Future<Core::Status> WriteFileAsync(
      std::string path, std::vector<std::byte> bytes,
      Core::Jobs::Executor::Type executor =
          Core::Jobs::Executor::Type::kBackground) = 0;
};

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

  static std::string NormalizePath(std::string path);
};

}  // namespace Fabrica::PAL
