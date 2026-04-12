#pragma once

#include "renderer/MathTypes.h"

namespace Fabrica::Renderer::Internal {

struct alignas(16) FrameUniforms {
  Math::Mat4f view_projection = Math::Identity();
  Math::Vec4f camera_position_exposure{};
  Math::Vec4f directional_light_direction_intensity{0.0f, -1.0f, 0.0f, 0.0f};
  Math::Vec4f directional_light_color{1.0f, 1.0f, 1.0f, 1.0f};
};

struct alignas(16) DrawPushConstants {
  Math::Mat4f model = Math::Identity();
  Math::Vec4f base_color{1.0f, 1.0f, 1.0f, 1.0f};
  Math::Vec4f material_params{0.0f, 0.5f, 0.5f, 1.0f};
  Math::Vec4f emissive_exposure{};
};

}  // namespace Fabrica::Renderer::Internal
