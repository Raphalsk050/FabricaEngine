#include "VulkanBackendTypes.h"

namespace Fabrica::RHI::Vulkan {
namespace {

VkImageViewType PickImageViewType(const RHITextureDesc& desc) {
  if (desc.depth > 1) {
    return VK_IMAGE_VIEW_TYPE_3D;
  }
  if (desc.array_layers > 1) {
    return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  }
  return VK_IMAGE_VIEW_TYPE_2D;
}

VkImageType PickImageType(const RHITextureDesc& desc) {
  return desc.depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
}

void CreateImageView(const VulkanContextState& state,
                     VulkanTextureResource& resource) {
  VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  view_info.image = resource.image;
  view_info.viewType = PickImageViewType(resource.desc);
  view_info.format = ToVkFormat(resource.desc.format);
  view_info.subresourceRange.aspectMask = ToVkImageAspect(resource.desc.aspect);
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = resource.desc.mip_levels;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = resource.desc.array_layers;
  vkCreateImageView(state.device, &view_info, nullptr, &resource.image_view);
}

}  // namespace

RHITextureCreateResult CreateVulkanTexture(VulkanContextState& state,
                                           const RHITextureDesc& desc) {
  const RHITextureHandle handle = state.textures.Allocate([&](VulkanTextureResource& resource) {
    resource.desc = desc;
    resource.layout = desc.initial_layout;

    VkImageCreateInfo create_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    create_info.imageType = PickImageType(desc);
    create_info.format = ToVkFormat(desc.format);
    create_info.extent = {desc.width, desc.height, desc.depth};
    create_info.mipLevels = desc.mip_levels;
    create_info.arrayLayers = desc.array_layers;
    create_info.samples = ToVkSampleCount(desc.samples);
    create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    create_info.usage = ToVkImageUsage(desc.usage);
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = ToVmaUsage(desc.memory);
    if (vmaCreateImage(state.allocator, &create_info, &allocation_info, &resource.image,
                       &resource.allocation, nullptr) == VK_SUCCESS) {
      CreateImageView(state, resource);
    }
  });

  if (!handle.IsValid()) {
    return {.result = RHIResult::ErrorOutOfMemory};
  }

  const VulkanTextureResource* resource = state.textures.Resolve(handle);
  if (resource == nullptr || resource->image == VK_NULL_HANDLE ||
      resource->allocation == VK_NULL_HANDLE || resource->image_view == VK_NULL_HANDLE) {
    state.textures.Destroy(handle, [&](VulkanTextureResource& failed_resource) {
      DestroyVulkanTexture(state, failed_resource);
    });
    return {.result = RHIResult::ErrorOutOfMemory};
  }

  return {.handle = handle, .result = RHIResult::Success};
}

RHITextureHandle CreateVulkanTextureFromImage(VulkanContextState& state,
                                              const RHITextureDesc& desc,
                                              VkImage image,
                                              VkImageView image_view,
                                              ERHITextureLayout layout) {
  return state.textures.Allocate([&](VulkanTextureResource& resource) {
    resource.desc = desc;
    resource.image = image;
    resource.image_view = image_view;
    resource.layout = layout;
    resource.owns_image = false;
  });
}

void DestroyVulkanTexture(VulkanContextState& state, VulkanTextureResource& resource) {
  if (resource.image_view != VK_NULL_HANDLE) {
    vkDestroyImageView(state.device, resource.image_view, nullptr);
  }

  if (resource.owns_image && resource.image != VK_NULL_HANDLE &&
      resource.allocation != VK_NULL_HANDLE) {
    vmaDestroyImage(state.allocator, resource.image, resource.allocation);
  }

  resource = VulkanTextureResource{};
}

}  // namespace Fabrica::RHI::Vulkan