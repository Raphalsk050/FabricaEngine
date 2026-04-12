#pragma once

#include <vector>

#include <Volk/volk.h>

#include "rhi/RHITypes.h"

namespace Fabrica::RHI::Vulkan {

struct VulkanContextState;

class VulkanDescriptorAllocator final {
 public:
  explicit VulkanDescriptorAllocator(VulkanContextState& state);
  ~VulkanDescriptorAllocator();

  bool Initialize();
  void Shutdown();
  void ResetFramePools(std::uint32_t frame_index);
  [[nodiscard]] VkDescriptorSet AllocateSet(VkDescriptorSetLayout layout,
                                            ERHIDescriptorFrequency frequency);

 private:
  struct PoolBucket {
    std::vector<VkDescriptorPool> pools;
    std::size_t active_pool_index = 0;
  };

  [[nodiscard]] VkDescriptorPool AcquirePool(PoolBucket& bucket, bool free_individual_sets);

  VulkanContextState* state_ = nullptr;
  PoolBucket per_frame_{};
  PoolBucket per_draw_{};
  PoolBucket persistent_{};
};

}  // namespace Fabrica::RHI::Vulkan
