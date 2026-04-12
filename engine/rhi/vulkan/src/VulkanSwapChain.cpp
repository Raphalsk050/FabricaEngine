#include "VulkanBackendTypes.h"

#include <algorithm>
#include <limits>

namespace Fabrica::RHI::Vulkan {
namespace {

VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
  for (const VkSurfaceFormatKHR& format : formats) {
    if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return format;
    }
  }
  return formats.front();
}

VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& present_modes,
                                   bool vsync_enabled) {
  if (vsync_enabled) {
    return VK_PRESENT_MODE_FIFO_KHR;
  }

  for (const VkPresentModeKHR mode : present_modes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return mode;
    }
  }
  for (const VkPresentModeKHR mode : present_modes) {
    if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
      return mode;
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ClampExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                       std::uint32_t width,
                       std::uint32_t height) {
  if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
    return capabilities.currentExtent;
  }

  VkExtent2D extent{};
  extent.width = std::clamp(width, capabilities.minImageExtent.width,
                            capabilities.maxImageExtent.width);
  extent.height = std::clamp(height, capabilities.minImageExtent.height,
                             capabilities.maxImageExtent.height);
  return extent;
}

}  // namespace

VulkanSwapChain::VulkanSwapChain(VulkanContextState& state) : state_(&state) {}

VulkanSwapChain::~VulkanSwapChain() { Shutdown(); }

bool VulkanSwapChain::Initialize(const RHIContextDesc& desc) {
  if (state_ == nullptr || state_->surface == VK_NULL_HANDLE) {
    return false;
  }

  vsync_enabled_ = desc.enable_vsync;
  CreateSwapChain(desc.initial_extent.width, desc.initial_extent.height);
  return swapchain_ != VK_NULL_HANDLE;
}

void VulkanSwapChain::Shutdown() { DestroySwapChainResources(); }

