#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "VulkanCommon.h"
#include "VulkanSlotMap.h"
#include "rhi/IRHIBuffer.h"
#include "rhi/IRHIDescriptorSet.h"
#include "rhi/IRHIFence.h"
#include "rhi/IRHIPipeline.h"
#include "rhi/IRHIRenderPass.h"
#include "rhi/IRHISampler.h"
#include "rhi/IRHIShader.h"
#include "rhi/IRHITexture.h"
#include "rhi/vulkan/VulkanCommandList.h"
#include "rhi/vulkan/VulkanDescriptorAllocator.h"
#include "rhi/vulkan/VulkanSwapChain.h"

namespace Fabrica::RHI::Vulkan {

class VulkanRHIContext;

constexpr std::size_t kMaxBuffers = 65536;
constexpr std::size_t kMaxTextures = 16384;
constexpr std::size_t kMaxSamplers = 4096;
constexpr std::size_t kMaxShaders = 4096;
constexpr std::size_t kMaxRenderPasses = 4096;
constexpr std::size_t kMaxPipelines = 4096;
constexpr std::size_t kMaxDescriptorSets = 4096;
constexpr std::size_t kMaxFences = 4096;

struct VulkanBufferResource final : IRHIBuffer {
  RHIBufferHandle handle{};
  RHIBufferDesc desc{};
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  void *mapped_data = nullptr;
  bool persistent_mapping = false;
  bool is_mapped = false;

  [[nodiscard]] RHIBufferHandle GetHandle() const override { return handle; }
  [[nodiscard]] const RHIBufferDesc &GetDesc() const override { return desc; }
  [[nodiscard]] std::uint64_t GetSize() const override { return desc.size; }
};

struct VulkanTextureResource final : IRHITexture {
  RHITextureHandle handle{};
  RHITextureDesc desc{};
  VkImage image = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VkImageView image_view = VK_NULL_HANDLE;
  ERHITextureLayout layout = ERHITextureLayout::Undefined;
  bool owns_image = true;

  [[nodiscard]] RHITextureHandle GetHandle() const override { return handle; }
  [[nodiscard]] const RHITextureDesc &GetDesc() const override { return desc; }
  [[nodiscard]] ERHITextureLayout GetLayout() const override { return layout; }
};

struct VulkanSamplerResource final : IRHISampler {
  RHISamplerHandle handle{};
  RHISamplerDesc desc{};
  VkSampler sampler = VK_NULL_HANDLE;

  [[nodiscard]] RHISamplerHandle GetHandle() const override { return handle; }
  [[nodiscard]] const RHISamplerDesc &GetDesc() const override { return desc; }
};

struct VulkanShaderResource final : IRHIShader {
  RHIShaderHandle handle{};
  RHIShaderDesc desc{};
  VkShaderModule module = VK_NULL_HANDLE;

  [[nodiscard]] RHIShaderHandle GetHandle() const override { return handle; }
  [[nodiscard]] const RHIShaderDesc &GetDesc() const override { return desc; }
};

struct VulkanRenderPassResource final : IRHIRenderPass {
  RHIRenderPassHandle handle{};
  RHIRenderPassDesc desc{};

  [[nodiscard]] RHIRenderPassHandle GetHandle() const override {
    return handle;
  }
  [[nodiscard]] const RHIRenderPassDesc &GetDesc() const override {
    return desc;
  }
};

struct VulkanPipelineResource final : IRHIPipeline {
  RHIPipelineHandle handle{};
  ERHIPipelineType type = ERHIPipelineType::Graphics;
  RHIGraphicsPipelineDesc graphics_desc{};
  RHIComputePipelineDesc compute_desc{};
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

  [[nodiscard]] RHIPipelineHandle GetHandle() const override { return handle; }
  [[nodiscard]] ERHIPipelineType GetType() const override { return type; }
  [[nodiscard]] const RHIGraphicsPipelineDesc *
  GetGraphicsDesc() const override {
    return type == ERHIPipelineType::Graphics ? &graphics_desc : nullptr;
  }
  [[nodiscard]] const RHIComputePipelineDesc *GetComputeDesc() const override {
    return type == ERHIPipelineType::Compute ? &compute_desc : nullptr;
  }
};

struct VulkanContextState;

struct VulkanDescriptorSetResource final : IRHIDescriptorSet {
  VulkanContextState *owner = nullptr;
  RHIDescriptorSetHandle handle{};
  RHIDescriptorSetDesc desc{};
  VkDescriptorSetLayout layout = VK_NULL_HANDLE;
  VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
  std::array<RHIDescriptorBufferWriteDesc, kMaxDescriptorBindings>
      buffer_writes{};
  std::array<RHIDescriptorTextureWriteDesc, kMaxDescriptorBindings>
      texture_writes{};
  std::array<RHIDescriptorSamplerWriteDesc, kMaxDescriptorBindings>
      sampler_writes{};
  std::array<bool, kMaxDescriptorBindings> has_buffer_write{};
  std::array<bool, kMaxDescriptorBindings> has_texture_write{};
  std::array<bool, kMaxDescriptorBindings> has_sampler_write{};

