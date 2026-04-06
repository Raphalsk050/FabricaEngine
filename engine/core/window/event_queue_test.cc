#include "core/common/test/test_framework.h"
#include "core/window/event_queue.h"

namespace {

using Fabrica::Core::Window::WindowEvent;
using Fabrica::Core::Window::WindowEventQueue;

FABRICA_TEST(WindowEventQueuePushAndPop) {
  WindowEventQueue<8> queue;
  const WindowEvent expected = WindowEvent::Resize(1920, 1080);
  const bool pushed = queue.Push(expected);
  FABRICA_EXPECT_TRUE(pushed);

  WindowEvent event;
  const bool popped = queue.Pop(&event);
  FABRICA_EXPECT_TRUE(popped);
  FABRICA_EXPECT_EQ(event.resize.width, 1920);
  FABRICA_EXPECT_EQ(event.resize.height, 1080);
}

}  // namespace

