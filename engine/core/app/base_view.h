#pragma once

#include <string_view>
#include <utility>

#include "core/async/future.h"
#include "core/common/assert.h"
#include "core/common/status.h"
#include "core/ecs/world.h"
#include "core/input/input_action_map.h"
#include "core/jobs/executor.h"
#include "core/runtime/engine_runtime.h"

namespace Fabrica::App {

/**
 * Defines the gameplay-facing lifecycle contract for runtime views.
 *
 * `ViewApplication` owns the runtime loop and calls these hooks in order:
 * configure, awake, start, update/render, and shutdown. The view does not own
 * runtime services; it only borrows world, input, and runtime pointers while
 * attached. These pointers become invalid immediately after detachment.
 *
 * Thread safety: Hooks are expected to run on the main runtime thread.
 *
 * @note `World()` access is valid only after attachment and before shutdown.
 * @see ViewApplication, Core::Runtime::EngineRuntime, Core::ECS::World
 *
 * @code
 * class GameView final : public BaseView {
 *  public:
 *   Status Awake() override {
 *     RegisterComponent<PlayerTag>();
 *     return Status::Ok();
 *   }
 * };
 * @endcode
 */
class BaseView {
 public:
  using Status = Core::Status;
  using FrameContext = Core::Runtime::FrameContext;
  using WindowConfig = Core::Window::WindowConfig;
  using WindowEvent = Core::Window::WindowEvent;
  using WindowEventType = Core::Window::WindowEventType;
  using EntityHandle = Core::ECS::EntityHandle;
  using EntityId = Core::ECS::EntityId;
  using InputActionEvent = Core::Input::InputActionEvent;
  using InputActionPhase = Core::Input::InputActionPhase;

  template <typename T>
  using Future = Core::Async::Future<T>;

  virtual ~BaseView() = default;

  /**
   * Override the initial window configuration before backend initialization.
   *
   * @param config Mutable window configuration prepared by the application.
   * @return `Status::Ok()` on success, or an error to abort initialization.
   */
  virtual Status ConfigureWindow(WindowConfig& /*config*/) { return Status::Ok(); }

  /**
   * Acquire resources that must exist before gameplay starts.
   *
   * @return `Status::Ok()` on success, or an error to stop startup.
   */
  virtual Status Awake() { return Status::Ok(); }

  /**
   * Start gameplay after all required systems are initialized.
   *
   * @return `Status::Ok()` on success, or an error to stop runtime.
   */
  virtual Status Start() { return Status::Ok(); }

  /**
   * Advance game logic for a single frame.
   *
   * @param frame Frame timing and index information.
   * @return `Status::Ok()` on success, or an error to stop runtime.
   */
  virtual Status Update(const FrameContext& /*frame*/) { return Status::Ok(); }

  /**
   * Submit rendering work for a single frame.
   *
   * @param frame Frame timing and index information.
   * @return `Status::Ok()` on success, or an error to stop runtime.
   */
  virtual Status Render(const FrameContext& /*frame*/) { return Status::Ok(); }

  /**
   * React to low-level window events emitted by the window backend.
   *
   * @param event Normalized window event.
   */
  virtual void OnEvent(const WindowEvent& /*event*/) {}

  /**
   * React to high-level input actions emitted by `InputActionMap`.
   *
   * @param event Action event containing action name, key, and phase.
   */
  virtual void OnInputAction(const InputActionEvent& /*event*/) {}

  /// Release view-owned resources before runtime teardown.
  virtual void Shutdown() {}

  /**
   * Request graceful runtime shutdown.
   *
   * Safe to call multiple times; requests are ignored when no runtime is
   * attached yet.
   */
  void RequestStop() {
    if (runtime_ != nullptr) {
      runtime_->RequestStop();
    }
  }

  /**
   * Check whether runtime shutdown was requested or already completed.
   *
   * @return `true` when runtime state is stopping/stopped; otherwise `false`.
   */
  bool IsStopRequested() const {
    if (runtime_ == nullptr) {
      return false;
    }
    const Core::Runtime::RuntimeState state = runtime_->GetState();
    return state == Core::Runtime::RuntimeState::kStopping ||
           state == Core::Runtime::RuntimeState::kStopped;
  }

  template <typename T>
  /**
   * Register a component type in the bound ECS world.
   *
   * @tparam T Trivially copyable component type.
   * @return `Status::Ok()` on success, or failed precondition when detached.
   */
  Status RegisterComponent() {
    const Status status = ValidateWorldBound();
    if (!status.ok()) {
      return status;
    }
    return world_->RegisterComponent<T>();
  }

