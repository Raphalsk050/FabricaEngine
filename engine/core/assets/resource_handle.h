#pragma once

#include <cstdint>

namespace Fabrica::Core::Assets {

/**
 * Stores a generational identifier for a typed resource instance.
 *
 * The handle is a POD carrier suitable for ECS components, serialization, and
 * hash-based containers.
 *
 * @tparam Tag Compile-time type tag to prevent cross-resource misuse.
 */
template <typename Tag>
struct ResourceHandle {
  std::uint32_t id = 0;
  ///< Monotonic resource identifier. Zero means invalid.

  std::uint32_t generation = 0;
  ///< Generation counter used to detect stale handles.

  /// Return true when the handle references a live resource id.
  bool IsValid() const { return id != 0; }

  friend bool operator==(const ResourceHandle& lhs, const ResourceHandle& rhs) {
    return lhs.id == rhs.id && lhs.generation == rhs.generation;
  }
};

}  // namespace Fabrica::Core::Assets
