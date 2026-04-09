#include <memory>
#include <string>

#include "core/app/view_application.h"
#include "core/common/test/test_framework.h"

namespace {

struct TestComponent {
  int value = 0;
};

class FakeWindowBackend final : public Fabrica::Core::Window::IWindowBackend {
 public:
  bool Initialize(const Fabrica::Core::Window::WindowConfig& config) override {
    initialized_ = true;
    framebuffer_size_ = {.x = config.width, .y = config.height};
    return true;
  }

  void PollEvents() override {
    if (!initialized_) {
      return;
    }

    ++poll_count_;
    if (event_sink_ && poll_count_ == 1) {
      event_sink_(Fabrica::Core::Window::WindowEvent::KeyDown(74, false));
    }
    if (event_sink_ && poll_count_ == 2) {
      event_sink_(Fabrica::Core::Window::WindowEvent::KeyUp(74));
    }
    if (event_sink_ && poll_count_ == 3) {
      should_close_ = true;
      event_sink_(Fabrica::Core::Window::WindowEvent::Close());
    }
  }

  bool PresentFrame() override { return initialized_; }
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
  int poll_count_ = 0;
  bool initialized_ = false;
  bool should_close_ = false;
};

class TestView final : public Fabrica::App::BaseView {
 public:
  Status ConfigureWindow(WindowConfig& config) override {
    config.title = "ViewApplicationTest";
    return Status::Ok();
  }

  Status Awake() override {
    ++awake_calls;

    const Status register_status = RegisterComponent<TestComponent>();
    if (!register_status.ok()) {
      return register_status;
    }

    entity_ = CreateEntity();
    const Status add_status = entity_.AddComponent<TestComponent>(7);
    if (!add_status.ok()) {
      return add_status;
    }

    component_added = entity_.HasComponent<TestComponent>();
    return BindInputAction("jump", 74);
  }

  Status Start() override {
    ++start_calls;
    return Status::Ok();
  }

  Status Update(const FrameContext&) override {
    ++update_calls;
    if (update_calls >= 2) {
      RequestStop();
    }
    return Status::Ok();
  }

  void OnEvent(const WindowEvent&) override { ++event_calls; }

  void OnInputAction(const InputActionEvent& event) override {
    if (event.action == "jump" && event.phase == InputActionPhase::kStarted) {
      ++jump_started_events;
    }
  }

  void Shutdown() override { ++shutdown_calls; }

  int awake_calls = 0;
  int start_calls = 0;
  int update_calls = 0;
  int event_calls = 0;
  int jump_started_events = 0;
  int shutdown_calls = 0;
  bool component_added = false;

 private:
  EntityHandle entity_;
};


class ShutdownProbeView final : public Fabrica::App::BaseView {
 public:
  explicit ShutdownProbeView(int* shutdown_calls)
      : shutdown_calls_(shutdown_calls) {}

  void Shutdown() override {
    if (shutdown_calls_ != nullptr) {
      ++(*shutdown_calls_);
    }
  }

 private:
  int* shutdown_calls_ = nullptr;
};
FABRICA_TEST(ViewApplicationRunsViewLifecycleAndInputRouting) {
  auto view = std::make_unique<TestView>();
  TestView* view_ptr = view.get();

  Fabrica::App::ViewApplication application(std::move(view));

  Fabrica::App::ViewApplicationConfig config;
  config.window_backend = std::make_unique<FakeWindowBackend>();
  config.shutdown_logger_on_exit = false;

  const Fabrica::Core::Status initialize_status =
      application.Initialize(std::move(config));
  FABRICA_EXPECT_TRUE(initialize_status.ok());

  const Fabrica::Core::Status run_status = application.Run();
  FABRICA_EXPECT_TRUE(run_status.ok());

  FABRICA_EXPECT_EQ(view_ptr->awake_calls, 1);
  FABRICA_EXPECT_EQ(view_ptr->start_calls, 1);
  FABRICA_EXPECT_TRUE(view_ptr->update_calls >= 1);
  FABRICA_EXPECT_TRUE(view_ptr->event_calls >= 1);
  FABRICA_EXPECT_EQ(view_ptr->jump_started_events, 1);
  FABRICA_EXPECT_EQ(view_ptr->shutdown_calls, 1);
  FABRICA_EXPECT_TRUE(view_ptr->component_added);
}


FABRICA_TEST(ViewApplicationDestructorShutsDownInitializedRuntime) {
  int shutdown_calls = 0;

  {
    auto view = std::make_unique<ShutdownProbeView>(&shutdown_calls);
    Fabrica::App::ViewApplication application(std::move(view));

    Fabrica::App::ViewApplicationConfig config;
    config.window_backend = std::make_unique<FakeWindowBackend>();
    config.shutdown_logger_on_exit = false;

    const Fabrica::Core::Status initialize_status =
        application.Initialize(std::move(config));
    FABRICA_EXPECT_TRUE(initialize_status.ok());
  }

  FABRICA_EXPECT_EQ(shutdown_calls, 1);
}
FABRICA_TEST(ViewApplicationRejectsNullView) {
  Fabrica::App::ViewApplication application(nullptr);
  FABRICA_EXPECT_TRUE(!application.Initialize().ok());
}

}  // namespace
