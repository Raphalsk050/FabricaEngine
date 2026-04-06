#include "core/ecs/world.h"

#include <algorithm>

namespace Fabrica::Core::ECS {

World::World(const WorldConfig& config) : config_(config) {
  entity_capacity_limit_ = config_.initial_entity_capacity;
  entity_records_.reserve(entity_capacity_limit_ + 1);
  entity_records_.push_back(EntityRecord{});

  archetypes_.reserve(16);
  archetypes_.push_back(Archetype{});
  archetypes_[0].mask = 0;
  const size_t root_capacity =
      std::max(config_.initial_archetype_entity_capacity, config_.initial_entity_capacity);
  archetypes_[0].Reserve(root_capacity);
  archetype_lookup_.emplace(0, 0);
}

EntityId World::CreateEntity() {
  if (runtime_sealed_ && alive_entity_count_ >= entity_capacity_limit_) {
    return EntityId::Invalid();
  }

  std::uint32_t index = 0;
  bool reused_index = false;

  if (!free_entity_indices_.empty()) {
    index = free_entity_indices_.back();
    free_entity_indices_.pop_back();
    reused_index = true;
  } else {
    index = static_cast<std::uint32_t>(entity_records_.size());
    entity_records_.push_back(EntityRecord{});
  }

  EntityRecord& record = entity_records_[index];
  record.alive = true;
  if (record.generation == 0) {
    record.generation = 1;
  }

  record.component_mask = 0;
  record.archetype_index = 0;

  size_t row = 0;
  if (!archetypes_[0].Append(MakeEntityId(index, record.generation), runtime_sealed_,
                             &row)) {
    record.alive = false;
    record.component_mask = 0;
    record.archetype_index = 0;
    record.row_in_archetype = 0;

    if (reused_index) {
      free_entity_indices_.push_back(index);
    } else {
      entity_records_.pop_back();
    }

    return EntityId::Invalid();
  }

  record.row_in_archetype = static_cast<std::uint32_t>(row);

  ++alive_entity_count_;
  return MakeEntityId(index, record.generation);
}

Core::Status World::DestroyEntity(EntityId entity) {
  EntityRecord* record = GetEntityRecord(entity);
  if (record == nullptr) {
    return Core::Status::NotFound("Entity not found");
  }

  RemoveRowFromArchetype(record->archetype_index, record->row_in_archetype);

  record->alive = false;
  record->component_mask = 0;
  record->archetype_index = 0;
  record->row_in_archetype = 0;
  ++record->generation;
  if (record->generation == 0) {
    record->generation = 1;
  }

  free_entity_indices_.push_back(EntityIndex(entity));
  if (alive_entity_count_ > 0) {
    --alive_entity_count_;
  }
  return Core::Status::Ok();
}

bool World::IsAlive(EntityId entity) const {
  return GetEntityRecord(entity) != nullptr;
}

size_t World::GetArchetypeCount() const { return archetypes_.size(); }

Core::Status World::ReserveEntities(size_t capacity) {
  if (runtime_sealed_) {
    return Core::Status(Core::ErrorCode::kFailedPrecondition,
                        "Cannot reserve entities after runtime seal");
  }

  if (capacity < alive_entity_count_) {
    return Core::Status::InvalidArgument(
        "Entity reserve capacity cannot be smaller than alive entity count");
  }

  entity_capacity_limit_ = capacity;
  entity_records_.reserve(entity_capacity_limit_ + 1);

  if (!archetypes_.empty()) {
    archetypes_[0].Reserve(entity_capacity_limit_);
  }

  return Core::Status::Ok();
}

Core::Status World::ReserveArchetype(ComponentMask mask, size_t entity_capacity) {
  if (runtime_sealed_) {
    return Core::Status(Core::ErrorCode::kFailedPrecondition,
                        "Cannot reserve archetypes after runtime seal");
  }

  const std::uint32_t archetype_index = GetOrCreateArchetype(mask);
  if (archetype_index == kInvalidArchetypeIndex) {
    return Core::Status(Core::ErrorCode::kInternal,
                        "Failed to create archetype");
  }

  archetypes_[archetype_index].Reserve(entity_capacity);
  return Core::Status::Ok();
}

Core::Status World::SealForRuntime() {
  if (entity_capacity_limit_ < alive_entity_count_) {
    entity_capacity_limit_ = alive_entity_count_;
  }

  for (Archetype& archetype : archetypes_) {
    archetype.entity_limit =
        std::max(archetype.entity_limit, archetype.entities.size());
  }

  runtime_sealed_ = true;
  return Core::Status::Ok();
}

ComponentMask World::ComponentBit(ComponentTypeIndex component) {
  return static_cast<ComponentMask>(1ull << component);
}

std::uint32_t World::EntityIndex(EntityId entity) {
  return static_cast<std::uint32_t>(entity.Value() & 0xFFFFFFFFull);
}

std::uint32_t World::EntityGeneration(EntityId entity) {
  return static_cast<std::uint32_t>(entity.Value() >> 32);
}

EntityId World::MakeEntityId(std::uint32_t index, std::uint32_t generation) {
  const std::uint64_t encoded =
      (static_cast<std::uint64_t>(generation) << 32) | index;
  return EntityId(encoded);
}

std::uint32_t World::GetOrCreateArchetype(ComponentMask mask) {
  const auto existing = archetype_lookup_.find(mask);
  if (existing != archetype_lookup_.end()) {
    return existing->second;
  }

  if (runtime_sealed_) {
    return kInvalidArchetypeIndex;
  }

  Archetype archetype;
  archetype.mask = mask;

  ComponentMask pending_mask = mask;
  while (pending_mask != 0) {
    const ComponentTypeIndex component_index =
        static_cast<ComponentTypeIndex>(std::countr_zero(pending_mask));
    pending_mask &= (pending_mask - 1);

    const ComponentInfo& component_info = component_infos_[component_index];
    if (!component_info.registered) {
      continue;
    }

    archetype.columns[component_index].active = true;
    archetype.columns[component_index].element_size = component_info.size;
  }

  archetype.Reserve(config_.initial_archetype_entity_capacity);

  const std::uint32_t new_index = static_cast<std::uint32_t>(archetypes_.size());
  archetypes_.push_back(std::move(archetype));
  archetype_lookup_.emplace(mask, new_index);
  return new_index;
}

World::EntityRecord* World::GetEntityRecord(EntityId entity) {
  const std::uint32_t index = EntityIndex(entity);
  if (index == kInvalidEntityIndex || index >= entity_records_.size()) {
    return nullptr;
  }

  EntityRecord& record = entity_records_[index];
  if (!record.alive || record.generation != EntityGeneration(entity)) {
    return nullptr;
  }
  return &record;
}

const World::EntityRecord* World::GetEntityRecord(EntityId entity) const {
  const std::uint32_t index = EntityIndex(entity);
  if (index == kInvalidEntityIndex || index >= entity_records_.size()) {
    return nullptr;
  }

  const EntityRecord& record = entity_records_[index];
  if (!record.alive || record.generation != EntityGeneration(entity)) {
    return nullptr;
  }
  return &record;
}

void World::RemoveRowFromArchetype(std::uint32_t archetype_index, size_t row) {
  Archetype& archetype = archetypes_[archetype_index];
  EntityId moved_entity = EntityId::Invalid();
  archetype.RemoveSwapBack(row, &moved_entity);

  if (moved_entity.IsValid()) {
    EntityRecord* moved_record = GetEntityRecord(moved_entity);
    if (moved_record != nullptr) {
      moved_record->row_in_archetype = static_cast<std::uint32_t>(row);
    }
  }
}

void World::CopySharedComponents(const Archetype& source_archetype,
                                 size_t source_row,
                                 Archetype* destination_archetype,
                                 size_t destination_row,
                                 ComponentMask shared_component_mask) {
  ComponentMask pending_mask = shared_component_mask;
  while (pending_mask != 0) {
    const ComponentTypeIndex component_index =
        static_cast<ComponentTypeIndex>(std::countr_zero(pending_mask));
    pending_mask &= (pending_mask - 1);

    const void* source = source_archetype.At(component_index, source_row);
    void* destination =
        destination_archetype->MutableAt(component_index, destination_row);
    const size_t element_size =
        destination_archetype->columns[component_index].element_size;
    std::memcpy(destination, source, element_size);
  }
}

void* World::MutableComponentAt(const EntityRecord& entity_record,
                                ComponentTypeIndex component_type) {
  Archetype& archetype = archetypes_[entity_record.archetype_index];
  return archetype.MutableAt(component_type, entity_record.row_in_archetype);
}

const void* World::ComponentAt(const EntityRecord& entity_record,
                               ComponentTypeIndex component_type) const {
  const Archetype& archetype = archetypes_[entity_record.archetype_index];
  return archetype.At(component_type, entity_record.row_in_archetype);
}

void World::Archetype::Reserve(size_t entity_capacity) {
  const size_t resolved_capacity = std::max(entity_capacity, entities.size());
  entity_limit = resolved_capacity;

  entities.reserve(std::max(entities.capacity(), resolved_capacity));
  for (ArchetypeColumn& column : columns) {
    if (!column.active) {
      continue;
    }

    const size_t requested_bytes = resolved_capacity * column.element_size;
    column.storage.reserve(std::max(column.storage.capacity(), requested_bytes));
  }
}

bool World::Archetype::Append(EntityId entity, bool runtime_sealed, size_t* out_row) {
  const size_t row = entities.size();

  if (runtime_sealed && row >= entity_limit) {
    return false;
  }

  for (const ArchetypeColumn& column : columns) {
    if (!column.active) {
      continue;
    }

    const size_t required_bytes = (row + 1) * column.element_size;
    if (runtime_sealed && required_bytes > (entity_limit * column.element_size)) {
      return false;
    }

    if (runtime_sealed && required_bytes > column.storage.capacity()) {
      return false;
    }
  }

  entities.push_back(entity);

  for (ArchetypeColumn& column : columns) {
    if (!column.active) {
      continue;
    }
    column.storage.resize((row + 1) * column.element_size);
  }

  if (out_row != nullptr) {
    *out_row = row;
  }
  return true;
}

void World::Archetype::RemoveSwapBack(size_t row, EntityId* moved_entity) {
  if (entities.empty()) {
    if (moved_entity != nullptr) {
      *moved_entity = EntityId::Invalid();
    }
    return;
  }

  const size_t last_row = entities.size() - 1;
  if (row > last_row) {
    if (moved_entity != nullptr) {
      *moved_entity = EntityId::Invalid();
    }
    return;
  }

  if (row != last_row) {
    if (moved_entity != nullptr) {
      *moved_entity = entities[last_row];
    }
    entities[row] = entities[last_row];

    for (ArchetypeColumn& column : columns) {
      if (!column.active) {
        continue;
      }

      const size_t element_size = column.element_size;
      std::byte* destination = column.storage.data() + (row * element_size);
      const std::byte* source = column.storage.data() + (last_row * element_size);
      std::memcpy(destination, source, element_size);
    }
  } else if (moved_entity != nullptr) {
    *moved_entity = EntityId::Invalid();
  }

  entities.pop_back();
  for (ArchetypeColumn& column : columns) {
    if (!column.active) {
      continue;
    }
    column.storage.resize(last_row * column.element_size);
  }
}

void* World::Archetype::MutableAt(ComponentTypeIndex component, size_t row) {
  ArchetypeColumn& column = columns[component];
  return column.storage.data() + (row * column.element_size);
}

const void* World::Archetype::At(ComponentTypeIndex component, size_t row) const {
  const ArchetypeColumn& column = columns[component];
  return column.storage.data() + (row * column.element_size);
}

}  // namespace Fabrica::Core::ECS
