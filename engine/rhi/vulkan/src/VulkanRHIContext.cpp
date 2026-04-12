#include "rhi/vulkan/VulkanRHIContext.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

#include "VulkanBackendTypes.h"

namespace Fabrica::RHI::Vulkan {
namespace {

VkQueue GetQueue(VulkanContextState& state, ERHIQueueType queue) {
  switch (queue) {
    case ERHIQueueType::Compute:
      return state.compute_queue;
    case ERHIQueueType::Transfer:
      return state.transfer_queue;
    case ERHIQueueType::Graphics:
    default:
      return state.graphics_queue;
  }
}

std::size_t GetQueueSlot(ERHIQueueType queue) {
  switch (queue) {
    case ERHIQueueType::Graphics:
      return 0;
    case ERHIQueueType::Compute:
      return 1;
    case ERHIQueueType::Transfer:
    default:
      return 2;
  }
}

void AdvanceFrame(VulkanContextState& state) {
  state.current_frame = (state.current_frame + 1u) % kFramesInFlight;
  ++state.frame_number;
}

void FlushMappedAllocationIfNeeded(VulkanContextState& state,
                                   const VulkanBufferResource& resource,
                                   std::size_t offset,
                                   std::size_t size) {
  if (resource.allocation == VK_NULL_HANDLE ||
      resource.desc.memory == ERHIMemoryType::DeviceLocal) {
    return;
  }

  vmaFlushAllocation(state.allocator, resource.allocation, offset, size);
}

template <typename RecordFn>
RHIResult ExecuteImmediate(VulkanContextState& state, RecordFn&& record_fn) {
  VkCommandBufferAllocateInfo alloc_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  alloc_info.commandPool = state.immediate_command_pool;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = 1;

  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  VkFence fence = VK_NULL_HANDLE;

  const VkResult allocate_result =
      vkAllocateCommandBuffers(state.device, &alloc_info, &command_buffer);
  if (allocate_result != VK_SUCCESS) {
    return ToRHIResult(allocate_result);
  }

  const auto cleanup = [&]() {
    if (fence != VK_NULL_HANDLE) {
      vkDestroyFence(state.device, fence, nullptr);
    }
    if (command_buffer != VK_NULL_HANDLE) {
      vkFreeCommandBuffers(state.device, state.immediate_command_pool, 1,
                           &command_buffer);
    }
  };

  VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  const VkResult begin_result = vkBeginCommandBuffer(command_buffer, &begin_info);
  if (begin_result != VK_SUCCESS) {
    cleanup();
    return ToRHIResult(begin_result);
  }

  record_fn(command_buffer);

  const VkResult end_result = vkEndCommandBuffer(command_buffer);
  if (end_result != VK_SUCCESS) {
    cleanup();
    return ToRHIResult(end_result);
  }

  VkCommandBufferSubmitInfo command_submit{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
  command_submit.commandBuffer = command_buffer;

  VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
  submit.commandBufferInfoCount = 1;
  submit.pCommandBufferInfos = &command_submit;

  VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  const VkResult fence_result = vkCreateFence(state.device, &fence_info, nullptr, &fence);
  if (fence_result != VK_SUCCESS) {
    cleanup();
    return ToRHIResult(fence_result);
  }

  const VkResult submit_result = vkQueueSubmit2(state.graphics_queue, 1, &submit, fence);
  if (submit_result != VK_SUCCESS) {
    cleanup();
    return ToRHIResult(submit_result);
  }

  const VkResult wait_result = vkWaitForFences(
      state.device, 1, &fence, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
  cleanup();
  return ToRHIResult(wait_result);
}

void ProcessDeferredDestroy(VulkanContextState& state, FrameResources& frame) {
  for (RHIBufferHandle handle : frame.deferred_buffers) {
    state.buffers.Destroy(handle, [&](VulkanBufferResource& resource) {
      DestroyVulkanBuffer(state, resource);
    });
  }
  frame.deferred_buffers.clear();

  for (RHITextureHandle handle : frame.deferred_textures) {
    state.textures.Destroy(handle, [&](VulkanTextureResource& resource) {
      DestroyVulkanTexture(state, resource);
    });
  }
  frame.deferred_textures.clear();

  for (RHISamplerHandle handle : frame.deferred_samplers) {
    state.samplers.Destroy(handle, [&](VulkanSamplerResource& resource) {
      DestroyVulkanSampler(state, resource);
    });
  }
  frame.deferred_samplers.clear();

  for (RHIShaderHandle handle : frame.deferred_shaders) {
    state.shaders.Destroy(handle, [&](VulkanShaderResource& resource) {
      DestroyVulkanShader(state, resource);
    });
  }
  frame.deferred_shaders.clear();

  for (RHIRenderPassHandle handle : frame.deferred_render_passes) {
    state.render_passes.Destroy(handle, [&](VulkanRenderPassResource& resource) {
      DestroyVulkanRenderPass(state, resource);
    });
  }
  frame.deferred_render_passes.clear();

  for (RHIPipelineHandle handle : frame.deferred_pipelines) {
    state.pipelines.Destroy(handle, [&](VulkanPipelineResource& resource) {
      DestroyVulkanPipeline(state, resource);
    });
  }
  frame.deferred_pipelines.clear();

  for (RHIDescriptorSetHandle handle : frame.deferred_descriptor_sets) {
    state.descriptor_sets.Destroy(handle, [&](VulkanDescriptorSetResource& resource) {
      DestroyVulkanDescriptorSet(state, resource);
    });
  }
  frame.deferred_descriptor_sets.clear();
}

}  // namespace

VulkanRHIContext::VulkanRHIContext() = default;

VulkanRHIContext::~VulkanRHIContext() { Shutdown(); }

RHIResult VulkanRHIContext::Initialize(const RHIContextDesc& desc) {
  if (state_ != nullptr && state_->initialized) {
    return RHIResult::Success;
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] Initialize validation=" << desc.enable_validation
                               << " vsync=" << desc.enable_vsync << " extent="
                               << desc.initial_extent.width << "x"
                               << desc.initial_extent.height;
  }

  state_ = std::make_unique<VulkanContextState>();
  if (!InitializeVulkanDevice(*state_, desc)) {
    if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
      FABRICA_LOG(Render, Debug)
          << "[RHI] Initialize failed during Vulkan device setup";
    }
    state_.reset();
    return RHIResult::ErrorValidation;
  }

