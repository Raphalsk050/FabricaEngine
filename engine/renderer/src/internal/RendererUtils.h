#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

#include "core/common/status.h"
#include "renderer/MathTypes.h"
#include "rhi/RHITypes.h"

namespace Fabrica::Renderer::Internal {

[[nodiscard]] inline Fabrica::Core::Status ToStatus(RHI::RHIResult result,
                                                    std::string_view context) {
  if (result == RHI::RHIResult::Success) {
    return Fabrica::Core::Status::Ok();
  }
  return Fabrica::Core::Status::Internal(std::string(context) +
                                         " failed with RHIResult=" +
                                         RHI::ToString(result));
}

[[nodiscard]] inline Fabrica::Core::StatusOr<std::vector<std::uint32_t>> ReadSpirvWords(
    const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return Fabrica::Core::Status::NotFound("Shader file not found: " + path.string());
  }

  const std::streamsize size = file.tellg();
  if (size <= 0 || (size % static_cast<std::streamsize>(sizeof(std::uint32_t))) != 0) {
    return Fabrica::Core::Status::InvalidArgument(
        "Shader bytecode is not 32-bit aligned: " + path.string());
  }

  std::vector<std::uint32_t> words(static_cast<std::size_t>(size / sizeof(std::uint32_t)));
  file.seekg(0, std::ios::beg);
  if (!file.read(reinterpret_cast<char*>(words.data()), size)) {
    return Fabrica::Core::Status::Internal("Failed to read shader bytecode: " + path.string());
  }

  return words;
}

[[nodiscard]] inline Math::Vec3f NormalizeOrDefault(const Math::Vec3f& value,
                                                    const Math::Vec3f& fallback) {
  const Math::Vec3f normalized = Math::Normalize(value);
  const float length = Math::Length(normalized);
  return length <= 1.0e-6f ? fallback : normalized;
}

}  // namespace Fabrica::Renderer::Internal
