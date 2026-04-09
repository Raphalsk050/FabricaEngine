#include "core/ecs/world.h"

namespace Fabrica::Core::ECS {

World::World(const WorldConfig& config)
    : config_(config),
      entity_store_(config.initial_entity_capacity),
      archetype_store_(config.initial_archetype_entity_capacity,
                       config.initial_entity_capacity) {}

EntityId World::CreateEntity() {
  if (!entity_store_.CanCreate(runtime_sealed_)) {
    return EntityId::Invalid();
  }

  const EntityStore::AcquireResult acquired = entity_store_.AcquireEntity();
  if (!acquired.entity.IsValid()) {
    return EntityId::Invalid();
  }

  size_t row = 0;
  if (!archetype_store_.Root().Append(acquired.entity, runtime_sealed_, &row)) {
    entity_store_.RollbackAcquire(acquired);
    return EntityId::Invalid();
  }

  entity_store_.FinalizeCreate(acquired, 0, row);
  return acquired.entity;
}

EntityHandle World::CreateEntityHandle() {
  return EntityHandle(this, CreateEntity());
}

EntityHandle World::GetEntityHandle(EntityId entity) {
  return EntityHandle(this, entity);
}

EntityHandle World::GetEntityHandle(EntityId entity) const {
  return EntityHandle(this, entity);
}

Core::Status World::DestroyEntity(EntityId entity) {
  EntityRecord* record = GetEntityRecord(entity);
  if (record == nullptr) {
    return Core::Status::NotFound("Entity not found");
  }

  RemoveRowFromArchetype(record->archetype_index, record->row_in_archetype);
  return entity_store_.DestroyEntity(entity);
}

bool World::IsAlive(EntityId entity) const {
  return entity_store_.IsAlive(entity);
}

size_t World::GetArchetypeCount() const { return archetype_store_.GetArchetypeCount(); }

Core::Status World::ReserveEntities(size_t capacity) {
  const Core::Status status = entity_store_.Reserve(capacity, runtime_sealed_);
  if (!status.ok()) {
    return status;
  }

  archetype_store_.Root().Reserve(capacity);
  return Core::Status::Ok();
}

Core::Status World::ReserveArchetype(ComponentMask mask, size_t entity_capacity) {
  return archetype_store_.ReserveArchetype(mask, entity_capacity, runtime_sealed_,
                                           component_registry_);
}

Core::Status World::SealForRuntime() {
  entity_store_.SealForRuntime();
  archetype_store_.SealForRuntime();
  runtime_sealed_ = true;
  return Core::Status::Ok();
}

ComponentMask World::ComponentBit(ComponentTypeIndex component) {
  return static_cast<ComponentMask>(1ull << component);
}

std::uint32_t World::GetOrCreateArchetype(ComponentMask mask) {
  return archetype_store_.GetOrCreateArchetype(mask, runtime_sealed_,
                                               component_registry_);
}

World::EntityRecord* World::GetEntityRecord(EntityId entity) {
  return entity_store_.Get(entity);
}

const World::EntityRecord* World::GetEntityRecord(EntityId entity) const {
  return entity_store_.Get(entity);
}

void World::RemoveRowFromArchetype(std::uint32_t archetype_index, size_t row) {
  EntityId moved_entity = EntityId::Invalid();
  archetype_store_.RemoveRow(archetype_index, row, &moved_entity);

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
  archetype_store_.CopySharedComponents(source_archetype, source_row,
                                        destination_archetype, destination_row,
                                        shared_component_mask);
}

void* World::MutableComponentAt(const EntityRecord& entity_record,
                                ComponentTypeIndex component_type) {
  Archetype& archetype = archetype_store_.Get(entity_record.archetype_index);
  return archetype.MutableAt(component_type, entity_record.row_in_archetype);
}

const void* World::ComponentAt(const EntityRecord& entity_record,
                               ComponentTypeIndex component_type) const {
  const Archetype& archetype = archetype_store_.Get(entity_record.archetype_index);
  return archetype.At(component_type, entity_record.row_in_archetype);
}

}  // namespace Fabrica::Core::ECS