  state_->descriptor_allocator = std::make_unique<VulkanDescriptorAllocator>(*state_);
  if (!state_->descriptor_allocator->Initialize()) {
    Shutdown();
    return RHIResult::ErrorValidation;
  }

  if (state_->surface != VK_NULL_HANDLE) {
    state_->swap_chain = std::make_unique<VulkanSwapChain>(*state_);
    if (!state_->swap_chain->Initialize(desc)) {
      Shutdown();
      return RHIResult::ErrorValidation;
    }
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] Initialize completed device='"
                               << state_->device_info.device_name << "'";
  }
  return RHIResult::Success;
}

void VulkanRHIContext::Shutdown() {
  if (state_ == nullptr) {
    return;
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] Shutdown";
  }

  if (state_->device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(state_->device);
  }

  if (state_->swap_chain != nullptr) {
    state_->swap_chain->Shutdown();
    state_->swap_chain.reset();
  }

  state_->descriptor_sets.DestroyAll([&](VulkanDescriptorSetResource& resource) {
    DestroyVulkanDescriptorSet(*state_, resource);
  });
  state_->pipelines.DestroyAll([&](VulkanPipelineResource& resource) {
    DestroyVulkanPipeline(*state_, resource);
  });
  state_->render_passes.DestroyAll([&](VulkanRenderPassResource& resource) {
    DestroyVulkanRenderPass(*state_, resource);
  });
  state_->shaders.DestroyAll([&](VulkanShaderResource& resource) {
    DestroyVulkanShader(*state_, resource);
  });
  state_->samplers.DestroyAll([&](VulkanSamplerResource& resource) {
    DestroyVulkanSampler(*state_, resource);
  });
  state_->textures.DestroyAll([&](VulkanTextureResource& resource) {
    DestroyVulkanTexture(*state_, resource);
  });
  state_->buffers.DestroyAll([&](VulkanBufferResource& resource) {
    DestroyVulkanBuffer(*state_, resource);
  });
  state_->fences.DestroyAll([&](VulkanFenceResource& resource) {
    DestroyVulkanFence(*state_, resource);
  });

  if (state_->descriptor_allocator != nullptr) {
    state_->descriptor_allocator->Shutdown();
    state_->descriptor_allocator.reset();
  }

  for (const auto& [_, pipeline] : state_->graphics_pipeline_cache) {
    if (pipeline != VK_NULL_HANDLE && state_->device != VK_NULL_HANDLE) {
      vkDestroyPipeline(state_->device, pipeline, nullptr);
    }
  }
  state_->graphics_pipeline_cache.clear();

  for (const auto& [_, pipeline] : state_->compute_pipeline_cache) {
    if (pipeline != VK_NULL_HANDLE && state_->device != VK_NULL_HANDLE) {
      vkDestroyPipeline(state_->device, pipeline, nullptr);
    }
  }
  state_->compute_pipeline_cache.clear();

  ShutdownVulkanDevice(*state_);
  state_.reset();
}

