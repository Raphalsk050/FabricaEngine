#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#include "core/common/status.h"
#include "core/ecs/archetype_store.h"
#include "core/ecs/component_registry.h"
#include "core/ecs/ecs_types.h"
#include "core/ecs/entity_store.h"

namespace Fabrica::Core::ECS {

class EntityHandle;

struct WorldConfig {
  size_t initial_entity_capacity = 4096;
  size_t initial_archetype_entity_capacity = 256;
};

class World {
 public:
  explicit World(const WorldConfig& config = {});

  EntityId CreateEntity();
  EntityHandle CreateEntityHandle();
  EntityHandle GetEntityHandle(EntityId entity);
  EntityHandle GetEntityHandle(EntityId entity) const;
  Core::Status DestroyEntity(EntityId entity);
  bool IsAlive(EntityId entity) const;

  size_t GetAliveEntityCount() const { return entity_store_.GetAliveCount(); }
  size_t GetArchetypeCount() const;

  Core::Status ReserveEntities(size_t capacity);
  Core::Status ReserveArchetype(ComponentMask mask, size_t entity_capacity);

  template <typename... Components>
  Core::Status ReserveArchetype(size_t entity_capacity);

  Core::Status SealForRuntime();
  bool IsRuntimeSealed() const { return runtime_sealed_; }

  template <typename T>
  Core::Status RegisterComponent();

  template <typename T>
  bool IsComponentRegistered() const;

  template <typename T>
  Core::Status AddComponent(EntityId entity, const T& component);

  template <typename T>
  Core::Status RemoveComponent(EntityId entity);

  template <typename T>
  bool HasComponent(EntityId entity) const;

  template <typename T>
  T* GetComponent(EntityId entity);

  template <typename T>
  const T* GetComponent(EntityId entity) const;

  template <typename... Components, typename Fn>
  void ForEach(Fn&& fn);

 private:
  using EntityRecord = EntityStore::EntityRecord;
  using Archetype = ArchetypeStore::Archetype;

  static constexpr std::uint32_t kInvalidArchetypeIndex =
      ArchetypeStore::kInvalidArchetypeIndex;
  static constexpr ComponentTypeIndex kInvalidComponentType =
      static_cast<ComponentTypeIndex>(kMaxComponentTypes);

  static ComponentMask ComponentBit(ComponentTypeIndex component);

  std::uint32_t GetOrCreateArchetype(ComponentMask mask);
  EntityRecord* GetEntityRecord(EntityId entity);
  const EntityRecord* GetEntityRecord(EntityId entity) const;
  void RemoveRowFromArchetype(std::uint32_t archetype_index, size_t row);
  void CopySharedComponents(const Archetype& source_archetype, size_t source_row,
                            Archetype* destination_archetype,
                            size_t destination_row,
                            ComponentMask shared_component_mask);
  void* MutableComponentAt(const EntityRecord& entity_record,
                           ComponentTypeIndex component_type);
  const void* ComponentAt(const EntityRecord& entity_record,
                          ComponentTypeIndex component_type) const;

  template <typename Fn, typename... Components, size_t... I>
  static void InvokeForEach(Fn& fn, Archetype* archetype,
                            const std::array<ComponentTypeIndex,
                                             sizeof...(Components)>& component_ids,
                            EntityId entity, size_t row,
                            std::index_sequence<I...>);

  WorldConfig config_;
  bool runtime_sealed_ = false;

  EntityStore entity_store_;
  ComponentRegistry component_registry_;
  ArchetypeStore archetype_store_;
};

class EntityHandle {
 public:
  EntityHandle() = default;

  bool IsValid() const;
  EntityId Id() const { return entity_; }

  Core::Status Destroy() const;

  template <typename T, typename... Args>
  Core::Status AddComponent(Args&&... args) const;

  template <typename T>
  Core::Status RemoveComponent() const;

  template <typename T>
  bool HasComponent() const;

  template <typename T>
  T* GetComponent() const;

  template <typename T>
  const T* GetComponentConst() const;

 private:
  friend class World;

  EntityHandle(World* world, EntityId entity)
      : mutable_world_(world), world_(world), entity_(entity) {}
  EntityHandle(const World* world, EntityId entity)
      : world_(world), entity_(entity) {}

  Core::Status ValidateBoundWorld() const;
  Core::Status ValidateMutableWorld() const;

