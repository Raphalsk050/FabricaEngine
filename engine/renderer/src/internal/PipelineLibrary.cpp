#include "internal/PipelineLibrary.h"

#include "internal/FrameData.h"
#include "internal/RendererBindings.h"
#include "internal/RendererUtils.h"

namespace Fabrica::Renderer::Internal {

Fabrica::Core::Status PipelineLibrary::Initialize(RHI::IRHIContext& rhi,
                                                  const RendererConfig& config,
                                                  const ShaderLibrary& shaders) {
  rhi_ = &rhi;

  RHI::RHIRenderPassDesc hdr_pass_desc{};
  hdr_pass_desc.color_attachment_count = 1;
  hdr_pass_desc.color_attachments[0].format = config.hdr_color_format;
  hdr_pass_desc.color_attachments[0].initial_layout = RHI::ERHITextureLayout::ColorAttachment;
  hdr_pass_desc.color_attachments[0].final_layout = RHI::ERHITextureLayout::ShaderReadOnly;

  const RHI::RHIRenderPassCreateResult hdr_render_pass_result = rhi.CreateRenderPass(hdr_pass_desc);
  Fabrica::Core::Status status = ToStatus(hdr_render_pass_result.result,
                                          "Create HDR render pass");
  if (!status.ok()) {
    Shutdown();
    return status;
  }
  hdr_render_pass_ = hdr_render_pass_result.handle;

  RHI::RHIRenderPassDesc present_pass_desc{};
  present_pass_desc.color_attachment_count = 1;
  present_pass_desc.color_attachments[0].format = config.swapchain_color_format;
  present_pass_desc.color_attachments[0].initial_layout =
      RHI::ERHITextureLayout::ColorAttachment;
  present_pass_desc.color_attachments[0].final_layout = RHI::ERHITextureLayout::Present;

  const RHI::RHIRenderPassCreateResult present_render_pass_result =
      rhi.CreateRenderPass(present_pass_desc);
  status = ToStatus(present_render_pass_result.result, "Create present render pass");
  if (!status.ok()) {
    Shutdown();
    return status;
  }
  present_render_pass_ = present_render_pass_result.handle;

  RHI::RHIGraphicsPipelineDesc pbr_pipeline_desc{};
  pbr_pipeline_desc.vertex_shader = shaders.GetPbrVertexShader();
  pbr_pipeline_desc.fragment_shader = shaders.GetPbrFragmentShader();
  pbr_pipeline_desc.render_pass = hdr_render_pass_;
  pbr_pipeline_desc.vertex_input = CreatePbrVertexInputDesc();
  pbr_pipeline_desc.layout.set_layout_count = 1;
  pbr_pipeline_desc.layout.set_layouts[0] = CreateFrameSetLayoutDesc();
  pbr_pipeline_desc.layout.push_constant_range_count = 1;
  pbr_pipeline_desc.layout.push_constant_ranges[0] = CreateDrawPushConstantRange();
  pbr_pipeline_desc.blend.attachment_count = 1;
  pbr_pipeline_desc.blend.attachments[0].write_mask = RHI::ERHIColorComponent::All;
  pbr_pipeline_desc.rasterizer.cull_mode = RHI::ERHICullMode::Back;

  const RHI::RHIPipelineCreateResult pbr_pipeline_result =
      rhi.CreateGraphicsPipeline(pbr_pipeline_desc);
  status = ToStatus(pbr_pipeline_result.result, "Create PBR pipeline");
  if (!status.ok()) {
    Shutdown();
    return status;
  }
  pbr_pipeline_ = pbr_pipeline_result.handle;

  RHI::RHIGraphicsPipelineDesc tonemapping_pipeline_desc{};
  tonemapping_pipeline_desc.vertex_shader = shaders.GetFullscreenVertexShader();
  tonemapping_pipeline_desc.fragment_shader = shaders.GetToneMappingFragmentShader();
  tonemapping_pipeline_desc.render_pass = present_render_pass_;
  tonemapping_pipeline_desc.layout.set_layout_count = 1;
  tonemapping_pipeline_desc.layout.set_layouts[0] = CreateToneMappingSetLayoutDesc();
  tonemapping_pipeline_desc.blend.attachment_count = 1;
  tonemapping_pipeline_desc.blend.attachments[0].write_mask = RHI::ERHIColorComponent::All;
  tonemapping_pipeline_desc.rasterizer.cull_mode = RHI::ERHICullMode::None;

  const RHI::RHIPipelineCreateResult tonemapping_pipeline_result =
      rhi.CreateGraphicsPipeline(tonemapping_pipeline_desc);
  status = ToStatus(tonemapping_pipeline_result.result, "Create tonemapping pipeline");
  if (!status.ok()) {
    Shutdown();
    return status;
  }
  tonemapping_pipeline_ = tonemapping_pipeline_result.handle;

  for (std::uint32_t frame_slot = 0; frame_slot < tonemapping_descriptor_sets_.size(); ++frame_slot) {
    RHI::RHIDescriptorSetDesc tonemapping_descriptor_desc{};
    tonemapping_descriptor_desc.layout = CreateToneMappingSetLayoutDesc();
    tonemapping_descriptor_desc.frequency = RHI::ERHIDescriptorFrequency::Persistent;

    const RHI::RHIDescriptorSetCreateResult tonemapping_descriptor_result =
        rhi.CreateDescriptorSet(tonemapping_descriptor_desc);
    status = ToStatus(tonemapping_descriptor_result.result,
                      "Create tonemapping descriptor set");
    if (!status.ok()) {
      Shutdown();
      return status;
    }
    tonemapping_descriptor_sets_[frame_slot] = tonemapping_descriptor_result.handle;
  }

  return Fabrica::Core::Status::Ok();
}

void PipelineLibrary::Shutdown() {
  if (rhi_ != nullptr) {
    for (RHI::RHIDescriptorSetHandle& descriptor_set : tonemapping_descriptor_sets_) {
      if (descriptor_set.IsValid()) {
        rhi_->Destroy(descriptor_set);
      }
      descriptor_set = {};
    }
    if (tonemapping_pipeline_.IsValid()) {
      rhi_->Destroy(tonemapping_pipeline_);
    }
    if (pbr_pipeline_.IsValid()) {
      rhi_->Destroy(pbr_pipeline_);
    }
    if (present_render_pass_.IsValid()) {
      rhi_->Destroy(present_render_pass_);
    }
    if (hdr_render_pass_.IsValid()) {
      rhi_->Destroy(hdr_render_pass_);
    }
  }

  tonemapping_pipeline_ = {};
  pbr_pipeline_ = {};
  present_render_pass_ = {};
  hdr_render_pass_ = {};
  rhi_ = nullptr;
}

Fabrica::Core::Status PipelineLibrary::UpdateToneMappingBindings(
    std::uint32_t frame_slot,
    RHI::RHITextureHandle hdr_texture,
    RHI::RHISamplerHandle sampler) {
  if (frame_slot >= tonemapping_descriptor_sets_.size()) {
    return Fabrica::Core::Status::InvalidArgument("Frame slot is out of range");
  }

  RHI::IRHIDescriptorSet* descriptor_set =
      rhi_->GetDescriptorSet(tonemapping_descriptor_sets_[frame_slot]);
  if (descriptor_set == nullptr) {
    return Fabrica::Core::Status::Internal(
        "Tone mapping descriptor set could not be resolved");
  }

  descriptor_set->BindTexture(0, {.texture = hdr_texture,
                                  .layout = RHI::ERHITextureLayout::ShaderReadOnly});
  descriptor_set->BindSampler(1, {.sampler = sampler});
  descriptor_set->FlushWrites();
  return Fabrica::Core::Status::Ok();
}

RHI::RHIDescriptorSetHandle PipelineLibrary::GetToneMappingDescriptorSet(
    std::uint32_t frame_slot) const {
  return frame_slot < tonemapping_descriptor_sets_.size()
             ? tonemapping_descriptor_sets_[frame_slot]
             : RHI::RHIDescriptorSetHandle{};
}

}  // namespace Fabrica::Renderer::Internal
