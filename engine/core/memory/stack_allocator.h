#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Fabrica::Core::Memory {

/**
 * Implements a double-ended stack allocator over one contiguous buffer.
 *
 * Front and back cursors grow toward each other, enabling two allocation
 * lifetimes in the same arena.
 */
class StackAllocator {
 public:
  /**
   * Captures a reversible allocator state.
   */
  struct Marker {
    size_t front_offset = 0;
    ///< Saved front cursor offset.

    size_t back_offset = 0;
    ///< Saved back cursor offset.
  };

  /// Allocate backing storage with fixed capacity in bytes.
  explicit StackAllocator(size_t capacity_bytes);

  /**
   * Allocate aligned bytes from the front cursor.
   */
  void* AllocateFront(size_t size,
                      size_t alignment = alignof(std::max_align_t));

  /**
   * Allocate aligned bytes from the back cursor.
   */
  void* AllocateBack(size_t size,
                     size_t alignment = alignof(std::max_align_t));

  /// Capture current front/back offsets.
  Marker Mark() const;

  /// Restore allocator cursors to a previously captured marker.
  void Rollback(const Marker& marker);

  /// Reset both cursors to an empty arena.
  void Reset();

  /// Return total bytes occupied by both stack directions.
  size_t GetUsedBytes() const { return front_offset_ + (buffer_.size() - back_offset_); }

  /// Return total arena capacity in bytes.
  size_t GetCapacity() const { return buffer_.size(); }

  /// Return current front cursor offset.
  size_t GetFrontOffset() const { return front_offset_; }

  /// Return current back cursor offset.
  size_t GetBackOffset() const { return back_offset_; }

 private:
  std::vector<std::byte> buffer_;
  size_t front_offset_ = 0;
  size_t back_offset_ = 0;
};

}  // namespace Fabrica::Core::Memory
