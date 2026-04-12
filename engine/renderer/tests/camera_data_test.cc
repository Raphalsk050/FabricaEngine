#include <cmath>
#include <cstddef>

#include "core/common/test/test_framework.h"
#include "renderer/CameraData.h"
#include "renderer/MathTypes.h"

namespace {

using Fabrica::Renderer::CameraData;
using Fabrica::Renderer::CameraSettings;
using Fabrica::Renderer::Math::LookAt;
using Fabrica::Renderer::Math::Mat4f;
using Fabrica::Renderer::Math::Multiply;
using Fabrica::Renderer::Math::Perspective;
using Fabrica::Renderer::Math::Radians;
using Fabrica::Renderer::Math::Vec3f;

FABRICA_TEST(CameraSettingsComputeEV100MatchesFilamentReference) {
  const CameraSettings settings{};
  const float ev100 = settings.ComputeEV100();

  FABRICA_EXPECT_TRUE(std::abs(ev100 - 14.965784f) < 1.0e-4f);
}

FABRICA_TEST(CameraSettingsComputeExposureMatchesFilamentReference) {
  const CameraSettings settings{};
  const float exposure = settings.ComputeExposure();
  const float expected_exposure = 1.0f / 38400.0f;

  FABRICA_EXPECT_TRUE(std::abs(exposure - expected_exposure) < 1.0e-8f);
}

FABRICA_TEST(CameraDataUpdateViewProjectionUsesProjectionTimesView) {
  CameraData camera{};
  camera.position = Vec3f{0.0f, 0.0f, 5.0f};
  camera.view = LookAt(camera.position, Vec3f{0.0f, 0.0f, 0.0f}, Vec3f{0.0f, 1.0f, 0.0f});
  camera.projection = Perspective(Radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);

  const Mat4f expected = Multiply(camera.projection, camera.view);
  camera.UpdateViewProjection();

  bool matches = true;
  for (std::size_t index = 0; index < camera.view_projection.elements.size(); ++index) {
    if (std::abs(camera.view_projection.elements[index] - expected.elements[index]) >
        1.0e-5f) {
      matches = false;
      break;
    }
  }

  FABRICA_EXPECT_TRUE(matches);
}

FABRICA_TEST(LookAtEncodesCameraTranslationInViewMatrix) {
  const Mat4f view = LookAt(Vec3f{0.0f, 0.0f, 5.0f},
                            Vec3f{0.0f, 0.0f, 0.0f},
                            Vec3f{0.0f, 1.0f, 0.0f});

  FABRICA_EXPECT_TRUE(std::abs(view.elements[14] + 5.0f) < 1.0e-5f);
}

}  // namespace

int main() { return Fabrica::Core::Test::Registry::Instance().RunAll(); }

