#include "pal/file_system.h"

#include <algorithm>
#include <fstream>

#include "core/config/config.h"
#include "core/logging/logger.h"

namespace Fabrica::PAL {

Core::Async::Future<std::vector<std::byte>>
LocalFileSystem::ReadFileAsync(std::string path, Core::Jobs::Executor::Type executor) {
  const std::string normalized_path = NormalizePath(std::move(path));
  return Core::Async::Future<std::vector<std::byte>>::Schedule(
      [normalized_path]() -> Core::StatusOr<std::vector<std::byte>> {
        std::ifstream file(normalized_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
          return Core::Status::NotFound("File not found: " + normalized_path);
        }

        const auto size = file.tellg();
        if (size < 0) {
          return Core::Status::Internal("Failed to read file size: " +
                                        normalized_path);
        }

        std::vector<std::byte> bytes(static_cast<size_t>(size));
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(bytes.data()), size);
        if (!file.good() && !file.eof()) {
          return Core::Status::Internal("Failed to read file bytes: " +
                                        normalized_path);
        }

#if FABRICA_PAL_VERBOSE_LOG
        FABRICA_LOG(PAL, Debug) << "[PAL][FS] Read complete: '" << normalized_path
                                << "' | size=" << bytes.size();
#endif
        return bytes;
      },
      executor);
}

Core::Async::Future<Core::Status> LocalFileSystem::WriteFileAsync(
    std::string path, std::vector<std::byte> bytes,
    Core::Jobs::Executor::Type executor) {
  const std::string normalized_path = NormalizePath(std::move(path));
  return Core::Async::Future<Core::Status>::Schedule(
      [normalized_path, bytes = std::move(bytes)]() mutable -> Core::Status {
        std::ofstream file(normalized_path, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
          return Core::Status::Internal("Failed to open file for writing: " +
                                        normalized_path);
        }

        file.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!file.good()) {
          return Core::Status::Internal("Failed to write file: " + normalized_path);
        }

#if FABRICA_PAL_VERBOSE_LOG
        FABRICA_LOG(PAL, Debug) << "[PAL][FS] Write complete: '" << normalized_path
                                << "' | size=" << bytes.size();
#endif
        return Core::Status::Ok();
      },
      executor);
}

std::string LocalFileSystem::NormalizePath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  return path;
}

}  // namespace Fabrica::PAL
