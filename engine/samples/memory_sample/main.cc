#include "core/common/status.h"
#include "core/memory/linear_allocator.h"
#include "core/memory/pool_allocator.h"
#include "core/memory/stack_allocator.h"

#include <cstdlib>
#include <iostream>

namespace {

using Fabrica::Core::Status;
using Fabrica::Core::Memory::LinearAllocator;
using Fabrica::Core::Memory::PoolAllocator;
using Fabrica::Core::Memory::StackAllocator;

struct Particle {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

/**
 * Demonstrate bump-pointer allocation for short-lived frame data.
 *
 * @return `Status::Ok()` when allocation and reset behavior match
 * expectations.
 */
Status RunLinearAllocatorDemo() {
  LinearAllocator allocator(64);
  void* first = allocator.Allocate(16);
  void* second = allocator.Allocate(24);
  void* third = allocator.Allocate(32);

  if (first == nullptr || second == nullptr) {
    return Status::Internal("Linear allocator failed before capacity");
  }

  if (third != nullptr) {
    return Status::Internal("Linear allocator should reject overflow request");
  }

  allocator.Reset();
  if (allocator.GetUsedBytes() != 0) {
    return Status::Internal("Linear allocator reset did not rewind cursor");
  }

  return Status::Ok();
}

/**
 * Demonstrate bidirectional stack allocation with marker rollback.
 *
 * @return `Status::Ok()` when front/back and rollback contracts hold.
 */
Status RunStackAllocatorDemo() {
  StackAllocator allocator(128);

  void* front = allocator.AllocateFront(32);
  const StackAllocator::Marker marker = allocator.Mark();
  void* back = allocator.AllocateBack(16);

  if (front == nullptr || back == nullptr) {
    return Status::Internal("Stack allocator failed to reserve expected blocks");
  }

  allocator.Rollback(marker);
  if (allocator.GetFrontOffset() != marker.front_offset ||
      allocator.GetBackOffset() != marker.back_offset) {
    return Status::Internal("Stack allocator rollback did not restore marker");
  }

  return Status::Ok();
}

/**
 * Demonstrate fixed-capacity object pooling for predictable allocation cost.
 *
 * @return `Status::Ok()` when pool allocation and reuse behave as expected.
 */
Status RunPoolAllocatorDemo() {
  PoolAllocator<Particle> allocator(2);

  Particle* first = allocator.Allocate();
  Particle* second = allocator.Allocate();
  Particle* overflow = allocator.Allocate();

  if (first == nullptr || second == nullptr) {
    return Status::Internal("Pool allocator failed for in-capacity allocations");
  }

  if (overflow != nullptr) {
    return Status::Internal("Pool allocator should return null when full");
  }

  allocator.Deallocate(first);
  Particle* reused = allocator.Allocate();
  if (reused == nullptr) {
    return Status::Internal("Pool allocator failed to reuse a freed slot");
  }

  allocator.Deallocate(second);
  allocator.Deallocate(reused);
  return Status::Ok();
}

}  // namespace

int main() {
  const Status linear_status = RunLinearAllocatorDemo();
  if (!linear_status.ok()) {
    std::cerr << "[memory_sample] " << linear_status.ToString() << '\n';
    return EXIT_FAILURE;
  }

  const Status stack_status = RunStackAllocatorDemo();
  if (!stack_status.ok()) {
    std::cerr << "[memory_sample] " << stack_status.ToString() << '\n';
    return EXIT_FAILURE;
  }

  const Status pool_status = RunPoolAllocatorDemo();
  if (!pool_status.ok()) {
    std::cerr << "[memory_sample] " << pool_status.ToString() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "[memory_sample] Linear, stack, and pool allocator demos passed\n";
  return EXIT_SUCCESS;
}
