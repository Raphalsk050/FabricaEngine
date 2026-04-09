#pragma once

#include <cstddef>
#include <cstdint>

#include "core/common/typed_id.h"

namespace Fabrica::Core::ECS {

/**
 * Maximum number of distinct registered component types.
 */
constexpr size_t kMaxComponentTypes = 64;

/**
 * Bitmask representing a component-set signature.
 */
using ComponentMask = std::uint64_t;

/**
 * Dense index assigned to each registered component type.
 */
using ComponentTypeIndex = std::uint8_t;

struct EntityIdTag {};

/**
 * Strongly typed entity identifier encoded as generation/index pair.
 */
using EntityId = Core::TypedId<EntityIdTag, std::uint64_t>;

}  // namespace Fabrica::Core::ECS
