#pragma once

#include <array>
#include <atomic>
#include <cstddef>

#include "core/window/window_types.h"

namespace Fabrica::Core::Window {

/**
 * Lock-free ring buffer for window events.
 *
 * Capacity must be a power of two so index wrapping can use a bit mask.
 *
 * @tparam Capacity Number of queue slots.
 */
template <size_t Capacity = 1024>
class WindowEventQueue {
 public:
  static_assert((Capacity & (Capacity - 1)) == 0,
                "Capacity must be power of two");

  /**
   * Push one event into the queue.
   *
   * @return True on success, false when queue is full.
   */
  bool Push(const WindowEvent& event) {
    const size_t head = head_.load(std::memory_order_relaxed);
    const size_t next = (head + 1) & kMask;
    const size_t tail = tail_.load(std::memory_order_acquire);
    if (next == tail) {
      return false;
    }

    events_[head] = event;
    head_.store(next, std::memory_order_release);
    return true;
  }

  /**
   * Pop one event from the queue.
   *
   * @param out_event Output pointer for dequeued event.
   * @return True when one event was dequeued.
   */
  bool Pop(WindowEvent* out_event) {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    const size_t head = head_.load(std::memory_order_acquire);
    if (tail == head) {
      return false;
    }

    *out_event = events_[tail];
    tail_.store((tail + 1) & kMask, std::memory_order_release);
    return true;
  }

  /// Return true when queue has no available events.
  bool Empty() const {
    return tail_.load(std::memory_order_acquire) ==
           head_.load(std::memory_order_acquire);
  }

 private:
  static constexpr size_t kMask = Capacity - 1;
  std::array<WindowEvent, Capacity> events_{};
  std::atomic<size_t> head_ = 0;
  std::atomic<size_t> tail_ = 0;
};

}  // namespace Fabrica::Core::Window
