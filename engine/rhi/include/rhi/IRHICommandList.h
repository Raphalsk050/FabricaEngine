#pragma once

#include <span>
#include <string_view>

#include "rhi/RHITypes.h"

namespace Fabrica::RHI {

class IRHICommandList {
 public:
  virtual ~IRHICommandList() = default;

  virtual void Begin() = 0;
  virtual void End() = 0;
  virtual void Reset() = 0;

  virtual void BeginRenderPass(RHIRenderPassHandle render_pass,
                               const RHIRenderPassBeginDesc& desc) = 0;
  virtual void EndRenderPass() = 0;

  virtual void SetViewport(const RHIViewport& viewport) = 0;
  virtual void SetScissor(const RHIRect& rect) = 0;
  virtual void BindPipeline(RHIPipelineHandle pipeline) = 0;
  virtual void BindVertexBuffers(std::uint32_t first_binding,
                                 std::span<RHIBufferHandle> buffers,
                                 std::span<std::uint64_t> offsets) = 0;
  virtual void BindIndexBuffer(RHIBufferHandle buffer,
                               std::uint64_t offset,
                               ERHIIndexType type) = 0;
  virtual void BindDescriptorSet(RHIDescriptorSetHandle set,
                                 std::uint32_t set_index) = 0;
  virtual void PushConstants(const void* data,
                             std::uint32_t size,
                             std::uint32_t offset,
                             ERHIShaderStage stage) = 0;
  virtual void Draw(std::uint32_t vertex_count,
                    std::uint32_t instance_count,
                    std::uint32_t first_vertex,
                    std::uint32_t first_instance) = 0;
  virtual void DrawIndexed(std::uint32_t index_count,
                           std::uint32_t instance_count,
                           std::uint32_t first_index,
                           std::int32_t vertex_offset,
                           std::uint32_t first_instance) = 0;
  virtual void Dispatch(std::uint32_t group_count_x,
                        std::uint32_t group_count_y,
                        std::uint32_t group_count_z) = 0;

  virtual void TransitionTexture(RHITextureHandle texture,
                                 ERHITextureLayout from,
                                 ERHITextureLayout to,
                                 ERHIPipelineStage src_stage,
                                 ERHIPipelineStage dst_stage,
                                 ERHIAccessFlags src_access,
                                 ERHIAccessFlags dst_access) = 0;
  virtual void TransitionBuffer(RHIBufferHandle buffer,
                                ERHIPipelineStage src_stage,
                                ERHIPipelineStage dst_stage,
                                ERHIAccessFlags src_access,
                                ERHIAccessFlags dst_access) = 0;

  virtual void CopyBuffer(RHIBufferHandle source,
                          RHIBufferHandle destination,
                          std::span<RHIBufferCopyRegion> regions) = 0;
  virtual void CopyBufferToTexture(
      RHIBufferHandle source,
      RHITextureHandle destination,
      std::span<RHIBufferTextureCopyRegion> regions) = 0;

  virtual void BeginDebugRegion(std::string_view name) = 0;
  virtual void EndDebugRegion() = 0;
  virtual void InsertDebugLabel(std::string_view name) = 0;
};

}  // namespace Fabrica::RHI