#include <cstdint>

#include "core/common/test/test_framework.h"
#include "core/memory/linear_allocator.h"

namespace {

using Fabrica::Core::Memory::LinearAllocator;

FABRICA_TEST(LinearAllocatorAllocatesSequentially) {
  LinearAllocator allocator(64);
  void* a = allocator.Allocate(16, alignof(std::uint32_t));
  void* b = allocator.Allocate(16, alignof(std::uint32_t));

  FABRICA_EXPECT_TRUE(a != nullptr);
  FABRICA_EXPECT_TRUE(b != nullptr);
  FABRICA_EXPECT_TRUE(a != b);
  FABRICA_EXPECT_EQ(allocator.GetUsedBytes(), 32u);
}

FABRICA_TEST(LinearAllocatorReturnsNullWhenFull) {
  LinearAllocator allocator(32);
  void* a = allocator.Allocate(24);
  void* b = allocator.Allocate(16);

  FABRICA_EXPECT_TRUE(a != nullptr);
  FABRICA_EXPECT_TRUE(b == nullptr);
}

FABRICA_TEST(LinearAllocatorResetReusesBuffer) {
  LinearAllocator allocator(32);
  void* first = allocator.Allocate(16);
  allocator.Reset();
  void* second = allocator.Allocate(16);

  FABRICA_EXPECT_TRUE(first != nullptr);
  FABRICA_EXPECT_TRUE(second != nullptr);
  FABRICA_EXPECT_EQ(allocator.GetUsedBytes(), 16u);
}

}  // namespace