void VulkanRHIContext::BeginFrame() {
  if (state_ == nullptr) {
    return;
  }

  FrameResources& frame = state_->frames[state_->current_frame];
  vkWaitForFences(state_->device, 1, &frame.frame_fence, VK_TRUE,
                  std::numeric_limits<std::uint64_t>::max());
  vkResetFences(state_->device, 1, &frame.frame_fence);

  ProcessDeferredDestroy(*state_, frame);
  state_->descriptor_allocator->ResetFramePools(state_->current_frame);

  for (VkCommandPool pool : frame.command_pools) {
    vkResetCommandPool(state_->device, pool, 0);
  }
  for (std::size_t index = 0; index < frame.command_lists.size(); ++index) {
    frame.command_lists[index]->Attach(frame.command_buffers[index], state_->current_frame);
    frame.command_lists[index]->Reset();
  }

  frame.image_acquired = false;
  frame.image_wait_consumed = false;
  frame.work_submitted = false;
  frame.submission_count = 0;
  frame.last_submission_semaphore = VK_NULL_HANDLE;
  frame.last_signaled_fence = {};

  if (state_->swap_chain != nullptr) {
    frame.image_acquired = state_->swap_chain->AcquireNextImage();
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] BeginFrame frame_number=" << state_->frame_number
                               << " current_frame=" << state_->current_frame
                               << " image_acquired=" << frame.image_acquired;
  }
}
void VulkanRHIContext::EndFrame() {
  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    if (state_ != nullptr) {
      FABRICA_LOG(Render, Debug) << "[RHI] EndFrame frame_number="
                                 << state_->frame_number << " current_frame="
                                 << state_->current_frame;
    }
  }
}

void VulkanRHIContext::Present() {
  if (state_ == nullptr) {
    return;
  }

  FrameResources& frame = state_->frames[state_->current_frame];
  std::array<VkSemaphoreSubmitInfo, 2> waits{};
  std::uint32_t wait_count = 0;

  if (frame.work_submitted && frame.last_submission_semaphore != VK_NULL_HANDLE) {
    waits[wait_count].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waits[wait_count].semaphore = frame.last_submission_semaphore;
    waits[wait_count].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    ++wait_count;
  }

  if (frame.image_acquired && !frame.image_wait_consumed) {
    waits[wait_count].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waits[wait_count].semaphore = frame.image_available;
    waits[wait_count].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    ++wait_count;
    frame.image_wait_consumed = true;
  }

  // Use the per-swapchain-image render_finished semaphore to avoid
  // the binary semaphore reuse hazard. Each swapchain image has its
  // own dedicated semaphore, indexed by image index (not frame-in-flight).
  // See: https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html
  VkSemaphore present_semaphore = VK_NULL_HANDLE;
  VkSemaphoreSubmitInfo signal_info{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
  VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
  submit.waitSemaphoreInfoCount = wait_count;
  submit.pWaitSemaphoreInfos = wait_count > 0 ? waits.data() : nullptr;
  if (frame.image_acquired && state_->swap_chain != nullptr) {
    present_semaphore = state_->swap_chain->GetCurrentRenderFinishedSemaphore();
    signal_info.semaphore = present_semaphore;
    signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    submit.signalSemaphoreInfoCount = 1;
    submit.pSignalSemaphoreInfos = &signal_info;
  }

  vkQueueSubmit2(state_->graphics_queue, 1, &submit, frame.frame_fence);
  frame.work_submitted = true;
  frame.last_submission_semaphore = VK_NULL_HANDLE;

  if (state_->swap_chain != nullptr && frame.image_acquired) {
    state_->swap_chain->Present(state_->graphics_queue, present_semaphore);
  } else if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] Present skipped image_acquired="
                               << frame.image_acquired;
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] Present frame_number=" << state_->frame_number
                               << " current_frame=" << state_->current_frame
                               << " work_submitted=" << frame.work_submitted
                               << " image_acquired=" << frame.image_acquired
                               << " submission_count=" << frame.submission_count;
  }

  AdvanceFrame(*state_);
}
RHIBufferCreateResult VulkanRHIContext::CreateBuffer(const RHIBufferDesc& desc) {
  if (state_ == nullptr) {
    return {.result = RHIResult::ErrorValidation};
  }

  const RHIBufferCreateResult result = CreateVulkanBuffer(*state_, desc);
  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] CreateBuffer size=" << desc.size
                               << " usage=" << static_cast<std::uint32_t>(desc.usage)
                               << " memory=" << static_cast<int>(desc.memory)
                               << " handle=" << result.handle.id
                               << " result=" << ToString(result.result);
  }
  return result;
}

RHITextureCreateResult VulkanRHIContext::CreateTexture(const RHITextureDesc& desc) {
  if (state_ == nullptr) {
    return {.result = RHIResult::ErrorValidation};
  }

  const RHITextureCreateResult result = CreateVulkanTexture(*state_, desc);
  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] CreateTexture extent=" << desc.width << "x"
                               << desc.height << "x" << desc.depth
                               << " format=" << static_cast<int>(desc.format)
                               << " usage=" << static_cast<std::uint32_t>(desc.usage)
                               << " handle=" << result.handle.id
                               << " result=" << ToString(result.result);
  }
  return result;
}

