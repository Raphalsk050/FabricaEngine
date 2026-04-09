#pragma once

#include <chrono>
#include <cstdint>
#include <memory>

#include "core/common/invocable.h"
#include "core/common/status.h"
#include "core/logging/logger.h"
#include "core/jobs/simple_foreground_executor.h"
#include "core/jobs/thread_pool_executor.h"
#include "core/window/iwindow_backend.h"
#include "core/window/window_system.h"

namespace Fabrica::Core::Runtime {

/**
 * Carries frame timing and indexing data for per-frame callbacks.
 */
struct FrameContext {
  std::uint64_t frame_index = 0;
  ///< Monotonic frame number starting at zero.

  double delta_seconds = 0.0;
  ///< Elapsed wall time since previous frame.
};

/**
 * Groups callback hooks consumed by `EngineRuntime`.
 */
struct RuntimeCallbacks {
  Core::Invocable<Core::Status(Window::WindowConfig&)> configure;
  ///< Called before backend initialization to mutate window config.

  Core::Invocable<Core::Status()> start;
  ///< Called once after runtime initialization.

  Core::Invocable<Core::Status(const FrameContext&)> update;
  ///< Called once per frame before render.

  Core::Invocable<Core::Status(const FrameContext&)> render;
  ///< Called once per frame after update.

  Core::Invocable<void(const Window::WindowEvent&)> on_event;
  ///< Called for each polled window event.

  Core::Invocable<void()> stop;
  ///< Called once during controlled shutdown.
};

/**
 * Configures runtime services and lifecycle policies.
 */
struct RuntimeConfig {
  Window::WindowConfig window_config;
  std::unique_ptr<Window::IWindowBackend> window_backend;
  RuntimeCallbacks callbacks;
  Jobs::ThreadPoolExecutor::Options background_executor_options{};
  Logging::LoggerConfig logger_config{};
  bool initialize_logger = true;
  bool shutdown_logger_on_exit = true;
  bool pump_foreground_until_empty = true;
};

/**
 * Describes coarse runtime lifecycle stages.
 */
enum class RuntimeState {
  kConstructed,
  kInitialized,
  kRunning,
  kStopping,
  kStopped,
};

/**
 * Owns engine runtime systems and executes the main loop.
 *
 * `EngineRuntime` wires windowing, executors, and callbacks into a deterministic
 * lifecycle with explicit stop semantics.
 *
 * Thread safety: API is intended for single-threaded runtime control.
 *
 * @note Call `Initialize()` exactly once before `Run()` or `Tick()`.
 */
class EngineRuntime {
 public:
  EngineRuntime();
  ~EngineRuntime();

  /**
   * Initialize runtime services from configuration.
   */
  Core::Status Initialize(RuntimeConfig config);

  /**
   * Execute one frame tick (events, update, render).
   */
  Core::Status Tick();

  /**
   * Run frame loop until stop is requested or an error occurs.
   */
  Core::Status Run();

  /// Request graceful stop at next safe loop boundary.
  void RequestStop();

  /// Shutdown runtime services and release owned resources.
  void Shutdown();

  /// Return current lifecycle state.
  RuntimeState GetState() const { return state_; }

  /// Return last recorded runtime error.
  const Core::Status& GetLastError() const { return last_error_; }

 private:
  /// Poll and dispatch all pending window events.
  Core::Status DispatchWindowEvents();

  /// Advance frame clock and invoke update/render callbacks.
  Core::Status AdvanceFrame();

  RuntimeState state_ = RuntimeState::kConstructed;
  Core::Status last_error_ = Core::Status::Ok();
  bool stop_requested_ = false;
  bool pump_foreground_until_empty_ = true;
  bool allow_stop_callback_ = false;
  bool shutdown_logger_on_exit_ = true;

  std::uint64_t frame_index_ = 0;
  std::chrono::steady_clock::time_point last_tick_timestamp_{};

  RuntimeCallbacks callbacks_;
  std::unique_ptr<Jobs::SimpleForegroundExecutor> foreground_executor_;
  std::unique_ptr<Jobs::ThreadPoolExecutor> background_executor_;
  std::unique_ptr<Window::WindowSystem> window_system_;
};

}  // namespace Fabrica::Core::Runtime
