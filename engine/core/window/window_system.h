#pragma once

#include <memory>

#include "core/window/event_queue.h"
#include "core/window/iwindow_backend.h"

namespace Fabrica::Core::Window {

class WindowSystem {
 public:
  explicit WindowSystem(std::unique_ptr<IWindowBackend> backend);

  bool Initialize(const WindowConfig& config);
  void PollEvents();
  bool PopEvent(WindowEvent* event);
  bool ShouldClose() const;
  Vec2i GetFramebufferSize() const;
  void* GetNativeHandle() const;
  void Shutdown();

 private:
  std::unique_ptr<IWindowBackend> backend_;
  WindowEventQueue<> event_queue_;
};

}  // namespace Fabrica::Core::Window