  [[nodiscard]] RHIDescriptorSetHandle GetHandle() const override {
    return handle;
  }
  [[nodiscard]] const RHIDescriptorSetDesc &GetDesc() const override {
    return desc;
  }
  void BindBuffer(std::uint32_t binding,
                  const RHIDescriptorBufferWriteDesc &write) override;
  void BindTexture(std::uint32_t binding,
                   const RHIDescriptorTextureWriteDesc &write) override;
  void BindSampler(std::uint32_t binding,
                   const RHIDescriptorSamplerWriteDesc &write) override;
  void FlushWrites() override;
};

struct VulkanFenceResource final : IRHIFence {
  VulkanContextState *owner = nullptr;
  RHIFenceHandle handle{};
  VkFence fence = VK_NULL_HANDLE;

  [[nodiscard]] RHIFenceHandle GetHandle() const override { return handle; }
  [[nodiscard]] bool IsSignaled() const override;
};

struct FrameResources {
  std::array<VkCommandPool, 3> command_pools{VK_NULL_HANDLE, VK_NULL_HANDLE,
                                             VK_NULL_HANDLE};
  std::array<VkCommandBuffer, 3> command_buffers{VK_NULL_HANDLE, VK_NULL_HANDLE,
                                                 VK_NULL_HANDLE};
  std::array<std::unique_ptr<VulkanCommandList>, 3> command_lists{};
  std::array<VkSemaphore, kMaxSubmittedCommandLists> submission_semaphores{};
  VkSemaphore image_available = VK_NULL_HANDLE;
  VkSemaphore user_fence_semaphore = VK_NULL_HANDLE;
  VkFence frame_fence = VK_NULL_HANDLE;
  bool image_acquired = false;
  bool image_wait_consumed = false;
  bool work_submitted = false;
  std::uint32_t submission_count = 0;
  VkSemaphore last_submission_semaphore = VK_NULL_HANDLE;
  RHIFenceHandle last_signaled_fence{};
  std::vector<RHIBufferHandle> deferred_buffers;
  std::vector<RHITextureHandle> deferred_textures;
  std::vector<RHISamplerHandle> deferred_samplers;
  std::vector<RHIShaderHandle> deferred_shaders;
  std::vector<RHIRenderPassHandle> deferred_render_passes;
  std::vector<RHIPipelineHandle> deferred_pipelines;
  std::vector<RHIDescriptorSetHandle> deferred_descriptor_sets;
};

using BufferPool = VulkanSlotMap<RHIBufferHandle, VulkanBufferResource,
                                 ERHIResourceType::kBuffer, kMaxBuffers>;
using TexturePool = VulkanSlotMap<RHITextureHandle, VulkanTextureResource,
                                  ERHIResourceType::kTexture, kMaxTextures>;
using SamplerPool = VulkanSlotMap<RHISamplerHandle, VulkanSamplerResource,
                                  ERHIResourceType::kSampler, kMaxSamplers>;
using ShaderPool = VulkanSlotMap<RHIShaderHandle, VulkanShaderResource,
                                 ERHIResourceType::kShader, kMaxShaders>;
using RenderPassPool =
    VulkanSlotMap<RHIRenderPassHandle, VulkanRenderPassResource,
                  ERHIResourceType::kRenderPass, kMaxRenderPasses>;
using PipelinePool = VulkanSlotMap<RHIPipelineHandle, VulkanPipelineResource,
                                   ERHIResourceType::kPipeline, kMaxPipelines>;
using DescriptorSetPool =
    VulkanSlotMap<RHIDescriptorSetHandle, VulkanDescriptorSetResource,
                  ERHIResourceType::kDescriptorSet, kMaxDescriptorSets>;
using FencePool = VulkanSlotMap<RHIFenceHandle, VulkanFenceResource,
                                ERHIResourceType::kFence, kMaxFences>;

struct VulkanContextState {
  RHIContextDesc desc{};
  RHIDeviceInfo device_info{};
  VkInstance instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VmaAllocator allocator = VK_NULL_HANDLE;
  VkPipelineCache native_pipeline_cache = VK_NULL_HANDLE;
  VkQueue graphics_queue = VK_NULL_HANDLE;
  VkQueue compute_queue = VK_NULL_HANDLE;
  VkQueue transfer_queue = VK_NULL_HANDLE;
  std::uint32_t graphics_queue_family = kInvalidQueueFamily;
  std::uint32_t compute_queue_family = kInvalidQueueFamily;
  std::uint32_t transfer_queue_family = kInvalidQueueFamily;
  std::uint32_t current_frame = 0;
  std::uint64_t frame_number = 0;
  bool initialized = false;
  bool validation_enabled = false;
  PFN_vkSetDebugUtilsObjectNameEXT set_debug_name = nullptr;
  PFN_vkCmdBeginDebugUtilsLabelEXT cmd_begin_debug_label = nullptr;
  PFN_vkCmdEndDebugUtilsLabelEXT cmd_end_debug_label = nullptr;
  PFN_vkCmdInsertDebugUtilsLabelEXT cmd_insert_debug_label = nullptr;
  std::array<FrameResources, kFramesInFlight> frames{};
  BufferPool buffers{};
  TexturePool textures{};
  SamplerPool samplers{};
  ShaderPool shaders{};
  RenderPassPool render_passes{};
  PipelinePool pipelines{};
  DescriptorSetPool descriptor_sets{};
  FencePool fences{};
  std::unordered_map<std::size_t, VkDescriptorSetLayout>
      descriptor_layout_cache;
  std::unordered_map<std::size_t, VkPipelineLayout> pipeline_layout_cache;
  std::unordered_map<std::size_t, VkPipeline> graphics_pipeline_cache;
  std::unordered_map<std::size_t, VkPipeline> compute_pipeline_cache;
  std::unique_ptr<VulkanDescriptorAllocator> descriptor_allocator;
  std::unique_ptr<VulkanSwapChain> swap_chain;
  VkCommandPool immediate_command_pool = VK_NULL_HANDLE;
};

bool InitializeVulkanDevice(VulkanContextState &state,
                            const RHIContextDesc &desc);
void ShutdownVulkanDevice(VulkanContextState &state);

RHIBufferCreateResult CreateVulkanBuffer(VulkanContextState &state,
                                         const RHIBufferDesc &desc);
RHITextureCreateResult CreateVulkanTexture(VulkanContextState &state,
                                           const RHITextureDesc &desc);
RHITextureHandle CreateVulkanTextureFromImage(VulkanContextState &state,
                                              const RHITextureDesc &desc,
                                              VkImage image,
                                              VkImageView image_view,
                                              ERHITextureLayout layout);
RHISamplerCreateResult CreateVulkanSampler(VulkanContextState &state,
                                           const RHISamplerDesc &desc);
RHIShaderCreateResult CreateVulkanShader(VulkanContextState &state,
                                         const RHIShaderDesc &desc);
RHIRenderPassCreateResult CreateVulkanRenderPass(VulkanContextState &state,
                                                 const RHIRenderPassDesc &desc);
RHIPipelineCreateResult
CreateVulkanGraphicsPipeline(VulkanContextState &state,
                             const RHIGraphicsPipelineDesc &desc);
RHIPipelineCreateResult
CreateVulkanComputePipeline(VulkanContextState &state,
                            const RHIComputePipelineDesc &desc);
RHIDescriptorSetCreateResult
CreateVulkanDescriptorSet(VulkanContextState &state,
                          const RHIDescriptorSetDesc &desc);
RHIFenceCreateResult CreateVulkanFence(VulkanContextState &state,
                                       bool signaled);

void DestroyVulkanBuffer(VulkanContextState &state,
                         VulkanBufferResource &resource);
void DestroyVulkanTexture(VulkanContextState &state,
                          VulkanTextureResource &resource);
void DestroyVulkanSampler(VulkanContextState &state,
                          VulkanSamplerResource &resource);
void DestroyVulkanShader(VulkanContextState &state,
                         VulkanShaderResource &resource);
void DestroyVulkanRenderPass(VulkanContextState &state,
                             VulkanRenderPassResource &resource);
void DestroyVulkanPipeline(VulkanContextState &state,
                           VulkanPipelineResource &resource);
void DestroyVulkanDescriptorSet(VulkanContextState &state,
                                VulkanDescriptorSetResource &resource);
void DestroyVulkanFence(VulkanContextState &state,
                        VulkanFenceResource &resource);

VkDescriptorSetLayout
GetOrCreateDescriptorSetLayout(VulkanContextState &state,
                               const RHIDescriptorSetLayoutDesc &desc);
VkPipelineLayout GetOrCreatePipelineLayout(VulkanContextState &state,
                                           const RHIPipelineLayoutDesc &desc);
void SetVulkanObjectDebugName(VulkanContextState &state,
                              std::uint64_t object_handle,
                              VkObjectType object_type, std::string_view name);

} // namespace Fabrica::RHI::Vulkan
