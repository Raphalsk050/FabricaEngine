#pragma once

#include <string>
#include <string_view>

#include "core/common/status.h"
#include "rhi/IRHIContext.h"

namespace Fabrica::Renderer::Internal {

class ShaderLibrary final {
 public:
  [[nodiscard]] Fabrica::Core::Status Initialize(RHI::IRHIContext& rhi,
                                                 std::string shader_root);
  void Shutdown();

  [[nodiscard]] bool IsInitialized() const { return initialized_; }
  [[nodiscard]] RHI::RHIShaderHandle GetPbrVertexShader() const { return pbr_vertex_shader_; }
  [[nodiscard]] RHI::RHIShaderHandle GetPbrFragmentShader() const {
    return pbr_fragment_shader_;
  }
  [[nodiscard]] RHI::RHIShaderHandle GetFullscreenVertexShader() const {
    return fullscreen_vertex_shader_;
  }
  [[nodiscard]] RHI::RHIShaderHandle GetToneMappingFragmentShader() const {
    return tonemapping_fragment_shader_;
  }

 private:
  [[nodiscard]] Fabrica::Core::Status LoadShader(std::string_view file_name,
                                                 RHI::ERHIShaderStage stage,
                                                 RHI::RHIShaderHandle& destination) const;

  RHI::IRHIContext* rhi_ = nullptr;
  std::string shader_root_;
  bool initialized_ = false;
  RHI::RHIShaderHandle pbr_vertex_shader_{};
  RHI::RHIShaderHandle pbr_fragment_shader_{};
  RHI::RHIShaderHandle fullscreen_vertex_shader_{};
  RHI::RHIShaderHandle tonemapping_fragment_shader_{};
};

}  // namespace Fabrica::Renderer::Internal
