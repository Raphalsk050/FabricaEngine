#include <algorithm>

#include "VulkanBackendTypes.h"

namespace Fabrica::RHI::Vulkan {

VulkanCommandList::VulkanCommandList(VulkanContextState& state,
                                     ERHIQueueType queue_type)
    : state_(&state), queue_type_(queue_type) {}

void VulkanCommandList::Attach(VkCommandBuffer command_buffer,
                               std::uint32_t frame_index) {
  command_buffer_ = command_buffer;
  frame_index_ = frame_index;
}

void VulkanCommandList::Begin() {
  if (command_buffer_ == VK_NULL_HANDLE) {
    return;
  }

  VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(command_buffer_, &begin_info);
  is_recording_ = true;
  inside_render_pass_ = false;
  current_pipeline_layout_ = VK_NULL_HANDLE;

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] CommandList Begin queue="
                               << static_cast<int>(queue_type_)
                               << " frame=" << frame_index_;
  }
}

void VulkanCommandList::End() {
  if (!is_recording_) {
    return;
  }
  if (inside_render_pass_) {
    EndRenderPass();
  }
  vkEndCommandBuffer(command_buffer_);
  is_recording_ = false;

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] CommandList End queue="
                               << static_cast<int>(queue_type_)
                               << " frame=" << frame_index_;
  }
}

void VulkanCommandList::Reset() {
  is_recording_ = false;
  inside_render_pass_ = false;
  current_pipeline_layout_ = VK_NULL_HANDLE;

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] CommandList Reset queue="
                               << static_cast<int>(queue_type_)
                               << " frame=" << frame_index_;
  }
}

void VulkanCommandList::BeginRenderPass(RHIRenderPassHandle render_pass,
                                        const RHIRenderPassBeginDesc& desc) {
  const VulkanRenderPassResource* pass = state_->render_passes.Resolve(render_pass);
  if (pass == nullptr) {
    return;
  }

  std::array<VkRenderingAttachmentInfo, kMaxColorAttachments> color_attachments{};
  for (std::uint32_t index = 0; index < desc.color_attachment_count; ++index) {
    const VulkanTextureResource* texture =
        state_->textures.Resolve(desc.color_attachments[index].texture);
    if (texture == nullptr) {
      continue;
    }

    VkRenderingAttachmentInfo& attachment = color_attachments[index];
    attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachment.imageView = texture->image_view;
    attachment.imageLayout = ToVkImageLayout(desc.color_attachments[index].layout);
    attachment.loadOp = desc.color_attachments[index].load_op == ERHILoadOp::Clear
                            ? VK_ATTACHMENT_LOAD_OP_CLEAR
                            : (desc.color_attachments[index].load_op == ERHILoadOp::Load
                                   ? VK_ATTACHMENT_LOAD_OP_LOAD
                                   : VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    attachment.storeOp = desc.color_attachments[index].store_op == ERHIStoreOp::Store
                             ? VK_ATTACHMENT_STORE_OP_STORE
                             : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.clearValue.color = {{desc.color_attachments[index].clear_value.r,
                                    desc.color_attachments[index].clear_value.g,
                                    desc.color_attachments[index].clear_value.b,
                                    desc.color_attachments[index].clear_value.a}};
  }

  VkRenderingAttachmentInfo depth_attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  VkRenderingAttachmentInfo* depth_attachment_ptr = nullptr;
  if (desc.has_depth_stencil_attachment) {
    const VulkanTextureResource* texture =
        state_->textures.Resolve(desc.depth_stencil_attachment.texture);
    if (texture != nullptr) {
      depth_attachment.imageView = texture->image_view;
      depth_attachment.imageLayout =
          ToVkImageLayout(desc.depth_stencil_attachment.layout);
      depth_attachment.loadOp =
          desc.depth_stencil_attachment.depth_load_op == ERHILoadOp::Clear
              ? VK_ATTACHMENT_LOAD_OP_CLEAR
              : (desc.depth_stencil_attachment.depth_load_op == ERHILoadOp::Load
                     ? VK_ATTACHMENT_LOAD_OP_LOAD
                     : VK_ATTACHMENT_LOAD_OP_DONT_CARE);
      depth_attachment.storeOp =
          desc.depth_stencil_attachment.depth_store_op == ERHIStoreOp::Store
              ? VK_ATTACHMENT_STORE_OP_STORE
              : VK_ATTACHMENT_STORE_OP_DONT_CARE;
      depth_attachment.clearValue.depthStencil = {
          desc.depth_stencil_attachment.clear_value.depth,
          desc.depth_stencil_attachment.clear_value.stencil};
      depth_attachment_ptr = &depth_attachment;
    }
  }

  VkRenderingInfo rendering_info{VK_STRUCTURE_TYPE_RENDERING_INFO};
  rendering_info.renderArea.offset = {desc.render_area.x, desc.render_area.y};
  rendering_info.renderArea.extent = {desc.render_area.width, desc.render_area.height};
  rendering_info.layerCount = desc.layer_count;
  rendering_info.colorAttachmentCount = desc.color_attachment_count;
  rendering_info.pColorAttachments = color_attachments.data();
  rendering_info.pDepthAttachment = depth_attachment_ptr;
  rendering_info.pStencilAttachment =
      depth_attachment_ptr != nullptr && pass->desc.has_depth_stencil_attachment &&
              HasStencilComponent(pass->desc.depth_stencil_attachment.format)
          ? depth_attachment_ptr
          : nullptr;

  vkCmdBeginRendering(command_buffer_, &rendering_info);
  inside_render_pass_ = true;

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] BeginRenderPass handle=" << render_pass.id
                               << " colors=" << desc.color_attachment_count
                               << " has_depth=" << desc.has_depth_stencil_attachment;
  }
}

