#include "VulkanBackendTypes.h"

namespace Fabrica::RHI::Vulkan {

RHIRenderPassCreateResult CreateVulkanRenderPass(VulkanContextState& state,
                                                 const RHIRenderPassDesc& desc) {
  const RHIRenderPassHandle handle = state.render_passes.Allocate(
      [&](VulkanRenderPassResource& resource) { resource.desc = desc; });
  if (!handle.IsValid()) {
    return {.result = RHIResult::ErrorOutOfMemory};
  }
  return {.handle = handle, .result = RHIResult::Success};
}

void DestroyVulkanRenderPass(VulkanContextState&, VulkanRenderPassResource& resource) {
  resource = VulkanRenderPassResource{};
}

}  // namespace Fabrica::RHI::Vulkan