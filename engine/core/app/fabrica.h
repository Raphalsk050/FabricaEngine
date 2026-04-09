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

/**
 * Construct and run a typed view application instance.
 *
 * This helper exposes the application entry point in the `Fabrica::Engine`
 * namespace so sample and game code can include a single public header.
 *
 * @tparam ViewT Concrete view type derived from `BaseView`.
 * @tparam Args Constructor argument types forwarded to `ViewT`.
 * @param config Runtime/application configuration.
 * @param args Constructor arguments forwarded to the view instance.
 * @return Runtime status from initialization or main loop execution.
 */
template <typename ViewT, typename... Args>
Status RunViewApplication(ViewApplicationConfig config = {}, Args&&... args) {
  return App::RunViewApplication<ViewT>(std::move(config),
                                        std::forward<Args>(args)...);
}

/**
 * Run a view application and convert status to process exit code.
 *
 * @tparam ViewT Concrete view type derived from `BaseView`.
 * @tparam Args Constructor argument types forwarded to `ViewT`.
 * @param config Runtime/application configuration.
 * @param args Constructor arguments forwarded to the view instance.
 * @return `EXIT_SUCCESS` when run status is ok; otherwise `EXIT_FAILURE`.
 */
template <typename ViewT, typename... Args>
int RunViewMain(ViewApplicationConfig config = {}, Args&&... args) {
  return App::RunViewMain<ViewT>(std::move(config), std::forward<Args>(args)...);
}

}  // namespace Fabrica::Engine

/**
 * Define a default `main()` that boots a specific `BaseView` implementation.
 */
#define FABRICA_DEFINE_VIEW_MAIN(ViewType)             \
  int main() {                                          \
    return ::Fabrica::Engine::RunViewMain<ViewType>(); \
  }
