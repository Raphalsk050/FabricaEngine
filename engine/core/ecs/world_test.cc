#include "core/common/test/test_framework.h"
#include "core/ecs/world.h"

namespace {

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

struct Health {
  int value = 0;
};

FABRICA_TEST(ECSWorldRegistersAndReadsComponents) {
  Fabrica::Core::ECS::World world;
  FABRICA_EXPECT_TRUE(world.RegisterComponent<Transform>().ok());

  const auto entity = world.CreateEntity();
  const Fabrica::Core::Status add_status =
      world.AddComponent(entity, Transform{.x = 10.0f, .y = 20.0f, .z = 30.0f});

  FABRICA_EXPECT_TRUE(add_status.ok());
  FABRICA_EXPECT_TRUE(world.HasComponent<Transform>(entity));

  Transform* transform = world.GetComponent<Transform>(entity);
  FABRICA_EXPECT_TRUE(transform != nullptr);
  FABRICA_EXPECT_EQ(transform->x, 10.0f);
  FABRICA_EXPECT_EQ(transform->y, 20.0f);
  FABRICA_EXPECT_EQ(transform->z, 30.0f);
}

FABRICA_TEST(ECSWorldMigratesEntityAcrossArchetypesKeepingData) {
  Fabrica::Core::ECS::World world;
  world.RegisterComponent<Transform>();
  world.RegisterComponent<Velocity>();

  const auto entity = world.CreateEntity();
  world.AddComponent(entity, Transform{.x = 1.0f, .y = 2.0f, .z = 3.0f});

  const size_t archetypes_before = world.GetArchetypeCount();
  const Fabrica::Core::Status add_status =
      world.AddComponent(entity, Velocity{.x = 4.0f, .y = 5.0f, .z = 6.0f});

  FABRICA_EXPECT_TRUE(add_status.ok());
  FABRICA_EXPECT_TRUE(world.GetArchetypeCount() >= archetypes_before);

  const Transform* transform = world.GetComponent<Transform>(entity);
  const Velocity* velocity = world.GetComponent<Velocity>(entity);
  FABRICA_EXPECT_TRUE(transform != nullptr);
  FABRICA_EXPECT_TRUE(velocity != nullptr);
  FABRICA_EXPECT_EQ(transform->x, 1.0f);
  FABRICA_EXPECT_EQ(velocity->z, 6.0f);
}

FABRICA_TEST(ECSWorldRemovesComponentAndKeepsOthers) {
  Fabrica::Core::ECS::World world;
  world.RegisterComponent<Transform>();
  world.RegisterComponent<Velocity>();

  const auto entity = world.CreateEntity();
  world.AddComponent(entity, Transform{.x = 3.0f, .y = 2.0f, .z = 1.0f});
  world.AddComponent(entity, Velocity{.x = 9.0f, .y = 8.0f, .z = 7.0f});

  const Fabrica::Core::Status remove_status = world.RemoveComponent<Velocity>(entity);
  FABRICA_EXPECT_TRUE(remove_status.ok());

  FABRICA_EXPECT_TRUE(world.HasComponent<Transform>(entity));
  FABRICA_EXPECT_TRUE(!world.HasComponent<Velocity>(entity));
  const Transform* transform = world.GetComponent<Transform>(entity);
  FABRICA_EXPECT_TRUE(transform != nullptr);
  FABRICA_EXPECT_EQ(transform->x, 3.0f);
}

FABRICA_TEST(ECSWorldForEachIteratesOnlyMatchingArchetypes) {
  Fabrica::Core::ECS::World world;
  world.RegisterComponent<Transform>();
  world.RegisterComponent<Health>();

  const auto a = world.CreateEntity();
  const auto b = world.CreateEntity();
  world.AddComponent(a, Transform{.x = 5.0f, .y = 0.0f, .z = 0.0f});
  world.AddComponent(a, Health{.value = 100});
  world.AddComponent(b, Transform{.x = 7.0f, .y = 0.0f, .z = 0.0f});

  int visited = 0;
  int health_total = 0;
  world.ForEach<Transform, Health>(
      [&visited, &health_total](Fabrica::Core::ECS::EntityId,
                                 Transform& transform, Health& health) {
        ++visited;
        health_total += health.value;
        transform.x += 1.0f;
      });

  FABRICA_EXPECT_EQ(visited, 1);
  FABRICA_EXPECT_EQ(health_total, 100);
  const Transform* transform_a = world.GetComponent<Transform>(a);
  const Transform* transform_b = world.GetComponent<Transform>(b);
  FABRICA_EXPECT_EQ(transform_a->x, 6.0f);
  FABRICA_EXPECT_EQ(transform_b->x, 7.0f);
}

FABRICA_TEST(ECSWorldRejectsStaleEntityHandles) {
  Fabrica::Core::ECS::World world;
  world.RegisterComponent<Health>();

  const auto first = world.CreateEntity();
  world.AddComponent(first, Health{.value = 10});
  FABRICA_EXPECT_TRUE(world.DestroyEntity(first).ok());

  const auto second = world.CreateEntity();
  FABRICA_EXPECT_TRUE(second != first);
  FABRICA_EXPECT_TRUE(!world.IsAlive(first));

  const Fabrica::Core::Status stale_status =
      world.AddComponent(first, Health{.value = 20});
  FABRICA_EXPECT_TRUE(!stale_status.ok());
}

}  // namespace
