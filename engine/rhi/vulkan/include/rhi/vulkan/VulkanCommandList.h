#pragma once

#include <span>
#include <string_view>

#include <Volk/volk.h>

#include "rhi/IRHICommandList.h"

namespace Fabrica::RHI::Vulkan {

struct VulkanContextState;

class VulkanCommandList final : public IRHICommandList {
 public:
  VulkanCommandList(VulkanContextState& state, ERHIQueueType queue_type);
  ~VulkanCommandList() override = default;

  void Attach(VkCommandBuffer command_buffer, std::uint32_t frame_index);
  [[nodiscard]] VkCommandBuffer GetNativeCommandBuffer() const {
    return command_buffer_;
  }
  [[nodiscard]] ERHIQueueType GetQueueType() const { return queue_type_; }
  [[nodiscard]] bool IsRecording() const { return is_recording_; }

  void Begin() override;
  void End() override;
  void Reset() override;
  void BeginRenderPass(RHIRenderPassHandle render_pass,
                       const RHIRenderPassBeginDesc& desc) override;
  void EndRenderPass() override;
  void SetViewport(const RHIViewport& viewport) override;
  void SetScissor(const RHIRect& rect) override;
  void BindPipeline(RHIPipelineHandle pipeline) override;
  void BindVertexBuffers(std::uint32_t first_binding,
                         std::span<RHIBufferHandle> buffers,
                         std::span<std::uint64_t> offsets) override;
  void BindIndexBuffer(RHIBufferHandle buffer,
                       std::uint64_t offset,
                       ERHIIndexType type) override;
  void BindDescriptorSet(RHIDescriptorSetHandle set, std::uint32_t set_index) override;
  void PushConstants(const void* data,
                     std::uint32_t size,
                     std::uint32_t offset,
                     ERHIShaderStage stage) override;
  void Draw(std::uint32_t vertex_count,
            std::uint32_t instance_count,
            std::uint32_t first_vertex,
            std::uint32_t first_instance) override;
  void DrawIndexed(std::uint32_t index_count,
                   std::uint32_t instance_count,
                   std::uint32_t first_index,
                   std::int32_t vertex_offset,
                   std::uint32_t first_instance) override;
  void Dispatch(std::uint32_t group_count_x,
                std::uint32_t group_count_y,
                std::uint32_t group_count_z) override;
  void TransitionTexture(RHITextureHandle texture,
                         ERHITextureLayout from,
                         ERHITextureLayout to,
                         ERHIPipelineStage src_stage,
                         ERHIPipelineStage dst_stage,
                         ERHIAccessFlags src_access,
                         ERHIAccessFlags dst_access) override;
  void TransitionBuffer(RHIBufferHandle buffer,
                        ERHIPipelineStage src_stage,
                        ERHIPipelineStage dst_stage,
                        ERHIAccessFlags src_access,
                        ERHIAccessFlags dst_access) override;
  void CopyBuffer(RHIBufferHandle source,
                  RHIBufferHandle destination,
                  std::span<RHIBufferCopyRegion> regions) override;
  void CopyBufferToTexture(RHIBufferHandle source,
                           RHITextureHandle destination,
                           std::span<RHIBufferTextureCopyRegion> regions) override;
  void BeginDebugRegion(std::string_view name) override;
  void EndDebugRegion() override;
  void InsertDebugLabel(std::string_view name) override;

 private:
  VulkanContextState* state_ = nullptr;
  VkCommandBuffer command_buffer_ = VK_NULL_HANDLE;
  VkPipelineLayout current_pipeline_layout_ = VK_NULL_HANDLE;
  ERHIQueueType queue_type_ = ERHIQueueType::Graphics;
  std::uint32_t frame_index_ = 0;
  bool is_recording_ = false;
  bool inside_render_pass_ = false;
};

}  // namespace Fabrica::RHI::Vulkan