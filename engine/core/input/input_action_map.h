#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/common/invocable.h"
#include "core/common/status.h"
#include "core/window/window_types.h"

namespace Fabrica::Core::Input {

/**
 * Describes the lifecycle of a logical input action.
 */
enum class InputActionPhase {
  kStarted,
  ///< Action transitioned from inactive to active.
  kRepeated,
  ///< Action remained active across repeated key events.
  kCompleted,
  ///< Action transitioned from active to inactive.
};

/**
 * Represents a mapped input action event delivered to gameplay.
 */
struct InputActionEvent {
  std::string action;
  ///< Logical action name.

  int key = 0;
  ///< Backend key code that triggered the action.

  InputActionPhase phase = InputActionPhase::kStarted;
  ///< Transition phase for the action.
};

/**
 * Translates window key events into named gameplay actions.
 *
 * The map supports many-to-many bindings between action names and key codes and
 * tracks pressed state to emit started/repeated/completed transitions.
 */
class InputActionMap {
 public:
  using ActionSink = Core::Invocable<void(const InputActionEvent&)>;

  /**
   * Bind one key to an action name.
   *
   * @param action Non-empty action name.
   * @param key Backend key code.
   */
  Core::Status Bind(std::string_view action, int key);

  /**
   * Remove one key binding from an action name.
   */
  Core::Status Unbind(std::string_view action, int key);

  /// Remove all bindings for one action name.
  void UnbindAll(std::string_view action);

  /// Return true when the key is bound to the action.
  bool IsBound(std::string_view action, int key) const;

  /// Return true when any bound key is currently pressed.
  bool IsPressed(std::string_view action) const;

  /**
   * Set sink callback used to emit translated action events.
   */
  void SetActionSink(ActionSink sink);

  /**
   * Consume one window event and emit action events as needed.
   */
  void HandleWindowEvent(const Window::WindowEvent& event);

 private:
  /// Validate action name format at API boundaries.
  static Core::Status ValidateActionName(std::string_view action);

  /// Recompute per-action pressed counters from currently pressed keys.
  void RebuildPressedState();

  /// Emit one translated action event when sink is configured.
  void EmitEvent(const std::string& action, int key, InputActionPhase phase);

  std::unordered_map<std::string, std::vector<int>> action_to_keys_;
  std::unordered_map<int, std::vector<std::string>> key_to_actions_;
  std::unordered_set<int> pressed_keys_;
  std::unordered_map<std::string, size_t> pressed_count_by_action_;
  ActionSink action_sink_;
};

}  // namespace Fabrica::Core::Input