RHISamplerCreateResult VulkanRHIContext::CreateSampler(const RHISamplerDesc& desc) {
  if (state_ == nullptr) {
    return {.result = RHIResult::ErrorValidation};
  }

  const RHISamplerCreateResult result = CreateVulkanSampler(*state_, desc);
  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] CreateSampler handle=" << result.handle.id
                               << " result=" << ToString(result.result);
  }
  return result;
}

RHIShaderCreateResult VulkanRHIContext::CreateShader(const RHIShaderDesc& desc) {
  if (state_ == nullptr) {
    return {.result = RHIResult::ErrorValidation};
  }

  const RHIShaderCreateResult result = CreateVulkanShader(*state_, desc);
  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] CreateShader stage=" << static_cast<int>(desc.stage)
                               << " bytecode_size=" << desc.bytecode_size
                               << " handle=" << result.handle.id
                               << " result=" << ToString(result.result);
  }
  return result;
}

RHIRenderPassCreateResult VulkanRHIContext::CreateRenderPass(
    const RHIRenderPassDesc& desc) {
  if (state_ == nullptr) {
    return {.result = RHIResult::ErrorValidation};
  }

  const RHIRenderPassCreateResult result = CreateVulkanRenderPass(*state_, desc);
  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] CreateRenderPass colors="
                               << desc.color_attachment_count << " has_depth="
                               << desc.has_depth_stencil_attachment << " handle="
                               << result.handle.id << " result=" << ToString(result.result);
  }
  return result;
}

RHIPipelineCreateResult VulkanRHIContext::CreateGraphicsPipeline(
    const RHIGraphicsPipelineDesc& desc) {
  if (state_ == nullptr) {
    return {.result = RHIResult::ErrorValidation};
  }

  const RHIPipelineCreateResult result = CreateVulkanGraphicsPipeline(*state_, desc);
  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] CreateGraphicsPipeline render_pass="
                               << desc.render_pass.id << " handle=" << result.handle.id
                               << " result=" << ToString(result.result);
  }
  return result;
}

RHIPipelineCreateResult VulkanRHIContext::CreateComputePipeline(
    const RHIComputePipelineDesc& desc) {
  if (state_ == nullptr) {
    return {.result = RHIResult::ErrorValidation};
  }

  const RHIPipelineCreateResult result = CreateVulkanComputePipeline(*state_, desc);
  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] CreateComputePipeline shader="
                               << desc.compute_shader.id << " handle=" << result.handle.id
                               << " result=" << ToString(result.result);
  }
  return result;
}

RHIDescriptorSetCreateResult VulkanRHIContext::CreateDescriptorSet(
    const RHIDescriptorSetDesc& desc) {
  if (state_ == nullptr) {
    return {.result = RHIResult::ErrorValidation};
  }

  const RHIDescriptorSetCreateResult result = CreateVulkanDescriptorSet(*state_, desc);
  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] CreateDescriptorSet frequency="
                               << static_cast<int>(desc.frequency) << " handle="
                               << result.handle.id << " result=" << ToString(result.result);
  }
  return result;
}

RHIResult VulkanRHIContext::UpdateBuffer(RHIBufferHandle handle,
                                         const void* data,
                                         std::size_t offset,
                                         std::size_t size) {
  if (state_ == nullptr) {
    return RHIResult::ErrorValidation;
  }

  VulkanBufferResource* buffer = state_->buffers.Resolve(handle);
  if (buffer == nullptr) {
    return RHIResult::ErrorInvalidHandle;
  }
  if (data == nullptr || size == 0 || offset > buffer->desc.size ||
      size > buffer->desc.size - offset) {
    return RHIResult::ErrorValidation;
  }

  RHIResult result = RHIResult::Success;
  if (buffer->desc.memory != ERHIMemoryType::DeviceLocal) {
    RHIMappedRange mapped_range{};
    result = MapBuffer(handle, mapped_range);
    if (result != RHIResult::Success || mapped_range.data == nullptr) {
      return result != RHIResult::Success ? result : RHIResult::ErrorValidation;
    }

    std::memcpy(static_cast<std::uint8_t*>(mapped_range.data) + offset, data, size);
    FlushMappedAllocationIfNeeded(*state_, *buffer, offset, size);
    UnmapBuffer(handle);
  } else {
    RHIBufferDesc staging_desc{};
    staging_desc.size = size;
    staging_desc.usage = ERHIBufferUsage::TransferSrc;
    staging_desc.memory = ERHIMemoryType::Staging;
    staging_desc.mapped_at_creation = true;

    const RHIBufferCreateResult staging_result = CreateVulkanBuffer(*state_, staging_desc);
    if (!staging_result.Succeeded()) {
      return staging_result.result;
    }

    VulkanBufferResource* staging = state_->buffers.Resolve(staging_result.handle);
    if (staging == nullptr || staging->mapped_data == nullptr) {
      state_->buffers.Destroy(staging_result.handle, [&](VulkanBufferResource& resource) {
        DestroyVulkanBuffer(*state_, resource);
      });
      return RHIResult::ErrorValidation;
    }

    std::memcpy(staging->mapped_data, data, size);
    FlushMappedAllocationIfNeeded(*state_, *staging, 0, size);

    result = ExecuteImmediate(*state_, [&](VkCommandBuffer command_buffer) {
      VkBufferCopy copy{};
      copy.srcOffset = 0;
      copy.dstOffset = offset;
      copy.size = size;
      vkCmdCopyBuffer(command_buffer, staging->buffer, buffer->buffer, 1, &copy);
    });

    state_->buffers.Destroy(staging_result.handle, [&](VulkanBufferResource& resource) {
      DestroyVulkanBuffer(*state_, resource);
    });
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] UpdateBuffer handle=" << handle.id
                               << " offset=" << offset << " size=" << size
                               << " result=" << ToString(result);
  }

  return result;
}

