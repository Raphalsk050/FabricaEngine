#pragma once

#include "core/window/iwindow_backend.h"

namespace Fabrica::Core::Window {

class GlfwWindowBackend final : public IWindowBackend {
 public:
  GlfwWindowBackend() = default;
  ~GlfwWindowBackend() override = default;

  bool Initialize(const WindowConfig& config) override;
  void PollEvents() override;
  bool PresentFrame() override;
  bool ShouldClose() const override;
  void* GetNativeHandle() const override;
  Vec2i GetFramebufferSize() const override;
  void Shutdown() override;
  void SetEventSink(EventSink sink) override;

 private:
  WindowConfig config_;
  EventSink event_sink_;
  void* native_window_ = nullptr;
  bool should_close_ = false;
  Vec2i framebuffer_size_{};
};

}  // namespace Fabrica::Core::Window
