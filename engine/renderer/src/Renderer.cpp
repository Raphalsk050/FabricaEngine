#include "renderer/Renderer.h"

#include <array>
#include <cstddef>
#include <memory>

#include "core/config/config.h"
#include "core/logging/logger.h"
#include "internal/FrameData.h"
#include "internal/FrameResources.h"
#include "internal/PipelineLibrary.h"
#include "internal/RendererUtils.h"
#include "internal/ShaderLibrary.h"

#ifndef FABRICA_RENDERER_DEFAULT_SHADER_DIR
#define FABRICA_RENDERER_DEFAULT_SHADER_DIR ""
#endif

namespace Fabrica::Renderer {

struct Renderer::State {
  RHI::IRHIContext* rhi = nullptr;
  RendererConfig config{};
  CameraData camera{};
  DirectionalLight directional_light{};
  RenderableSpan renderables{};
  std::uint32_t frame_slot = 0;
  bool initialized = false;
  bool frame_started = false;
  Internal::ShaderLibrary shaders{};
  Internal::FrameResources frame_resources{};
  Internal::PipelineLibrary pipelines{};
};

Renderer::Renderer() = default;

Renderer::~Renderer() { Shutdown(); }

bool Renderer::IsInitialized() const {
  return state_ != nullptr && state_->initialized;
}

Core::Status Renderer::Initialize(RHI::IRHIContext& rhi, RendererConfig config) {
  if (IsInitialized()) {
    return Core::Status::Ok();
  }

  state_ = std::make_unique<State>();
  state_->rhi = &rhi;
  state_->config = std::move(config);
  if (state_->config.shader_root.empty()) {
    state_->config.shader_root = FABRICA_RENDERER_DEFAULT_SHADER_DIR;
  }

  if (state_->config.shader_root.empty()) {
    state_.reset();
    return Core::Status::InvalidArgument("Renderer requires a valid shader root directory");
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[Renderer] Initialize shader_root="
                               << state_->config.shader_root;
  }

  Core::Status status = state_->shaders.Initialize(rhi, state_->config.shader_root);
  if (!status.ok()) {
    Shutdown();
    return status;
  }

  status = state_->frame_resources.Initialize(rhi);
  if (!status.ok()) {
    Shutdown();
    return status;
  }

  status = state_->pipelines.Initialize(rhi, state_->config, state_->shaders);
  if (!status.ok()) {
    Shutdown();
    return status;
  }

  state_->initialized = true;
  return Core::Status::Ok();
}

void Renderer::Shutdown() {
  if (state_ == nullptr) {
    return;
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[Renderer] Shutdown";
  }

  state_->pipelines.Shutdown();
  state_->frame_resources.Shutdown();
  state_->shaders.Shutdown();
  state_.reset();
}

Core::Status Renderer::BeginFrame(const CameraData& camera, const LightEnvironment& lights) {
  if (!IsInitialized()) {
    return Core::Status(Core::ErrorCode::kFailedPrecondition,
                        "Renderer must be initialized before BeginFrame");
  }

  state_->camera = camera;
  state_->camera.UpdateViewProjection();
  state_->renderables = {};
  state_->frame_started = true;

  if (!lights.directional_lights.empty()) {
    state_->directional_light = lights.directional_lights.front();
  } else {
    state_->directional_light = DirectionalLight{};
  }

  Core::Status status = state_->frame_resources.UpdateFrameUniforms(
      state_->frame_slot,
      state_->camera,
      state_->directional_light);
  if (!status.ok()) {
    return status;
  }

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[Renderer] BeginFrame exposure="
                               << state_->camera.settings.ComputeExposure()
                               << " frame_slot=" << state_->frame_slot;
  }

  return Core::Status::Ok();
}

void Renderer::Submit(RenderableSpan renderables) {
  if (!IsInitialized()) {
    return;
  }
  state_->renderables = renderables;
}

