#include "core/assets/resource_manager.h"

namespace Fabrica::Core::Assets {

Async::Future<TextureHandle> ResourceManager::LoadTexture(const AssetPath& path,
                                                          LoadPriority priority) {
  return LoadResource<TextureHandle>(path, priority, texture_cache_, "Texture");
}

Async::Future<MeshHandle> ResourceManager::LoadMesh(const AssetPath& path,
                                                    LoadPriority priority) {
  return LoadResource<MeshHandle>(path, priority, mesh_cache_, "Mesh");
}

Async::Future<AudioHandle> ResourceManager::LoadAudio(const AssetPath& path,
                                                      LoadPriority priority) {
  return LoadResource<AudioHandle>(path, priority, audio_cache_, "Audio");
}

int ResourceManager::ToTaskPriority(LoadPriority priority) {
  switch (priority) {
    case LoadPriority::kLow:
      return Jobs::kLowTaskPriority;
    case LoadPriority::kNormal:
      return Jobs::kNormalTaskPriority;
    case LoadPriority::kHigh:
      return Jobs::kHighTaskPriority;
    case LoadPriority::kCritical:
      return Jobs::kCriticalTaskPriority;
  }
  return Jobs::kNormalTaskPriority;
}

}  // namespace Fabrica::Core::Assets

