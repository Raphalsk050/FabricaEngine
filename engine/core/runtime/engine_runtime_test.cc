#include "core/common/test/test_framework.h"
#include "core/runtime/engine_runtime.h"

#include <memory>

namespace {

class FakeWindowBackend final : public Fabrica::Core::Window::IWindowBackend {
 public:
  explicit FakeWindowBackend(int close_after_polls)
      : close_after_polls_(close_after_polls) {}

  bool Initialize(const Fabrica::Core::Window::WindowConfig& config) override {
    framebuffer_size_ = {.x = config.width, .y = config.height};
    initialized_ = true;
    return true;
  }

  void PollEvents() override {
    if (!initialized_) {
      return;
    }

    ++poll_count_;
    if (event_sink_ && poll_count_ == 1) {
      event_sink_(Fabrica::Core::Window::WindowEvent::Focus(true));
    }

    if (poll_count_ >= close_after_polls_) {
      should_close_ = true;
      if (event_sink_) {
        event_sink_(Fabrica::Core::Window::WindowEvent::Close());
      }
    }
  }

  bool ShouldClose() const override { return should_close_; }

  void* GetNativeHandle() const override {
    return initialized_ ? reinterpret_cast<void*>(0x1) : nullptr;
  }

  Fabrica::Core::Window::Vec2i GetFramebufferSize() const override {
    return framebuffer_size_;
  }

  void Shutdown() override {
    initialized_ = false;
    should_close_ = false;
  }

  void SetEventSink(EventSink sink) override { event_sink_ = std::move(sink); }

 private:
  EventSink event_sink_;
  Fabrica::Core::Window::Vec2i framebuffer_size_{};
  int close_after_polls_ = 1;
  int poll_count_ = 0;
  bool initialized_ = false;
  bool should_close_ = false;
};

class FailingWindowBackend final : public Fabrica::Core::Window::IWindowBackend {
 public:
  bool Initialize(const Fabrica::Core::Window::WindowConfig&) override {
    return false;
  }
  void PollEvents() override {}
  bool ShouldClose() const override { return true; }
  void* GetNativeHandle() const override { return nullptr; }
  Fabrica::Core::Window::Vec2i GetFramebufferSize() const override { return {}; }
  void Shutdown() override {}
  void SetEventSink(EventSink) override {}
};

FABRICA_TEST(EngineRuntimeRunsLifecycleAndCallbacks) {
  Fabrica::Core::Runtime::EngineRuntime runtime;

  int update_calls = 0;
  int render_calls = 0;
  int event_calls = 0;
  bool stop_called = false;

  Fabrica::Core::Runtime::RuntimeConfig config;
  config.window_backend = std::make_unique<FakeWindowBackend>(3);
  config.callbacks.start = Fabrica::Core::Invocable<Fabrica::Core::Status()>(
      []() { return Fabrica::Core::Status::Ok(); });
  config.callbacks.update =
      Fabrica::Core::Invocable<Fabrica::Core::Status(const Fabrica::Core::Runtime::FrameContext&)>(
          [&update_calls](const Fabrica::Core::Runtime::FrameContext&) {
            ++update_calls;
            return Fabrica::Core::Status::Ok();
          });
  config.callbacks.render =
      Fabrica::Core::Invocable<Fabrica::Core::Status(const Fabrica::Core::Runtime::FrameContext&)>(
          [&render_calls](const Fabrica::Core::Runtime::FrameContext&) {
            ++render_calls;
            return Fabrica::Core::Status::Ok();
          });
  config.callbacks.on_event =
      Fabrica::Core::Invocable<void(const Fabrica::Core::Window::WindowEvent&)>(
          [&event_calls](const Fabrica::Core::Window::WindowEvent&) {
            ++event_calls;
          });
  config.callbacks.stop = Fabrica::Core::Invocable<void()>([&stop_called]() {
    stop_called = true;
  });

  FABRICA_EXPECT_TRUE(runtime.Initialize(std::move(config)).ok());
  const Fabrica::Core::Status run_status = runtime.Run();

  FABRICA_EXPECT_TRUE(run_status.ok());
  FABRICA_EXPECT_TRUE(update_calls >= 1);
  FABRICA_EXPECT_TRUE(render_calls >= 1);
  FABRICA_EXPECT_TRUE(event_calls >= 1);
  FABRICA_EXPECT_TRUE(stop_called);
  FABRICA_EXPECT_EQ(runtime.GetState(), Fabrica::Core::Runtime::RuntimeState::kStopped);
}

FABRICA_TEST(EngineRuntimeStopsOnUpdateFailure) {
  Fabrica::Core::Runtime::EngineRuntime runtime;

  bool stop_called = false;
  Fabrica::Core::Runtime::RuntimeConfig config;
  config.window_backend = std::make_unique<FakeWindowBackend>(10);
  config.callbacks.start = Fabrica::Core::Invocable<Fabrica::Core::Status()>(
      []() { return Fabrica::Core::Status::Ok(); });
  config.callbacks.update =
      Fabrica::Core::Invocable<Fabrica::Core::Status(const Fabrica::Core::Runtime::FrameContext&)>(
          [](const Fabrica::Core::Runtime::FrameContext&) {
            return Fabrica::Core::Status::Internal("Forced update failure");
          });
  config.callbacks.stop = Fabrica::Core::Invocable<void()>([&stop_called]() {
    stop_called = true;
  });

  FABRICA_EXPECT_TRUE(runtime.Initialize(std::move(config)).ok());
  const Fabrica::Core::Status run_status = runtime.Run();

  FABRICA_EXPECT_TRUE(!run_status.ok());
  FABRICA_EXPECT_TRUE(stop_called);
}

FABRICA_TEST(EngineRuntimeCleansUpAfterInitializationFailure) {
  Fabrica::Core::Runtime::EngineRuntime runtime;

  Fabrica::Core::Runtime::RuntimeConfig failing_config;
  failing_config.window_backend = std::make_unique<FailingWindowBackend>();
  const Fabrica::Core::Status failing_status =
      runtime.Initialize(std::move(failing_config));

  FABRICA_EXPECT_TRUE(!failing_status.ok());
  FABRICA_EXPECT_EQ(runtime.GetState(), Fabrica::Core::Runtime::RuntimeState::kStopped);

  Fabrica::Core::Runtime::RuntimeConfig recovery_config;
  recovery_config.window_backend = std::make_unique<FakeWindowBackend>(1);
  recovery_config.callbacks.start = Fabrica::Core::Invocable<Fabrica::Core::Status()>(
      []() { return Fabrica::Core::Status::Ok(); });

  FABRICA_EXPECT_TRUE(runtime.Initialize(std::move(recovery_config)).ok());
  FABRICA_EXPECT_TRUE(runtime.Run().ok());
}

}  // namespace
