#include <vector>

#include "core/common/test/test_framework.h"
#include "core/input/input_action_map.h"

namespace {

using Fabrica::Core::Input::InputActionEvent;
using Fabrica::Core::Input::InputActionMap;
using Fabrica::Core::Input::InputActionPhase;
using Fabrica::Core::Window::WindowEvent;

FABRICA_TEST(InputActionMapBindsAndUnbindsMultipleKeys) {
  InputActionMap map;

  FABRICA_EXPECT_TRUE(map.Bind("jump", 32).ok());
  FABRICA_EXPECT_TRUE(map.Bind("jump", 74).ok());
  FABRICA_EXPECT_TRUE(map.IsBound("jump", 32));
  FABRICA_EXPECT_TRUE(map.IsBound("jump", 74));

  FABRICA_EXPECT_TRUE(map.Unbind("jump", 32).ok());
  FABRICA_EXPECT_TRUE(!map.IsBound("jump", 32));
  FABRICA_EXPECT_TRUE(map.IsBound("jump", 74));

  FABRICA_EXPECT_TRUE(!map.Unbind("jump", 32).ok());
}

FABRICA_TEST(InputActionMapTracksPressedStateAcrossMultipleKeys) {
  InputActionMap map;
  std::vector<InputActionEvent> events;

  map.Bind("run", 65);
  map.Bind("run", 68);
  map.SetActionSink(Fabrica::Core::Invocable<void(const InputActionEvent&)>(
      [&events](const InputActionEvent& event) { events.push_back(event); }));

  map.HandleWindowEvent(WindowEvent::KeyDown(65, false));
  map.HandleWindowEvent(WindowEvent::KeyDown(68, false));
  FABRICA_EXPECT_TRUE(map.IsPressed("run"));

  map.HandleWindowEvent(WindowEvent::KeyUp(65));
  FABRICA_EXPECT_TRUE(map.IsPressed("run"));

  map.HandleWindowEvent(WindowEvent::KeyUp(68));
  FABRICA_EXPECT_TRUE(!map.IsPressed("run"));

  FABRICA_EXPECT_EQ(events.size(), 2u);
  FABRICA_EXPECT_EQ(events[0].phase, InputActionPhase::kStarted);
  FABRICA_EXPECT_EQ(events[1].phase, InputActionPhase::kCompleted);
}

FABRICA_TEST(InputActionMapEmitsRepeatPhase) {
  InputActionMap map;
  std::vector<InputActionEvent> events;

  map.Bind("fire", 70);
  map.SetActionSink(Fabrica::Core::Invocable<void(const InputActionEvent&)>(
      [&events](const InputActionEvent& event) { events.push_back(event); }));

  map.HandleWindowEvent(WindowEvent::KeyDown(70, false));
  map.HandleWindowEvent(WindowEvent::KeyDown(70, true));
  map.HandleWindowEvent(WindowEvent::KeyUp(70));

  FABRICA_EXPECT_EQ(events.size(), 3u);
  FABRICA_EXPECT_EQ(events[0].phase, InputActionPhase::kStarted);
  FABRICA_EXPECT_EQ(events[1].phase, InputActionPhase::kRepeated);
  FABRICA_EXPECT_EQ(events[2].phase, InputActionPhase::kCompleted);
}

}  // namespace
