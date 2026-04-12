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

  if (config.graphics_api == WindowGraphicsApi::kNone) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  } else {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
  }

  GLFWwindow* window = glfwCreateWindow(config.width, config.height,
                                        config.title.c_str(), nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    return false;
  }

  native_window_ = window;
  glfwSetWindowUserPointer(window, this);

  if (config.graphics_api == WindowGraphicsApi::kOpenGL) {
    glfwMakeContextCurrent(window);
    glfwSwapInterval(config.vsync_enabled ? 1 : 0);
  }

  glfwSetFramebufferSizeCallback(
      window, +[](GLFWwindow* glfw_window, int width, int height) {
        auto* backend = static_cast<GlfwWindowBackend*>(
            glfwGetWindowUserPointer(glfw_window));
        if (backend == nullptr) {
          return;
        }
        backend->framebuffer_size_ = {.x = width, .y = height};
        if (backend->event_sink_) {
          backend->event_sink_(WindowEvent::Resize(width, height));
        }
      });

  glfwSetWindowFocusCallback(window, +[](GLFWwindow* glfw_window, int focused) {
    auto* backend = static_cast<GlfwWindowBackend*>(
        glfwGetWindowUserPointer(glfw_window));
    if (backend == nullptr || !backend->event_sink_) {
      return;
    }
    backend->event_sink_(WindowEvent::Focus(focused != 0));
  });

  glfwSetWindowCloseCallback(window, +[](GLFWwindow* glfw_window) {
    auto* backend = static_cast<GlfwWindowBackend*>(
        glfwGetWindowUserPointer(glfw_window));
    if (backend == nullptr) {
      return;
    }
    backend->should_close_ = true;
    if (backend->event_sink_) {
      backend->event_sink_(WindowEvent::Close());
    }
  });

  glfwSetKeyCallback(window, +[](GLFWwindow* glfw_window, int key, int, int action,
                                  int) {
    auto* backend = static_cast<GlfwWindowBackend*>(
        glfwGetWindowUserPointer(glfw_window));
    if (backend == nullptr || !backend->event_sink_) {
      return;
    }
    if (action == GLFW_PRESS) {
      backend->event_sink_(WindowEvent::KeyDown(key, false));
    } else if (action == GLFW_REPEAT) {
      backend->event_sink_(WindowEvent::KeyDown(key, true));
    } else if (action == GLFW_RELEASE) {
      backend->event_sink_(WindowEvent::KeyUp(key));
    }
  });

  glfwSetCursorPosCallback(window, +[](GLFWwindow* glfw_window, double x, double y) {
    auto* backend = static_cast<GlfwWindowBackend*>(
        glfwGetWindowUserPointer(glfw_window));
    if (backend == nullptr || !backend->event_sink_) {
      return;
    }
    backend->event_sink_(
        WindowEvent::MouseMove(static_cast<float>(x), static_cast<float>(y)));
  });

  glfwSetMouseButtonCallback(
      window, +[](GLFWwindow* glfw_window, int button, int action, int) {
        auto* backend = static_cast<GlfwWindowBackend*>(
            glfwGetWindowUserPointer(glfw_window));
        if (backend == nullptr || !backend->event_sink_) {
          return;
        }
        backend->event_sink_(WindowEvent::MouseButton(button, action));
      });

#if FABRICA_WINDOW_VERBOSE_LOG
  FABRICA_LOG(Window, Debug)
      << "[Window] Created " << config.width << "x" << config.height
      << " @ monitor " << config.monitor_index << " | backend=GLFW | api="
      << (config.graphics_api == WindowGraphicsApi::kOpenGL ? "OpenGL" : "None")
      << " | vsync=" << (config.vsync_enabled ? "ON" : "OFF");
#endif
  return true;
#else
  return false;
#endif
}

void GlfwWindowBackend::PollEvents() {
#if defined(FABRICA_USE_GLFW)
  glfwPollEvents();
  if (native_window_ == nullptr) {
    should_close_ = true;
    return;
  }

  should_close_ =
      glfwWindowShouldClose(static_cast<GLFWwindow*>(native_window_)) != 0;
#else
  should_close_ = true;
#endif
}

bool GlfwWindowBackend::PresentFrame() {
#if defined(FABRICA_USE_GLFW)
  if (native_window_ == nullptr) {
    return false;
  }

  if (config_.graphics_api == WindowGraphicsApi::kNone) {
    return true;
  }

  GLFWwindow* window = static_cast<GLFWwindow*>(native_window_);
  glfwMakeContextCurrent(window);
  glfwSwapBuffers(window);
  return true;
#else
  return false;
#endif
}

bool GlfwWindowBackend::ShouldClose() const { return should_close_; }

void* GlfwWindowBackend::GetNativeHandle() const { return native_window_; }

Vec2i GlfwWindowBackend::GetFramebufferSize() const { return framebuffer_size_; }

void GlfwWindowBackend::Shutdown() {
#if defined(FABRICA_USE_GLFW)
  if (native_window_ != nullptr) {
    if (config_.graphics_api == WindowGraphicsApi::kOpenGL) {
      glfwMakeContextCurrent(nullptr);
    }
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
