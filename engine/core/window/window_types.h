#pragma once

#include <string>

namespace Fabrica::Core::Window {

/**
 * Stores integer 2D coordinates.
 */
struct Vec2i {
  int x = 0;
  ///< Horizontal component.

  int y = 0;
  ///< Vertical component.
};

/**
 * Configures initial native window creation settings.
 */
struct WindowConfig {
  int width = 1280;
  ///< Initial window width in pixels.

  int height = 720;
  ///< Initial window height in pixels.

  std::string title = "FabricaEngine";
  ///< Initial window title.

  bool vsync_enabled = true;
  ///< Enables swap interval synchronization when supported.

  int monitor_index = 0;
  ///< Target monitor index for fullscreen or placement policies.
};

/**
 * Enumerates normalized window events emitted by backends.
 */
enum class WindowEventType {
  kWindowResize,
  ///< Framebuffer or window size changed.

  kKeyDown,
  ///< Key transitioned to pressed state.

  kKeyUp,
  ///< Key transitioned to released state.

  kMouseMove,
  ///< Pointer moved within window coordinates.

  kMouseButton,
  ///< Mouse button changed state.

  kWindowClose,
  ///< Window requested closure.

  kWindowFocus
  ///< Window focus changed.
};

/**
 * Represents one normalized window event payload.
 *
 * Static factories guarantee that union payload fields match event type.
 */
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

  /// Build a resize event.
  static WindowEvent Resize(int width, int height) {
    WindowEvent event;
    event.type = WindowEventType::kWindowResize;
    event.resize = {.width = width, .height = height};
    return event;
  }

  /// Build a key-down event.
  static WindowEvent KeyDown(int key, bool repeat) {
    WindowEvent event;
    event.type = WindowEventType::kKeyDown;
    event.key = {.key = key, .repeat = repeat};
    return event;
  }

  /// Build a key-up event.
  static WindowEvent KeyUp(int key) {
    WindowEvent event;
    event.type = WindowEventType::kKeyUp;
    event.key = {.key = key, .repeat = false};
    return event;
  }

  /// Build a mouse-move event.
  static WindowEvent MouseMove(float x, float y) {
    WindowEvent event;
    event.type = WindowEventType::kMouseMove;
    event.mouse_move = {.x = x, .y = y};
    return event;
  }

  /// Build a mouse-button event.
  static WindowEvent MouseButton(int button, int action) {
    WindowEvent event;
    event.type = WindowEventType::kMouseButton;
    event.mouse_button = {.button = button, .action = action};
    return event;
  }

  /// Build a close-request event.
  static WindowEvent Close() {
    WindowEvent event;
    event.type = WindowEventType::kWindowClose;
    return event;
  }

  /// Build a focus-change event.
  static WindowEvent Focus(bool focused) {
    WindowEvent event;
    event.type = WindowEventType::kWindowFocus;
    event.focus = {.focused = focused};
    return event;
  }
};

}  // namespace Fabrica::Core::Window
