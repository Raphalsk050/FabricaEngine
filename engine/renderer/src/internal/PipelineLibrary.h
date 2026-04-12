#pragma once

#include <array>

#include "core/common/status.h"
#include "renderer/Renderer.h"
#include "rhi/IRHIContext.h"
#include "internal/ShaderLibrary.h"

namespace Fabrica::Renderer::Internal {

class PipelineLibrary final {
 public:
  [[nodiscard]] Fabrica::Core::Status Initialize(RHI::IRHIContext& rhi,
                                                 const RendererConfig& config,
                                                 const ShaderLibrary& shaders);
  void Shutdown();

  [[nodiscard]] Fabrica::Core::Status UpdateToneMappingBindings(
      std::uint32_t frame_slot,
      RHI::RHITextureHandle hdr_texture,
      RHI::RHISamplerHandle sampler);

  [[nodiscard]] RHI::RHIRenderPassHandle GetHdrRenderPass() const { return hdr_render_pass_; }
  [[nodiscard]] RHI::RHIRenderPassHandle GetPresentRenderPass() const {
    return present_render_pass_;
  }
  [[nodiscard]] RHI::RHIPipelineHandle GetPbrPipeline() const { return pbr_pipeline_; }
  [[nodiscard]] RHI::RHIPipelineHandle GetToneMappingPipeline() const {
    return tonemapping_pipeline_;
  }
  [[nodiscard]] RHI::RHIDescriptorSetHandle GetToneMappingDescriptorSet(
      std::uint32_t frame_slot) const;

 private:
  RHI::IRHIContext* rhi_ = nullptr;
  RHI::RHIRenderPassHandle hdr_render_pass_{};
  RHI::RHIRenderPassHandle present_render_pass_{};
  RHI::RHIPipelineHandle pbr_pipeline_{};
  RHI::RHIPipelineHandle tonemapping_pipeline_{};
  std::array<RHI::RHIDescriptorSetHandle, RHI::kFramesInFlight> tonemapping_descriptor_sets_{};
};

}  // namespace Fabrica::Renderer::Internal
