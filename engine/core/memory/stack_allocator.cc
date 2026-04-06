#include "core/memory/stack_allocator.h"

#include <cassert>

namespace Fabrica::Core::Memory {

namespace {

bool IsPowerOfTwo(size_t value) { return value != 0 && ((value & (value - 1)) == 0); }

std::uintptr_t AlignUp(std::uintptr_t value, size_t alignment) {
  return (value + (alignment - 1)) & ~(static_cast<std::uintptr_t>(alignment - 1));
}

std::uintptr_t AlignDown(std::uintptr_t value, size_t alignment) {
  return value & ~(static_cast<std::uintptr_t>(alignment - 1));
}

}  // namespace

StackAllocator::StackAllocator(size_t capacity_bytes)
    : buffer_(capacity_bytes), back_offset_(capacity_bytes) {}

void* StackAllocator::AllocateFront(size_t size, size_t alignment) {
  if (size == 0) {
    return nullptr;
  }

  assert(IsPowerOfTwo(alignment));

  std::uintptr_t base = reinterpret_cast<std::uintptr_t>(buffer_.data());
  const std::uintptr_t current_ptr = base + front_offset_;
  const std::uintptr_t aligned_ptr = AlignUp(current_ptr, alignment);
  const size_t padding = static_cast<size_t>(aligned_ptr - current_ptr);
  const size_t required = padding + size;

  if (front_offset_ + required > back_offset_) {
    return nullptr;
  }

  front_offset_ += required;
  return reinterpret_cast<void*>(aligned_ptr);
}

void* StackAllocator::AllocateBack(size_t size, size_t alignment) {
  if (size == 0) {
    return nullptr;
  }

  assert(IsPowerOfTwo(alignment));

  const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(buffer_.data());
  const std::uintptr_t current_end = base + back_offset_;
  const std::uintptr_t proposed_start = AlignDown(current_end - size, alignment);

  if (proposed_start < base + front_offset_) {
    return nullptr;
  }

  back_offset_ = static_cast<size_t>(proposed_start - base);
  return reinterpret_cast<void*>(proposed_start);
}

StackAllocator::Marker StackAllocator::Mark() const {
  return Marker{.front_offset = front_offset_, .back_offset = back_offset_};
}

void StackAllocator::Rollback(const Marker& marker) {
  if (marker.front_offset > marker.back_offset || marker.back_offset > buffer_.size()) {
    return;
  }
  front_offset_ = marker.front_offset;
  back_offset_ = marker.back_offset;
}

void StackAllocator::Reset() {
  front_offset_ = 0;
  back_offset_ = buffer_.size();
}

}  // namespace Fabrica::Core::Memory

