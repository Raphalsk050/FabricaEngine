#include <memory>

#include "core/common/test/test_framework.h"
#include "core/window/window_system.h"

namespace {

class FakeWindowBackend final : public Fabrica::Core::Window::IWindowBackend {
 public:
  bool Initialize(const Fabrica::Core::Window::WindowConfig& config) override {
    framebuffer_ = {.x = config.width, .y = config.height};
    initialized_ = true;
    return true;
  }

  void PollEvents() override {
    if (event_sink_) {
      event_sink_(Fabrica::Core::Window::WindowEvent::Resize(framebuffer_.x,
                                                             framebuffer_.y));
    }
  }

  bool PresentFrame() override {
    ++present_calls_;
    return initialized_;
  }

  bool ShouldClose() const override { return false; }
  void* GetNativeHandle() const override {
    return initialized_ ? reinterpret_cast<void*>(0x1234) : nullptr;
  }
  Fabrica::Core::Window::Vec2i GetFramebufferSize() const override {
    return framebuffer_;
  }
  void Shutdown() override { initialized_ = false; }
  void SetEventSink(EventSink sink) override { event_sink_ = std::move(sink); }

 private:
  EventSink event_sink_;
  Fabrica::Core::Window::Vec2i framebuffer_{};
  int present_calls_ = 0;
  bool initialized_ = false;
};

FABRICA_TEST(WindowSystemCollectsBackendEvents) {
  auto backend = std::make_unique<FakeWindowBackend>();
  Fabrica::Core::Window::WindowSystem window_system(std::move(backend));

  Fabrica::Core::Window::WindowConfig config;
  config.width = 1600;
  config.height = 900;
  const bool initialized = window_system.Initialize(config);
  FABRICA_EXPECT_TRUE(initialized);

  window_system.PollEvents();
  Fabrica::Core::Window::WindowEvent event;
  const bool popped = window_system.PopEvent(&event);
  FABRICA_EXPECT_TRUE(popped);
  FABRICA_EXPECT_EQ(event.resize.width, 1600);
  FABRICA_EXPECT_EQ(event.resize.height, 900);
  FABRICA_EXPECT_TRUE(window_system.PresentFrame());
}

}  // namespace