RHIResult VulkanRHIContext::UpdateTexture(RHITextureHandle handle,
                                          const RHITextureUpdateDesc& desc) {
  if (state_ == nullptr) {
    return RHIResult::ErrorValidation;
  }

  VulkanTextureResource* texture = state_->textures.Resolve(handle);
  if (texture == nullptr) {
    return RHIResult::ErrorInvalidHandle;
  }
  if (desc.data == nullptr || desc.data_size == 0) {
    return RHIResult::ErrorValidation;
  }

  RHIBufferDesc staging_desc{};
  staging_desc.size = desc.data_size;
  staging_desc.usage = ERHIBufferUsage::TransferSrc;
  staging_desc.memory = ERHIMemoryType::Staging;
  staging_desc.mapped_at_creation = true;

  const RHIBufferCreateResult staging_result = CreateVulkanBuffer(*state_, staging_desc);
  if (!staging_result.Succeeded()) {
    return staging_result.result;
  }

  VulkanBufferResource* staging = state_->buffers.Resolve(staging_result.handle);
  if (staging == nullptr || staging->mapped_data == nullptr) {
    state_->buffers.Destroy(staging_result.handle, [&](VulkanBufferResource& resource) {
      DestroyVulkanBuffer(*state_, resource);
    });
    return RHIResult::ErrorValidation;
  }

  std::memcpy(staging->mapped_data, desc.data, static_cast<std::size_t>(desc.data_size));
  FlushMappedAllocationIfNeeded(*state_, *staging, 0,
                                static_cast<std::size_t>(desc.data_size));
  const ERHITextureLayout previous_layout = texture->layout;

  const RHIResult result = ExecuteImmediate(*state_, [&](VkCommandBuffer command_buffer) {
    VkImageMemoryBarrier2 to_transfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    to_transfer.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    to_transfer.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    to_transfer.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    to_transfer.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    to_transfer.oldLayout = ToVkImageLayout(previous_layout);
    to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_transfer.image = texture->image;
    to_transfer.subresourceRange.aspectMask = ToVkImageAspect(desc.aspect);
    to_transfer.subresourceRange.baseMipLevel = desc.mip_level;
    to_transfer.subresourceRange.levelCount = 1;
    to_transfer.subresourceRange.baseArrayLayer = desc.base_array_layer;
    to_transfer.subresourceRange.layerCount = desc.layer_count;

    VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &to_transfer;
    vkCmdPipelineBarrier2(command_buffer, &dependency);

    VkBufferImageCopy copy{};
    copy.bufferOffset = 0;
    copy.bufferRowLength = desc.row_pitch;
    copy.bufferImageHeight = desc.slice_pitch;
    copy.imageSubresource.aspectMask = ToVkImageAspect(desc.aspect);
    copy.imageSubresource.mipLevel = desc.mip_level;
    copy.imageSubresource.baseArrayLayer = desc.base_array_layer;
    copy.imageSubresource.layerCount = desc.layer_count;
    copy.imageExtent = {desc.width, desc.height, desc.depth};
    vkCmdCopyBufferToImage(command_buffer, staging->buffer, texture->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    VkImageMemoryBarrier2 to_final{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    to_final.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    to_final.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    to_final.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    to_final.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    to_final.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_final.newLayout = ToVkImageLayout(previous_layout);
    to_final.image = texture->image;
    to_final.subresourceRange = to_transfer.subresourceRange;
    dependency.pImageMemoryBarriers = &to_final;
    vkCmdPipelineBarrier2(command_buffer, &dependency);
  });

  state_->buffers.Destroy(staging_result.handle, [&](VulkanBufferResource& resource) {
    DestroyVulkanBuffer(*state_, resource);
  });

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] UpdateTexture handle=" << handle.id
                               << " bytes=" << desc.data_size
                               << " result=" << ToString(result);
  }

  return result;
}

