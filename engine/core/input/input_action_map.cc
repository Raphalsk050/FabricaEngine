#include "core/input/input_action_map.h"

#include <algorithm>

namespace Fabrica::Core::Input {

Core::Status InputActionMap::Bind(std::string_view action, int key) {
  const Core::Status validate_status = ValidateActionName(action);
  if (!validate_status.ok()) {
    return validate_status;
  }

  std::string action_name(action);
  std::vector<int>& keys = action_to_keys_[action_name];
  if (std::find(keys.begin(), keys.end(), key) != keys.end()) {
    return Core::Status(Core::ErrorCode::kAlreadyExists,
                        "Action is already bound to the requested key");
  }

  keys.push_back(key);

  std::vector<std::string>& actions = key_to_actions_[key];
  if (std::find(actions.begin(), actions.end(), action_name) == actions.end()) {
    actions.push_back(action_name);
  }

  if (pressed_keys_.contains(key)) {
    size_t& pressed_count = pressed_count_by_action_[action_name];
    ++pressed_count;
    if (pressed_count == 1) {
      EmitEvent(action_name, key, InputActionPhase::kStarted);
    }
  }

  return Core::Status::Ok();
}

Core::Status InputActionMap::Unbind(std::string_view action, int key) {
  const Core::Status validate_status = ValidateActionName(action);
  if (!validate_status.ok()) {
    return validate_status;
  }

  const std::string action_name(action);
  const auto action_it = action_to_keys_.find(action_name);
  if (action_it == action_to_keys_.end()) {
    return Core::Status::NotFound("Input action is not registered");
  }

  std::vector<int>& keys = action_it->second;
  const auto key_it = std::find(keys.begin(), keys.end(), key);
  if (key_it == keys.end()) {
    return Core::Status::NotFound("Input action is not bound to the requested key");
  }

  keys.erase(key_it);
  if (keys.empty()) {
    action_to_keys_.erase(action_it);
  }

  const auto key_binding_it = key_to_actions_.find(key);
  if (key_binding_it != key_to_actions_.end()) {
    std::vector<std::string>& action_names = key_binding_it->second;
    action_names.erase(
        std::remove(action_names.begin(), action_names.end(), action_name),
        action_names.end());
    if (action_names.empty()) {
      key_to_actions_.erase(key_binding_it);
    }
  }

  RebuildPressedState();
  return Core::Status::Ok();
}

void InputActionMap::UnbindAll(std::string_view action) {
  if (!ValidateActionName(action).ok()) {
    return;
  }

  const std::string action_name(action);
  const auto action_it = action_to_keys_.find(action_name);
  if (action_it == action_to_keys_.end()) {
    return;
  }

  for (int key : action_it->second) {
    const auto key_binding_it = key_to_actions_.find(key);
    if (key_binding_it == key_to_actions_.end()) {
      continue;
    }

    std::vector<std::string>& action_names = key_binding_it->second;
    action_names.erase(
        std::remove(action_names.begin(), action_names.end(), action_name),
        action_names.end());
    if (action_names.empty()) {
      key_to_actions_.erase(key_binding_it);
    }
  }

  action_to_keys_.erase(action_it);
  RebuildPressedState();
}

bool InputActionMap::IsBound(std::string_view action, int key) const {
  const std::string action_name(action);
  const auto action_it = action_to_keys_.find(action_name);
  if (action_it == action_to_keys_.end()) {
    return false;
  }

  const std::vector<int>& keys = action_it->second;
  return std::find(keys.begin(), keys.end(), key) != keys.end();
}

bool InputActionMap::IsPressed(std::string_view action) const {
  const std::string action_name(action);
  const auto pressed_it = pressed_count_by_action_.find(action_name);
  return pressed_it != pressed_count_by_action_.end() && pressed_it->second > 0;
}

void InputActionMap::SetActionSink(ActionSink sink) { action_sink_ = std::move(sink); }

void InputActionMap::HandleWindowEvent(const Window::WindowEvent& event) {
  if (event.type == Window::WindowEventType::kKeyDown) {
    const int key = event.key.key;

    const auto actions_it = key_to_actions_.find(key);
    if (actions_it == key_to_actions_.end()) {
      return;
    }

    if (event.key.repeat) {
      for (const std::string& action : actions_it->second) {
        EmitEvent(action, key, InputActionPhase::kRepeated);
      }
      return;
    }

    const bool inserted = pressed_keys_.insert(key).second;
    if (!inserted) {
      return;
    }

    for (const std::string& action : actions_it->second) {
      size_t& pressed_count = pressed_count_by_action_[action];
      ++pressed_count;
      if (pressed_count == 1) {
        EmitEvent(action, key, InputActionPhase::kStarted);
      }
    }
    return;
  }

  if (event.type == Window::WindowEventType::kKeyUp) {
    const int key = event.key.key;
    const auto actions_it = key_to_actions_.find(key);
    if (actions_it == key_to_actions_.end()) {
      return;
    }

    if (pressed_keys_.erase(key) == 0) {
      return;
    }

    for (const std::string& action : actions_it->second) {
      const auto count_it = pressed_count_by_action_.find(action);
      if (count_it == pressed_count_by_action_.end()) {
        continue;
      }

      if (count_it->second > 0) {
        --count_it->second;
      }

      if (count_it->second == 0) {
        pressed_count_by_action_.erase(count_it);
        EmitEvent(action, key, InputActionPhase::kCompleted);
      }
    }
  }
}

Core::Status InputActionMap::ValidateActionName(std::string_view action) {
  if (action.empty()) {
    return Core::Status::InvalidArgument("Input action name cannot be empty");
  }
  return Core::Status::Ok();
}

void InputActionMap::RebuildPressedState() {
  pressed_count_by_action_.clear();

  for (int key : pressed_keys_) {
    const auto key_binding_it = key_to_actions_.find(key);
    if (key_binding_it == key_to_actions_.end()) {
      continue;
    }

    for (const std::string& action_name : key_binding_it->second) {
      ++pressed_count_by_action_[action_name];
    }
  }
}

void InputActionMap::EmitEvent(const std::string& action, int key,
                               InputActionPhase phase) {
  if (!action_sink_) {
    return;
  }

  action_sink_(InputActionEvent{
      .action = action,
      .key = key,
      .phase = phase,
  });
}

}  // namespace Fabrica::Core::Input
