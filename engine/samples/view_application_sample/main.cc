#include "core/app/view_application.h"
#include "core/common/status.h"

#include <cstdlib>
#include <iostream>
#include <memory>

namespace {

using Fabrica::App::BaseView;
using Fabrica::App::ViewApplication;
using Fabrica::App::ViewApplicationConfig;
using Fabrica::Core::Status;
using Fabrica::Core::Window::IWindowBackend;
using Fabrica::Core::Window::Vec2i;
using Fabrica::Core::Window::WindowConfig;
using Fabrica::Core::Window::WindowEvent;

struct PlayerController {
  float move_speed = 0.0f;
};

/**
 * Emit deterministic keyboard and close events for `ViewApplication` demos.
 */
class ScriptedViewBackend final : public IWindowBackend {
 public:
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
      event_sink_(WindowEvent::KeyDown(74, false));
    }
    if (event_sink_ && poll_count_ == 2) {
      event_sink_(WindowEvent::KeyUp(74));
    }
    if (event_sink_ && poll_count_ == 3) {
      should_close_ = true;
      event_sink_(WindowEvent::Close());
    }
  }

  bool PresentFrame() override { return initialized_; }

  bool ShouldClose() const override { return should_close_; }

  void* GetNativeHandle() const override {
    return initialized_ ? reinterpret_cast<void*>(0x2) : nullptr;
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
 * Demonstrate the high-level view lifecycle API used by game code.
 *
 * The sample registers ECS components in `Awake`, listens to mapped input in
 * `OnInputAction`, and relies on `ViewApplication` for runtime orchestration.
 */
class SampleGameplayView final : public BaseView {
 public:
  Status ConfigureWindow(WindowConfig& config) override {
    config.title = "View Application Sample";
    return Status::Ok();
  }

  Status Awake() override {
    Status status = RegisterComponent<PlayerController>();
    if (!status.ok()) {
      return status;
    }

    auto player = CreateEntity();
    status = player.AddComponent<PlayerController>(6.0f);
    if (!status.ok()) {
      return status;
    }

    return BindInputAction("jump", 74);
  }

  Status Update(const FrameContext&) override {
    ++update_calls_;
    return Status::Ok();
  }

  void OnInputAction(const InputActionEvent& event) override {
    if (event.action == "jump" && event.phase == InputActionPhase::kStarted) {
      ++jump_started_events_;
    }
  }

  void Shutdown() override { shutdown_called_ = true; }

  int update_calls() const { return update_calls_; }
  int jump_started_events() const { return jump_started_events_; }
  bool shutdown_called() const { return shutdown_called_; }

 private:
  int update_calls_ = 0;
  int jump_started_events_ = 0;
  bool shutdown_called_ = false;
};

/**
 * Run `ViewApplication` end-to-end using a scripted backend.
 *
 * @return `Status::Ok()` when lifecycle and input-routing expectations are met.
 */
Status RunViewApplicationDemo() {
  auto view = std::make_unique<SampleGameplayView>();
  SampleGameplayView* view_ptr = view.get();

  ViewApplication application(std::move(view));

  ViewApplicationConfig config;
  config.window_backend = std::make_unique<ScriptedViewBackend>();
  config.shutdown_logger_on_exit = false;

  Status status = application.Initialize(std::move(config));
  if (!status.ok()) {
    return status;
  }

  status = application.Run();
  if (!status.ok()) {
    return status;
  }

  if (view_ptr->update_calls() == 0 || view_ptr->jump_started_events() == 0 ||
      !view_ptr->shutdown_called()) {
    return Status::Internal(
        "View lifecycle did not execute expected update/input/shutdown stages");
  }

  return Status::Ok();
}

}  // namespace

int main() {
  const Status status = RunViewApplicationDemo();
  if (!status.ok()) {
    std::cerr << "[view_application_sample] Failed: " << status.ToString() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "[view_application_sample] View lifecycle and input routing succeeded\n";
  return EXIT_SUCCESS;
}