RHIResult VulkanRHIContext::MapBuffer(RHIBufferHandle handle, RHIMappedRange& out_range) {
  out_range = {};
  if (state_ == nullptr) {
    return RHIResult::ErrorValidation;
  }

  VulkanBufferResource* buffer = state_->buffers.Resolve(handle);
  if (buffer == nullptr) {
    return RHIResult::ErrorInvalidHandle;
  }
  if (buffer->desc.memory == ERHIMemoryType::DeviceLocal ||
      buffer->allocation == VK_NULL_HANDLE) {
    return RHIResult::ErrorUnsupported;
  }

  if (buffer->mapped_data == nullptr) {
    const VkResult map_result =
        vmaMapMemory(state_->allocator, buffer->allocation, &buffer->mapped_data);
    if (map_result != VK_SUCCESS) {
      return ToRHIResult(map_result);
    }
  }

  buffer->is_mapped = true;
  out_range.data = buffer->mapped_data;
  out_range.offset = 0;
  out_range.size = static_cast<std::size_t>(buffer->desc.size);

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] MapBuffer handle=" << handle.id
                               << " size=" << out_range.size
                               << " persistent=" << buffer->persistent_mapping;
  }

  return RHIResult::Success;
}

void VulkanRHIContext::UnmapBuffer(RHIBufferHandle handle) {
  if (state_ == nullptr) {
    return;
  }

  VulkanBufferResource* buffer = state_->buffers.Resolve(handle);
  if (buffer == nullptr || buffer->allocation == VK_NULL_HANDLE || !buffer->is_mapped) {
    return;
  }

  FlushMappedAllocationIfNeeded(*state_, *buffer, 0, VK_WHOLE_SIZE);
  if (!buffer->persistent_mapping) {
    vmaUnmapMemory(state_->allocator, buffer->allocation);
    buffer->mapped_data = nullptr;
    buffer->is_mapped = false;
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] UnmapBuffer handle=" << handle.id
                               << " persistent=" << buffer->persistent_mapping;
  }
}

IRHICommandList* VulkanRHIContext::GetCommandList(ERHIQueueType queue) {
  if (state_ == nullptr) {
    return nullptr;
  }

  FrameResources& frame = state_->frames[state_->current_frame];
  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] GetCommandList queue="
                               << static_cast<int>(queue) << " current_frame="
                               << state_->current_frame;
  }
  return frame.command_lists[GetQueueSlot(queue)].get();
}

