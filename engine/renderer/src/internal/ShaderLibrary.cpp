#include "internal/ShaderLibrary.h"

#include <cstring>
#include <filesystem>

#include "internal/RendererUtils.h"

namespace Fabrica::Renderer::Internal {

Fabrica::Core::Status ShaderLibrary::Initialize(RHI::IRHIContext& rhi,
                                                std::string shader_root) {
  if (initialized_) {
    return Fabrica::Core::Status::Ok();
  }

  if (shader_root.empty()) {
    return Fabrica::Core::Status::InvalidArgument(
        "Shader library requires a valid shader root directory");
  }

  rhi_ = &rhi;
  shader_root_ = std::move(shader_root);

  Fabrica::Core::Status status =
      LoadShader("pbr.vert.spv", RHI::ERHIShaderStage::Vertex, pbr_vertex_shader_);
  if (!status.ok()) {
    Shutdown();
    return status;
  }

  status = LoadShader("pbr.frag.spv", RHI::ERHIShaderStage::Fragment, pbr_fragment_shader_);
  if (!status.ok()) {
    Shutdown();
    return status;
  }

  status = LoadShader("fullscreen.vert.spv", RHI::ERHIShaderStage::Vertex,
                      fullscreen_vertex_shader_);
  if (!status.ok()) {
    Shutdown();
    return status;
  }

  status = LoadShader("tonemapping.frag.spv", RHI::ERHIShaderStage::Fragment,
                      tonemapping_fragment_shader_);
  if (!status.ok()) {
    Shutdown();
    return status;
  }

  initialized_ = true;
  return Fabrica::Core::Status::Ok();
}

void ShaderLibrary::Shutdown() {
  if (rhi_ != nullptr) {
    if (tonemapping_fragment_shader_.IsValid()) {
      rhi_->Destroy(tonemapping_fragment_shader_);
    }
    if (fullscreen_vertex_shader_.IsValid()) {
      rhi_->Destroy(fullscreen_vertex_shader_);
    }
    if (pbr_fragment_shader_.IsValid()) {
      rhi_->Destroy(pbr_fragment_shader_);
    }
    if (pbr_vertex_shader_.IsValid()) {
      rhi_->Destroy(pbr_vertex_shader_);
    }
  }

  tonemapping_fragment_shader_ = {};
  fullscreen_vertex_shader_ = {};
  pbr_fragment_shader_ = {};
  pbr_vertex_shader_ = {};
  initialized_ = false;
  rhi_ = nullptr;
  shader_root_.clear();
}

Fabrica::Core::Status ShaderLibrary::LoadShader(std::string_view file_name,
                                                RHI::ERHIShaderStage stage,
                                                RHI::RHIShaderHandle& destination) const {
  const std::filesystem::path path = std::filesystem::path(shader_root_) / std::string(file_name);
  Fabrica::Core::StatusOr<std::vector<std::uint32_t>> words = ReadSpirvWords(path);
  if (!words.ok()) {
    return words.status();
  }

  const std::vector<std::uint32_t>& shader_words = words.value();
  RHI::RHIShaderDesc shader_desc{};
  shader_desc.stage = stage;
  shader_desc.bytecode = shader_words.data();
  shader_desc.bytecode_size = shader_words.size() * sizeof(std::uint32_t);
  std::memcpy(shader_desc.entry_point, "main", sizeof("main"));

  const RHI::RHIShaderCreateResult shader_result = rhi_->CreateShader(shader_desc);
  Fabrica::Core::Status status = ToStatus(shader_result.result, "Create shader");
  if (!status.ok()) {
    return status;
  }

  destination = shader_result.handle;
  return Fabrica::Core::Status::Ok();
}

}  // namespace Fabrica::Renderer::Internal