  World* mutable_world_ = nullptr;
  const World* world_ = nullptr;
  EntityId entity_ = EntityId::Invalid();
};

template <typename T>
Core::Status World::RegisterComponent() {
  return component_registry_.Register<T>(runtime_sealed_);
}

template <typename T>
bool World::IsComponentRegistered() const {
  return component_registry_.IsRegistered<T>();
}

template <typename T>
Core::Status World::AddComponent(EntityId entity, const T& component) {
  const ComponentTypeIndex component_type = component_registry_.RequireTypeIndex<T>();
  if (component_type == kInvalidComponentType) {
    return Core::Status(Core::ErrorCode::kFailedPrecondition,
                        "Component type was not registered");
  }

  EntityRecord* entity_record = GetEntityRecord(entity);
  if (entity_record == nullptr) {
    return Core::Status::NotFound("Entity not found");
  }

  const ComponentMask component_bit = ComponentBit(component_type);
  if ((entity_record->component_mask & component_bit) != 0) {
    void* destination = MutableComponentAt(*entity_record, component_type);
    std::memcpy(destination, &component, sizeof(T));
    return Core::Status::Ok();
  }

  const std::uint32_t source_archetype_index = entity_record->archetype_index;
  const size_t source_row = entity_record->row_in_archetype;

  const ComponentMask target_mask = entity_record->component_mask | component_bit;
  const std::uint32_t destination_archetype_index = GetOrCreateArchetype(target_mask);
  if (destination_archetype_index == kInvalidArchetypeIndex) {
    return Core::Status(Core::ErrorCode::kFailedPrecondition,
                        "Target archetype is unavailable in runtime-sealed mode");
  }

  Archetype& source_archetype = archetype_store_.Get(source_archetype_index);
  Archetype& destination_archetype = archetype_store_.Get(destination_archetype_index);

  size_t destination_row = 0;
  if (!destination_archetype.Append(entity, runtime_sealed_, &destination_row)) {
    return Core::Status(Core::ErrorCode::kOutOfRange,
                        "Target archetype capacity exceeded");
  }

  CopySharedComponents(source_archetype, source_row, &destination_archetype,
                       destination_row, entity_record->component_mask);

  void* new_component_destination =
      destination_archetype.MutableAt(component_type, destination_row);
  std::memcpy(new_component_destination, &component, sizeof(T));

  RemoveRowFromArchetype(source_archetype_index, source_row);

  entity_record->component_mask = target_mask;
  entity_record->archetype_index = destination_archetype_index;
  entity_record->row_in_archetype = static_cast<std::uint32_t>(destination_row);
  return Core::Status::Ok();
}

template <typename T>
Core::Status World::RemoveComponent(EntityId entity) {
  const ComponentTypeIndex component_type = component_registry_.RequireTypeIndex<T>();
  if (component_type == kInvalidComponentType) {
    return Core::Status::Ok();
  }

  EntityRecord* entity_record = GetEntityRecord(entity);
  if (entity_record == nullptr) {
    return Core::Status::NotFound("Entity not found");
  }

  const ComponentMask component_bit = ComponentBit(component_type);
  if ((entity_record->component_mask & component_bit) == 0) {
    return Core::Status::Ok();
  }

  const std::uint32_t source_archetype_index = entity_record->archetype_index;
  const size_t source_row = entity_record->row_in_archetype;
  const ComponentMask target_mask = entity_record->component_mask & ~component_bit;

  const std::uint32_t destination_archetype_index = GetOrCreateArchetype(target_mask);
  if (destination_archetype_index == kInvalidArchetypeIndex) {
    return Core::Status(Core::ErrorCode::kFailedPrecondition,
                        "Target archetype is unavailable in runtime-sealed mode");
  }

  Archetype& source_archetype = archetype_store_.Get(source_archetype_index);
  Archetype& destination_archetype = archetype_store_.Get(destination_archetype_index);

  size_t destination_row = 0;
  if (!destination_archetype.Append(entity, runtime_sealed_, &destination_row)) {
    return Core::Status(Core::ErrorCode::kOutOfRange,
                        "Target archetype capacity exceeded");
  }

  CopySharedComponents(source_archetype, source_row, &destination_archetype,
                       destination_row, target_mask);
  RemoveRowFromArchetype(source_archetype_index, source_row);

  entity_record->component_mask = target_mask;
  entity_record->archetype_index = destination_archetype_index;
  entity_record->row_in_archetype = static_cast<std::uint32_t>(destination_row);
  return Core::Status::Ok();
}

template <typename T>
bool World::HasComponent(EntityId entity) const {
  const ComponentTypeIndex component_type = component_registry_.RequireTypeIndex<T>();
  if (component_type == kInvalidComponentType) {
    return false;
  }

  const EntityRecord* entity_record = GetEntityRecord(entity);
  if (entity_record == nullptr) {
    return false;
  }

  return (entity_record->component_mask & ComponentBit(component_type)) != 0;
}

template <typename T>
T* World::GetComponent(EntityId entity) {
  const ComponentTypeIndex component_type = component_registry_.RequireTypeIndex<T>();
  if (component_type == kInvalidComponentType) {
    return nullptr;
  }

  EntityRecord* entity_record = GetEntityRecord(entity);
  if (entity_record == nullptr) {
    return nullptr;
  }

  if ((entity_record->component_mask & ComponentBit(component_type)) == 0) {
    return nullptr;
  }

  return static_cast<T*>(MutableComponentAt(*entity_record, component_type));
}

template <typename T>
const T* World::GetComponent(EntityId entity) const {
  const ComponentTypeIndex component_type = component_registry_.RequireTypeIndex<T>();
  if (component_type == kInvalidComponentType) {
    return nullptr;
  }

  const EntityRecord* entity_record = GetEntityRecord(entity);
  if (entity_record == nullptr) {
    return nullptr;
  }

  if ((entity_record->component_mask & ComponentBit(component_type)) == 0) {
    return nullptr;
  }

  return static_cast<const T*>(ComponentAt(*entity_record, component_type));
}

template <typename... Components, typename Fn>
void World::ForEach(Fn&& fn) {
  static_assert(sizeof...(Components) > 0,
                "ForEach requires at least one component type");

  const std::array<ComponentTypeIndex, sizeof...(Components)> component_ids = {
      component_registry_.RequireTypeIndex<Components>()...};
  for (ComponentTypeIndex component_id : component_ids) {
    if (component_id == kInvalidComponentType) {
      return;
    }
  }

  ComponentMask query_mask = 0;
  for (ComponentTypeIndex component_id : component_ids) {
    query_mask |= ComponentBit(component_id);
  }

  auto& callback = fn;
  for (Archetype& archetype : archetype_store_.MutableArchetypes()) {
    if ((archetype.mask & query_mask) != query_mask) {
      continue;
    }

    for (size_t row = 0; row < archetype.entities.size(); ++row) {
      InvokeForEach<decltype(callback), Components...>(
          callback, &archetype, component_ids, archetype.entities[row], row,
          std::index_sequence_for<Components...>{});
    }
  }
}

template <typename... Components>
Core::Status World::ReserveArchetype(size_t entity_capacity) {
  ComponentMask mask = 0;
  const Core::Status build_mask_status =
      component_registry_.BuildMask<Components...>(&mask);
  if (!build_mask_status.ok()) {
    return build_mask_status;
  }
  return ReserveArchetype(mask, entity_capacity);
}

template <typename Fn, typename... Components, size_t... I>
void World::InvokeForEach(
    Fn& fn, Archetype* archetype,
    const std::array<ComponentTypeIndex, sizeof...(Components)>& component_ids,
    EntityId entity, size_t row, std::index_sequence<I...>) {
  if constexpr (std::is_invocable_v<Fn, EntityId, Components&...>) {
    fn(entity,
       *static_cast<Components*>(archetype->MutableAt(component_ids[I], row))...);
  } else {
    fn(*static_cast<Components*>(archetype->MutableAt(component_ids[I], row))...);
  }
}

inline bool EntityHandle::IsValid() const {
  return world_ != nullptr && entity_.IsValid() && world_->IsAlive(entity_);
}

inline Core::Status EntityHandle::ValidateBoundWorld() const {
  if (world_ == nullptr) {
    return Core::Status(Core::ErrorCode::kFailedPrecondition,
                        "Entity handle is not attached to a world");
  }
  return Core::Status::Ok();
}

inline Core::Status EntityHandle::ValidateMutableWorld() const {
  if (mutable_world_ == nullptr) {
    return Core::Status(Core::ErrorCode::kFailedPrecondition,
                        "Entity handle is read-only for const world access");
  }
  return Core::Status::Ok();
}

inline Core::Status EntityHandle::Destroy() const {
  const Core::Status status = ValidateMutableWorld();
  if (!status.ok()) {
    return status;
  }
  return mutable_world_->DestroyEntity(entity_);
}

template <typename T, typename... Args>
Core::Status EntityHandle::AddComponent(Args&&... args) const {
  const Core::Status status = ValidateMutableWorld();
  if (!status.ok()) {
    return status;
  }
  return mutable_world_->AddComponent(entity_, T{std::forward<Args>(args)...});
}

template <typename T>
Core::Status EntityHandle::RemoveComponent() const {
  const Core::Status status = ValidateMutableWorld();
  if (!status.ok()) {
    return status;
  }
  return mutable_world_->RemoveComponent<T>(entity_);
}

template <typename T>
bool EntityHandle::HasComponent() const {
  if (world_ == nullptr) {
    return false;
  }
  return world_->HasComponent<T>(entity_);
}

template <typename T>
T* EntityHandle::GetComponent() const {
  if (mutable_world_ == nullptr) {
    return nullptr;
  }
  return mutable_world_->GetComponent<T>(entity_);
}

template <typename T>
const T* EntityHandle::GetComponentConst() const {
  if (world_ == nullptr) {
    return nullptr;
  }
  return world_->GetComponent<T>(entity_);
}

}  // namespace Fabrica::Core::ECS