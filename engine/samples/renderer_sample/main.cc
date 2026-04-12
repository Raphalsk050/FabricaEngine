#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "core/common/status.h"
#include "core/window/glfw_window_backend.h"
#include "core/window/window_system.h"
#include "renderer/LightTypes.h"
#include "renderer/MaterialData.h"
#include "renderer/MathTypes.h"
#include "renderer/MeshData.h"
#include "renderer/Renderer.h"
#include "rhi/RHIFactory.h"

namespace {

using Fabrica::Core::Status;
using Fabrica::Core::Window::GlfwWindowBackend;
using Fabrica::Core::Window::WindowConfig;
using Fabrica::Core::Window::WindowEvent;
using Fabrica::Core::Window::WindowEventType;
using Fabrica::Core::Window::WindowGraphicsApi;
using Fabrica::Core::Window::WindowSystem;
using Fabrica::Renderer::CameraData;
using Fabrica::Renderer::DirectionalLight;
using Fabrica::Renderer::LightEnvironment;
using Fabrica::Renderer::MaterialData;
using Fabrica::Renderer::Math::LookAt;
using Fabrica::Renderer::Math::Perspective;
using Fabrica::Renderer::Math::Radians;
using Fabrica::Renderer::Math::Vec3f;
using Fabrica::Renderer::MeshData;
using Fabrica::Renderer::PbrVertex;
using Fabrica::Renderer::RenderableItem;
using Fabrica::Renderer::Renderer;
using Fabrica::RHI::ERHIBackend;
using Fabrica::RHI::ERHIBufferUsage;
using Fabrica::RHI::ERHIIndexType;
using Fabrica::RHI::ERHIMemoryType;
using Fabrica::RHI::ERHIWindowHandleType;
using Fabrica::RHI::RHIBufferDesc;
using Fabrica::RHI::RHIBufferHandle;
using Fabrica::RHI::RHIContextDesc;
using Fabrica::RHI::RHIFactory;
using Fabrica::RHI::IRHIContext;

constexpr int kEscapeKey = 256;

struct SampleMesh {
  RHIBufferHandle vertex_buffer{};
  RHIBufferHandle index_buffer{};
  std::uint32_t vertex_count = 0;
  std::uint32_t index_count = 0;
};

[[nodiscard]] Status ToStatus(Fabrica::RHI::RHIResult result, const char* context) {
  if (result == Fabrica::RHI::RHIResult::Success) {
    return Status::Ok();
  }
  return Status::Internal(std::string(context) + " failed");
}

[[nodiscard]] int ParseFrameLimit(int argc, char** argv) {
  constexpr std::string_view kPrefix = "--frames=";
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (!argument.starts_with(kPrefix)) {
      continue;
    }

    const std::string_view value = argument.substr(kPrefix.size());
    if (value.empty()) {
      return -1;
    }
    return std::atoi(value.data());
  }
  return -1;
}

[[nodiscard]] Status CreateStaticBuffer(IRHIContext& rhi,
                                        std::span<const std::byte> bytes,
                                        ERHIBufferUsage usage,
                                        RHIBufferHandle& out_handle,
                                        std::uint32_t stride) {
  RHIBufferDesc desc{};
  desc.size = bytes.size();
  desc.stride = stride;
  desc.usage = usage;
  desc.memory = ERHIMemoryType::HostCoherent;
  desc.mapped_at_creation = true;

  const auto create_result = rhi.CreateBuffer(desc);
  Status status = ToStatus(create_result.result, "CreateBuffer");
  if (!status.ok()) {
    return status;
  }

  out_handle = create_result.handle;
  status = ToStatus(rhi.UpdateBuffer(out_handle, bytes.data(), 0, bytes.size()), "UpdateBuffer");
  if (!status.ok()) {
    rhi.Destroy(out_handle);
    out_handle = {};
  }
  return status;
}

