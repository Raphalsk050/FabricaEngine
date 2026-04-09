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

enum class InputActionPhase {
  kStarted,
  kRepeated,
  kCompleted,
};

struct InputActionEvent {
  std::string action;
  int key = 0;
  InputActionPhase phase = InputActionPhase::kStarted;
};

class InputActionMap {
 public:
  using ActionSink = Core::Invocable<void(const InputActionEvent&)>;

  Core::Status Bind(std::string_view action, int key);
  Core::Status Unbind(std::string_view action, int key);
  void UnbindAll(std::string_view action);

  bool IsBound(std::string_view action, int key) const;
  bool IsPressed(std::string_view action) const;

  void SetActionSink(ActionSink sink);
  void HandleWindowEvent(const Window::WindowEvent& event);

 private:
  static Core::Status ValidateActionName(std::string_view action);
  void RebuildPressedState();
  void EmitEvent(const std::string& action, int key, InputActionPhase phase);

  std::unordered_map<std::string, std::vector<int>> action_to_keys_;
  std::unordered_map<int, std::vector<std::string>> key_to_actions_;
  std::unordered_set<int> pressed_keys_;
  std::unordered_map<std::string, size_t> pressed_count_by_action_;
  ActionSink action_sink_;
};

}  // namespace Fabrica::Core::Input
