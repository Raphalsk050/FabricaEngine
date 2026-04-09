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

/**
 * Configures initial capacities for ECS world storage.
 */
struct WorldConfig {
  size_t initial_entity_capacity = 4096;
  ///< Initial capacity target for entity metadata storage.

  size_t initial_archetype_entity_capacity = 256;
  ///< Initial per-archetype row reservation.
};

/**
 * Coordinates entity lifetime, component registration, and archetype storage.
 *
 * The world implements an archetype ECS with generation-safe entity ids. All
 * mutations route through this facade to keep `EntityStore`,
 * `ComponentRegistry`, and `ArchetypeStore` consistent.
 *
 * Thread safety: Not thread-safe. Mutate from one owner thread.
 *
 * @see EntityHandle, ComponentRegistry, ArchetypeStore
 */
class World {
 public:
  explicit World(const WorldConfig& config = {});

  /**
   * Create and return a new entity id.
   */
  EntityId CreateEntity();

  /**
   * Create and return a mutable entity handle.
   */
  EntityHandle CreateEntityHandle();

  /**
   * Build a mutable entity handle for an existing id.
   */
  EntityHandle GetEntityHandle(EntityId entity);

  /**
   * Build a read-only entity handle for an existing id.
   */
  EntityHandle GetEntityHandle(EntityId entity) const;

  /**
   * Destroy an entity and remove all associated components.
   */
  Core::Status DestroyEntity(EntityId entity);

  /// Return true when entity id resolves to a live record.
  bool IsAlive(EntityId entity) const;

  /// Return number of alive entities.
  size_t GetAliveEntityCount() const { return entity_store_.GetAliveCount(); }

  /// Return number of allocated archetypes.
  size_t GetArchetypeCount() const;

  /**
   * Reserve entity metadata capacity before runtime seal.
   */
  Core::Status ReserveEntities(size_t capacity);

  /**
   * Reserve capacity for a concrete archetype mask before runtime seal.
   */
  Core::Status ReserveArchetype(ComponentMask mask, size_t entity_capacity);

  /**
   * Reserve capacity for archetype described by component type pack.
   */
  template <typename... Components>
  Core::Status ReserveArchetype(size_t entity_capacity);

  /**
   * Seal world layout to prevent registration/capacity growth at runtime.
   */
  Core::Status SealForRuntime();

  /// Return true when world is in runtime-sealed mode.
  bool IsRuntimeSealed() const { return runtime_sealed_; }

  /**
   * Register component type and assign dense type index.
   */
  template <typename T>
  Core::Status RegisterComponent();

  /**
   * Return true when component type is already registered.
   */
  template <typename T>
  bool IsComponentRegistered() const;

  /**
   * Add or overwrite one component on an entity.
   */
  template <typename T>
  Core::Status AddComponent(EntityId entity, const T& component);

  /**
   * Remove one component from an entity when present.
   */
  template <typename T>
  Core::Status RemoveComponent(EntityId entity);

  /**
   * Return true when entity currently owns component type `T`.
   */
  template <typename T>
  bool HasComponent(EntityId entity) const;

  /**
   * Return mutable component pointer or null when absent.
   */
  template <typename T>
  T* GetComponent(EntityId entity);

  /**
   * Return const component pointer or null when absent.
   */
  template <typename T>
  const T* GetComponent(EntityId entity) const;

  /**
   * Iterate entities that match component query.
   *
   * Callback signature may be `(EntityId, Components&...)` or
   * `(Components&...)`.
   */
  template <typename... Components, typename Fn>
  void ForEach(Fn&& fn);

 private:
  using EntityRecord = EntityStore::EntityRecord;
  using Archetype = ArchetypeStore::Archetype;

  static constexpr std::uint32_t kInvalidArchetypeIndex =
      ArchetypeStore::kInvalidArchetypeIndex;
  static constexpr ComponentTypeIndex kInvalidComponentType =
      static_cast<ComponentTypeIndex>(kMaxComponentTypes);

  /// Convert a type index into a one-hot component mask bit.
  static ComponentMask ComponentBit(ComponentTypeIndex component);

  /// Resolve or create target archetype for a component mask.
  std::uint32_t GetOrCreateArchetype(ComponentMask mask);

  /// Return mutable entity record for a live entity id.
  EntityRecord* GetEntityRecord(EntityId entity);

  /// Return const entity record for a live entity id.
  const EntityRecord* GetEntityRecord(EntityId entity) const;

  /// Remove one row from an archetype and patch moved entity metadata.
  void RemoveRowFromArchetype(std::uint32_t archetype_index, size_t row);

  /// Copy shared component columns between source and destination archetypes.
  void CopySharedComponents(const Archetype& source_archetype, size_t source_row,
                            Archetype* destination_archetype,
                            size_t destination_row,
                            ComponentMask shared_component_mask);

  /// Return mutable component pointer by record/type.
  void* MutableComponentAt(const EntityRecord& entity_record,
                           ComponentTypeIndex component_type);

  /// Return const component pointer by record/type.
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

/**
 * Lightweight facade that binds an entity id to a world instance.
 *
 * Handles may be mutable or read-only depending on how they were created.
 */
class EntityHandle {
 public:
  EntityHandle() = default;

  /// Return true when handle is attached to a live entity.
  bool IsValid() const;

  /// Return underlying entity id.
  EntityId Id() const { return entity_; }

  /// Destroy the bound entity through mutable world access.
  Core::Status Destroy() const;

  /**
   * Add one component to the bound entity.
   */
  template <typename T, typename... Args>
  Core::Status AddComponent(Args&&... args) const;

  /**
   * Remove one component from the bound entity.
   */
  template <typename T>
  Core::Status RemoveComponent() const;

  /**
   * Return true when bound entity has component type `T`.
   */
  template <typename T>
  bool HasComponent() const;

  /**
   * Return mutable component pointer, or null when unavailable.
   */
  template <typename T>
  T* GetComponent() const;

  /**
   * Return const component pointer, or null when unavailable.
   */
  template <typename T>
  const T* GetComponentConst() const;

 private:
  friend class World;

  EntityHandle(World* world, EntityId entity)
      : mutable_world_(world), world_(world), entity_(entity) {}
  EntityHandle(const World* world, EntityId entity)
      : world_(world), entity_(entity) {}

  /// Ensure handle is bound to any world instance.
  Core::Status ValidateBoundWorld() const;

  /// Ensure handle supports mutable operations.
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
