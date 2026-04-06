#pragma once

#include <chrono>
#include <cstdint>
#include <memory>

#include "core/common/invocable.h"
#include "core/common/status.h"
#include "core/jobs/simple_foreground_executor.h"
#include "core/jobs/thread_pool_executor.h"
#include "core/window/iwindow_backend.h"
#include "core/window/window_system.h"

namespace Fabrica::Core::Runtime {

struct FrameContext {
  std::uint64_t frame_index = 0;
  double delta_seconds = 0.0;
};

struct RuntimeCallbacks {
  Core::Invocable<Core::Status(Window::WindowConfig&)> configure;
  Core::Invocable<Core::Status()> start;
  Core::Invocable<Core::Status(const FrameContext&)> update;
  Core::Invocable<Core::Status(const FrameContext&)> render;
  Core::Invocable<void(const Window::WindowEvent&)> on_event;
  Core::Invocable<void()> stop;
};

struct RuntimeConfig {
  Window::WindowConfig window_config;
  std::unique_ptr<Window::IWindowBackend> window_backend;
  RuntimeCallbacks callbacks;
  Jobs::ThreadPoolExecutor::Options background_executor_options{};
  bool pump_foreground_until_empty = true;
};

enum class RuntimeState {
  kConstructed,
  kInitialized,
  kRunning,
  kStopping,
  kStopped,
};

class EngineRuntime {
 public:
  EngineRuntime();
  ~EngineRuntime();

  Core::Status Initialize(RuntimeConfig config);
  Core::Status Tick();
  Core::Status Run();

  void RequestStop();
  void Shutdown();

  RuntimeState GetState() const { return state_; }
  const Core::Status& GetLastError() const { return last_error_; }

 private:
  Core::Status DispatchWindowEvents();
  Core::Status AdvanceFrame();

  RuntimeState state_ = RuntimeState::kConstructed;
  Core::Status last_error_ = Core::Status::Ok();
  bool stop_requested_ = false;
  bool pump_foreground_until_empty_ = true;
  bool allow_stop_callback_ = false;

  std::uint64_t frame_index_ = 0;
  std::chrono::steady_clock::time_point last_tick_timestamp_{};

  RuntimeCallbacks callbacks_;
  std::unique_ptr<Jobs::SimpleForegroundExecutor> foreground_executor_;
  std::unique_ptr<Jobs::ThreadPoolExecutor> background_executor_;
  std::unique_ptr<Window::WindowSystem> window_system_;
};

}  // namespace Fabrica::Core::Runtime
