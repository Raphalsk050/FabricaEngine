#pragma once

#include <string>

#include "core/common/status.h"

namespace Fabrica::Core::Assets {

class AssetPath {
 public:
  static Core::StatusOr<AssetPath> Create(std::string path);

  const std::string& Str() const { return path_; }

  friend bool operator==(const AssetPath& lhs, const AssetPath& rhs) {
    return lhs.path_ == rhs.path_;
  }

 private:
  explicit AssetPath(std::string path) : path_(std::move(path)) {}

  std::string path_;
};

}  // namespace Fabrica::Core::Assets

