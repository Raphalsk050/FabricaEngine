#pragma once

#include "renderer/MathTypes.h"
#include "rhi/RHITypes.h"

namespace Fabrica::Renderer {

struct MaterialData {
  Math::Vec4f base_color{1.0f, 1.0f, 1.0f, 1.0f};
  float metallic = 0.0f;
  float roughness = 0.5f;
  float reflectance = 0.5f;
  float ao = 1.0f;
  Math::Vec3f emissive{};
  float emissive_exposure_compensation = 0.0f;

  RHI::RHITextureHandle base_color_map{};
  RHI::RHITextureHandle normal_map{};
  RHI::RHITextureHandle metallic_roughness_map{};
  RHI::RHITextureHandle ao_map{};
  RHI::RHITextureHandle emissive_map{};
};

}  // namespace Fabrica::Renderer
