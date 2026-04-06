#include "core/common/test/test_framework.h"
#include "core/window/glfw_window_backend.h"

namespace {

FABRICA_TEST(GlfwBackendInitializeFailsWhenGlfwIsNotEnabled) {
  Fabrica::Core::Window::GlfwWindowBackend backend;
  Fabrica::Core::Window::WindowConfig config;

  const bool initialized = backend.Initialize(config);
#if defined(FABRICA_USE_GLFW)
  FABRICA_EXPECT_TRUE(initialized);
#else
  FABRICA_EXPECT_TRUE(!initialized);
  FABRICA_EXPECT_TRUE(!backend.PresentFrame());
#endif
}

}  // namespace
