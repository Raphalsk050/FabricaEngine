#include "core/window/glfw_window_backend.h"

#include "core/config/config.h"
#include "core/logging/logger.h"

#if defined(FABRICA_USE_GLFW)
#include <GLFW/glfw3.h>
#endif

namespace Fabrica::Core::Window {

bool GlfwWindowBackend::Initialize(const WindowConfig& config) {
  config_ = config;
  framebuffer_size_ = {.x = config.width, .y = config.height};

#if defined(FABRICA_USE_GLFW)
  if (!glfwInit()) {
    return false;
  }

  GLFWwindow* window = glfwCreateWindow(config.width, config.height,
                                        config.title.c_str(), nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    return false;
  }

  native_window_ = window;
  glfwSetWindowUserPointer(window, this);

#if FABRICA_WINDOW_VERBOSE_LOG
  FABRICA_LOG(Window, Debug)
      << "[Window] Created " << config.width << "x" << config.height
      << " @ monitor " << config.monitor_index << " | backend=GLFW | vsync="
      << (config.vsync_enabled ? "ON" : "OFF");
#endif
  return true;
#else
  return false;
#endif
}

void GlfwWindowBackend::PollEvents() {
#if defined(FABRICA_USE_GLFW)
  glfwPollEvents();
  should_close_ = glfwWindowShouldClose(static_cast<GLFWwindow*>(native_window_)) != 0;
#else
  should_close_ = true;
#endif
}

bool GlfwWindowBackend::ShouldClose() const { return should_close_; }

void* GlfwWindowBackend::GetNativeHandle() const { return native_window_; }

Vec2i GlfwWindowBackend::GetFramebufferSize() const { return framebuffer_size_; }

void GlfwWindowBackend::Shutdown() {
#if defined(FABRICA_USE_GLFW)
  if (native_window_ != nullptr) {
    glfwDestroyWindow(static_cast<GLFWwindow*>(native_window_));
    native_window_ = nullptr;
  }
  glfwTerminate();
#else
  native_window_ = nullptr;
#endif
#if FABRICA_WINDOW_VERBOSE_LOG
  FABRICA_LOG(Window, Debug) << "[Window] Destroyed window backend";
#endif
}

void GlfwWindowBackend::SetEventSink(EventSink sink) {
  event_sink_ = std::move(sink);
}

}  // namespace Fabrica::Core::Window
