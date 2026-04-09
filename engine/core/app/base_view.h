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

  virtual Status ConfigureWindow(WindowConfig& /*config*/) { return Status::Ok(); }
  virtual Status Awake() { return Status::Ok(); }
  virtual Status Start() { return Status::Ok(); }
  virtual Status Update(const FrameContext& /*frame*/) { return Status::Ok(); }
  virtual Status Render(const FrameContext& /*frame*/) { return Status::Ok(); }
  virtual void OnEvent(const WindowEvent& /*event*/) {}
  virtual void OnInputAction(const InputActionEvent& /*event*/) {}
  virtual void Shutdown() {}

  void RequestStop() {
    if (runtime_ != nullptr) {
      runtime_->RequestStop();
    }
  }

  bool IsStopRequested() const {
    if (runtime_ == nullptr) {
      return false;
    }
    const Core::Runtime::RuntimeState state = runtime_->GetState();
    return state == Core::Runtime::RuntimeState::kStopping ||
           state == Core::Runtime::RuntimeState::kStopped;
  }

  template <typename T>
  Status RegisterComponent() {
    const Status status = ValidateWorldBound();
    if (!status.ok()) {
      return status;
    }
    return world_->RegisterComponent<T>();
  }

  EntityHandle CreateEntity() {
    const Status status = ValidateWorldBound();
    if (!status.ok()) {
      return {};
    }
    return world_->CreateEntityHandle();
  }

  Status BindInputAction(std::string_view action, int key) {
    if (input_actions_ == nullptr) {
      return Status(Core::ErrorCode::kFailedPrecondition,
                    "Input actions are unavailable before view initialization");
    }
    return input_actions_->Bind(action, key);
  }

  Status UnbindInputAction(std::string_view action, int key) {
    if (input_actions_ == nullptr) {
      return Status(Core::ErrorCode::kFailedPrecondition,
                    "Input actions are unavailable before view initialization");
    }
    return input_actions_->Unbind(action, key);
  }

  void UnbindAllInputAction(std::string_view action) {
    if (input_actions_ != nullptr) {
      input_actions_->UnbindAll(action);
    }
  }

  bool IsInputActionPressed(std::string_view action) const {
    return input_actions_ != nullptr && input_actions_->IsPressed(action);
  }

  Core::ECS::World& World() {
    FABRICA_ASSERT(world_ != nullptr,
                   "BaseView::World called before the view is attached");
    return *world_;
  }

  const Core::ECS::World& World() const {
    FABRICA_ASSERT(world_ != nullptr,
                   "BaseView::World called before the view is attached");
    return *world_;
  }

  template <typename T, typename Fn>
  Future<T> ScheduleForeground(Fn&& fn) {
    return Future<T>::Schedule(std::forward<Fn>(fn),
                               Core::Jobs::Executor::Type::kForeground);
  }

  template <typename T, typename Fn>
  Future<T> ScheduleBackground(Fn&& fn) {
    return Future<T>::Schedule(std::forward<Fn>(fn),
                               Core::Jobs::Executor::Type::kBackground);
  }

 private:
  friend class ViewApplication;

  void AttachContext(Core::Runtime::EngineRuntime* runtime, Core::ECS::World* world,
                     Core::Input::InputActionMap* input_actions) {
    runtime_ = runtime;
    world_ = world;
    input_actions_ = input_actions;
  }

  void DetachContext() {
    input_actions_ = nullptr;
    world_ = nullptr;
    runtime_ = nullptr;
  }

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
