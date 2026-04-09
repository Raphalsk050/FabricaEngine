#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/common/status.h"
#include "core/ecs/ecs_types.h"

namespace Fabrica::Core::ECS {

/**
 * Manages entity id allocation, liveness, and archetype placement metadata.
 *
 * Entity ids use generation/index encoding to invalidate stale handles after
 * destroy/reuse cycles.
 */
class EntityStore {
 public:
  /**
   * Persistent metadata tracked for each allocated entity index.
   */
  struct EntityRecord {
    std::uint32_t generation = 1;
    bool alive = false;
    ComponentMask component_mask = 0;
    std::uint32_t archetype_index = 0;
    std::uint32_t row_in_archetype = 0;
  };

  /**
   * Captures details of a provisional acquire operation.
   */
  struct AcquireResult {
    EntityId entity = EntityId::Invalid();
    std::uint32_t index = 0;
    bool reused_index = false;
    bool appended_record = false;
  };

  static constexpr std::uint32_t kInvalidEntityIndex = 0;

  /// Construct store with optional pre-reserved entity capacity.
  explicit EntityStore(size_t initial_entity_capacity = 0)
      : entity_capacity_limit_(initial_entity_capacity) {
    entity_records_.reserve(entity_capacity_limit_ + 1);
    free_entity_indices_.reserve(entity_capacity_limit_);
    entity_records_.push_back(EntityRecord{});
  }

  /// Return true when a new entity may be created under current seal policy.
  bool CanCreate(bool runtime_sealed) const {
    return !runtime_sealed || alive_entity_count_ < entity_capacity_limit_;
  }

  /**
   * Acquire a new live entity id and mutable record slot.
   */
  AcquireResult AcquireEntity() {
    AcquireResult result;

    std::uint32_t index = 0;
    bool reused_index = false;
    bool appended_record = false;

    if (!free_entity_indices_.empty()) {
      index = free_entity_indices_.back();
      free_entity_indices_.pop_back();
      reused_index = true;
    } else {
      index = static_cast<std::uint32_t>(entity_records_.size());
      entity_records_.push_back(EntityRecord{});
      appended_record = true;
    }

    EntityRecord& record = entity_records_[index];
    record.alive = true;
    if (record.generation == 0) {
      record.generation = 1;
    }
    record.component_mask = 0;
    record.archetype_index = 0;
    record.row_in_archetype = 0;

    ++alive_entity_count_;

    result.entity = MakeEntityId(index, record.generation);
    result.index = index;
    result.reused_index = reused_index;
    result.appended_record = appended_record;
    return result;
  }

  /**
   * Roll back a provisional acquire operation.
   */
  void RollbackAcquire(const AcquireResult& result) {
    if (result.entity.IsValid()) {
      EntityRecord& record = entity_records_[result.index];
      record.alive = false;
      record.component_mask = 0;
      record.archetype_index = 0;
      record.row_in_archetype = 0;
    }

    if (result.reused_index) {
      free_entity_indices_.push_back(result.index);
    } else if (result.appended_record && !entity_records_.empty()) {
      entity_records_.pop_back();
    }

    if (alive_entity_count_ > 0) {
      --alive_entity_count_;
    }
  }

  /**
   * Finalize archetype placement for a newly acquired entity.
   */
  void FinalizeCreate(const AcquireResult& result, std::uint32_t archetype_index,
                      size_t row_in_archetype) {
    EntityRecord& record = entity_records_[result.index];
    record.archetype_index = archetype_index;
    record.row_in_archetype = static_cast<std::uint32_t>(row_in_archetype);
  }

  /**
   * Destroy an entity and recycle its index.
   */
  Core::Status DestroyEntity(EntityId entity) {
    EntityRecord* record = Get(entity);
    if (record == nullptr) {
      return Core::Status::NotFound("Entity not found");
    }

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

  /**
   * Reserve entity capacity before runtime seal.
   */
  Core::Status Reserve(size_t capacity, bool runtime_sealed) {
    if (runtime_sealed) {
      return Core::Status(Core::ErrorCode::kFailedPrecondition,
                          "Cannot reserve entities after runtime seal");
    }

    if (capacity < alive_entity_count_) {
      return Core::Status::InvalidArgument(
          "Entity reserve capacity cannot be smaller than alive entity count");
    }

    entity_capacity_limit_ = capacity;
    entity_records_.reserve(entity_capacity_limit_ + 1);
    free_entity_indices_.reserve(entity_capacity_limit_);
    return Core::Status::Ok();
  }

  /// Freeze capacity policy for runtime mode.
  void SealForRuntime() {
    if (entity_capacity_limit_ < alive_entity_count_) {
      entity_capacity_limit_ = alive_entity_count_;
    }
    free_entity_indices_.reserve(entity_capacity_limit_);
  }

  /// Return mutable record pointer for a live entity id.
  EntityRecord* Get(EntityId entity) {
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

  /// Return const record pointer for a live entity id.
  const EntityRecord* Get(EntityId entity) const {
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

  /// Return true when entity id currently resolves to a live record.
  bool IsAlive(EntityId entity) const { return Get(entity) != nullptr; }

  /// Return number of currently alive entities.
  size_t GetAliveCount() const { return alive_entity_count_; }

  /// Decode index bits from an encoded entity id.
  static std::uint32_t EntityIndex(EntityId entity) {
    return static_cast<std::uint32_t>(entity.Value() & 0xFFFFFFFFull);
  }

  /// Decode generation bits from an encoded entity id.
  static std::uint32_t EntityGeneration(EntityId entity) {
    return static_cast<std::uint32_t>(entity.Value() >> 32);
  }

  /// Compose an entity id from index and generation.
  static EntityId MakeEntityId(std::uint32_t index, std::uint32_t generation) {
    const std::uint64_t encoded =
        (static_cast<std::uint64_t>(generation) << 32) | index;
    return EntityId(encoded);
  }

 private:
  std::uint32_t alive_entity_count_ = 0;
  size_t entity_capacity_limit_ = 0;

  std::vector<EntityRecord> entity_records_;
  std::vector<std::uint32_t> free_entity_indices_;
};

}  // namespace Fabrica::Core::ECS
