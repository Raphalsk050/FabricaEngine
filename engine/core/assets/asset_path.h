#pragma once

#include <string>

#include "core/common/status.h"

namespace Fabrica::Core::Assets {

/**
 * Represents a validated asset path used by the resource pipeline.
 *
 * This value object centralizes path validation so loading APIs can rely on a
 * normalized and non-empty path contract.
 */
class AssetPath {
 public:
  /**
   * Validate and construct an `AssetPath`.
   *
   * @param path Raw path string from caller input.
   * @return Valid `AssetPath` on success, or validation error status.
   */
  static Core::StatusOr<AssetPath> Create(std::string path);

  /// Return the underlying normalized path string.
  const std::string& Str() const { return path_; }

  friend bool operator==(const AssetPath& lhs, const AssetPath& rhs) {
    return lhs.path_ == rhs.path_;
  }

 private:
  explicit AssetPath(std::string path) : path_(std::move(path)) {}

  std::string path_;
};

}  // namespace Fabrica::Core::Assets