  /**
   * Create a new entity handle in the bound ECS world.
   *
   * Returns an invalid handle when called before the view is attached.
   */
  EntityHandle CreateEntity() {
    const Status status = ValidateWorldBound();
    if (!status.ok()) {
      return {};
    }
    return world_->CreateEntityHandle();
  }

  /**
   * Bind a keyboard key to a named input action.
   *
   * @param action Action identifier used by gameplay callbacks.
   * @param key Backend key code.
   * @return `Status::Ok()` on success, or failed precondition when detached.
   */
  Status BindInputAction(std::string_view action, int key) {
    if (input_actions_ == nullptr) {
      return Status(Core::ErrorCode::kFailedPrecondition,
                    "Input actions are unavailable before view initialization");
    }
    return input_actions_->Bind(action, key);
  }

  /**
   * Remove a specific key binding from an input action.
   *
   * @param action Action identifier.
   * @param key Backend key code to unbind.
   * @return `Status::Ok()` on success, or failed precondition when detached.
   */
  Status UnbindInputAction(std::string_view action, int key) {
    if (input_actions_ == nullptr) {
      return Status(Core::ErrorCode::kFailedPrecondition,
                    "Input actions are unavailable before view initialization");
    }
    return input_actions_->Unbind(action, key);
  }

  /**
   * Remove all key bindings associated with an input action.
   *
   * Calls are ignored when input services are unavailable.
   */
  void UnbindAllInputAction(std::string_view action) {
    if (input_actions_ != nullptr) {
      input_actions_->UnbindAll(action);
    }
  }

  /**
   * Check whether any key bound to an action is currently pressed.
   *
   * @return `true` when action is active; `false` otherwise.
   */
  bool IsInputActionPressed(std::string_view action) const {
    return input_actions_ != nullptr && input_actions_->IsPressed(action);
  }

  /**
   * Access the mutable ECS world bound to this view.
   *
   * @pre View must be attached by `ViewApplication`.
   */
  Core::ECS::World& World() {
    FABRICA_ASSERT(world_ != nullptr,
                   "BaseView::World called before the view is attached");
    return *world_;
  }

  /**
   * Access the read-only ECS world bound to this view.
   *
   * @pre View must be attached by `ViewApplication`.
   */
  const Core::ECS::World& World() const {
    FABRICA_ASSERT(world_ != nullptr,
                   "BaseView::World called before the view is attached");
    return *world_;
  }

  template <typename T, typename Fn>
  /**
   * Schedule work on the foreground executor and return its future.
   *
   * @tparam T Future value type.
   * @param fn Callable executed on the foreground executor.
   */
  Future<T> ScheduleForeground(Fn&& fn) {
    return Future<T>::Schedule(std::forward<Fn>(fn),
                               Core::Jobs::Executor::Type::kForeground);
  }

  template <typename T, typename Fn>
  /**
   * Schedule work on the background executor and return its future.
   *
   * @tparam T Future value type.
   * @param fn Callable executed on the background executor.
   */
  Future<T> ScheduleBackground(Fn&& fn) {
    return Future<T>::Schedule(std::forward<Fn>(fn),
                               Core::Jobs::Executor::Type::kBackground);
  }

 private:
  friend class ViewApplication;

  /**
   * Bind runtime service pointers owned by `ViewApplication`.
   *
   * @pre All pointers refer to live objects for the active runtime instance.
   */
  void AttachContext(Core::Runtime::EngineRuntime* runtime, Core::ECS::World* world,
                     Core::Input::InputActionMap* input_actions) {
    runtime_ = runtime;
    world_ = world;
    input_actions_ = input_actions;
  }

  /// Clear all borrowed runtime service pointers during detachment.
  void DetachContext() {
    input_actions_ = nullptr;
    world_ = nullptr;
    runtime_ = nullptr;
  }

  /**
   * Validate that the ECS world is available to the view.
   *
   * @return `Status::Ok()` when attached, failed precondition otherwise.
   */
  Status ValidateWorldBound() const {
    if (world_ == nullptr) {
      return Status(Core::ErrorCode::kFailedPrecondition,
                    "View world is unavailable before initialization");
    }
    return Status::Ok();
  }

  Core::Runtime::EngineRuntime* runtime_ = nullptr;
  Core::ECS::World* world_ = nullptr;
  Core::Input::InputActionMap* input_actions_ = nullptr;
};

}  // namespace Fabrica::App
