#include "core/common/status.h"
#include "core/window/iwindow_backend.h"
#include "core/window/window_system.h"

#include <cstdlib>
#include <iostream>
#include <memory>

namespace {

using Fabrica::Core::Status;
using Fabrica::Core::Window::IWindowBackend;
using Fabrica::Core::Window::Vec2i;
using Fabrica::Core::Window::WindowConfig;
using Fabrica::Core::Window::WindowEvent;
using Fabrica::Core::Window::WindowEventType;
using Fabrica::Core::Window::WindowSystem;

/**
 * Emit a deterministic event script without depending on a platform window API.
 *
 * The backend is intentionally simple for onboarding: every `PollEvents()` call
 * emits one known event so `WindowSystem` behavior is easy to inspect.
 *
 * Thread safety: Not thread-safe. Use from one control thread.
 *
 * @code
 * auto backend = std::make_unique<ScriptedWindowBackend>();
 * WindowSystem window_system(std::move(backend));
 * window_system.Initialize(WindowConfig{});
 * @endcode
 */
class ScriptedWindowBackend final : public IWindowBackend {
 public:
  bool Initialize(const WindowConfig& config) override {
    framebuffer_size_ = {.x = config.width, .y = config.height};
    initialized_ = true;
    should_close_ = false;
    poll_count_ = 0;
    return true;
  }

  void PollEvents() override {
    if (!initialized_ || !event_sink_) {
      return;
    }

    if (poll_count_ == 0) {
      event_sink_(WindowEvent::Resize(framebuffer_size_.x, framebuffer_size_.y));
    } else if (poll_count_ == 1) {
      event_sink_(WindowEvent::KeyDown(74, false));
    } else if (poll_count_ == 2) {
      event_sink_(WindowEvent::KeyUp(74));
    } else if (poll_count_ == 3) {
      should_close_ = true;
      event_sink_(WindowEvent::Close());
    }

    ++poll_count_;
  }

  bool PresentFrame() override { return initialized_; }

  bool ShouldClose() const override { return should_close_; }

  void* GetNativeHandle() const override {
    return initialized_ ? reinterpret_cast<void*>(0xCAFE) : nullptr;
  }

  Vec2i GetFramebufferSize() const override { return framebuffer_size_; }

  void Shutdown() override {
    initialized_ = false;
    should_close_ = false;
  }

  void SetEventSink(EventSink sink) override { event_sink_ = std::move(sink); }

 private:
  EventSink event_sink_;
  Vec2i framebuffer_size_{};
  int poll_count_ = 0;
  bool initialized_ = false;
  bool should_close_ = false;
};

/**
 * Demonstrate event queueing and draining through `WindowSystem`.
 *
 * @return `Status::Ok()` when scripted events flow from backend to queue.
 */
Status RunWindowDemo() {
  auto backend = std::make_unique<ScriptedWindowBackend>();
  WindowSystem window_system(std::move(backend));

  WindowConfig config;
  config.width = 1600;
  config.height = 900;
  config.title = "Window System Sample";

  if (!window_system.Initialize(config)) {
    return Status::Unavailable("Window system failed to initialize scripted backend");
  }

  bool saw_resize = false;
  bool saw_close = false;
  for (int i = 0; i < 4; ++i) {
    window_system.PollEvents();

    WindowEvent event;
    while (window_system.PopEvent(&event)) {
      if (event.type == WindowEventType::kWindowResize) {
        saw_resize = true;
      }
      if (event.type == WindowEventType::kWindowClose) {
        saw_close = true;
      }
    }
  }

  if (!window_system.PresentFrame()) {
    return Status::Unavailable("Window system failed to present frame");
  }

  if (!saw_resize || !saw_close) {
    return Status::Internal("Expected resize and close events were not observed");
  }

  if (!window_system.ShouldClose()) {
    return Status::Internal("Window close state should be set after scripted close event");
  }

  window_system.Shutdown();
  return Status::Ok();
}

}  // namespace

int main() {
  const Status status = RunWindowDemo();
  if (!status.ok()) {
    std::cerr << "[window_sample] Failed: " << status.ToString() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "[window_sample] Backend-to-queue event flow completed successfully\n";
  return EXIT_SUCCESS;
}