void VulkanRHIContext::SubmitCommandLists(std::span<IRHICommandList*> lists,
                                          RHIFenceHandle signal_fence) {
  if (state_ == nullptr || lists.empty()) {
    return;
  }

  FrameResources& frame = state_->frames[state_->current_frame];
  const std::size_t available_submission_slots =
      frame.submission_semaphores.size() -
      std::min<std::size_t>(frame.submission_count, frame.submission_semaphores.size());

  std::vector<VulkanCommandList*> valid_lists;
  valid_lists.reserve(std::min(lists.size(), available_submission_slots));
  for (IRHICommandList* list : lists) {
    if (list == nullptr || valid_lists.size() >= available_submission_slots) {
      continue;
    }
    valid_lists.push_back(static_cast<VulkanCommandList*>(list));
  }

  if (valid_lists.empty()) {
    if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
      FABRICA_LOG(Render, Error)
          << "[RHI] SubmitCommandLists dropped submission because no submission slots remain";
    }
    return;
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] SubmitCommandLists count="
                               << valid_lists.size() << " fence=" << signal_fence.id
                               << " submission_base=" << frame.submission_count;
  }

  VkSemaphore chain_wait = frame.last_submission_semaphore;
  VulkanCommandList* last_list = nullptr;

  for (std::size_t index = 0; index < valid_lists.size(); ++index) {
    VulkanCommandList* list = valid_lists[index];
    last_list = list;

    VkCommandBufferSubmitInfo command_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    command_info.commandBuffer = list->GetNativeCommandBuffer();

    std::array<VkSemaphoreSubmitInfo, 2> waits{};
    std::uint32_t wait_count = 0;
    if (chain_wait != VK_NULL_HANDLE) {
      waits[wait_count].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
      waits[wait_count].semaphore = chain_wait;
      waits[wait_count].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      ++wait_count;
    }
    if (!frame.image_wait_consumed && frame.image_acquired &&
        list->GetQueueType() == ERHIQueueType::Graphics) {
      waits[wait_count].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
      waits[wait_count].semaphore = frame.image_available;
      waits[wait_count].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      ++wait_count;
      frame.image_wait_consumed = true;
    }

    std::array<VkSemaphoreSubmitInfo, 2> signals{};
    std::uint32_t signal_count = 0;
    const VkSemaphore batch_signal = frame.submission_semaphores[frame.submission_count++];
    signals[signal_count].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signals[signal_count].semaphore = batch_signal;
    signals[signal_count].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    ++signal_count;

    if (signal_fence.IsValid() && index + 1u == valid_lists.size()) {
      signals[signal_count].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
      signals[signal_count].semaphore = frame.user_fence_semaphore;
      signals[signal_count].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      ++signal_count;
    }

    VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submit.waitSemaphoreInfoCount = wait_count;
    submit.pWaitSemaphoreInfos = wait_count > 0 ? waits.data() : nullptr;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &command_info;
    submit.signalSemaphoreInfoCount = signal_count;
    submit.pSignalSemaphoreInfos = signal_count > 0 ? signals.data() : nullptr;

    vkQueueSubmit2(GetQueue(*state_, list->GetQueueType()), 1, &submit, VK_NULL_HANDLE);
    chain_wait = batch_signal;
  }

  if (signal_fence.IsValid() && last_list != nullptr) {
    VulkanFenceResource* fence = state_->fences.Resolve(signal_fence);
    if (fence != nullptr) {
      VkSemaphoreSubmitInfo wait_info{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
      wait_info.semaphore = frame.user_fence_semaphore;
      wait_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

      VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
      submit.waitSemaphoreInfoCount = 1;
      submit.pWaitSemaphoreInfos = &wait_info;
      vkResetFences(state_->device, 1, &fence->fence);
      vkQueueSubmit2(GetQueue(*state_, last_list->GetQueueType()), 1, &submit,
                     fence->fence);
    }
  }

  frame.last_submission_semaphore = chain_wait;
  frame.work_submitted = true;
  frame.last_signaled_fence = signal_fence;
}
RHIFenceCreateResult VulkanRHIContext::CreateFence(bool signaled) {
  if (state_ == nullptr) {
    return {.result = RHIResult::ErrorValidation};
  }

  const RHIFenceCreateResult result = CreateVulkanFence(*state_, signaled);
  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] CreateFence signaled=" << signaled
                               << " handle=" << result.handle.id
                               << " result=" << ToString(result.result);
  }
  return result;
}

void VulkanRHIContext::WaitFence(RHIFenceHandle handle) {
  if (state_ == nullptr) {
    return;
  }

  const VulkanFenceResource* fence = state_->fences.Resolve(handle);
  if (fence == nullptr) {
    return;
  }

  vkWaitForFences(state_->device, 1, &fence->fence, VK_TRUE,
                  std::numeric_limits<std::uint64_t>::max());

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] WaitFence handle=" << handle.id;
  }
}

void VulkanRHIContext::DestroyFence(RHIFenceHandle handle) {
  if (state_ == nullptr || !handle.IsValid()) {
    return;
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] DestroyFence handle=" << handle.id
                               << " deferred=false";
  }

  state_->fences.Destroy(handle, [&](VulkanFenceResource& resource) {
    DestroyVulkanFence(*state_, resource);
  });
}

IRHISwapChain* VulkanRHIContext::GetSwapChain() {
  return state_ != nullptr ? state_->swap_chain.get() : nullptr;
}

void VulkanRHIContext::Destroy(RHIBufferHandle handle) {
  if (state_ == nullptr || !handle.IsValid()) {
    return;
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] DestroyBuffer handle=" << handle.id
                               << " deferred=true";
  }
  state_->frames[state_->current_frame].deferred_buffers.push_back(handle);
}

void VulkanRHIContext::Destroy(RHITextureHandle handle) {
  if (state_ == nullptr || !handle.IsValid()) {
    return;
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] DestroyTexture handle=" << handle.id
                               << " deferred=true";
  }
  state_->frames[state_->current_frame].deferred_textures.push_back(handle);
}

void VulkanRHIContext::Destroy(RHISamplerHandle handle) {
  if (state_ == nullptr || !handle.IsValid()) {
    return;
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] DestroySampler handle=" << handle.id
                               << " deferred=true";
  }
  state_->frames[state_->current_frame].deferred_samplers.push_back(handle);
}

void VulkanRHIContext::Destroy(RHIShaderHandle handle) {
  if (state_ == nullptr || !handle.IsValid()) {
    return;
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] DestroyShader handle=" << handle.id
                               << " deferred=true";
  }
  state_->frames[state_->current_frame].deferred_shaders.push_back(handle);
}

void VulkanRHIContext::Destroy(RHIRenderPassHandle handle) {
  if (state_ == nullptr || !handle.IsValid()) {
    return;
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] DestroyRenderPass handle=" << handle.id
                               << " deferred=true";
  }
  state_->frames[state_->current_frame].deferred_render_passes.push_back(handle);
}