void VulkanCommandList::EndRenderPass() {
  if (!inside_render_pass_) {
    return;
  }
  vkCmdEndRendering(command_buffer_);
  inside_render_pass_ = false;

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] EndRenderPass queue="
                               << static_cast<int>(queue_type_)
                               << " frame=" << frame_index_;
  }
}

void VulkanCommandList::SetViewport(const RHIViewport& viewport) {
  const VkViewport native_viewport{viewport.x, viewport.y, viewport.width,
                                   viewport.height, viewport.min_depth,
                                   viewport.max_depth};
  vkCmdSetViewport(command_buffer_, 0, 1, &native_viewport);
}

void VulkanCommandList::SetScissor(const RHIRect& rect) {
  const VkRect2D native_rect{{rect.x, rect.y}, {rect.width, rect.height}};
  vkCmdSetScissor(command_buffer_, 0, 1, &native_rect);
}

void VulkanCommandList::BindPipeline(RHIPipelineHandle pipeline) {
  const VulkanPipelineResource* resource = state_->pipelines.Resolve(pipeline);
  if (resource == nullptr || resource->pipeline == VK_NULL_HANDLE) {
    return;
  }

  current_pipeline_layout_ = resource->pipeline_layout;
  vkCmdBindPipeline(command_buffer_,
                    resource->type == ERHIPipelineType::Compute
                        ? VK_PIPELINE_BIND_POINT_COMPUTE
                        : VK_PIPELINE_BIND_POINT_GRAPHICS,
                    resource->pipeline);

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] BindPipeline handle=" << pipeline.id
                               << " type=" << static_cast<int>(resource->type);
  }
}

void VulkanCommandList::BindVertexBuffers(std::uint32_t first_binding,
                                          std::span<RHIBufferHandle> buffers,
                                          std::span<std::uint64_t> offsets) {
  std::array<VkBuffer, kMaxVertexBindings> native_buffers{};
  std::array<VkDeviceSize, kMaxVertexBindings> native_offsets{};
  const std::uint32_t count =
      static_cast<std::uint32_t>(std::min(buffers.size(), offsets.size()));
  for (std::uint32_t index = 0; index < count; ++index) {
    const VulkanBufferResource* buffer = state_->buffers.Resolve(buffers[index]);
    if (buffer == nullptr) {
      return;
    }
    native_buffers[index] = buffer->buffer;
    native_offsets[index] = offsets[index];
  }

  vkCmdBindVertexBuffers(command_buffer_, first_binding, count, native_buffers.data(),
                         native_offsets.data());
}

void VulkanCommandList::BindIndexBuffer(RHIBufferHandle buffer,
                                        std::uint64_t offset,
                                        ERHIIndexType type) {
  const VulkanBufferResource* resource = state_->buffers.Resolve(buffer);
  if (resource == nullptr) {
    return;
  }

  vkCmdBindIndexBuffer(command_buffer_, resource->buffer, offset, ToVkIndexType(type));
}

