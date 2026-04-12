#pragma once

#include <vector>

#include <Volk/volk.h>

#include "rhi/IRHISwapChain.h"

namespace Fabrica::RHI::Vulkan {

struct VulkanContextState;

class VulkanSwapChain final : public IRHISwapChain {
public:
  explicit VulkanSwapChain(VulkanContextState &state);
  ~VulkanSwapChain() override;

  bool Initialize(const RHIContextDesc &desc);
  void Shutdown();
  void Recreate(std::uint32_t width, std::uint32_t height);
  void Present(VkQueue queue, VkSemaphore wait_semaphore);

  /// Returns the render_finished semaphore for the currently acquired image.
  /// This semaphore is indexed by swapchain image index (NOT frame-in-flight),
  /// which prevents the binary semaphore reuse hazard described in
  /// https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html
  [[nodiscard]] VkSemaphore GetCurrentRenderFinishedSemaphore() const;

  bool AcquireNextImage() override;
  [[nodiscard]] RHITextureHandle GetCurrentBackBuffer() const override;
  [[nodiscard]] std::uint32_t GetCurrentImageIndex() const override;
  [[nodiscard]] RHIExtent2D GetExtent() const override;
  bool Resize(std::uint32_t width, std::uint32_t height) override;
  void SetVSync(bool enabled) override;

  [[nodiscard]] VkSwapchainKHR GetNativeSwapChain() const { return swapchain_; }
  [[nodiscard]] VkFormat GetImageFormat() const { return image_format_; }
  [[nodiscard]] VkColorSpaceKHR GetColorSpace() const { return color_space_; }
  [[nodiscard]] std::uint32_t GetImageCount() const {
    return static_cast<std::uint32_t>(images_.size());
  }

private:
  void DestroySwapChainResources();
  void CreateSwapChain(std::uint32_t width, std::uint32_t height);
  void CreateImageViewsAndHandles();
  void CreatePerImageSemaphores();
  void DestroyPerImageSemaphores();

  VulkanContextState *state_ = nullptr;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkFormat image_format_ = VK_FORMAT_UNDEFINED;
  VkColorSpaceKHR color_space_ = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  std::vector<VkImage> images_;
  std::vector<VkImageView> image_views_;
  std::vector<RHITextureHandle> back_buffers_;
  /// Per-swapchain-image semaphores used to signal render completion before
  /// presentation. Indexed by the acquired image index from
  /// vkAcquireNextImageKHR, guaranteeing that we never signal a semaphore
  /// while the presentation engine is still consuming it.
  std::vector<VkSemaphore> render_finished_semaphores_;
  std::uint32_t current_image_index_ = 0;
  RHIExtent2D extent_{};
  bool vsync_enabled_ = true;
};

} // namespace Fabrica::RHI::Vulkan
