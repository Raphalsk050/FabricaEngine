#pragma once

#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/async/future.h"
#include "core/assets/asset_path.h"
#include "core/assets/resource_handle.h"
#include "core/config/config.h"
#include "core/logging/logger.h"

namespace Fabrica::Core::Assets {

/**
 * Defines scheduling urgency for asynchronous resource loading tasks.
 */
enum class LoadPriority {
  kLow,
  ///< Background task with minimal urgency.
  kNormal,
  ///< Default task urgency.
  kHigh,
  ///< Elevated urgency for near-term use.
  kCritical,
  ///< Highest urgency for frame-critical assets.
};

struct TextureTag {};
struct MeshTag {};
struct AudioTag {};

using TextureHandle = ResourceHandle<TextureTag>;
using MeshHandle = ResourceHandle<MeshTag>;
using AudioHandle = ResourceHandle<AudioTag>;

/**
 * Loads and deduplicates runtime resources through asynchronous pipelines.
 *
 * The manager implements a cache-aside strategy backed by weak futures.
 * Concurrent requests for the same path share in-flight work. Handle
 * allocation is serialized to preserve unique ids.
 *
 * Thread safety: Public loading methods are thread-safe.
 *
 * @note Cache entries are removed when the in-flight future expires.
 * @see AssetPath, ResourceHandle, Async::Future
 */
class ResourceManager {
 public:
  /**
   * Load a texture resource and return its asynchronous handle.
   *
   * @param path Valid asset path.
   * @param priority Scheduling priority mapped to job priority.
   * @return Future resolving to texture handle or load error.
   */
  Async::Future<TextureHandle> LoadTexture(const AssetPath& path,
                                           LoadPriority priority);

  /**
   * Load a mesh resource and return its asynchronous handle.
   */
  Async::Future<MeshHandle> LoadMesh(const AssetPath& path, LoadPriority priority);

  /**
   * Load an audio resource and return its asynchronous handle.
   */
  Async::Future<AudioHandle> LoadAudio(const AssetPath& path, LoadPriority priority);

 private:
  /**
   * Execute the generic read/decode/upload chain for one resource category.
   *
   * @tparam HandleT Typed resource handle.
   * @param path Valid source path.
   * @param priority Requested load priority.
   * @param cache Weak-future cache for this resource category.
   * @param resource_kind Logging label for diagnostics.
   */
  template <typename HandleT>
  Async::Future<HandleT> LoadResource(const AssetPath& path, LoadPriority priority,
                                      std::unordered_map<std::string,
                                                         Async::WeakFuture<HandleT>>& cache,
                                      const char* resource_kind);

  /**
   * Allocate a new typed handle while holding `mutex_`.
   *
   * @tparam HandleT Typed handle category.
   * @return Newly allocated handle with generation set to `1`.
   * @pre Caller holds `mutex_`.
   */
  template <typename HandleT>
  HandleT AllocateHandleLocked();

  /// Convert `LoadPriority` to scheduler integer priority.
  static int ToTaskPriority(LoadPriority priority);

  std::mutex mutex_;
  std::uint32_t next_handle_id_ = 1;

  std::unordered_map<std::string, Async::WeakFuture<TextureHandle>> texture_cache_;
  std::unordered_map<std::string, Async::WeakFuture<MeshHandle>> mesh_cache_;
  std::unordered_map<std::string, Async::WeakFuture<AudioHandle>> audio_cache_;
};

template <typename HandleT>
Async::Future<HandleT> ResourceManager::LoadResource(
    const AssetPath& path, LoadPriority priority,
    std::unordered_map<std::string, Async::WeakFuture<HandleT>>& cache,
    const char* resource_kind) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto cache_it = cache.find(path.Str());
    if (cache_it != cache.end()) {
      if (auto cached_future = cache_it->second.Lock()) {
#if FABRICA_ASSETS_VERBOSE_LOG
        FABRICA_LOG(Assets, Debug) << "[Assets] Load requested: '" << path.Str()
                                   << "' | priority=" << static_cast<int>(priority)
                                   << " | dedup=true";
#endif
        return *cached_future;
      }
      cache.erase(cache_it);
    }
  }

#if FABRICA_ASSETS_VERBOSE_LOG
  FABRICA_LOG(Assets, Debug) << "[Assets] Load requested: '" << path.Str()
                             << "' | priority=" << static_cast<int>(priority)
                             << " | dedup=false";
#endif

  const int task_priority = ToTaskPriority(priority);
  auto read_stage = Async::Future<std::vector<std::byte>>::Schedule(
      [path]() -> Core::StatusOr<std::vector<std::byte>> {
        std::ifstream file(path.Str(), std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
          return Core::Status::NotFound("Asset not found: " + path.Str());
        }

        const auto size = file.tellg();
        if (size < 0) {
          return Core::Status::Internal("Failed to read asset size");
        }

        std::vector<std::byte> bytes(static_cast<size_t>(size));
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(bytes.data()), size);
        if (!file.good() && !file.eof()) {
          return Core::Status::Internal("Failed to read asset bytes");
        }

#if FABRICA_ASSETS_VERBOSE_LOG
        FABRICA_LOG(Assets, Debug) << "[Assets] File read complete: '" << path.Str()
                                   << "' | size=" << bytes.size() << "B";
#endif
        return bytes;
      },
      Jobs::Executor::Type::kBackground);
  read_stage.UpdatePriority(task_priority);

  struct DecodedData {
    size_t bytes = 0;
  };

  auto decode_stage = read_stage.Then(
      [path](const Core::StatusOr<std::vector<std::byte>>& file_data)
          -> Core::StatusOr<DecodedData> {
        if (!file_data.ok()) {
          return file_data.status();
        }
        DecodedData decoded{.bytes = file_data.value().size()};
#if FABRICA_ASSETS_VERBOSE_LOG
        FABRICA_LOG(Assets, Debug) << "[Assets] Decode complete: '" << path.Str()
                                   << "' | bytes=" << decoded.bytes;
#endif
        return decoded;
      },
      Jobs::Executor::Type::kBackground);
  decode_stage.UpdatePriority(task_priority);

  auto upload_stage = decode_stage.Then(
      [this, path, resource_kind](const Core::StatusOr<DecodedData>& decoded)
          -> Core::StatusOr<HandleT> {
        if (!decoded.ok()) {
          return decoded.status();
        }
        std::lock_guard<std::mutex> lock(mutex_);
        HandleT handle = AllocateHandleLocked<HandleT>();
#if FABRICA_ASSETS_VERBOSE_LOG
        FABRICA_LOG(Assets, Debug)
            << "[Assets] Resource ready: '" << path.Str() << "' | kind="
            << resource_kind << " | handle=" << handle.id;
#endif
        return handle;
      },
      Jobs::Executor::Type::kForeground);

  {
    std::lock_guard<std::mutex> lock(mutex_);
    cache[path.Str()] = Async::WeakFuture<HandleT>(upload_stage);
  }
  return upload_stage;
}

template <typename HandleT>
HandleT ResourceManager::AllocateHandleLocked() {
  HandleT handle;
  handle.id = next_handle_id_++;
  handle.generation = 1;
  return handle;
}

}  // namespace Fabrica::Core::Assets
