#pragma once

#include <cstdlib>
#include <memory>
#include <utility>

#include "core/app/base_view.h"
#include "core/logging/logger.h"
#include "core/window/iwindow_backend.h"

namespace Fabrica::App {

struct ViewApplicationConfig {
  Core::Window::WindowConfig window_config{};
  std::unique_ptr<Core::Window::IWindowBackend> window_backend;
  Core::Jobs::ThreadPoolExecutor::Options background_executor_options{};
  Core::Logging::LoggerConfig logger_config{};
  bool pump_foreground_until_empty = true;
  bool shutdown_logger_on_exit = true;
};

class ViewApplication {
 public:
  explicit ViewApplication(std::unique_ptr<BaseView> view);
  ~ViewApplication();

  Core::Status Initialize(ViewApplicationConfig config = {});
  Core::Status Run();

  void RequestStop();
  void Shutdown();

  BaseView* GetView() { return view_.get(); }
  const BaseView* GetView() const { return view_.get(); }

 private:
  std::unique_ptr<Core::Window::IWindowBackend> ResolveWindowBackend(
      std::unique_ptr<Core::Window::IWindowBackend> backend) const;

  std::unique_ptr<BaseView> view_;
  Core::Runtime::EngineRuntime runtime_;
  Core::ECS::World world_;
  Core::Input::InputActionMap input_actions_;
};

template <typename ViewT, typename... Args>
Core::Status RunViewApplication(ViewApplicationConfig config = {},
                                Args&&... args) {
  auto view = std::make_unique<ViewT>(std::forward<Args>(args)...);
  ViewApplication application(std::move(view));

  const Core::Status initialize_status = application.Initialize(std::move(config));
  if (!initialize_status.ok()) {
    return initialize_status;
  }

  return application.Run();
}

template <typename ViewT, typename... Args>
int RunViewMain(ViewApplicationConfig config = {}, Args&&... args) {
  const Core::Status run_status =
      RunViewApplication<ViewT>(std::move(config), std::forward<Args>(args)...);
  return run_status.ok() ? EXIT_SUCCESS : EXIT_FAILURE;
}

}  // namespace Fabrica::App
