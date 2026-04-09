#pragma once

#include "core/common/invocable.h"
#include "core/window/window_types.h"

namespace Fabrica::Core::Window {

/**
 * Abstracts platform-specific window creation and event polling.
 *
 * `WindowSystem` depends on this interface to keep runtime logic independent
 * from concrete backends such as GLFW.
 */
class IWindowBackend {
 public:
  using EventSink = Core::Invocable<void(const WindowEvent&)>;

  virtual ~IWindowBackend() = default;

  /**
   * Initialize native window resources.
   *
   * @param config Window creation parameters.
   * @return True when backend is ready.
   */
  virtual bool Initialize(const WindowConfig& config) = 0;

  /// Poll native events and forward them through the configured sink.
  virtual void PollEvents() = 0;

  /// Present rendered frame to display surface.
  virtual bool PresentFrame() = 0;

  /// Return true when native window requests closure.
  virtual bool ShouldClose() const = 0;

  /// Return native window handle for integration points.
  virtual void* GetNativeHandle() const = 0;

  /// Return current framebuffer size in pixels.
  virtual Vec2i GetFramebufferSize() const = 0;

  /// Release backend-owned native resources.
  virtual void Shutdown() = 0;

  /// Configure callback sink for translated window events.
  virtual void SetEventSink(EventSink sink) = 0;
};

}  // namespace Fabrica::Core::Window