void VulkanSwapChain::DestroySwapChainResources() {
  if (state_ == nullptr || state_->device == VK_NULL_HANDLE) {
    return;
  }

  DestroyPerImageSemaphores();

  for (RHITextureHandle handle : back_buffers_) {
    state_->textures.Destroy(handle,
                             [&](VulkanTextureResource& resource) {
                               DestroyVulkanTexture(*state_, resource);
                             });
  }
  back_buffers_.clear();
  image_views_.clear();
  images_.clear();

  if (swapchain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(state_->device, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
  }
}

void VulkanSwapChain::CreateImageViewsAndHandles() {
  image_views_.resize(images_.size());
  back_buffers_.resize(images_.size());

  RHITextureDesc back_buffer_desc{};
  back_buffer_desc.width = extent_.width;
  back_buffer_desc.height = extent_.height;
  back_buffer_desc.format = ERHITextureFormat::B8G8R8A8_UNORM;
  back_buffer_desc.usage = ERHITextureUsage::ColorAttachment | ERHITextureUsage::TransferDst |
                           ERHITextureUsage::TransferSrc | ERHITextureUsage::Sampled;
  back_buffer_desc.aspect = ERHITextureAspectFlags::Color;
  back_buffer_desc.memory = ERHIMemoryType::DeviceLocal;
  back_buffer_desc.initial_layout = ERHITextureLayout::Undefined;

  for (std::size_t index = 0; index < images_.size(); ++index) {
    VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_info.image = images_[index];
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = image_format_;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;
    vkCreateImageView(state_->device, &view_info, nullptr, &image_views_[index]);

    back_buffers_[index] = CreateVulkanTextureFromImage(
        *state_, back_buffer_desc, images_[index], image_views_[index],
        back_buffer_desc.initial_layout);
  }
}

void VulkanSwapChain::CreateSwapChain(std::uint32_t width, std::uint32_t height) {
  VkSurfaceCapabilitiesKHR capabilities{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state_->physical_device, state_->surface,
                                            &capabilities);

  std::uint32_t format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(state_->physical_device, state_->surface,
                                       &format_count, nullptr);
  std::vector<VkSurfaceFormatKHR> formats(format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(state_->physical_device, state_->surface,
                                       &format_count, formats.data());

  std::uint32_t present_mode_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(state_->physical_device, state_->surface,
                                            &present_mode_count, nullptr);
  std::vector<VkPresentModeKHR> present_modes(present_mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(state_->physical_device, state_->surface,
                                            &present_mode_count,
                                            present_modes.data());

  const VkSurfaceFormatKHR surface_format = ChooseSurfaceFormat(formats);
  const VkPresentModeKHR present_mode =
      ChoosePresentMode(present_modes, vsync_enabled_);
  const VkExtent2D swap_extent = ClampExtent(capabilities, width, height);

  std::uint32_t image_count = std::max(capabilities.minImageCount + 1u, 3u);
  if (capabilities.maxImageCount > 0) {
    image_count = std::min(image_count, capabilities.maxImageCount);
  }

  VkSwapchainCreateInfoKHR create_info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  create_info.surface = state_->surface;
  create_info.minImageCount = image_count;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.imageExtent = swap_extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                           VK_IMAGE_USAGE_SAMPLED_BIT;
  create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.preTransform = capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = present_mode;
  create_info.clipped = VK_TRUE;
  create_info.oldSwapchain = swapchain_;

  VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
  if (vkCreateSwapchainKHR(state_->device, &create_info, nullptr, &new_swapchain) !=
      VK_SUCCESS) {
    return;
  }

  if (swapchain_ != VK_NULL_HANDLE) {
    DestroySwapChainResources();
  }

  swapchain_ = new_swapchain;
  image_format_ = surface_format.format;
  color_space_ = surface_format.colorSpace;
  extent_ = {swap_extent.width, swap_extent.height};

  std::uint32_t actual_image_count = 0;
  vkGetSwapchainImagesKHR(state_->device, swapchain_, &actual_image_count, nullptr);
  images_.resize(actual_image_count);
  vkGetSwapchainImagesKHR(state_->device, swapchain_, &actual_image_count,
                          images_.data());
  CreateImageViewsAndHandles();
  CreatePerImageSemaphores();
}

bool VulkanSwapChain::AcquireNextImage() {
  if (swapchain_ == VK_NULL_HANDLE) {
    return false;
  }

  FrameResources& frame = state_->frames[state_->current_frame];
  const VkResult result = vkAcquireNextImageKHR(
      state_->device, swapchain_, std::numeric_limits<std::uint64_t>::max(),
      frame.image_available, VK_NULL_HANDLE, &current_image_index_);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    Recreate(extent_.width, extent_.height);
    return false;
  }
  frame.image_acquired = result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR;
  return frame.image_acquired;
}

void VulkanSwapChain::Present(VkQueue queue, VkSemaphore wait_semaphore) {
  if (swapchain_ == VK_NULL_HANDLE) {
    return;
  }

  VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  present_info.waitSemaphoreCount = wait_semaphore != VK_NULL_HANDLE ? 1u : 0u;
  present_info.pWaitSemaphores = wait_semaphore != VK_NULL_HANDLE ? &wait_semaphore : nullptr;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain_;
  present_info.pImageIndices = &current_image_index_;
  vkQueuePresentKHR(queue, &present_info);
}

void VulkanSwapChain::Recreate(std::uint32_t width, std::uint32_t height) {
  if (state_ == nullptr || state_->device == VK_NULL_HANDLE) {
    return;
  }
  vkDeviceWaitIdle(state_->device);
  CreateSwapChain(width, height);
}

bool VulkanSwapChain::Resize(std::uint32_t width, std::uint32_t height) {
  Recreate(width, height);
  return swapchain_ != VK_NULL_HANDLE;
}

void VulkanSwapChain::SetVSync(bool enabled) {
  if (vsync_enabled_ == enabled) {
    return;
  }
  vsync_enabled_ = enabled;
  Recreate(extent_.width, extent_.height);
}

RHITextureHandle VulkanSwapChain::GetCurrentBackBuffer() const {
  if (back_buffers_.empty()) {
    return {};
  }
  return back_buffers_[current_image_index_];
}

std::uint32_t VulkanSwapChain::GetCurrentImageIndex() const {
  return current_image_index_;
}

RHIExtent2D VulkanSwapChain::GetExtent() const { return extent_; }

VkSemaphore VulkanSwapChain::GetCurrentRenderFinishedSemaphore() const {
  if (current_image_index_ < render_finished_semaphores_.size()) {
    return render_finished_semaphores_[current_image_index_];
  }
  return VK_NULL_HANDLE;
}

void VulkanSwapChain::CreatePerImageSemaphores() {
  DestroyPerImageSemaphores();

  render_finished_semaphores_.resize(images_.size(), VK_NULL_HANDLE);
  VkSemaphoreCreateInfo sem_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  for (std::size_t i = 0; i < images_.size(); ++i) {
    if (vkCreateSemaphore(state_->device, &sem_info, nullptr,
                          &render_finished_semaphores_[i]) != VK_SUCCESS) {
      FABRICA_LOG(Render, Error)
          << "[RHI] Failed to create per-image render_finished semaphore index=" << i;
    }
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug)
        << "[RHI] Created " << render_finished_semaphores_.size()
        << " per-swapchain-image render_finished semaphores";
  }
}

void VulkanSwapChain::DestroyPerImageSemaphores() {
  if (state_ == nullptr || state_->device == VK_NULL_HANDLE) {
    return;
  }

  for (VkSemaphore sem : render_finished_semaphores_) {
    if (sem != VK_NULL_HANDLE) {
      vkDestroySemaphore(state_->device, sem, nullptr);
    }
  }
  render_finished_semaphores_.clear();
}

}  // namespace Fabrica::RHI::Vulkan
