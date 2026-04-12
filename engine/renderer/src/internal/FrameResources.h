#pragma once

#include <array>

#include "core/common/status.h"
#include "renderer/CameraData.h"
#include "renderer/LightTypes.h"
#include "rhi/IRHIContext.h"
#include "internal/FrameData.h"

namespace Fabrica::Renderer::Internal {

class FrameResources final {
 public:
  [[nodiscard]] Fabrica::Core::Status Initialize(RHI::IRHIContext& rhi);
  void Shutdown();

  [[nodiscard]] Fabrica::Core::Status UpdateFrameUniforms(std::uint32_t frame_slot,
                                                          const CameraData& camera,
                                                          const DirectionalLight& light);

  [[nodiscard]] RHI::RHIBufferHandle GetFrameUniformBuffer(std::uint32_t frame_slot) const;
  [[nodiscard]] RHI::RHISamplerHandle GetLinearSampler() const { return linear_sampler_; }
  [[nodiscard]] RHI::RHIDescriptorSetHandle GetFrameDescriptorSet(
      std::uint32_t frame_slot) const;

 private:
  struct FrameSlotResources {
    RHI::RHIBufferHandle uniform_buffer{};
    RHI::RHIDescriptorSetHandle descriptor_set{};
  };

  RHI::IRHIContext* rhi_ = nullptr;
  RHI::RHISamplerHandle linear_sampler_{};
  std::array<FrameSlotResources, RHI::kFramesInFlight> frame_slots_{};
};

}  // namespace Fabrica::Renderer::Internal
