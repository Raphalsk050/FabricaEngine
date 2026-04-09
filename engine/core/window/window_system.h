#pragma once

#include <memory>

#include "core/window/event_queue.h"
#include "core/window/iwindow_backend.h"

namespace Fabrica::Core::Window {

/**
 * Composes a window backend with an event queue facade.
 *
 * The system adapts push-based backend callbacks into a pull-based queue API
 * consumed by runtime code.
 */
class WindowSystem {
 public:
  /// Adopt ownership of a concrete backend implementation.
  explicit WindowSystem(std::unique_ptr<IWindowBackend> backend);

  /**
   * Initialize the backend and connect event sink wiring.
   */
  bool Initialize(const WindowConfig& config);

  /// Poll backend events and enqueue translated window events.
  void PollEvents();

  /// Present one frame through backend swap/present path.
  bool PresentFrame();

  /**
   * Pop one queued event.
   *
   * @param event Output pointer for dequeued event.
   * @return True when an event was available.
   */
  bool PopEvent(WindowEvent* event);

  /// Return true when backend reports close request.
  bool ShouldClose() const;

  /// Return current framebuffer size in pixels.
  Vec2i GetFramebufferSize() const;

  /// Return backend native handle for integration points.
  void* GetNativeHandle() const;

  /// Shutdown backend and clear transient queue state.
  void Shutdown();

 private:
  std::unique_ptr<IWindowBackend> backend_;
  WindowEventQueue<> event_queue_;
};

}  // namespace Fabrica::Core::Window
