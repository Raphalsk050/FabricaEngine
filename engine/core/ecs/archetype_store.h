#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <vector>

#include "core/common/status.h"
#include "core/ecs/component_registry.h"
#include "core/ecs/ecs_types.h"

namespace Fabrica::Core::ECS {

class ArchetypeStore {
 public:
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

    void Reserve(size_t entity_capacity) {
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

    bool Append(EntityId entity, bool runtime_sealed, size_t* out_row) {
      const size_t row = entities.size();

      if (runtime_sealed && (row >= entity_limit || row >= entities.capacity())) {
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

    void RemoveSwapBack(size_t row, EntityId* moved_entity) {
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

    void* MutableAt(ComponentTypeIndex component, size_t row) {
      ArchetypeColumn& column = columns[component];
      return column.storage.data() + (row * column.element_size);
    }

    const void* At(ComponentTypeIndex component, size_t row) const {
      const ArchetypeColumn& column = columns[component];
      return column.storage.data() + (row * column.element_size);
    }
  };

  static constexpr std::uint32_t kInvalidArchetypeIndex =
      std::numeric_limits<std::uint32_t>::max();

  ArchetypeStore(size_t initial_archetype_entity_capacity, size_t initial_entity_capacity)
      : initial_archetype_entity_capacity_(initial_archetype_entity_capacity) {
    archetypes_.reserve(16);
    archetypes_.push_back(Archetype{});
    archetypes_[0].mask = 0;

    const size_t root_capacity =
        std::max(initial_archetype_entity_capacity, initial_entity_capacity);
    archetypes_[0].Reserve(root_capacity);
    archetype_lookup_.emplace(0, 0);
  }

  size_t GetArchetypeCount() const { return archetypes_.size(); }

  Archetype& Root() { return archetypes_[0]; }
  const Archetype& Root() const { return archetypes_[0]; }

  Archetype& Get(std::uint32_t index) { return archetypes_[index]; }
  const Archetype& Get(std::uint32_t index) const { return archetypes_[index]; }

  std::vector<Archetype>& MutableArchetypes() { return archetypes_; }
  const std::vector<Archetype>& Archetypes() const { return archetypes_; }

  Core::Status ReserveArchetype(ComponentMask mask, size_t entity_capacity,
                                bool runtime_sealed,
                                const ComponentRegistry& component_registry) {
    if (runtime_sealed) {
      return Core::Status(Core::ErrorCode::kFailedPrecondition,
                          "Cannot reserve archetypes after runtime seal");
    }

    const std::uint32_t archetype_index =
        GetOrCreateArchetype(mask, runtime_sealed, component_registry);
    if (archetype_index == kInvalidArchetypeIndex) {
      return Core::Status(Core::ErrorCode::kInternal,
                          "Failed to create archetype");
    }

    archetypes_[archetype_index].Reserve(entity_capacity);
    return Core::Status::Ok();
  }

  std::uint32_t GetOrCreateArchetype(ComponentMask mask, bool runtime_sealed,
                                     const ComponentRegistry& component_registry) {
    const auto existing = archetype_lookup_.find(mask);
    if (existing != archetype_lookup_.end()) {
      return existing->second;
    }

    if (runtime_sealed) {
      return kInvalidArchetypeIndex;
    }

    Archetype archetype;
    archetype.mask = mask;

    ComponentMask pending_mask = mask;
    while (pending_mask != 0) {
      const ComponentTypeIndex component_index =
          static_cast<ComponentTypeIndex>(std::countr_zero(pending_mask));
      pending_mask &= (pending_mask - 1);

      if (!component_registry.IsIndexRegistered(component_index)) {
        continue;
      }

      const ComponentInfo& component_info = component_registry.GetInfo(component_index);
      archetype.columns[component_index].active = true;
      archetype.columns[component_index].element_size = component_info.size;
    }

    archetype.Reserve(initial_archetype_entity_capacity_);

    const std::uint32_t new_index = static_cast<std::uint32_t>(archetypes_.size());
    archetypes_.push_back(std::move(archetype));
    archetype_lookup_.emplace(mask, new_index);
    return new_index;
  }

  void SealForRuntime() {
    for (Archetype& archetype : archetypes_) {
      archetype.entity_limit = std::max(archetype.entity_limit, archetype.entities.size());
    }
  }

  void RemoveRow(std::uint32_t archetype_index, size_t row, EntityId* moved_entity) {
    archetypes_[archetype_index].RemoveSwapBack(row, moved_entity);
  }

  void CopySharedComponents(const Archetype& source_archetype, size_t source_row,
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

 private:
  size_t initial_archetype_entity_capacity_ = 0;
  std::vector<Archetype> archetypes_;
  std::unordered_map<ComponentMask, std::uint32_t> archetype_lookup_;
};

}  // namespace Fabrica::Core::ECS