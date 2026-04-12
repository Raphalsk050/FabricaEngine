#include "VulkanBackendTypes.h"

namespace Fabrica::RHI::Vulkan {

RHIBufferCreateResult CreateVulkanBuffer(VulkanContextState& state,
                                         const RHIBufferDesc& desc) {
  const RHIBufferHandle handle = state.buffers.Allocate([&](VulkanBufferResource& resource) {
    resource.desc = desc;

    VkBufferCreateInfo create_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    create_info.size = desc.size;
    create_info.usage = ToVkBufferUsage(desc.usage);
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = ToVmaUsage(desc.memory);
    if (desc.memory == ERHIMemoryType::HostVisible ||
        desc.memory == ERHIMemoryType::HostCoherent ||
        desc.memory == ERHIMemoryType::Staging || desc.mapped_at_creation) {
      allocation_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
      allocation_info.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
      resource.persistent_mapping = true;
    }

    VmaAllocationInfo info{};
    if (vmaCreateBuffer(state.allocator, &create_info, &allocation_info,
                        &resource.buffer, &resource.allocation, &info) == VK_SUCCESS) {
      resource.mapped_data = info.pMappedData;
      resource.is_mapped = resource.mapped_data != nullptr;
    }
  });

  if (!handle.IsValid()) {
    return {.result = RHIResult::ErrorOutOfMemory};
  }

  const VulkanBufferResource* resource = state.buffers.Resolve(handle);
  if (resource == nullptr || resource->buffer == VK_NULL_HANDLE ||
      resource->allocation == VK_NULL_HANDLE) {
    state.buffers.Destroy(handle, [&](VulkanBufferResource& failed_resource) {
      DestroyVulkanBuffer(state, failed_resource);
    });
    return {.result = RHIResult::ErrorOutOfMemory};
  }

  return {.handle = handle, .result = RHIResult::Success};
}

void DestroyVulkanBuffer(VulkanContextState& state, VulkanBufferResource& resource) {
  if (!resource.persistent_mapping && resource.is_mapped && resource.allocation != VK_NULL_HANDLE) {
    vmaUnmapMemory(state.allocator, resource.allocation);
  }
  if (resource.buffer != VK_NULL_HANDLE && resource.allocation != VK_NULL_HANDLE) {
    vmaDestroyBuffer(state.allocator, resource.buffer, resource.allocation);
  }
  resource = VulkanBufferResource{};
}

}  // namespace Fabrica::RHI::Vulkan