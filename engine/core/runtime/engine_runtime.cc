#include "core/runtime/engine_runtime.h"

#include "core/config/config.h"
#include "core/logging/logger.h"

namespace Fabrica::Core::Runtime {

EngineRuntime::EngineRuntime() = default;

EngineRuntime::~EngineRuntime() { Shutdown(); }

Core::Status EngineRuntime::Initialize(RuntimeConfig config) {
  if (state_ != RuntimeState::kConstructed && state_ != RuntimeState::kStopped) {
    return Core::Status(Core::ErrorCode::kFailedPrecondition,
                        "Runtime already initialized");
  }

  if (config.window_backend == nullptr) {
    return Core::Status::InvalidArgument("Runtime requires a window backend");
  }

  callbacks_ = std::move(config.callbacks);
  Window::WindowConfig resolved_window_config = std::move(config.window_config);

  if (callbacks_.configure) {
    Core::Status configure_status = callbacks_.configure(resolved_window_config);
    if (!configure_status.ok()) {
      last_error_ = configure_status;
      return configure_status;
    }
  }

  state_ = RuntimeState::kInitialized;
  allow_stop_callback_ = false;

  foreground_executor_ = std::make_unique<Jobs::SimpleForegroundExecutor>();

  Jobs::ThreadPoolExecutor::Options background_options =
      std::move(config.background_executor_options);
  if (background_options.foreground_executor == nullptr) {
    background_options.foreground_executor = foreground_executor_.get();
  }
  background_executor_ =
      std::make_unique<Jobs::ThreadPoolExecutor>(std::move(background_options));

  Jobs::Executor::SetForegroundExecutor(foreground_executor_.get());
  Jobs::Executor::SetBackgroundExecutor(background_executor_.get());

  window_system_ =
      std::make_unique<Window::WindowSystem>(std::move(config.window_backend));
  const bool window_initialized = window_system_->Initialize(resolved_window_config);
  if (!window_initialized) {
    last_error_ = Core::Status::Unavailable("Failed to initialize window backend");
    Shutdown();
    return last_error_;
  }

  pump_foreground_until_empty_ = config.pump_foreground_until_empty;
  frame_index_ = 0;
  stop_requested_ = false;

  if (callbacks_.start) {
    Core::Status start_status = callbacks_.start();
    if (!start_status.ok()) {
      last_error_ = start_status;
      Shutdown();
      return last_error_;
    }
  }

  allow_stop_callback_ = true;
  state_ = RuntimeState::kRunning;
  last_error_ = Core::Status::Ok();
  last_tick_timestamp_ = std::chrono::steady_clock::now();

#if FABRICA_WINDOW_VERBOSE_LOG
  FABRICA_LOG(Game, Info) << "[Runtime] Runtime initialized and entering main loop";
#endif

  return Core::Status::Ok();
}

Core::Status EngineRuntime::Tick() {
  if (state_ != RuntimeState::kRunning) {
    return Core::Status(Core::ErrorCode::kFailedPrecondition,
                        "Tick called while runtime is not running");
  }

  if (stop_requested_) {
    return Core::Status::Cancelled("Runtime stop requested");
  }

  window_system_->PollEvents();
  Core::Status event_status = DispatchWindowEvents();
  if (!event_status.ok()) {
    last_error_ = event_status;
    RequestStop();
    return event_status;
  }

  if (stop_requested_) {
    return Core::Status::Cancelled("Runtime stop requested");
  }

  foreground_executor_->Pump(pump_foreground_until_empty_);

  Core::Status frame_status = AdvanceFrame();
  if (!frame_status.ok()) {
    last_error_ = frame_status;
    RequestStop();
    return frame_status;
  }

  if (!window_system_->PresentFrame()) {
    last_error_ = Core::Status::Unavailable("Failed to present window frame");
    RequestStop();
    return last_error_;
  }

  if (window_system_->ShouldClose()) {
    RequestStop();
  }

  ++frame_index_;
  return Core::Status::Ok();
}

Core::Status EngineRuntime::Run() {
  if (state_ != RuntimeState::kRunning) {
    return Core::Status(Core::ErrorCode::kFailedPrecondition,
                        "Run called before runtime is running");
  }

  while (!stop_requested_) {
    const Core::Status tick_status = Tick();
    if (!tick_status.ok() && tick_status.code() != Core::ErrorCode::kCancelled) {
      break;
    }
  }

  Shutdown();
  return last_error_;
}

void EngineRuntime::RequestStop() {
  stop_requested_ = true;
  if (state_ == RuntimeState::kRunning) {
    state_ = RuntimeState::kStopping;
  }
}

void EngineRuntime::Shutdown() {
  if (state_ == RuntimeState::kConstructed || state_ == RuntimeState::kStopped) {
    return;
  }

  state_ = RuntimeState::kStopping;

  if (allow_stop_callback_ && callbacks_.stop) {
    callbacks_.stop();
  }

  if (window_system_ != nullptr) {
    window_system_->Shutdown();
    window_system_.reset();
  }

  if (background_executor_ != nullptr) {
    background_executor_->Shutdown();
    background_executor_.reset();
  }

  foreground_executor_.reset();

  Jobs::Executor::SetBackgroundExecutor(nullptr);
  Jobs::Executor::SetForegroundExecutor(nullptr);

  callbacks_ = RuntimeCallbacks{};
  stop_requested_ = false;
  allow_stop_callback_ = false;
  state_ = RuntimeState::kStopped;
}

Core::Status EngineRuntime::DispatchWindowEvents() {
  Window::WindowEvent event;
  while (window_system_->PopEvent(&event)) {
    if (callbacks_.on_event) {
      callbacks_.on_event(event);
    }

    if (event.type == Window::WindowEventType::kWindowClose) {
      RequestStop();
      break;
    }
  }

  return Core::Status::Ok();
}

Core::Status EngineRuntime::AdvanceFrame() {
  const auto now = std::chrono::steady_clock::now();
  const auto elapsed = now - last_tick_timestamp_;
  last_tick_timestamp_ = now;

  const FrameContext frame{
      .frame_index = frame_index_,
      .delta_seconds = std::chrono::duration<double>(elapsed).count(),
  };

  if (callbacks_.update) {
    const Core::Status update_status = callbacks_.update(frame);
    if (!update_status.ok()) {
      return update_status;
    }
  }

  if (callbacks_.render) {
    const Core::Status render_status = callbacks_.render(frame);
    if (!render_status.ok()) {
      return render_status;
    }
  }

  return Core::Status::Ok();
}

}  // namespace Fabrica::Core::Runtime
