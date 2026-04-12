#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "rhi/RHITypes.h"

namespace Fabrica::RHI::Vulkan {

template <typename HandleT, typename ResourceT, ERHIResourceType Type, std::size_t Capacity>
class VulkanSlotMap final {
 public:
  struct Slot {
    ResourceT resource{};
    std::uint16_t generation = 1;
    bool occupied = false;
  };

  VulkanSlotMap() {
    for (std::size_t index = 0; index < Capacity; ++index) {
      free_indices_[index] = static_cast<std::uint32_t>(Capacity - 1u - index);
    }
  }

  template <typename Initializer>
  HandleT Allocate(Initializer&& initializer) {
    if (free_count_ == 0) {
      return {};
    }

    const std::uint32_t index = free_indices_[--free_count_];
    Slot& slot = slots_[index];
    slot.resource = ResourceT{};
    initializer(slot.resource);
    slot.occupied = true;

    HandleT handle{EncodeHandleId(slot.generation, Type, index)};
    slot.resource.handle = handle;
    return handle;
  }

  [[nodiscard]] bool IsValid(HandleT handle) const {
    if (!handle.IsValid() || DecodeHandleType(handle.id) != Type) {
      return false;
    }

    const std::uint64_t index = DecodeHandleIndex(handle.id);
    if (index >= Capacity) {
      return false;
    }

    const Slot& slot = slots_[static_cast<std::size_t>(index)];
    return slot.occupied && slot.generation == DecodeHandleGeneration(handle.id);
  }

  [[nodiscard]] ResourceT* Resolve(HandleT handle) {
    if (!IsValid(handle)) {
      return nullptr;
    }
    return &slots_[static_cast<std::size_t>(DecodeHandleIndex(handle.id))].resource;
  }

  [[nodiscard]] const ResourceT* Resolve(HandleT handle) const {
    if (!IsValid(handle)) {
      return nullptr;
    }
    return &slots_[static_cast<std::size_t>(DecodeHandleIndex(handle.id))].resource;
  }

  template <typename DestroyFn>
  void Destroy(HandleT handle, DestroyFn&& destroy_fn) {
    if (!IsValid(handle)) {
      return;
    }

    const std::size_t index = static_cast<std::size_t>(DecodeHandleIndex(handle.id));
    Slot& slot = slots_[index];
    destroy_fn(slot.resource);
    slot.resource = ResourceT{};
    slot.occupied = false;
    slot.generation = slot.generation == std::numeric_limits<std::uint16_t>::max()
                          ? 1u
                          : static_cast<std::uint16_t>(slot.generation + 1u);
    free_indices_[free_count_++] = static_cast<std::uint32_t>(index);
  }

  template <typename DestroyFn>
  void DestroyAll(DestroyFn&& destroy_fn) {
    for (std::size_t index = 0; index < Capacity; ++index) {
      Slot& slot = slots_[index];
      if (!slot.occupied) {
        continue;
      }

      destroy_fn(slot.resource);
      slot.resource = ResourceT{};
      slot.occupied = false;
      slot.generation = slot.generation == std::numeric_limits<std::uint16_t>::max()
                            ? 1u
                            : static_cast<std::uint16_t>(slot.generation + 1u);
    }

    free_count_ = Capacity;
    for (std::size_t index = 0; index < Capacity; ++index) {
      free_indices_[index] = static_cast<std::uint32_t>(Capacity - 1u - index);
    }
  }

 private:
  std::array<Slot, Capacity> slots_{};
  std::array<std::uint32_t, Capacity> free_indices_{};
  std::size_t free_count_ = Capacity;
};

}  // namespace Fabrica::RHI::Vulkan


