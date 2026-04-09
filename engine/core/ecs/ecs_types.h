#pragma once

#include <cstddef>
#include <cstdint>

#include "core/common/typed_id.h"

namespace Fabrica::Core::ECS {

constexpr size_t kMaxComponentTypes = 64;
using ComponentMask = std::uint64_t;
using ComponentTypeIndex = std::uint8_t;

struct EntityIdTag {};
using EntityId = Core::TypedId<EntityIdTag, std::uint64_t>;

}  // namespace Fabrica::Core::ECS