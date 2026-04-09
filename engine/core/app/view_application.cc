#include "core/app/view_application.h"

#include <utility>

#include "core/window/glfw_window_backend.h"

namespace Fabrica::App {

ViewApplication::ViewApplication(std::unique_ptr<BaseView> view)
    : view_(std::move(view)) {}

ViewApplication::~ViewApplication() { Shutdown(); }

Core::Status ViewApplication::Initialize(ViewApplicationConfig config) {
  if (view_ == nullptr) {
    return Core::Status::InvalidArgument("View application requires a valid view");
  }

  std::unique_ptr<Core::Window::IWindowBackend> window_backend =
      ResolveWindowBackend(std::move(config.window_backend));
  if (window_backend == nullptr) {
    return Core::Status::Unavailable(
        "No window backend is available. Provide one in ViewApplicationConfig");
  }

  view_->AttachContext(&runtime_, &world_, &input_actions_);

  input_actions_.SetActionSink(Core::Input::InputActionMap::ActionSink(
      [this](const Core::Input::InputActionEvent& event) {
        if (view_ != nullptr) {
          view_->OnInputAction(event);
        }
      }));

  Core::Runtime::RuntimeConfig runtime_config;
  runtime_config.window_config = std::move(config.window_config);
  runtime_config.window_backend = std::move(window_backend);
  runtime_config.background_executor_options =
      std::move(config.background_executor_options);
  runtime_config.pump_foreground_until_empty = config.pump_foreground_until_empty;
  runtime_config.initialize_logger = true;
  runtime_config.shutdown_logger_on_exit = config.shutdown_logger_on_exit;
  runtime_config.logger_config = std::move(config.logger_config);

  runtime_config.callbacks.configure =
      Core::Invocable<Core::Status(Core::Window::WindowConfig&)>(
          [this](Core::Window::WindowConfig& window_config) {
            return view_->ConfigureWindow(window_config);
          });

  runtime_config.callbacks.start = Core::Invocable<Core::Status()>([this]() {
    const Core::Status awake_status = view_->Awake();
    if (!awake_status.ok()) {
      return awake_status;
    }
    return view_->Start();
  });

  runtime_config.callbacks.update =
      Core::Invocable<Core::Status(const Core::Runtime::FrameContext&)>(
          [this](const Core::Runtime::FrameContext& frame) {
            return view_->Update(frame);
          });

  runtime_config.callbacks.render =
      Core::Invocable<Core::Status(const Core::Runtime::FrameContext&)>(
          [this](const Core::Runtime::FrameContext& frame) {
            return view_->Render(frame);
          });

  runtime_config.callbacks.on_event =
      Core::Invocable<void(const Core::Window::WindowEvent&)>(
          [this](const Core::Window::WindowEvent& event) {
            view_->OnEvent(event);
            input_actions_.HandleWindowEvent(event);
          });

  runtime_config.callbacks.stop = Core::Invocable<void()>([this]() {
    if (view_ != nullptr) {
      view_->Shutdown();
      view_->DetachContext();
    }
    input_actions_.SetActionSink(Core::Input::InputActionMap::ActionSink{});
  });

  const Core::Status initialize_status = runtime_.Initialize(std::move(runtime_config));
  if (!initialize_status.ok()) {
    input_actions_.SetActionSink(Core::Input::InputActionMap::ActionSink{});
    view_->DetachContext();
  }

  return initialize_status;
}

Core::Status ViewApplication::Run() { return runtime_.Run(); }

void ViewApplication::RequestStop() { runtime_.RequestStop(); }

void ViewApplication::Shutdown() {
  runtime_.Shutdown();
  input_actions_.SetActionSink(Core::Input::InputActionMap::ActionSink{});
  if (view_ != nullptr) {
    view_->DetachContext();
  }
}

std::unique_ptr<Core::Window::IWindowBackend> ViewApplication::ResolveWindowBackend(
    std::unique_ptr<Core::Window::IWindowBackend> backend) const {
  if (backend != nullptr) {
    return backend;
  }

#if defined(FABRICA_USE_GLFW)
  return std::make_unique<Core::Window::GlfwWindowBackend>();
#else
  return nullptr;
#endif
}

}  // namespace Fabrica::App
