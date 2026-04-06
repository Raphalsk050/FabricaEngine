#include <cstdint>

#include "core/common/test/test_framework.h"
#include "core/memory/stack_allocator.h"

namespace {

using Fabrica::Core::Memory::StackAllocator;

FABRICA_TEST(StackAllocatorAllocatesFromBothEnds) {
  StackAllocator allocator(64);
  void* front = allocator.AllocateFront(16);
  void* back = allocator.AllocateBack(16);

  FABRICA_EXPECT_TRUE(front != nullptr);
  FABRICA_EXPECT_TRUE(back != nullptr);
  FABRICA_EXPECT_TRUE(front != back);
  FABRICA_EXPECT_EQ(allocator.GetUsedBytes(), 32u);
}

FABRICA_TEST(StackAllocatorRollbackRestoresOffsets) {
  StackAllocator allocator(64);
  allocator.AllocateFront(8);
  StackAllocator::Marker marker = allocator.Mark();
  allocator.AllocateFront(8);
  allocator.AllocateBack(8);

  allocator.Rollback(marker);
  FABRICA_EXPECT_EQ(allocator.GetFrontOffset(), marker.front_offset);
  FABRICA_EXPECT_EQ(allocator.GetBackOffset(), marker.back_offset);
}

FABRICA_TEST(StackAllocatorResetClearsUsage) {
  StackAllocator allocator(64);
  allocator.AllocateFront(16);
  allocator.AllocateBack(16);
  allocator.Reset();

  FABRICA_EXPECT_EQ(allocator.GetUsedBytes(), 0u);
  FABRICA_EXPECT_EQ(allocator.GetFrontOffset(), 0u);
  FABRICA_EXPECT_EQ(allocator.GetBackOffset(), 64u);
}

}  // namespace

