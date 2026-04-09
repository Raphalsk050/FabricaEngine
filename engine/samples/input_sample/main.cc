#include "core/common/status.h"
#include "core/input/input_action_map.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

using Fabrica::Core::Input::InputActionEvent;
using Fabrica::Core::Input::InputActionMap;
using Fabrica::Core::Input::InputActionPhase;
using Fabrica::Core::Status;
using Fabrica::Core::Window::WindowEvent;

/**
 * Demonstrate input binding, pressed-state tracking, and phase transitions.
 *
 * @return `Status::Ok()` when action events follow expected lifecycle order.
 */
Status RunInputDemo() {
  InputActionMap input_actions;
  std::vector<InputActionEvent> observed_events;

  Status status = input_actions.Bind("jump", 32);
  if (!status.ok()) {
    return status;
  }

  status = input_actions.Bind("jump", 74);
  if (!status.ok()) {
    return status;
  }

  input_actions.SetActionSink(Fabrica::Core::Invocable<void(const InputActionEvent&)>(
      [&observed_events](const InputActionEvent& event) {
        observed_events.push_back(event);
      }));

  input_actions.HandleWindowEvent(WindowEvent::KeyDown(32, false));
  input_actions.HandleWindowEvent(WindowEvent::KeyDown(32, true));
  input_actions.HandleWindowEvent(WindowEvent::KeyDown(74, false));
  input_actions.HandleWindowEvent(WindowEvent::KeyUp(32));
  input_actions.HandleWindowEvent(WindowEvent::KeyUp(74));

  if (input_actions.IsPressed("jump")) {
    return Status::Internal("Jump action should be released after key-up events");
  }

  if (observed_events.size() != 3) {
    return Status::Internal("Expected exactly 3 translated action events");
  }

  if (observed_events[0].phase != InputActionPhase::kStarted ||
      observed_events[1].phase != InputActionPhase::kRepeated ||
      observed_events[2].phase != InputActionPhase::kCompleted) {
    return Status::Internal("Input phase sequence does not match expected transitions");
  }

  return Status::Ok();
}

}  // namespace

int main() {
  const Status status = RunInputDemo();
  if (!status.ok()) {
    std::cerr << "[input_sample] Failed: " << status.ToString() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "[input_sample] Action binding and phase transitions completed successfully\n";
  return EXIT_SUCCESS;
}
