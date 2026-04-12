#include "internal/FrameResources.h"

#include <cstring>

#include "internal/RendererBindings.h"
#include "internal/RendererUtils.h"

namespace Fabrica::Renderer::Internal {

Fabrica::Core::Status FrameResources::Initialize(RHI::IRHIContext& rhi) {
  rhi_ = &rhi;

  RHI::RHISamplerDesc sampler_desc{};
  sampler_desc.min_filter = RHI::ERHIFilter::Linear;
  sampler_desc.mag_filter = RHI::ERHIFilter::Linear;
  sampler_desc.mip_filter = RHI::ERHIFilter::Linear;
  sampler_desc.address_u = RHI::ERHISamplerAddressMode::ClampToEdge;
  sampler_desc.address_v = RHI::ERHISamplerAddressMode::ClampToEdge;
  sampler_desc.address_w = RHI::ERHISamplerAddressMode::ClampToEdge;

  const RHI::RHISamplerCreateResult sampler_result = rhi.CreateSampler(sampler_desc);
  Fabrica::Core::Status status = ToStatus(sampler_result.result, "Create linear sampler");
  if (!status.ok()) {
    Shutdown();
    return status;
  }
  linear_sampler_ = sampler_result.handle;

  for (std::uint32_t frame_slot = 0; frame_slot < frame_slots_.size(); ++frame_slot) {
    RHI::RHIBufferDesc frame_buffer_desc{};
    frame_buffer_desc.size = sizeof(FrameUniforms);
    frame_buffer_desc.usage = RHI::ERHIBufferUsage::UniformBuffer;
    frame_buffer_desc.memory = RHI::ERHIMemoryType::HostCoherent;
    frame_buffer_desc.mapped_at_creation = true;

    const RHI::RHIBufferCreateResult frame_buffer_result = rhi.CreateBuffer(frame_buffer_desc);
    status = ToStatus(frame_buffer_result.result, "Create frame uniform buffer");
    if (!status.ok()) {
      Shutdown();
      return status;
    }
    frame_slots_[frame_slot].uniform_buffer = frame_buffer_result.handle;

    RHI::RHIDescriptorSetDesc frame_descriptor_desc{};
    frame_descriptor_desc.layout = CreateFrameSetLayoutDesc();
    frame_descriptor_desc.frequency = RHI::ERHIDescriptorFrequency::Persistent;

    const RHI::RHIDescriptorSetCreateResult descriptor_result =
        rhi.CreateDescriptorSet(frame_descriptor_desc);
    status = ToStatus(descriptor_result.result, "Create frame descriptor set");
    if (!status.ok()) {
      Shutdown();
      return status;
    }
    frame_slots_[frame_slot].descriptor_set = descriptor_result.handle;

    RHI::IRHIDescriptorSet* descriptor_set =
        rhi.GetDescriptorSet(frame_slots_[frame_slot].descriptor_set);
    if (descriptor_set == nullptr) {
      Shutdown();
      return Fabrica::Core::Status::Internal("Frame descriptor set could not be resolved");
    }

    descriptor_set->BindBuffer(0, {.buffer = frame_slots_[frame_slot].uniform_buffer,
                                   .offset = 0,
                                   .range = sizeof(FrameUniforms)});
    descriptor_set->FlushWrites();
  }

  return Fabrica::Core::Status::Ok();
}

void FrameResources::Shutdown() {
  if (rhi_ != nullptr) {
    for (FrameSlotResources& frame_slot : frame_slots_) {
      if (frame_slot.descriptor_set.IsValid()) {
        rhi_->Destroy(frame_slot.descriptor_set);
      }
      if (frame_slot.uniform_buffer.IsValid()) {
        rhi_->Destroy(frame_slot.uniform_buffer);
      }
      frame_slot = {};
    }

    if (linear_sampler_.IsValid()) {
      rhi_->Destroy(linear_sampler_);
    }
  }

  linear_sampler_ = {};
  rhi_ = nullptr;
}

Fabrica::Core::Status FrameResources::UpdateFrameUniforms(std::uint32_t frame_slot,
                                                          const CameraData& camera,
                                                          const DirectionalLight& light) {
  if (frame_slot >= frame_slots_.size()) {
    return Fabrica::Core::Status::InvalidArgument("Frame slot is out of range");
  }

  FrameUniforms frame_uniforms{};
  frame_uniforms.view_projection = camera.view_projection;
  frame_uniforms.camera_position_exposure = {
      camera.position.x,
      camera.position.y,
      camera.position.z,
      camera.settings.ComputeExposure()};

  const Math::Vec3f light_direction = NormalizeOrDefault(light.direction, {0.0f, -1.0f, 0.0f});
  frame_uniforms.directional_light_direction_intensity = {
      light_direction.x,
      light_direction.y,
      light_direction.z,
      light.illuminance_lux * camera.settings.ComputeExposure()};
  frame_uniforms.directional_light_color = {light.color.x, light.color.y, light.color.z, 1.0f};

  RHI::RHIMappedRange mapped_range{};
  const RHI::RHIResult map_result =
      rhi_->MapBuffer(frame_slots_[frame_slot].uniform_buffer, mapped_range);
  if (map_result != RHI::RHIResult::Success || mapped_range.data == nullptr) {
    return ToStatus(map_result, "Map frame uniform buffer");
  }

  std::memcpy(mapped_range.data, &frame_uniforms, sizeof(frame_uniforms));
  rhi_->UnmapBuffer(frame_slots_[frame_slot].uniform_buffer);
  return Fabrica::Core::Status::Ok();
}

RHI::RHIBufferHandle FrameResources::GetFrameUniformBuffer(std::uint32_t frame_slot) const {
  return frame_slot < frame_slots_.size() ? frame_slots_[frame_slot].uniform_buffer
                                          : RHI::RHIBufferHandle{};
}

RHI::RHIDescriptorSetHandle FrameResources::GetFrameDescriptorSet(
    std::uint32_t frame_slot) const {
  return frame_slot < frame_slots_.size() ? frame_slots_[frame_slot].descriptor_set
                                          : RHI::RHIDescriptorSetHandle{};
}

}  // namespace Fabrica::Renderer::Internal
