#pragma once

#include <cstdint>
#include <span>

#include "renderer/MaterialData.h"
#include "renderer/MathTypes.h"
#include "rhi/RHITypes.h"

namespace Fabrica::Renderer {

struct PbrVertex {
  Math::Vec3f position{};
  Math::Vec3f normal{0.0f, 0.0f, 1.0f};
  Math::Vec2f uv{};
};

struct MeshData {
  RHI::RHIBufferHandle vertex_buffer{};
  RHI::RHIBufferHandle index_buffer{};
  std::uint32_t vertex_count = 0;
  std::uint32_t index_count = 0;
  std::uint32_t vertex_stride = sizeof(PbrVertex);
  RHI::ERHIIndexType index_type = RHI::ERHIIndexType::Uint32;
};

struct RenderableItem {
  MeshData mesh{};
  MaterialData material{};
  Math::Mat4f model_matrix = Math::Identity();
};

using RenderableSpan = std::span<const RenderableItem>;

}  // namespace Fabrica::Renderer
