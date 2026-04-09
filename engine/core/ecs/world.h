#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/common/status.h"
#include "core/common/typed_id.h"

namespace Fabrica::Core::ECS {

constexpr size_t kMaxComponentTypes = 64;
using ComponentMask = std::uint64_t;
using ComponentTypeIndex = std::uint8_t;

struct EntityIdTag {};
using EntityId = Core::TypedId<EntityIdTag, std::uint64_t>;

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

  size_t GetAliveEntityCount() const { return alive_entity_count_; }
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
  struct ComponentInfo {
    size_t size = 0;
    size_t alignment = 0;
    const char* debug_name = "";
    bool registered = false;
  };

  struct ArchetypeColumn {
    bool active = false;
    size_t element_size = 0;
    std::vector<std::byte> storage;
  };

  struct Archetype {
    ComponentMask mask = 0;
    size_t entity_limit = 0;
    std::vector<EntityId> entities;
    std::array<ArchetypeColumn, kMaxComponentTypes> columns{};

    void Reserve(size_t entity_capacity);
    bool Append(EntityId entity, bool runtime_sealed, size_t* out_row);
    void RemoveSwapBack(size_t row, EntityId* moved_entity);
    void* MutableAt(ComponentTypeIndex component, size_t row);
    const void* At(ComponentTypeIndex component, size_t row) const;
  };

  struct EntityRecord {
    std::uint32_t generation = 1;
    bool alive = false;
    ComponentMask component_mask = 0;
    std::uint32_t archetype_index = 0;
    std::uint32_t row_in_archetype = 0;
  };

  static constexpr std::uint32_t kInvalidEntityIndex = 0;
  static constexpr std::uint32_t kInvalidArchetypeIndex =
      std::numeric_limits<std::uint32_t>::max();
  static constexpr ComponentTypeIndex kInvalidComponentType =
      static_cast<ComponentTypeIndex>(kMaxComponentTypes);

  static ComponentMask ComponentBit(ComponentTypeIndex component);
  static std::uint32_t EntityIndex(EntityId entity);
  static std::uint32_t EntityGeneration(EntityId entity);
  static EntityId MakeEntityId(std::uint32_t index, std::uint32_t generation);

  template <typename T>
  static const void* ComponentTypeKey();

  template <typename T>
  ComponentTypeIndex FindComponentTypeIndex() const;

  template <typename T>
  ComponentTypeIndex RequireComponentTypeIndex() const;

  template <typename... Components>
  Core::Status BuildMask(ComponentMask* out_mask) const;

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
  std::uint32_t alive_entity_count_ = 0;
  bool runtime_sealed_ = false;
  size_t entity_capacity_limit_ = 0;

  std::vector<EntityRecord> entity_records_;
  std::vector<std::uint32_t> free_entity_indices_;

  std::array<ComponentInfo, kMaxComponentTypes> component_infos_{};
  ComponentTypeIndex next_component_type_ = 0;
  std::unordered_map<const void*, ComponentTypeIndex> component_type_lookup_;

  std::vector<Archetype> archetypes_;
  std::unordered_map<ComponentMask, std::uint32_t> archetype_lookup_;
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
  static_assert(std::is_trivially_copyable_v<T>,
                "ECS components must be trivially copyable");
  static_assert(std::is_trivially_destructible_v<T>,
                "ECS components must be trivially destructible");

  if constexpr (alignof(T) > alignof(std::max_align_t)) {
    return Core::Status(Core::ErrorCode::kFailedPrecondition,
                        "Component alignment exceeds ECS storage guarantee");
  } else {
    if (runtime_sealed_) {
      return Core::Status(Core::ErrorCode::kFailedPrecondition,
                          "Cannot register components after runtime seal");
    }

    const void* component_key = ComponentTypeKey<T>();
    if (component_type_lookup_.contains(component_key)) {
      return Core::Status::Ok();
    }

    if (next_component_type_ >= kMaxComponentTypes) {
      return Core::Status(Core::ErrorCode::kOutOfRange,
                          "Maximum component type count reached");
    }

    const ComponentTypeIndex component_type = next_component_type_;
    ++next_component_type_;

    component_type_lookup_.emplace(component_key, component_type);
    component_infos_[component_type] = ComponentInfo{
        .size = sizeof(T),
        .alignment = alignof(T),
        .debug_name = typeid(T).name(),
        .registered = true};
    return Core::Status::Ok();
  }
}

template <typename T>
bool World::IsComponentRegistered() const {
  return FindComponentTypeIndex<T>() != kInvalidComponentType;
}

template <typename T>
Core::Status World::AddComponent(EntityId entity, const T& component) {
  const ComponentTypeIndex component_type = RequireComponentTypeIndex<T>();
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

  Archetype& source_archetype = archetypes_[source_archetype_index];
  Archetype& destination_archetype = archetypes_[destination_archetype_index];

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
  const ComponentTypeIndex component_type = RequireComponentTypeIndex<T>();
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

  Archetype& source_archetype = archetypes_[source_archetype_index];
  Archetype& destination_archetype = archetypes_[destination_archetype_index];

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
  const ComponentTypeIndex component_type = RequireComponentTypeIndex<T>();
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
  const ComponentTypeIndex component_type = RequireComponentTypeIndex<T>();
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
  const ComponentTypeIndex component_type = RequireComponentTypeIndex<T>();
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
      RequireComponentTypeIndex<Components>()...};
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
  for (Archetype& archetype : archetypes_) {
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
  const Core::Status build_mask_status = BuildMask<Components...>(&mask);
  if (!build_mask_status.ok()) {
    return build_mask_status;
  }
  return ReserveArchetype(mask, entity_capacity);
}

template <typename T>
const void* World::ComponentTypeKey() {
  static int key = 0;
  return &key;
}

template <typename T>
ComponentTypeIndex World::FindComponentTypeIndex() const {
  const auto lookup_it = component_type_lookup_.find(ComponentTypeKey<T>());
  if (lookup_it == component_type_lookup_.end()) {
    return kInvalidComponentType;
  }
  return lookup_it->second;
}

template <typename T>
ComponentTypeIndex World::RequireComponentTypeIndex() const {
  const ComponentTypeIndex component_type = FindComponentTypeIndex<T>();
  if (component_type == kInvalidComponentType) {
    return kInvalidComponentType;
  }

  const ComponentInfo& info = component_infos_[component_type];
  if (!info.registered) {
    return kInvalidComponentType;
  }
  return component_type;
}

template <typename... Components>
Core::Status World::BuildMask(ComponentMask* out_mask) const {
  if (out_mask == nullptr) {
    return Core::Status::InvalidArgument("Output mask pointer cannot be null");
  }

  ComponentMask mask = 0;
  if constexpr (sizeof...(Components) > 0) {
    const std::array<ComponentTypeIndex, sizeof...(Components)> component_ids = {
        RequireComponentTypeIndex<Components>()...};

    for (ComponentTypeIndex component_id : component_ids) {
      if (component_id == kInvalidComponentType) {
        return Core::Status(Core::ErrorCode::kFailedPrecondition,
                            "Component type was not registered");
      }
      mask |= ComponentBit(component_id);
    }
  }

  *out_mask = mask;
  return Core::Status::Ok();
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