void VulkanRHIContext::Destroy(RHIPipelineHandle handle) {
  if (state_ == nullptr || !handle.IsValid()) {
    return;
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] DestroyPipeline handle=" << handle.id
                               << " deferred=true";
  }
  state_->frames[state_->current_frame].deferred_pipelines.push_back(handle);
}

void VulkanRHIContext::Destroy(RHIDescriptorSetHandle handle) {
  if (state_ == nullptr || !handle.IsValid()) {
    return;
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] DestroyDescriptorSet handle=" << handle.id
                               << " deferred=true";
  }
  state_->frames[state_->current_frame].deferred_descriptor_sets.push_back(handle);
}

void VulkanRHIContext::SetDebugName(RHIBufferHandle handle, std::string_view name) {
  if (state_ == nullptr) {
    return;
  }

  const VulkanBufferResource* buffer = state_->buffers.Resolve(handle);
  if (buffer != nullptr) {
    SetVulkanObjectDebugName(*state_, ToDebugHandleValue(buffer->buffer),
                             VK_OBJECT_TYPE_BUFFER, name);
  }
}

void VulkanRHIContext::SetDebugName(RHITextureHandle handle, std::string_view name) {
  if (state_ == nullptr) {
    return;
  }

  const VulkanTextureResource* texture = state_->textures.Resolve(handle);
  if (texture != nullptr) {
    SetVulkanObjectDebugName(*state_, ToDebugHandleValue(texture->image),
                             VK_OBJECT_TYPE_IMAGE, name);
  }
}

const RHIDeviceInfo& VulkanRHIContext::GetDeviceInfo() const {
  static const RHIDeviceInfo kEmptyDeviceInfo{};
  return state_ != nullptr ? state_->device_info : kEmptyDeviceInfo;
}

bool VulkanRHIContext::IsValid(RHIBufferHandle handle) const {
  return state_ != nullptr && state_->buffers.IsValid(handle);
}

bool VulkanRHIContext::IsValid(RHITextureHandle handle) const {
  return state_ != nullptr && state_->textures.IsValid(handle);
}

bool VulkanRHIContext::IsValid(RHISamplerHandle handle) const {
  return state_ != nullptr && state_->samplers.IsValid(handle);
}

bool VulkanRHIContext::IsValid(RHIShaderHandle handle) const {
  return state_ != nullptr && state_->shaders.IsValid(handle);
}

bool VulkanRHIContext::IsValid(RHIRenderPassHandle handle) const {
  return state_ != nullptr && state_->render_passes.IsValid(handle);
}

bool VulkanRHIContext::IsValid(RHIPipelineHandle handle) const {
  return state_ != nullptr && state_->pipelines.IsValid(handle);
}

bool VulkanRHIContext::IsValid(RHIDescriptorSetHandle handle) const {
  return state_ != nullptr && state_->descriptor_sets.IsValid(handle);
}

bool VulkanRHIContext::IsValid(RHIFenceHandle handle) const {
  return state_ != nullptr && state_->fences.IsValid(handle);
}

const IRHIBuffer* VulkanRHIContext::GetBuffer(RHIBufferHandle handle) const {
  return state_ != nullptr ? state_->buffers.Resolve(handle) : nullptr;
}

const IRHITexture* VulkanRHIContext::GetTexture(RHITextureHandle handle) const {
  return state_ != nullptr ? state_->textures.Resolve(handle) : nullptr;
}

const IRHISampler* VulkanRHIContext::GetSampler(RHISamplerHandle handle) const {
  return state_ != nullptr ? state_->samplers.Resolve(handle) : nullptr;
}

const IRHIShader* VulkanRHIContext::GetShader(RHIShaderHandle handle) const {
  return state_ != nullptr ? state_->shaders.Resolve(handle) : nullptr;
}

const IRHIRenderPass* VulkanRHIContext::GetRenderPass(
    RHIRenderPassHandle handle) const {
  return state_ != nullptr ? state_->render_passes.Resolve(handle) : nullptr;
}

const IRHIPipeline* VulkanRHIContext::GetPipeline(RHIPipelineHandle handle) const {
  return state_ != nullptr ? state_->pipelines.Resolve(handle) : nullptr;
}

IRHIDescriptorSet* VulkanRHIContext::GetDescriptorSet(RHIDescriptorSetHandle handle) {
  return state_ != nullptr ? state_->descriptor_sets.Resolve(handle) : nullptr;
}

const IRHIDescriptorSet* VulkanRHIContext::GetDescriptorSet(
    RHIDescriptorSetHandle handle) const {
  return state_ != nullptr ? state_->descriptor_sets.Resolve(handle) : nullptr;
}

const IRHIFence* VulkanRHIContext::GetFence(RHIFenceHandle handle) const {
  return state_ != nullptr ? state_->fences.Resolve(handle) : nullptr;
}

}  // namespace Fabrica::RHI::Vulkan



