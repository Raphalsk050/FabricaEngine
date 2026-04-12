#pragma once

#include <memory>
#include <string>

#include "core/common/status.h"
#include "renderer/CameraData.h"
#include "renderer/LightTypes.h"
#include "renderer/MeshData.h"
#include "renderer/ShaderKey.h"
#include "rhi/IRHIContext.h"
#include "rhi/RenderGraph.h"

namespace Fabrica::Renderer {

struct RendererConfig {
  std::string shader_root;
  RHI::ERHITextureFormat hdr_color_format = RHI::ERHITextureFormat::R16G16B16A16_FLOAT;
  RHI::ERHITextureFormat swapchain_color_format = RHI::ERHITextureFormat::B8G8R8A8_UNORM;
};

class Renderer {
 public:
  Renderer();
  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  [[nodiscard]] Core::Status Initialize(RHI::IRHIContext& rhi,
                                        RendererConfig config = {});
  void Shutdown();

  [[nodiscard]] Core::Status BeginFrame(const CameraData& camera,
                                        const LightEnvironment& lights);
  void Submit(RenderableSpan renderables);
  [[nodiscard]] Core::Status EndFrame();

  [[nodiscard]] bool IsInitialized() const;

 private:
  struct State;
  std::unique_ptr<State> state_;
};

}  // namespace Fabrica::Renderer

