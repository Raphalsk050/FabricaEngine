#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Fabrica::Core::Memory {

class LinearAllocator {
 public:
  explicit LinearAllocator(size_t capacity_bytes);

  void* Allocate(size_t size,
                 size_t alignment = alignof(std::max_align_t));
  void Reset();

  size_t GetUsedBytes() const { return offset_bytes_; }
  size_t GetCapacity() const { return buffer_.size(); }

 private:
  std::vector<std::byte> buffer_;
  size_t offset_bytes_ = 0;
};

}  // namespace Fabrica::Core::Memory