[[nodiscard]] SampleMesh CreateQuadMesh(IRHIContext& rhi) {
  const std::array<PbrVertex, 4> vertices{{
      {{-0.8f, -0.8f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
      {{ 0.8f, -0.8f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
      {{ 0.8f,  0.8f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
      {{-0.8f,  0.8f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
  }};
  const std::array<std::uint32_t, 6> indices{{0, 1, 2, 2, 3, 0}};

  SampleMesh mesh{};
  const Status vertex_status = CreateStaticBuffer(
      rhi,
      std::as_bytes(std::span(vertices)),
      ERHIBufferUsage::VertexBuffer,
      mesh.vertex_buffer,
      sizeof(PbrVertex));
  if (!vertex_status.ok()) {
    return {};
  }

  const Status index_status = CreateStaticBuffer(
      rhi,
      std::as_bytes(std::span(indices)),
      ERHIBufferUsage::IndexBuffer,
      mesh.index_buffer,
      sizeof(std::uint32_t));
  if (!index_status.ok()) {
    rhi.Destroy(mesh.vertex_buffer);
    return {};
  }

  mesh.vertex_count = static_cast<std::uint32_t>(vertices.size());
  mesh.index_count = static_cast<std::uint32_t>(indices.size());
  return mesh;
}

void DestroyMesh(IRHIContext& rhi, SampleMesh& mesh) {
  if (mesh.index_buffer.IsValid()) {
    rhi.Destroy(mesh.index_buffer);
  }
  if (mesh.vertex_buffer.IsValid()) {
    rhi.Destroy(mesh.vertex_buffer);
  }
  mesh = {};
}

[[nodiscard]] Status RunRendererSample(int frame_limit) {
  WindowConfig window_config{};
  window_config.width = 1280;
  window_config.height = 720;
  window_config.title = "FabricaEngine Renderer Sample";
  window_config.graphics_api = WindowGraphicsApi::kNone;
  window_config.vsync_enabled = true;

  WindowSystem window_system(std::make_unique<GlfwWindowBackend>());
  if (!window_system.Initialize(window_config)) {
    return Status::Unavailable("Falha ao inicializar a janela GLFW para o sample do renderer");
  }

  RHIContextDesc context_desc{};
  context_desc.window_handle = window_system.GetNativeHandle();
  context_desc.window_handle_type = ERHIWindowHandleType::GLFW;
  context_desc.initial_extent = {
      static_cast<std::uint32_t>(window_config.width),
      static_cast<std::uint32_t>(window_config.height)};
  context_desc.enable_validation = true;
  context_desc.enable_vsync = true;
  std::memcpy(context_desc.application_name, "FabricaRendererSample",
              sizeof("FabricaRendererSample"));

  std::unique_ptr<IRHIContext> rhi = RHIFactory::CreateContext(ERHIBackend::Vulkan, context_desc);
  if (rhi == nullptr) {
    window_system.Shutdown();
    return Status::Unavailable("Falha ao criar o contexto Vulkan via RHIFactory");
  }

  Renderer renderer;
  Status status = renderer.Initialize(*rhi);
  if (!status.ok()) {
    rhi->Shutdown();
    window_system.Shutdown();
    return status;
  }

  SampleMesh sample_mesh = CreateQuadMesh(*rhi);
  if (!sample_mesh.vertex_buffer.IsValid() || !sample_mesh.index_buffer.IsValid()) {
    renderer.Shutdown();
    rhi->Shutdown();
    window_system.Shutdown();
    return Status::Internal("Falha ao criar a geometria procedural do sample");
  }

  MaterialData material{};
  material.base_color = {0.95f, 0.62f, 0.18f, 1.0f};
  material.metallic = 0.1f;
  material.roughness = 0.35f;
  material.reflectance = 0.5f;

  RenderableItem renderable{};
  renderable.mesh = MeshData{.vertex_buffer = sample_mesh.vertex_buffer,
                             .index_buffer = sample_mesh.index_buffer,
                             .vertex_count = sample_mesh.vertex_count,
                             .index_count = sample_mesh.index_count,
                             .vertex_stride = sizeof(PbrVertex),
                             .index_type = ERHIIndexType::Uint32};
  renderable.material = material;

  DirectionalLight directional_light{};
  directional_light.direction = {-0.4f, -1.0f, -0.2f};
  directional_light.illuminance_lux = 110000.0f;
  directional_light.color = {1.0f, 0.98f, 0.95f};
  const std::array<DirectionalLight, 1> directional_lights{directional_light};
  LightEnvironment lights{.directional_lights = directional_lights};

  std::uint32_t rendered_frames = 0;
  bool running = true;
  while (running && !window_system.ShouldClose()) {
    window_system.PollEvents();

    WindowEvent event{};
    while (window_system.PopEvent(&event)) {
      if (event.type == WindowEventType::kWindowClose) {
        running = false;
      } else if (event.type == WindowEventType::kKeyDown && event.key.key == kEscapeKey) {
        running = false;
      } else if (event.type == WindowEventType::kWindowResize) {
        if (event.resize.width > 0 && event.resize.height > 0) {
          rhi->GetSwapChain()->Resize(static_cast<std::uint32_t>(event.resize.width),
                                      static_cast<std::uint32_t>(event.resize.height));
        }
      }
    }

    const auto framebuffer_size = window_system.GetFramebufferSize();
    if (framebuffer_size.x <= 0 || framebuffer_size.y <= 0) {
      continue;
    }

    CameraData camera{};
    camera.position = {0.0f, 0.0f, 2.4f};
    camera.view = LookAt(camera.position, Vec3f{0.0f, 0.0f, 0.0f}, Vec3f{0.0f, 1.0f, 0.0f});
    camera.projection = Perspective(Radians(60.0f),
                                    static_cast<float>(framebuffer_size.x) /
                                        static_cast<float>(framebuffer_size.y),
                                    0.1f,
                                    100.0f);

    rhi->BeginFrame();
    status = renderer.BeginFrame(camera, lights);
    if (!status.ok()) {
      break;
    }

    const std::array<RenderableItem, 1> renderables{renderable};
    renderer.Submit(renderables);

    status = renderer.EndFrame();
    if (!status.ok()) {
      break;
    }

    rhi->EndFrame();
    rhi->Present();

    ++rendered_frames;
    if (frame_limit > 0 && rendered_frames >= static_cast<std::uint32_t>(frame_limit)) {
      running = false;
    }
  }

  DestroyMesh(*rhi, sample_mesh);
  renderer.Shutdown();
  rhi->Shutdown();
  window_system.Shutdown();
  return status;
}

}  // namespace

int main(int argc, char** argv) {
  const int frame_limit = ParseFrameLimit(argc, argv);
  const Status status = RunRendererSample(frame_limit);
  if (!status.ok()) {
    std::cerr << "[renderer_sample] Failed: " << status.ToString() << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
