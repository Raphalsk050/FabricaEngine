#include "core/memory/linear_allocator.h"

#include <cassert>

namespace Fabrica::Core::Memory {

namespace {

bool IsPowerOfTwo(size_t value) { return value != 0 && ((value & (value - 1)) == 0); }

}  // namespace

LinearAllocator::LinearAllocator(size_t capacity_bytes)
    : buffer_(capacity_bytes) {}

void* LinearAllocator::Allocate(size_t size, size_t alignment) {
  if (size == 0) {
    return nullptr;
  }

  assert(IsPowerOfTwo(alignment));

  const std::uintptr_t base_ptr =
      reinterpret_cast<std::uintptr_t>(buffer_.data());
  const std::uintptr_t current_ptr = base_ptr + offset_bytes_;
  const std::uintptr_t aligned_ptr =
      (current_ptr + (alignment - 1)) & ~(static_cast<std::uintptr_t>(alignment - 1));
  const size_t padding = static_cast<size_t>(aligned_ptr - current_ptr);
  const size_t required_bytes = padding + size;

  if (offset_bytes_ + required_bytes > buffer_.size()) {
    return nullptr;
  }

  offset_bytes_ += required_bytes;
  return reinterpret_cast<void*>(aligned_ptr);
}

void LinearAllocator::Reset() { offset_bytes_ = 0; }

}  // namespace Fabrica::Core::Memory
