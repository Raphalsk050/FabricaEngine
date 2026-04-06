#include <cstdint>

#include "core/common/test/test_framework.h"
#include "core/memory/pool_allocator.h"

namespace {

struct PooledValue {
  explicit PooledValue(int v) : value(v) {}
  int value = 0;
};

FABRICA_TEST(PoolAllocatorAllocatesAndDeallocatesInO1) {
  Fabrica::Core::Memory::PoolAllocator<PooledValue> allocator(2);

  PooledValue* a = allocator.Allocate(1);
  PooledValue* b = allocator.Allocate(2);
  PooledValue* c = allocator.Allocate(3);

  FABRICA_EXPECT_TRUE(a != nullptr);
  FABRICA_EXPECT_TRUE(b != nullptr);
  FABRICA_EXPECT_TRUE(c == nullptr);
  FABRICA_EXPECT_EQ(allocator.GetFreeCount(), 0u);

  allocator.Deallocate(a);
  FABRICA_EXPECT_EQ(allocator.GetFreeCount(), 1u);

  PooledValue* d = allocator.Allocate(4);
  FABRICA_EXPECT_TRUE(d != nullptr);
  FABRICA_EXPECT_EQ(d->value, 4);

  allocator.Deallocate(b);
  allocator.Deallocate(d);
  FABRICA_EXPECT_EQ(allocator.GetFreeCount(), 2u);
}

}  // namespace

