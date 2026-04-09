#include "core/common/status.h"
#include "core/ecs/world.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

using Fabrica::Core::ECS::EntityId;
using Fabrica::Core::ECS::World;
using Fabrica::Core::Status;

struct Transform {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct Velocity {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

/**
 * Compare floating-point values with a small tolerance.
 */
bool NearlyEqual(float lhs, float rhs) {
  return std::fabs(lhs - rhs) < 0.0001f;
}

/**
 * Demonstrate component registration, archetype migration, and world sealing.
 *
 * @return `Status::Ok()` when ECS contracts are respected.
 */
Status RunEcsDemo() {
  World world;
  Status status = world.RegisterComponent<Transform>();
  if (!status.ok()) {
    return status;
  }

  status = world.RegisterComponent<Velocity>();
  if (!status.ok()) {
    return status;
  }

  auto player = world.CreateEntityHandle();
  status = player.AddComponent<Transform>(1.0f, 2.0f, 3.0f);
  if (!status.ok()) {
    return status;
  }

  status = player.AddComponent<Velocity>(0.5f, 0.0f, 0.0f);
  if (!status.ok()) {
    return status;
  }

  world.ForEach<Transform, Velocity>(
      [](EntityId, Transform& transform, Velocity& velocity) {
        transform.x += velocity.x;
        transform.y += velocity.y;
        transform.z += velocity.z;
      });

  const Transform* transform = player.GetComponentConst<Transform>();
  if (transform == nullptr || !NearlyEqual(transform->x, 1.5f)) {
    return Status::Internal("ECS system pass did not update Transform.x");
  }

  World runtime_world;
  runtime_world.RegisterComponent<Transform>();
  runtime_world.ReserveEntities(1);
  runtime_world.ReserveArchetype<Transform>(1);
  runtime_world.SealForRuntime();

  const auto first_entity = runtime_world.CreateEntity();
  const auto second_entity = runtime_world.CreateEntity();
  if (!first_entity.IsValid() || second_entity.IsValid()) {
    return Status::Internal(
        "Sealed world should cap entity creation at reserved capacity");
  }

  return Status::Ok();
}

}  // namespace

int main() {
  const Status status = RunEcsDemo();
  if (!status.ok()) {
    std::cerr << "[ecs_sample] Failed: " << status.ToString() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "[ecs_sample] Archetype migration and runtime seal checks passed\n";
  return EXIT_SUCCESS;
}
