#pragma once

#include <utility>

#include "core/app/view_application.h"

namespace Fabrica::Engine {

using Status = Core::Status;
using ErrorCode = Core::ErrorCode;
using FrameContext = Core::Runtime::FrameContext;
using WindowConfig = Core::Window::WindowConfig;
using WindowEvent = Core::Window::WindowEvent;
using WindowEventType = Core::Window::WindowEventType;
using InputActionEvent = Core::Input::InputActionEvent;
using InputActionPhase = Core::Input::InputActionPhase;
using EntityId = Core::ECS::EntityId;
using EntityHandle = Core::ECS::EntityHandle;
using BaseView = App::BaseView;
using ViewApplication = App::ViewApplication;
using ViewApplicationConfig = App::ViewApplicationConfig;

template <typename T>
using Future = Core::Async::Future<T>;

template <typename ViewT, typename... Args>
Status RunViewApplication(ViewApplicationConfig config = {}, Args&&... args) {
  return App::RunViewApplication<ViewT>(std::move(config),
                                        std::forward<Args>(args)...);
}

template <typename ViewT, typename... Args>
int RunViewMain(ViewApplicationConfig config = {}, Args&&... args) {
  return App::RunViewMain<ViewT>(std::move(config), std::forward<Args>(args)...);
}

}  // namespace Fabrica::Engine

#define FABRICA_DEFINE_VIEW_MAIN(ViewType)             \
  int main() {                                          \
    return ::Fabrica::Engine::RunViewMain<ViewType>(); \
  }
