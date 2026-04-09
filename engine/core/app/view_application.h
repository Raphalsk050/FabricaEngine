#pragma once

#include <cstdlib>
#include <memory>
#include <utility>

#include "core/app/base_view.h"
#include "core/logging/logger.h"
#include "core/window/iwindow_backend.h"

namespace Fabrica::App {

/**
 * Configures startup services and runtime policy for `ViewApplication`.
 *
 * Values are consumed during `Initialize()`. Ownership of backend pointers is
 * transferred to the application.
 */
struct ViewApplicationConfig {
  Core::Window::WindowConfig window_config{};
  ///< Initial window settings provided to runtime initialization.

  std::unique_ptr<Core::Window::IWindowBackend> window_backend;
  ///< Optional backend override. Null selects the default backend.

  Core::Jobs::ThreadPoolExecutor::Options background_executor_options{};
  ///< Thread-pool and scheduler settings for background work.

  Core::Logging::LoggerConfig logger_config{};
  ///< Logging sink, level, and queue configuration.

  bool pump_foreground_until_empty = true;
  ///< Drains foreground tasks each frame when enabled.

  bool shutdown_logger_on_exit = true;
  ///< Stops the global logger during application shutdown.
};

/**
 * Owns runtime wiring and drives a single `BaseView` instance.
 *
 * The application composes windowing, ECS world, input mapping, and runtime
 * executors, then forwards lifecycle callbacks to the view. It also binds and
 * unbinds view context pointers to enforce explicit startup/shutdown ordering.
 *
 * Thread safety: Public methods are expected on the main thread.
 *
 * @note `Initialize()` must succeed before `Run()`.
 * @see BaseView, Core::Runtime::EngineRuntime
 */
class ViewApplication {
 public:
  /// Adopt ownership of a pre-constructed view.
  explicit ViewApplication(std::unique_ptr<BaseView> view);
  ~ViewApplication();

  /**
   * Initialize runtime services and attach view context.
   *
   * @param config Startup configuration copied/moved into runtime systems.
   * @return `Status::Ok()` on success, error status otherwise.
   */
  Core::Status Initialize(ViewApplicationConfig config = {});

  /**
   * Execute the runtime loop until stop or failure.
   *
   * @return Final run status.
   */
  Core::Status Run();

  /// Request graceful shutdown of the underlying runtime loop.
  void RequestStop();

  /// Shutdown runtime services and detach view context.
  void Shutdown();

  /// Return mutable access to the owned view.
  BaseView* GetView() { return view_.get(); }

  /// Return read-only access to the owned view.
  const BaseView* GetView() const { return view_.get(); }

 private:
  /**
   * Resolve which backend instance should back the window system.
   *
   * @param backend Optional caller-provided backend.
   * @return Final backend instance used by runtime initialization.
   */
  std::unique_ptr<Core::Window::IWindowBackend> ResolveWindowBackend(
      std::unique_ptr<Core::Window::IWindowBackend> backend) const;

  std::unique_ptr<BaseView> view_;
  Core::Runtime::EngineRuntime runtime_;
  Core::ECS::World world_;
  Core::Input::InputActionMap input_actions_;
};

/**
 * Build, initialize, and run a `ViewApplication` from a concrete view type.
 *
 * @tparam ViewT Concrete view type derived from `BaseView`.
 * @tparam Args Constructor argument types forwarded to `ViewT`.
 * @param config Runtime/application configuration.
 * @param args Constructor arguments forwarded to `ViewT`.
 * @return Runtime status from initialize or run phases.
 */
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

/**
 * Run a typed view application and map status to process exit code.
 *
 * @return `EXIT_SUCCESS` when runtime completes successfully.
 */
template <typename ViewT, typename... Args>
int RunViewMain(ViewApplicationConfig config = {}, Args&&... args) {
  const Core::Status run_status =
      RunViewApplication<ViewT>(std::move(config), std::forward<Args>(args)...);
  return run_status.ok() ? EXIT_SUCCESS : EXIT_FAILURE;
}

}  // namespace Fabrica::App