void VulkanCommandList::BindDescriptorSet(RHIDescriptorSetHandle set,
                                          std::uint32_t set_index) {
  const VulkanDescriptorSetResource* resource = state_->descriptor_sets.Resolve(set);
  if (resource == nullptr || current_pipeline_layout_ == VK_NULL_HANDLE) {
    return;
  }

  const VkPipelineBindPoint bind_point =
      queue_type_ == ERHIQueueType::Compute ? VK_PIPELINE_BIND_POINT_COMPUTE
                                            : VK_PIPELINE_BIND_POINT_GRAPHICS;
  vkCmdBindDescriptorSets(command_buffer_, bind_point, current_pipeline_layout_, set_index,
                          1, &resource->descriptor_set, 0, nullptr);
}

void VulkanCommandList::PushConstants(const void* data,
                                      std::uint32_t size,
                                      std::uint32_t offset,
                                      ERHIShaderStage stage) {
  if (current_pipeline_layout_ == VK_NULL_HANDLE) {
    return;
  }

  vkCmdPushConstants(command_buffer_, current_pipeline_layout_, ToVkShaderStage(stage),
                     offset, size, data);
}

void VulkanCommandList::Draw(std::uint32_t vertex_count,
                             std::uint32_t instance_count,
                             std::uint32_t first_vertex,
                             std::uint32_t first_instance) {
  vkCmdDraw(command_buffer_, vertex_count, instance_count, first_vertex, first_instance);

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] Draw vertices=" << vertex_count
                               << " instances=" << instance_count;
  }
}

void VulkanCommandList::DrawIndexed(std::uint32_t index_count,
                                    std::uint32_t instance_count,
                                    std::uint32_t first_index,
                                    std::int32_t vertex_offset,
                                    std::uint32_t first_instance) {
  vkCmdDrawIndexed(command_buffer_, index_count, instance_count, first_index,
                   vertex_offset, first_instance);

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] DrawIndexed indices=" << index_count
                               << " instances=" << instance_count;
  }
}

void VulkanCommandList::Dispatch(std::uint32_t group_count_x,
                                 std::uint32_t group_count_y,
                                 std::uint32_t group_count_z) {
  vkCmdDispatch(command_buffer_, group_count_x, group_count_y, group_count_z);

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] Dispatch groups=" << group_count_x << ","
                               << group_count_y << "," << group_count_z;
  }
}

void VulkanCommandList::TransitionTexture(RHITextureHandle texture,
                                          ERHITextureLayout from,
                                          ERHITextureLayout to,
                                          ERHIPipelineStage src_stage,
                                          ERHIPipelineStage dst_stage,
                                          ERHIAccessFlags src_access,
                                          ERHIAccessFlags dst_access) {
  VulkanTextureResource* resource = state_->textures.Resolve(texture);
  if (resource == nullptr) {
    return;
  }

  VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  barrier.srcStageMask = ToVkPipelineStage(src_stage);
  barrier.srcAccessMask = ToVkAccessFlags(src_access);
  barrier.dstStageMask = ToVkPipelineStage(dst_stage);
  barrier.dstAccessMask = ToVkAccessFlags(dst_access);
  barrier.oldLayout = ToVkImageLayout(from);
  barrier.newLayout = ToVkImageLayout(to);
  barrier.image = resource->image;
  barrier.subresourceRange.aspectMask = ToVkImageAspect(resource->desc.aspect);
  barrier.subresourceRange.levelCount = resource->desc.mip_levels;
  barrier.subresourceRange.layerCount = resource->desc.array_layers;

  VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dependency.imageMemoryBarrierCount = 1;
  dependency.pImageMemoryBarriers = &barrier;
  vkCmdPipelineBarrier2(command_buffer_, &dependency);
  resource->layout = to;

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] TransitionTexture handle=" << texture.id
                               << " from=" << static_cast<int>(from)
                               << " to=" << static_cast<int>(to)
                               << " src_stage=" << static_cast<std::uint32_t>(src_stage)
                               << " dst_stage=" << static_cast<std::uint32_t>(dst_stage)
                               << " src_access=" << static_cast<std::uint32_t>(src_access)
                               << " dst_access=" << static_cast<std::uint32_t>(dst_access);
  }
}

