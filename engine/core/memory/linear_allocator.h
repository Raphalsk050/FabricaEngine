#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Fabrica::Core::Memory {

/**
 * Provides bump-pointer allocation over a preallocated byte buffer.
 *
 * `LinearAllocator` never frees individual allocations; callers reset the whole
 * arena once all transient data is no longer needed.
 */
class LinearAllocator {
 public:
  /// Allocate backing storage with fixed capacity in bytes.
  explicit LinearAllocator(size_t capacity_bytes);

  /**
   * Reserve aligned memory from the current linear cursor.
   *
   * @param size Number of bytes to allocate.
   * @param alignment Requested alignment in bytes.
   * @return Pointer to allocated block, or null on capacity exhaustion.
   */
  void* Allocate(size_t size,
                 size_t alignment = alignof(std::max_align_t));

  /// Rewind allocator cursor to the start of the buffer.
  void Reset();

  /// Return bytes currently consumed from the arena.
  size_t GetUsedBytes() const { return offset_bytes_; }

  /// Return total arena capacity in bytes.
  size_t GetCapacity() const { return buffer_.size(); }

 private:
  std::vector<std::byte> buffer_;
  size_t offset_bytes_ = 0;
};

}  // namespace Fabrica::Core::Memory
