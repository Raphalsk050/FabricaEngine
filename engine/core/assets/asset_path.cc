#include "core/assets/asset_path.h"

#include <algorithm>

namespace Fabrica::Core::Assets {

Core::StatusOr<AssetPath> AssetPath::Create(std::string path) {
  if (path.empty()) {
    return Core::Status::InvalidArgument("Asset path cannot be empty");
  }

  std::replace(path.begin(), path.end(), '\\', '/');
  return AssetPath(std::move(path));
}

}  // namespace Fabrica::Core::Assets

