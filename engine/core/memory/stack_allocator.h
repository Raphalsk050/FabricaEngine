#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Fabrica::Core::Memory {

class StackAllocator {
 public:
  struct Marker {
    size_t front_offset = 0;
    size_t back_offset = 0;
  };

  explicit StackAllocator(size_t capacity_bytes);

  void* AllocateFront(size_t size,
                      size_t alignment = alignof(std::max_align_t));
  void* AllocateBack(size_t size,
                     size_t alignment = alignof(std::max_align_t));

  Marker Mark() const;
  void Rollback(const Marker& marker);
  void Reset();

  size_t GetUsedBytes() const { return front_offset_ + (buffer_.size() - back_offset_); }
  size_t GetCapacity() const { return buffer_.size(); }
  size_t GetFrontOffset() const { return front_offset_; }
  size_t GetBackOffset() const { return back_offset_; }

 private:
  std::vector<std::byte> buffer_;
  size_t front_offset_ = 0;
  size_t back_offset_ = 0;
};

}  // namespace Fabrica::Core::Memory

