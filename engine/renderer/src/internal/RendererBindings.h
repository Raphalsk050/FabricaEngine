#pragma once

#include <cstddef>
#include <cstdint>

#include "renderer/MeshData.h"
#include "rhi/RHITypes.h"
#include "internal/FrameData.h"

namespace Fabrica::Renderer::Internal {

[[nodiscard]] inline RHI::RHIDescriptorSetLayoutDesc CreateFrameSetLayoutDesc() {
  RHI::RHIDescriptorSetLayoutDesc frame_set_layout{};
  frame_set_layout.binding_count = 1;
  frame_set_layout.bindings[0].binding = 0;
  frame_set_layout.bindings[0].type = RHI::ERHIDescriptorType::UniformBuffer;
  frame_set_layout.bindings[0].count = 1;
  frame_set_layout.bindings[0].stage_mask =
      RHI::ERHIShaderStage::Vertex | RHI::ERHIShaderStage::Fragment;
  return frame_set_layout;
}

[[nodiscard]] inline RHI::RHIDescriptorSetLayoutDesc CreateToneMappingSetLayoutDesc() {
  RHI::RHIDescriptorSetLayoutDesc layout{};
  layout.binding_count = 2;
  layout.bindings[0].binding = 0;
  layout.bindings[0].type = RHI::ERHIDescriptorType::SampledTexture;
  layout.bindings[0].count = 1;
  layout.bindings[0].stage_mask = RHI::ERHIShaderStage::Fragment;
  layout.bindings[1].binding = 1;
  layout.bindings[1].type = RHI::ERHIDescriptorType::Sampler;
  layout.bindings[1].count = 1;
  layout.bindings[1].stage_mask = RHI::ERHIShaderStage::Fragment;
  return layout;
}

[[nodiscard]] inline RHI::RHIVertexInputDesc CreatePbrVertexInputDesc() {
  RHI::RHIVertexInputDesc vertex_input{};
  vertex_input.binding_count = 1;
  vertex_input.bindings[0].binding = 0;
  vertex_input.bindings[0].stride = sizeof(PbrVertex);
  vertex_input.bindings[0].input_rate = RHI::ERHIVertexInputRate::PerVertex;
  vertex_input.attribute_count = 3;
  vertex_input.attributes[0] = {
      .location = 0,
      .binding = 0,
      .format = RHI::ERHITextureFormat::R32G32B32_FLOAT,
      .offset = static_cast<std::uint32_t>(offsetof(PbrVertex, position))};
  vertex_input.attributes[1] = {
      .location = 1,
      .binding = 0,
      .format = RHI::ERHITextureFormat::R32G32B32_FLOAT,
      .offset = static_cast<std::uint32_t>(offsetof(PbrVertex, normal))};
  vertex_input.attributes[2] = {
      .location = 2,
      .binding = 0,
      .format = RHI::ERHITextureFormat::R32G32_FLOAT,
      .offset = static_cast<std::uint32_t>(offsetof(PbrVertex, uv))};
  return vertex_input;
}

[[nodiscard]] inline RHI::RHIPushConstantRangeDesc CreateDrawPushConstantRange() {
  return {
      .offset = 0,
      .size = sizeof(DrawPushConstants),
      .stage_mask = RHI::ERHIShaderStage::Vertex | RHI::ERHIShaderStage::Fragment};
}

}  // namespace Fabrica::Renderer::Internal
