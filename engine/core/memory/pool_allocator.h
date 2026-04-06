#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace Fabrica::Core::Memory {

template <typename T, size_t BlockSize = 4096>
class PoolAllocator {
 public:
  explicit PoolAllocator(size_t initial_capacity)
      : slots_(initial_capacity) {
    static_assert(BlockSize > 0);
    static_assert(BlockSize >= sizeof(Slot), "BlockSize must be >= slot size");
    for (size_t i = 0; i < slots_.size(); ++i) {
      slots_[i].next_free = (i + 1 < slots_.size()) ? (i + 1) : kInvalidIndex;
    }
    free_head_ = slots_.empty() ? kInvalidIndex : 0;
    free_count_ = slots_.size();
  }

  ~PoolAllocator() {
    for (Slot& slot : slots_) {
      if (slot.occupied) {
        reinterpret_cast<T*>(&slot.storage)->~T();
        slot.occupied = false;
      }
    }
  }

  template <typename... Args>
  T* Allocate(Args&&... args) {
    if (free_head_ == kInvalidIndex) {
      return nullptr;
    }
    const size_t index = free_head_;
    Slot& slot = slots_[index];
    free_head_ = slot.next_free;
    slot.next_free = kInvalidIndex;
    slot.occupied = true;
    --free_count_;

    return new (&slot.storage) T(std::forward<Args>(args)...);
  }

  void Deallocate(T* ptr) {
    if (ptr == nullptr) {
      return;
    }
    const auto* byte_ptr = reinterpret_cast<const std::byte*>(ptr);
    const auto* base = reinterpret_cast<const std::byte*>(slots_.data());
    const ptrdiff_t distance = byte_ptr - base;
    if (distance < 0) {
      return;
    }

    const size_t index = static_cast<size_t>(distance) / sizeof(Slot);
    if (index >= slots_.size()) {
      return;
    }

    Slot& slot = slots_[index];
    if (!slot.occupied) {
      return;
    }

    ptr->~T();
    slot.occupied = false;
    slot.next_free = free_head_;
    free_head_ = index;
    ++free_count_;
  }

  size_t GetFreeCount() const { return free_count_; }
  size_t GetTotalCount() const { return slots_.size(); }

 private:
  static constexpr size_t kInvalidIndex = static_cast<size_t>(-1);

  struct Slot {
    std::aligned_storage_t<sizeof(T), alignof(T)> storage;
    size_t next_free = kInvalidIndex;
    bool occupied = false;
  };

  std::vector<Slot> slots_;
  size_t free_head_ = kInvalidIndex;
  size_t free_count_ = 0;
};

}  // namespace Fabrica::Core::Memory
