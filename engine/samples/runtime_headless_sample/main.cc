#include "core/common/status.h"
#include "core/runtime/engine_runtime.h"

#include <cstdlib>
#include <iostream>
#include <memory>

namespace {

using Fabrica::Core::Runtime::EngineRuntime;
using Fabrica::Core::Runtime::FrameContext;
using Fabrica::Core::Runtime::RuntimeConfig;
using Fabrica::Core::Status;
using Fabrica::Core::Window::IWindowBackend;
using Fabrica::Core::Window::Vec2i;
using Fabrica::Core::Window::WindowConfig;
using Fabrica::Core::Window::WindowEvent;

/**
 * Provide deterministic runtime events without creating a real OS window.
 *
 * The backend emits a focus event on the first poll and a close event after a
 * fixed number of polls so runtime shutdown remains predictable in CI.
 */
class HeadlessWindowBackend final : public IWindowBackend {
 public:
  explicit HeadlessWindowBackend(int close_after_polls)
      : close_after_polls_(close_after_polls) {}

  bool Initialize(const WindowConfig& config) override {
    initialized_ = true;
    should_close_ = false;
    poll_count_ = 0;
    framebuffer_size_ = {.x = config.width, .y = config.height};
    return true;
  }

  void PollEvents() override {
    if (!initialized_) {
      return;
    }

    ++poll_count_;
    if (event_sink_ && poll_count_ == 1) {
      event_sink_(WindowEvent::Focus(true));
    }

    if (event_sink_ && poll_count_ >= close_after_polls_) {
      should_close_ = true;
      event_sink_(WindowEvent::Close());
    }
  }

  bool PresentFrame() override { return initialized_; }

  bool ShouldClose() const override { return should_close_; }

  void* GetNativeHandle() const override {
    return initialized_ ? reinterpret_cast<void*>(0x1) : nullptr;
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
  int close_after_polls_ = 1;
  int poll_count_ = 0;
  bool initialized_ = false;
  bool should_close_ = false;
};

/**
 * Demonstrate runtime callback wiring and frame loop execution.
 *
 * @return `Status::Ok()` when startup, update, render, and stop callbacks run.
 */
Status RunRuntimeDemo() {
  EngineRuntime runtime;

  int update_calls = 0;
  int render_calls = 0;
  int event_calls = 0;
  bool stop_called = false;

  RuntimeConfig config;
  config.initialize_logger = false;
  config.shutdown_logger_on_exit = false;
  config.window_backend = std::make_unique<HeadlessWindowBackend>(4);
  config.callbacks.start = Fabrica::Core::Invocable<Status()>([]() {
    return Status::Ok();
  });
  config.callbacks.update =
      Fabrica::Core::Invocable<Status(const FrameContext&)>(
          [&update_calls](const FrameContext&) {
            ++update_calls;
            return Status::Ok();
          });
  config.callbacks.render =
      Fabrica::Core::Invocable<Status(const FrameContext&)>(
          [&render_calls](const FrameContext&) {
            ++render_calls;
            return Status::Ok();
          });
  config.callbacks.on_event =
      Fabrica::Core::Invocable<void(const WindowEvent&)>(
          [&event_calls](const WindowEvent&) { ++event_calls; });
  config.callbacks.stop = Fabrica::Core::Invocable<void()>(
      [&stop_called]() { stop_called = true; });

  Status status = runtime.Initialize(std::move(config));
  if (!status.ok()) {
    return status;
  }

  status = runtime.Run();
  if (!status.ok()) {
    return status;
  }

  if (update_calls == 0 || render_calls == 0 || event_calls == 0 || !stop_called) {
    return Status::Internal("Runtime lifecycle callbacks were not fully executed");
  }

  return Status::Ok();
}

}  // namespace

int main() {
  const Status status = RunRuntimeDemo();
  if (!status.ok()) {
    std::cerr << "[runtime_headless_sample] Failed: " << status.ToString() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "[runtime_headless_sample] Runtime lifecycle executed successfully\n";
  return EXIT_SUCCESS;
}
