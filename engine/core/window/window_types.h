#pragma once

#include <string>

namespace Fabrica::Core::Window {

struct Vec2i {
  int x = 0;
  int y = 0;
};

struct WindowConfig {
  int width = 1280;
  int height = 720;
  std::string title = "FabricaEngine";
  bool vsync_enabled = true;
  int monitor_index = 0;
};

enum class WindowEventType {
  kWindowResize,
  kKeyDown,
  kKeyUp,
  kMouseMove,
  kMouseButton,
  kWindowClose,
  kWindowFocus
};

struct WindowEvent {
  WindowEventType type = WindowEventType::kWindowClose;
  union {
    struct {
      int width;
      int height;
    } resize;
    struct {
      int key;
      bool repeat;
    } key;
    struct {
      float x;
      float y;
    } mouse_move;
    struct {
      int button;
      int action;
    } mouse_button;
    struct {
      bool focused;
    } focus;
  };

  static WindowEvent Resize(int width, int height) {
    WindowEvent event;
    event.type = WindowEventType::kWindowResize;
    event.resize = {.width = width, .height = height};
    return event;
  }

  static WindowEvent KeyDown(int key, bool repeat) {
    WindowEvent event;
    event.type = WindowEventType::kKeyDown;
    event.key = {.key = key, .repeat = repeat};
    return event;
  }

  static WindowEvent KeyUp(int key) {
    WindowEvent event;
    event.type = WindowEventType::kKeyUp;
    event.key = {.key = key, .repeat = false};
    return event;
  }

  static WindowEvent MouseMove(float x, float y) {
    WindowEvent event;
    event.type = WindowEventType::kMouseMove;
    event.mouse_move = {.x = x, .y = y};
    return event;
  }

  static WindowEvent MouseButton(int button, int action) {
    WindowEvent event;
    event.type = WindowEventType::kMouseButton;
    event.mouse_button = {.button = button, .action = action};
    return event;
  }

  static WindowEvent Close() {
    WindowEvent event;
    event.type = WindowEventType::kWindowClose;
    return event;
  }

  static WindowEvent Focus(bool focused) {
    WindowEvent event;
    event.type = WindowEventType::kWindowFocus;
    event.focus = {.focused = focused};
    return event;
  }
};

}  // namespace Fabrica::Core::Window

