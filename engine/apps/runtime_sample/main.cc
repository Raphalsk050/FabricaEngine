#include <cstdlib>

#include "core/logging/logger.h"
#include "core/runtime/engine_runtime.h"
#include "core/window/glfw_window_backend.h"

int main() {
  Fabrica::Core::Logging::Logger::Instance().Initialize();

  Fabrica::Core::Runtime::RuntimeConfig config;
  config.window_config.width = 1280;
  config.window_config.height = 720;
  config.window_config.title = "FabricaEngine Runtime Sample";
  config.window_backend = std::make_unique<Fabrica::Core::Window::GlfwWindowBackend>();

  config.callbacks.start = Fabrica::Core::Invocable<Fabrica::Core::Status()>([]() {
    FABRICA_LOG(Game, Info) << "[Game] Runtime sample started";
    return Fabrica::Core::Status::Ok();
  });

  config.callbacks.update =
      Fabrica::Core::Invocable<Fabrica::Core::Status(const Fabrica::Core::Runtime::FrameContext&)>(
          [](const Fabrica::Core::Runtime::FrameContext&) {
            return Fabrica::Core::Status::Ok();
          });

  config.callbacks.render =
      Fabrica::Core::Invocable<Fabrica::Core::Status(const Fabrica::Core::Runtime::FrameContext&)>(
          [](const Fabrica::Core::Runtime::FrameContext&) {
            return Fabrica::Core::Status::Ok();
          });

  config.callbacks.on_event =
      Fabrica::Core::Invocable<void(const Fabrica::Core::Window::WindowEvent&)>(
          [](const Fabrica::Core::Window::WindowEvent& event) {
            if (event.type == Fabrica::Core::Window::WindowEventType::kWindowResize) {
              FABRICA_LOG(Window, Info)
                  << "[Window] Resize " << event.resize.width << "x"
                  << event.resize.height;
            }
          });

  config.callbacks.stop = Fabrica::Core::Invocable<void()>([]() {
    FABRICA_LOG(Game, Info) << "[Game] Runtime sample stopped";
  });

  Fabrica::Core::Runtime::EngineRuntime runtime;
  const Fabrica::Core::Status initialize_status = runtime.Initialize(std::move(config));
  if (!initialize_status.ok()) {
    FABRICA_LOG(Game, Error)
        << "[Game] Failed to initialize runtime: " << initialize_status.ToString();
    Fabrica::Core::Logging::Logger::Instance().Shutdown();
    return EXIT_FAILURE;
  }

  const Fabrica::Core::Status run_status = runtime.Run();
  if (!run_status.ok()) {
    FABRICA_LOG(Game, Error) << "[Game] Runtime ended with error: "
                             << run_status.ToString();
  }

  Fabrica::Core::Logging::Logger::Instance().Shutdown();
  return run_status.ok() ? EXIT_SUCCESS : EXIT_FAILURE;
}
