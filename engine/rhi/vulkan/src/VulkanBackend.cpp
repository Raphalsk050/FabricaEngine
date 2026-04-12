#include "rhi/RHIFactory.h"
#include "rhi/vulkan/VulkanRHIContext.h"

namespace Fabrica::RHI::Vulkan {

void EnsureVulkanBackendRegistered() {
  static const bool kRegistered = [] {
    RHIFactory::RegisterBackend(ERHIBackend::Vulkan,
                                [] { return std::make_unique<VulkanRHIContext>(); });
    return true;
  }();
  (void)kRegistered;
}

}  // namespace Fabrica::RHI::Vulkan