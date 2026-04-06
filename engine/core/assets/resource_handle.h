#pragma once

#include <cstdint>

namespace Fabrica::Core::Assets {

template <typename Tag>
struct ResourceHandle {
  std::uint32_t id = 0;
  std::uint32_t generation = 0;

  bool IsValid() const { return id != 0; }

  friend bool operator==(const ResourceHandle& lhs, const ResourceHandle& rhs) {
    return lhs.id == rhs.id && lhs.generation == rhs.generation;
  }
};

}  // namespace Fabrica::Core::Assets

