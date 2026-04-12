#pragma once

#include <span>

#include "renderer/MathTypes.h"

namespace Fabrica::Renderer {

struct DirectionalLight {
  Math::Vec3f direction{0.0f, -1.0f, 0.0f};
  float illuminance_lux = 110000.0f;
  Math::Vec3f color{1.0f, 1.0f, 1.0f};
  float color_temperature_kelvin = 6500.0f;
};

struct PointLight {
  Math::Vec3f position{};
  float luminous_power = 1000.0f;
  Math::Vec3f color{1.0f, 1.0f, 1.0f};
  float radius = 5.0f;
};

struct SpotLight {
  Math::Vec3f position{};
  float luminous_power = 1000.0f;
  Math::Vec3f direction{0.0f, -1.0f, 0.0f};
  float radius = 5.0f;
  Math::Vec3f color{1.0f, 1.0f, 1.0f};
  float inner_angle = Math::Radians(20.0f);
  float outer_angle = Math::Radians(30.0f);
};

struct LightEnvironment {
  std::span<const DirectionalLight> directional_lights{};
  std::span<const PointLight> point_lights{};
  std::span<const SpotLight> spot_lights{};
};

}  // namespace Fabrica::Renderer
