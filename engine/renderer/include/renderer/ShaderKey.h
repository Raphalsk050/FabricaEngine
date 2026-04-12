#pragma once

#include <compare>
#include <cstdint>

namespace Fabrica::Renderer {

struct ShaderKey {
  std::uint32_t flags = 0;

  static constexpr std::uint32_t kHasNormalMap = 1u << 0u;
  static constexpr std::uint32_t kHasEmissive = 1u << 1u;
  static constexpr std::uint32_t kHasAOMap = 1u << 2u;
  static constexpr std::uint32_t kHasClearCoat = 1u << 3u;
  static constexpr std::uint32_t kHasAnisotropy = 1u << 4u;
  static constexpr std::uint32_t kIsClothModel = 1u << 5u;
  static constexpr std::uint32_t kHasSubsurfaceColor = 1u << 6u;

  auto operator<=>(const ShaderKey&) const = default;
};

}  // namespace Fabrica::Renderer