Core::Status Renderer::EndFrame() {
  if (!IsInitialized()) {
    return Core::Status(Core::ErrorCode::kFailedPrecondition,
                        "Renderer must be initialized before EndFrame");
  }
  if (!state_->frame_started) {
    return Core::Status(Core::ErrorCode::kFailedPrecondition,
                        "Renderer::BeginFrame must be called before EndFrame");
  }

  RHI::IRHISwapChain* swap_chain = state_->rhi->GetSwapChain();
  if (swap_chain == nullptr) {
    return Core::Status::Unavailable("Renderer requires a swap chain-backed RHI context");
  }

  const std::uint32_t frame_slot = state_->frame_slot;
  const RHI::RHITextureHandle back_buffer = swap_chain->GetCurrentBackBuffer();
  const RHI::IRHITexture* back_buffer_texture = state_->rhi->GetTexture(back_buffer);
  if (back_buffer_texture == nullptr) {
    return Core::Status::Internal("Renderer could not resolve the current back buffer");
  }

  const RHI::RHIExtent2D extent = swap_chain->GetExtent();
  RHI::RenderGraph graph;
  const RHI::RenderGraphTextureHandle back_buffer_handle =
      graph.ImportTexture(back_buffer, back_buffer_texture->GetDesc(),
                          back_buffer_texture->GetLayout(), "swapchain_back_buffer");

  RHI::RHITextureDesc hdr_texture_desc{};
  hdr_texture_desc.width = extent.width;
  hdr_texture_desc.height = extent.height;
  hdr_texture_desc.format = state_->config.hdr_color_format;
  hdr_texture_desc.usage =
      RHI::ERHITextureUsage::ColorAttachment | RHI::ERHITextureUsage::Sampled;
  hdr_texture_desc.aspect = RHI::ERHITextureAspectFlags::Color;
  hdr_texture_desc.initial_layout = RHI::ERHITextureLayout::Undefined;
  const RHI::RenderGraphTextureHandle hdr_texture =
      graph.CreateTexture(hdr_texture_desc, "renderer_hdr_color");

  const RHI::RenderGraphBufferHandle frame_uniform_buffer = graph.ImportBuffer(
      state_->frame_resources.GetFrameUniformBuffer(frame_slot),
      {.size = sizeof(Internal::FrameUniforms),
       .stride = sizeof(Internal::FrameUniforms),
       .usage = RHI::ERHIBufferUsage::UniformBuffer,
       .memory = RHI::ERHIMemoryType::HostCoherent,
       .mapped_at_creation = true},
      "frame_uniform_buffer");

  graph.AddGraphicsPass(
      "RendererMainColor",
      [&](RHI::RenderGraph::PassBuilder& builder) {
        builder.Read(frame_uniform_buffer,
                     RHI::ERHIPipelineStage::VertexShader |
                         RHI::ERHIPipelineStage::FragmentShader,
                     RHI::ERHIAccessFlags::UniformRead);
        builder.Write(hdr_texture, RHI::ERHITextureLayout::ColorAttachment,
                      RHI::ERHIPipelineStage::ColorAttachmentOutput,
                      RHI::ERHIAccessFlags::ColorWrite);
      },
      [&](const RHI::RenderGraph::PassContext& context) {
        const RHI::RHITextureHandle hdr_handle = context.graph.ResolveTexture(hdr_texture);

        RHI::RHIRenderPassBeginDesc begin_desc{};
        begin_desc.render_area = {
            .x = 0,
            .y = 0,
            .width = extent.width,
            .height = extent.height,
        };
        begin_desc.layer_count = 1;
        begin_desc.color_attachment_count = 1;
        begin_desc.color_attachments[0].texture = hdr_handle;
        begin_desc.color_attachments[0].layout = RHI::ERHITextureLayout::ColorAttachment;
        begin_desc.color_attachments[0].load_op = RHI::ERHILoadOp::Clear;
        begin_desc.color_attachments[0].store_op = RHI::ERHIStoreOp::Store;
        begin_desc.color_attachments[0].clear_value = {.r = 0.02f,
                                                       .g = 0.02f,
                                                       .b = 0.02f,
                                                       .a = 1.0f};

        context.command_list.BeginDebugRegion("RendererMainColor");
        context.command_list.BeginRenderPass(state_->pipelines.GetHdrRenderPass(), begin_desc);
        context.command_list.SetViewport({.x = 0.0f,
                                          .y = 0.0f,
                                          .width = static_cast<float>(extent.width),
                                          .height = static_cast<float>(extent.height),
                                          .min_depth = 0.0f,
                                          .max_depth = 1.0f});
        context.command_list.SetScissor({.x = 0,
                                         .y = 0,
                                         .width = extent.width,
                                         .height = extent.height});
        context.command_list.BindPipeline(state_->pipelines.GetPbrPipeline());
        context.command_list.BindDescriptorSet(
            state_->frame_resources.GetFrameDescriptorSet(frame_slot), 0);

        for (const RenderableItem& renderable : state_->renderables) {
          Internal::DrawPushConstants push_constants{};
          push_constants.model = renderable.model_matrix;
          push_constants.base_color = renderable.material.base_color;
          push_constants.material_params = {renderable.material.metallic,
                                            renderable.material.roughness,
                                            renderable.material.reflectance,
                                            renderable.material.ao};
          push_constants.emissive_exposure = {renderable.material.emissive.x,
                                              renderable.material.emissive.y,
                                              renderable.material.emissive.z,
                                              renderable.material.emissive_exposure_compensation};

          context.command_list.PushConstants(
              &push_constants, sizeof(push_constants), 0,
              RHI::ERHIShaderStage::Vertex | RHI::ERHIShaderStage::Fragment);

          std::array<RHI::RHIBufferHandle, 1> vertex_buffers{renderable.mesh.vertex_buffer};
          std::array<std::uint64_t, 1> offsets{0};
          context.command_list.BindVertexBuffers(0, vertex_buffers, offsets);

          if (renderable.mesh.index_buffer.IsValid() && renderable.mesh.index_count > 0) {
            context.command_list.BindIndexBuffer(renderable.mesh.index_buffer, 0,
                                                 renderable.mesh.index_type);
            context.command_list.DrawIndexed(renderable.mesh.index_count, 1, 0, 0, 0);
          } else if (renderable.mesh.vertex_count > 0) {
            context.command_list.Draw(renderable.mesh.vertex_count, 1, 0, 0);
          }
        }

        context.command_list.EndRenderPass();
        context.command_list.EndDebugRegion();
      });

  graph.AddGraphicsPass(
      "RendererToneMapping",
      [&](RHI::RenderGraph::PassBuilder& builder) {
        builder.Read(hdr_texture, RHI::ERHITextureLayout::ShaderReadOnly,
                     RHI::ERHIPipelineStage::FragmentShader,
                     RHI::ERHIAccessFlags::ShaderRead);
        builder.Write(back_buffer_handle, RHI::ERHITextureLayout::ColorAttachment,
                      RHI::ERHIPipelineStage::ColorAttachmentOutput,
                      RHI::ERHIAccessFlags::ColorWrite);
      },
      [&](const RHI::RenderGraph::PassContext& context) {
        const RHI::RHITextureHandle hdr_handle = context.graph.ResolveTexture(hdr_texture);
        Core::Status update_status = state_->pipelines.UpdateToneMappingBindings(
            frame_slot,
            hdr_handle,
            state_->frame_resources.GetLinearSampler());
        if (!update_status.ok()) {
          return;
        }

        RHI::RHIRenderPassBeginDesc begin_desc{};
        begin_desc.render_area = {.x = 0,
                                  .y = 0,
                                  .width = extent.width,
                                  .height = extent.height};
        begin_desc.layer_count = 1;
        begin_desc.color_attachment_count = 1;
        begin_desc.color_attachments[0].texture = back_buffer;
        begin_desc.color_attachments[0].layout = RHI::ERHITextureLayout::ColorAttachment;
        begin_desc.color_attachments[0].load_op = RHI::ERHILoadOp::Load;
        begin_desc.color_attachments[0].store_op = RHI::ERHIStoreOp::Store;

        context.command_list.BeginDebugRegion("RendererToneMapping");
        context.command_list.BeginRenderPass(state_->pipelines.GetPresentRenderPass(), begin_desc);
        context.command_list.SetViewport({.x = 0.0f,
                                          .y = 0.0f,
                                          .width = static_cast<float>(extent.width),
                                          .height = static_cast<float>(extent.height),
                                          .min_depth = 0.0f,
                                          .max_depth = 1.0f});
        context.command_list.SetScissor({.x = 0,
                                         .y = 0,
                                         .width = extent.width,
                                         .height = extent.height});
        context.command_list.BindPipeline(state_->pipelines.GetToneMappingPipeline());
        context.command_list.BindDescriptorSet(
            state_->pipelines.GetToneMappingDescriptorSet(frame_slot), 0);
        context.command_list.Draw(3, 1, 0, 0);
        context.command_list.EndRenderPass();
        context.command_list.TransitionTexture(back_buffer,
                                               RHI::ERHITextureLayout::ColorAttachment,
                                               RHI::ERHITextureLayout::Present,
                                               RHI::ERHIPipelineStage::ColorAttachmentOutput,
                                               RHI::ERHIPipelineStage::BottomOfPipe,
                                               RHI::ERHIAccessFlags::ColorWrite,
                                               RHI::ERHIAccessFlags::MemoryRead);
        context.command_list.EndDebugRegion();
      });

  graph.MarkOutput(back_buffer_handle);
  graph.Execute(*state_->rhi);

  const std::size_t submitted_renderable_count = state_->renderables.size();
  state_->renderables = {};
  state_->frame_started = false;
  state_->frame_slot = (state_->frame_slot + 1u) % RHI::kFramesInFlight;

  if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
    FABRICA_LOG(Render, Debug) << "[Renderer] EndFrame renderables="
                               << submitted_renderable_count << " extent=" << extent.width
                               << "x" << extent.height << " next_frame_slot="
                               << state_->frame_slot;
  }

  return Core::Status::Ok();
}

}  // namespace Fabrica::Renderer

