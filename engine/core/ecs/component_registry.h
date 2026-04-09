#pragma once

#include <array>
#include <cstddef>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>

#include "core/common/status.h"
#include "core/ecs/ecs_types.h"

namespace Fabrica::Core::ECS {

/**
 * Describes runtime storage metadata for one component type.
 */
struct ComponentInfo {
  size_t size = 0;
  ///< Size in bytes of one component instance.

  size_t alignment = 0;
  ///< Alignment requirement for the component type.

  const char* debug_name = "";
  ///< Non-stable debug label from RTTI.

  bool registered = false;
  ///< True when this slot is occupied by a registered type.
};

/**
 * Maps C++ component types to dense runtime indices.
 *
 * The registry owns the canonical mapping used by masks and archetype columns.
 * Registration is intentionally closed after runtime seal.
 */
class ComponentRegistry {
 public:
  /**
   * Register one component type and assign its dense index.
   *
   * @tparam T Trivially copyable/destructible component type.
   * @param runtime_sealed Whether runtime has entered immutable layout mode.
   * @return Ok when type is registered or already known.
   */
  template <typename T>
  Core::Status Register(bool runtime_sealed);

  /**
   * Check whether a component type was already registered.
   */
  template <typename T>
  bool IsRegistered() const;

  /**
   * Return component index for a registered type.
   *
   * Returns `kInvalidComponentType` when type is not registered.
   */
  template <typename T>
  ComponentTypeIndex RequireTypeIndex() const;

  /**
   * Build a component mask from a type pack.
   *
   * @param out_mask Output pointer that receives resulting mask.
   */
  template <typename... Components>
  Core::Status BuildMask(ComponentMask* out_mask) const;

  /// Return true when a dense index points to a registered component type.
  bool IsIndexRegistered(ComponentTypeIndex index) const {
    return index < kMaxComponentTypes && component_infos_[index].registered;
  }

  /// Return metadata for a given component index.
  const ComponentInfo& GetInfo(ComponentTypeIndex index) const {
    return component_infos_[index];
  }

 private:
  static constexpr ComponentTypeIndex kInvalidComponentType =
      static_cast<ComponentTypeIndex>(kMaxComponentTypes);

  template <typename T>
  static const void* ComponentTypeKey();

  template <typename T>
  ComponentTypeIndex FindTypeIndex() const;

  std::array<ComponentInfo, kMaxComponentTypes> component_infos_{};
  ComponentTypeIndex next_component_type_ = 0;
  std::unordered_map<const void*, ComponentTypeIndex> component_type_lookup_;
};

template <typename T>
Core::Status ComponentRegistry::Register(bool runtime_sealed) {
  static_assert(std::is_trivially_copyable_v<T>,
                "ECS components must be trivially copyable");
  static_assert(std::is_trivially_destructible_v<T>,
                "ECS components must be trivially destructible");

  if constexpr (alignof(T) > alignof(std::max_align_t)) {
    return Core::Status(Core::ErrorCode::kFailedPrecondition,
                        "Component alignment exceeds ECS storage guarantee");
  } else {
    if (runtime_sealed) {
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
bool ComponentRegistry::IsRegistered() const {
  return FindTypeIndex<T>() != kInvalidComponentType;
}

template <typename T>
ComponentTypeIndex ComponentRegistry::RequireTypeIndex() const {
  const ComponentTypeIndex component_type = FindTypeIndex<T>();
  if (component_type == kInvalidComponentType) {
    return kInvalidComponentType;
  }

  if (!component_infos_[component_type].registered) {
    return kInvalidComponentType;
  }

  return component_type;
}

template <typename... Components>
Core::Status ComponentRegistry::BuildMask(ComponentMask* out_mask) const {
  if (out_mask == nullptr) {
    return Core::Status::InvalidArgument("Output mask pointer cannot be null");
  }

  ComponentMask mask = 0;
  if constexpr (sizeof...(Components) > 0) {
    const std::array<ComponentTypeIndex, sizeof...(Components)> component_ids = {
        RequireTypeIndex<Components>()...};

    for (ComponentTypeIndex component_id : component_ids) {
      if (component_id == kInvalidComponentType) {
        return Core::Status(Core::ErrorCode::kFailedPrecondition,
                            "Component type was not registered");
      }
      mask |= static_cast<ComponentMask>(1ull << component_id);
    }
  }

  *out_mask = mask;
  return Core::Status::Ok();
}

template <typename T>
const void* ComponentRegistry::ComponentTypeKey() {
  static int key = 0;
  return &key;
}

template <typename T>
ComponentTypeIndex ComponentRegistry::FindTypeIndex() const {
  const auto lookup_it = component_type_lookup_.find(ComponentTypeKey<T>());
  if (lookup_it == component_type_lookup_.end()) {
    return kInvalidComponentType;
  }
  return lookup_it->second;
}

}  // namespace Fabrica::Core::ECS
