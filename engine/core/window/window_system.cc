#include "core/window/window_system.h"

#include "core/config/config.h"
#include "core/logging/logger.h"

namespace Fabrica::Core::Window {

WindowSystem::WindowSystem(std::unique_ptr<IWindowBackend> backend)
    : backend_(std::move(backend)) {}

bool WindowSystem::Initialize(const WindowConfig& config) {
  if (!backend_) {
    return false;
  }

  backend_->SetEventSink(
      IWindowBackend::EventSink([this](const WindowEvent& event) {
        event_queue_.Push(event);
      }));

  return backend_->Initialize(config);
}

void WindowSystem::PollEvents() {
  if (backend_) {
    backend_->PollEvents();
  }
}

bool WindowSystem::PopEvent(WindowEvent* event) {
  return event_queue_.Pop(event);
}

bool WindowSystem::ShouldClose() const {
  return backend_ != nullptr && backend_->ShouldClose();
}

Vec2i WindowSystem::GetFramebufferSize() const {
  if (!backend_) {
    return {};
  }
  return backend_->GetFramebufferSize();
}

void* WindowSystem::GetNativeHandle() const {
  if (!backend_) {
    return nullptr;
  }
  return backend_->GetNativeHandle();
}

void WindowSystem::Shutdown() {
  if (backend_) {
    backend_->Shutdown();
  }
}

}  // namespace Fabrica::Core::Window

