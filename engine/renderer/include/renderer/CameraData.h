#pragma once

#include <cmath>

#include "renderer/MathTypes.h"

namespace Fabrica::Renderer {

struct CameraSettings {
  float aperture = 16.0f;
  float shutter_speed = 1.0f / 125.0f;
  float sensitivity = 100.0f;

  [[nodiscard]] float ComputeEV100() const {
    return std::log2((aperture * aperture) / shutter_speed * 100.0f / sensitivity);
  }

  [[nodiscard]] float ComputeExposure() const {
    const float ev100 = ComputeEV100();
    return 1.0f / (std::pow(2.0f, ev100) * 1.2f);
  }
};

struct CameraData {
  Math::Mat4f view = Math::Identity();
  Math::Mat4f projection = Math::Identity();
  Math::Mat4f view_projection = Math::Identity();
  Math::Vec3f position{};
  float near_plane = 0.1f;
  float far_plane = 1000.0f;
  CameraSettings settings{};

  void UpdateViewProjection() {
    view_projection = Math::Multiply(projection, view);
  }
};

}  // namespace Fabrica::Renderer
