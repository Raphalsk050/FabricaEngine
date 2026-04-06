#pragma once

#include "core/common/invocable.h"
#include "core/window/window_types.h"

namespace Fabrica::Core::Window {

class IWindowBackend {
 public:
  using EventSink = Core::Invocable<void(const WindowEvent&)>;

  virtual ~IWindowBackend() = default;
  virtual bool Initialize(const WindowConfig& config) = 0;
  virtual void PollEvents() = 0;
  virtual bool ShouldClose() const = 0;
  virtual void* GetNativeHandle() const = 0;
  virtual Vec2i GetFramebufferSize() const = 0;
  virtual void Shutdown() = 0;

  virtual void SetEventSink(EventSink sink) = 0;
};

}  // namespace Fabrica::Core::Window