void VulkanCommandList::TransitionBuffer(RHIBufferHandle buffer,
                                         ERHIPipelineStage src_stage,
                                         ERHIPipelineStage dst_stage,
                                         ERHIAccessFlags src_access,
                                         ERHIAccessFlags dst_access) {
  const VulkanBufferResource* resource = state_->buffers.Resolve(buffer);
  if (resource == nullptr) {
    return;
  }

  VkBufferMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
  barrier.srcStageMask = ToVkPipelineStage(src_stage);
  barrier.srcAccessMask = ToVkAccessFlags(src_access);
  barrier.dstStageMask = ToVkPipelineStage(dst_stage);
  barrier.dstAccessMask = ToVkAccessFlags(dst_access);
  barrier.buffer = resource->buffer;
  barrier.offset = 0;
  barrier.size = resource->desc.size;

  VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dependency.bufferMemoryBarrierCount = 1;
  dependency.pBufferMemoryBarriers = &barrier;
  vkCmdPipelineBarrier2(command_buffer_, &dependency);

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] TransitionBuffer handle=" << buffer.id
                               << " src_stage=" << static_cast<std::uint32_t>(src_stage)
                               << " dst_stage=" << static_cast<std::uint32_t>(dst_stage)
                               << " src_access=" << static_cast<std::uint32_t>(src_access)
                               << " dst_access=" << static_cast<std::uint32_t>(dst_access);
  }
}

void VulkanCommandList::CopyBuffer(RHIBufferHandle source,
                                   RHIBufferHandle destination,
                                   std::span<RHIBufferCopyRegion> regions) {
  const VulkanBufferResource* src = state_->buffers.Resolve(source);
  const VulkanBufferResource* dst = state_->buffers.Resolve(destination);
  if (src == nullptr || dst == nullptr || regions.empty()) {
    return;
  }

  std::vector<VkBufferCopy> copies(regions.size());
  for (std::size_t index = 0; index < regions.size(); ++index) {
    copies[index].srcOffset = regions[index].src_offset;
    copies[index].dstOffset = regions[index].dst_offset;
    copies[index].size = regions[index].size;
  }
  vkCmdCopyBuffer(command_buffer_, src->buffer, dst->buffer,
                  static_cast<std::uint32_t>(copies.size()), copies.data());

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] CopyBuffer src=" << source.id
                               << " dst=" << destination.id
                               << " regions=" << regions.size();
  }
}

void VulkanCommandList::CopyBufferToTexture(
    RHIBufferHandle source,
    RHITextureHandle destination,
    std::span<RHIBufferTextureCopyRegion> regions) {
  const VulkanBufferResource* src = state_->buffers.Resolve(source);
  VulkanTextureResource* dst = state_->textures.Resolve(destination);
  if (src == nullptr || dst == nullptr || regions.empty()) {
    return;
  }

  std::vector<VkBufferImageCopy> copies(regions.size());
  for (std::size_t index = 0; index < regions.size(); ++index) {
    copies[index].bufferOffset = regions[index].buffer_offset;
    copies[index].bufferRowLength = regions[index].buffer_row_length;
    copies[index].bufferImageHeight = regions[index].buffer_image_height;
    copies[index].imageSubresource.aspectMask = ToVkImageAspect(regions[index].aspect);
    copies[index].imageSubresource.mipLevel = regions[index].texture_mip_level;
    copies[index].imageSubresource.baseArrayLayer = regions[index].base_array_layer;
    copies[index].imageSubresource.layerCount = regions[index].layer_count;
    copies[index].imageOffset = {regions[index].texture_offset.x,
                                 regions[index].texture_offset.y,
                                 regions[index].texture_offset.z};
    copies[index].imageExtent = {regions[index].texture_extent.width,
                                 regions[index].texture_extent.height,
                                 regions[index].texture_extent.depth};
  }

  vkCmdCopyBufferToImage(command_buffer_, src->buffer, dst->image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         static_cast<std::uint32_t>(copies.size()), copies.data());
  dst->layout = ERHITextureLayout::TransferDst;

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[RHI] CopyBufferToTexture src=" << source.id
                               << " dst=" << destination.id
                               << " regions=" << regions.size();
  }
}

void VulkanCommandList::BeginDebugRegion(std::string_view name) {
  if (state_->cmd_begin_debug_label == nullptr) {
    return;
  }

  std::string label_name(name);
  VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
  label.pLabelName = label_name.c_str();
  state_->cmd_begin_debug_label(command_buffer_, &label);
}

void VulkanCommandList::EndDebugRegion() {
  if (state_->cmd_end_debug_label != nullptr) {
    state_->cmd_end_debug_label(command_buffer_);
  }
}

void VulkanCommandList::InsertDebugLabel(std::string_view name) {
  if (state_->cmd_insert_debug_label == nullptr) {
    return;
  }

  std::string label_name(name);
  VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
  label.pLabelName = label_name.c_str();
  state_->cmd_insert_debug_label(command_buffer_, &label);
}

}  // namespace Fabrica::RHI::Vulkan