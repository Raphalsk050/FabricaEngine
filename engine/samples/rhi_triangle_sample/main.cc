#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/common/status.h"
#include "core/window/glfw_window_backend.h"
#include "core/window/window_system.h"
#include "rhi/IRHISwapChain.h"
#include "rhi/IRHITexture.h"
#include "rhi/RHIFactory.h"

namespace {

using Fabrica::Core::Status;
using Fabrica::Core::Window::GlfwWindowBackend;
using Fabrica::Core::Window::WindowConfig;
using Fabrica::Core::Window::WindowEvent;
using Fabrica::Core::Window::WindowEventType;
using Fabrica::Core::Window::WindowGraphicsApi;
using Fabrica::Core::Window::WindowSystem;
using Fabrica::RHI::ERHIBackend;
using Fabrica::RHI::ERHIAccessFlags;
using Fabrica::RHI::ERHIBufferUsage;
using Fabrica::RHI::ERHIColorComponent;
using Fabrica::RHI::ERHICullMode;
using Fabrica::RHI::ERHIFrontFace;
using Fabrica::RHI::ERHILoadOp;
using Fabrica::RHI::ERHIMemoryType;
using Fabrica::RHI::ERHIPipelineStage;
using Fabrica::RHI::ERHIQueueType;
using Fabrica::RHI::ERHIShaderStage;
using Fabrica::RHI::ERHIStoreOp;
using Fabrica::RHI::ERHITextureFormat;
using Fabrica::RHI::ERHITextureLayout;
using Fabrica::RHI::ERHIVertexInputRate;
using Fabrica::RHI::ERHIWindowHandleType;
using Fabrica::RHI::IRHICommandList;
using Fabrica::RHI::IRHIContext;
using Fabrica::RHI::IRHISwapChain;
using Fabrica::RHI::RHIContextDesc;
using Fabrica::RHI::RHIBufferDesc;
using Fabrica::RHI::RHIBufferHandle;
using Fabrica::RHI::RHIGraphicsPipelineDesc;
using Fabrica::RHI::RHIPipelineHandle;
using Fabrica::RHI::RHIRenderPassBeginDesc;
using Fabrica::RHI::RHIRenderPassDesc;
using Fabrica::RHI::RHIRenderPassHandle;
using Fabrica::RHI::RHIResult;
using Fabrica::RHI::RHIShaderDesc;
using Fabrica::RHI::RHIShaderHandle;
using Fabrica::RHI::RHITextureHandle;
using Fabrica::RHI::RHIViewport;
using Fabrica::RHI::RHIFactory;
using Fabrica::RHI::RHIMappedRange;

constexpr int kEscapeKey = 256;
constexpr std::string_view kDefaultShaderDir = FABRICA_RHI_TRIANGLE_SHADER_DIR;

struct TriangleVertex {
  float position[2];
  float color[3];
};

struct TriangleResources {
  RHIShaderHandle vertex_shader{};
  RHIShaderHandle fragment_shader{};
  RHIRenderPassHandle render_pass{};
  RHIPipelineHandle pipeline{};
  RHIBufferHandle vertex_buffer{};
  bool initialized = false;

  void Destroy(IRHIContext& rhi) {
    if (vertex_buffer.IsValid()) {
      rhi.Destroy(vertex_buffer);
    }
    if (pipeline.IsValid()) {
      rhi.Destroy(pipeline);
    }
    if (render_pass.IsValid()) {
      rhi.Destroy(render_pass);
    }
    if (fragment_shader.IsValid()) {
      rhi.Destroy(fragment_shader);
    }
    if (vertex_shader.IsValid()) {
      rhi.Destroy(vertex_shader);
    }

    vertex_buffer = {};
    pipeline = {};
    render_pass = {};
    fragment_shader = {};
    vertex_shader = {};
    initialized = false;
  }
};

[[nodiscard]] Status ToStatus(RHIResult result, const char* context) {
  if (result == RHIResult::Success) {
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

[[nodiscard]] Status ReadSpirvWords(const std::filesystem::path& path,
                                    std::vector<std::uint32_t>& out_words) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return Status::NotFound("Shader file could not be opened");
  }

  const std::streamsize size = file.tellg();
  if (size <= 0 || (size % static_cast<std::streamsize>(sizeof(std::uint32_t))) != 0) {
    return Status::InvalidArgument("Shader file size is invalid for SPIR-V");
  }

  out_words.resize(static_cast<std::size_t>(size) / sizeof(std::uint32_t));
  file.seekg(0, std::ios::beg);
  file.read(reinterpret_cast<char*>(out_words.data()), size);
  if (!file) {
    out_words.clear();
    return Status::Internal("Shader file read failed");
  }

  return Status::Ok();
}

[[nodiscard]] Status LoadShader(IRHIContext& rhi,
                                std::string_view shader_root,
                                std::string_view file_name,
                                ERHIShaderStage stage,
                                RHIShaderHandle& out_shader) {
  std::vector<std::uint32_t> words;
  const Status read_status = ReadSpirvWords(std::filesystem::path(shader_root) /
                                                std::filesystem::path(file_name),
                                            words);
  if (!read_status.ok()) {
    return read_status;
  }

  RHIShaderDesc shader_desc{};
  shader_desc.stage = stage;
  shader_desc.bytecode = words.data();
  shader_desc.bytecode_size = words.size() * sizeof(std::uint32_t);
  std::memcpy(shader_desc.entry_point, "main", sizeof("main"));

  const auto shader_result = rhi.CreateShader(shader_desc);
  Status status = ToStatus(shader_result.result, "CreateShader");
  if (!status.ok()) {
    return status;
  }

  out_shader = shader_result.handle;
  return Status::Ok();
}

[[nodiscard]] Status CreateTriangleVertexBuffer(IRHIContext& rhi, RHIBufferHandle& out_buffer) {
  constexpr std::array<TriangleVertex, 3> kTriangleVertices{{
      {{-0.65f, -0.55f}, {1.0f, 0.25f, 0.20f}},
      {{ 0.65f, -0.55f}, {0.15f, 0.95f, 0.30f}},
      {{ 0.00f,  0.70f}, {0.20f, 0.45f, 1.00f}},
  }};

  RHIBufferDesc buffer_desc{};
  buffer_desc.size = sizeof(kTriangleVertices);
  buffer_desc.stride = sizeof(TriangleVertex);
  buffer_desc.usage = ERHIBufferUsage::VertexBuffer;
  buffer_desc.memory = ERHIMemoryType::HostCoherent;

  const auto create_result = rhi.CreateBuffer(buffer_desc);
  Status status = ToStatus(create_result.result, "CreateBuffer");
  if (!status.ok()) {
    return status;
  }

  out_buffer = create_result.handle;
  RHIMappedRange mapped_range{};
  status = ToStatus(rhi.MapBuffer(out_buffer, mapped_range), "MapBuffer");
  if (!status.ok()) {
    rhi.Destroy(out_buffer);
    out_buffer = {};
    return status;
  }

  if (mapped_range.data == nullptr) {
    rhi.UnmapBuffer(out_buffer);
    rhi.Destroy(out_buffer);
    out_buffer = {};
    return Status::Internal("MapBuffer returned a null pointer");
  }

  std::memcpy(mapped_range.data, kTriangleVertices.data(), sizeof(kTriangleVertices));
  rhi.UnmapBuffer(out_buffer);
  return Status::Ok();
}

[[nodiscard]] Status InitializeTriangleResources(IRHIContext& rhi,
                                                 ERHITextureFormat swapchain_format,
                                                 std::string_view shader_root,
                                                 TriangleResources& resources) {
  if (resources.initialized) {
    return Status::Ok();
  }

  Status status = LoadShader(rhi, shader_root, "triangle.vert.spv", ERHIShaderStage::Vertex,
                             resources.vertex_shader);
  if (!status.ok()) {
    resources.Destroy(rhi);
    return status;
  }

  status = LoadShader(rhi, shader_root, "triangle.frag.spv", ERHIShaderStage::Fragment,
                      resources.fragment_shader);
  if (!status.ok()) {
    resources.Destroy(rhi);
    return status;
  }

  RHIRenderPassDesc render_pass_desc{};
  render_pass_desc.color_attachment_count = 1;
  render_pass_desc.color_attachments[0].format = swapchain_format;
  render_pass_desc.color_attachments[0].initial_layout = ERHITextureLayout::ColorAttachment;
  render_pass_desc.color_attachments[0].final_layout = ERHITextureLayout::ColorAttachment;

  const auto render_pass_result = rhi.CreateRenderPass(render_pass_desc);
  status = ToStatus(render_pass_result.result, "CreateRenderPass");
  if (!status.ok()) {
    resources.Destroy(rhi);
    return status;
  }
  resources.render_pass = render_pass_result.handle;

  RHIGraphicsPipelineDesc pipeline_desc{};
  pipeline_desc.vertex_shader = resources.vertex_shader;
  pipeline_desc.fragment_shader = resources.fragment_shader;
  pipeline_desc.render_pass = resources.render_pass;
  pipeline_desc.vertex_input.binding_count = 1;
  pipeline_desc.vertex_input.bindings[0] = {
      .binding = 0,
      .stride = sizeof(TriangleVertex),
      .input_rate = ERHIVertexInputRate::PerVertex,
  };
  pipeline_desc.vertex_input.attribute_count = 2;
  pipeline_desc.vertex_input.attributes[0] = {
      .location = 0,
      .binding = 0,
      .format = ERHITextureFormat::R32G32_FLOAT,
      .offset = 0,
  };
  pipeline_desc.vertex_input.attributes[1] = {
      .location = 1,
      .binding = 0,
      .format = ERHITextureFormat::R32G32B32_FLOAT,
      .offset = sizeof(float) * 2,
  };
  pipeline_desc.blend.attachment_count = 1;
  pipeline_desc.blend.attachments[0].write_mask = ERHIColorComponent::All;
  pipeline_desc.rasterizer.cull_mode = ERHICullMode::None;
  pipeline_desc.rasterizer.front_face = ERHIFrontFace::CounterClockwise;

  const auto pipeline_result = rhi.CreateGraphicsPipeline(pipeline_desc);
  status = ToStatus(pipeline_result.result, "CreateGraphicsPipeline");
  if (!status.ok()) {
    resources.Destroy(rhi);
    return status;
  }
  resources.pipeline = pipeline_result.handle;

  status = CreateTriangleVertexBuffer(rhi, resources.vertex_buffer);
  if (!status.ok()) {
    resources.Destroy(rhi);
    return status;
  }

  rhi.SetDebugName(resources.vertex_buffer, "rhi_triangle_sample.vertex_buffer");
  resources.initialized = true;
  return Status::Ok();
}

void RecordTriangleFrame(IRHICommandList& command_list,
                         const TriangleResources& resources,
                         RHITextureHandle back_buffer,
                         ERHITextureLayout current_layout,
                         Fabrica::RHI::RHIExtent2D extent) {
  const ERHIPipelineStage src_stage =
      current_layout == ERHITextureLayout::Undefined ? ERHIPipelineStage::None
                                                     : ERHIPipelineStage::BottomOfPipe;
  const ERHIAccessFlags src_access =
      current_layout == ERHITextureLayout::Undefined ? ERHIAccessFlags::None
                                                     : ERHIAccessFlags::MemoryRead;

  command_list.Reset();
  command_list.Begin();
  command_list.BeginDebugRegion("RhiTriangleSample");
  command_list.TransitionTexture(back_buffer,
                                 current_layout,
                                 ERHITextureLayout::ColorAttachment,
                                 src_stage,
                                 ERHIPipelineStage::ColorAttachmentOutput,
                                 src_access,
                                 ERHIAccessFlags::ColorWrite);

  RHIRenderPassBeginDesc begin_desc{};
  begin_desc.render_area = {.x = 0, .y = 0, .width = extent.width, .height = extent.height};
  begin_desc.layer_count = 1;
  begin_desc.color_attachment_count = 1;
  begin_desc.color_attachments[0].texture = back_buffer;
  begin_desc.color_attachments[0].layout = ERHITextureLayout::ColorAttachment;
  begin_desc.color_attachments[0].load_op = ERHILoadOp::Clear;
  begin_desc.color_attachments[0].store_op = ERHIStoreOp::Store;
  begin_desc.color_attachments[0].clear_value = {.r = 0.08f, .g = 0.10f, .b = 0.14f, .a = 1.0f};

  command_list.BeginRenderPass(resources.render_pass, begin_desc);
  command_list.SetViewport(RHIViewport{.x = 0.0f,
                                       .y = 0.0f,
                                       .width = static_cast<float>(extent.width),
                                       .height = static_cast<float>(extent.height),
                                       .min_depth = 0.0f,
                                       .max_depth = 1.0f});
  command_list.SetScissor({.x = 0, .y = 0, .width = extent.width, .height = extent.height});
  command_list.BindPipeline(resources.pipeline);

  std::array<RHIBufferHandle, 1> vertex_buffers{resources.vertex_buffer};
  std::array<std::uint64_t, 1> offsets{0};
  command_list.BindVertexBuffers(0, vertex_buffers, offsets);
  command_list.Draw(3, 1, 0, 0);
  command_list.EndRenderPass();
  command_list.TransitionTexture(back_buffer,
                                 ERHITextureLayout::ColorAttachment,
                                 ERHITextureLayout::Present,
                                 ERHIPipelineStage::ColorAttachmentOutput,
                                 ERHIPipelineStage::BottomOfPipe,
                                 ERHIAccessFlags::ColorWrite,
                                 ERHIAccessFlags::MemoryRead);
  command_list.EndDebugRegion();
  command_list.End();
}

[[nodiscard]] Status RunTriangleSample(int frame_limit) {
  WindowConfig window_config{};
  window_config.width = 1280;
  window_config.height = 720;
  window_config.title = "FabricaEngine RHI Triangle Sample";
  window_config.graphics_api = WindowGraphicsApi::kNone;
  window_config.vsync_enabled = true;

  WindowSystem window_system(std::make_unique<GlfwWindowBackend>());
  if (!window_system.Initialize(window_config)) {
    return Status::Unavailable("Falha ao inicializar a janela GLFW para o sample do triângulo");
  }

  RHIContextDesc context_desc{};
  context_desc.window_handle = window_system.GetNativeHandle();
  context_desc.window_handle_type = ERHIWindowHandleType::GLFW;
  context_desc.initial_extent = {
      static_cast<std::uint32_t>(window_config.width),
      static_cast<std::uint32_t>(window_config.height)};
  context_desc.enable_validation = true;
  context_desc.enable_vsync = true;
  std::memcpy(context_desc.application_name, "FabricaRhiTriangleSample",
              sizeof("FabricaRhiTriangleSample"));

  std::unique_ptr<IRHIContext> rhi = RHIFactory::CreateContext(ERHIBackend::Vulkan, context_desc);
  if (rhi == nullptr) {
    window_system.Shutdown();
    return Status::Unavailable("Falha ao criar o contexto Vulkan via RHIFactory");
  }

  TriangleResources resources{};
  Status status = Status::Ok();
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

    rhi->BeginFrame();

    IRHISwapChain* swap_chain = rhi->GetSwapChain();
    if (swap_chain == nullptr) {
      status = Status::Unavailable("Swapchain indisponível no contexto RHI");
      rhi->EndFrame();
      break;
    }

    const RHITextureHandle back_buffer = swap_chain->GetCurrentBackBuffer();
    const Fabrica::RHI::IRHITexture* back_buffer_texture = rhi->GetTexture(back_buffer);
    if (back_buffer_texture == nullptr) {
      status = Status::Internal("Falha ao resolver o back buffer atual da swapchain");
      rhi->EndFrame();
      break;
    }

    status = InitializeTriangleResources(*rhi,
                                         back_buffer_texture->GetDesc().format,
                                         kDefaultShaderDir,
                                         resources);
    if (!status.ok()) {
      rhi->EndFrame();
      break;
    }

    IRHICommandList* graphics_list = rhi->GetCommandList(ERHIQueueType::Graphics);
    if (graphics_list == nullptr) {
      status = Status::Unavailable("Falha ao obter command list gráfico do RHI");
      rhi->EndFrame();
      break;
    }

    RecordTriangleFrame(*graphics_list,
                        resources,
                        back_buffer,
                        back_buffer_texture->GetLayout(),
                        swap_chain->GetExtent());

    std::array<IRHICommandList*, 1> command_lists{graphics_list};
    rhi->SubmitCommandLists(command_lists, {});
    rhi->EndFrame();
    rhi->Present();

    ++rendered_frames;
    if (frame_limit > 0 && rendered_frames >= static_cast<std::uint32_t>(frame_limit)) {
      running = false;
    }
  }

  resources.Destroy(*rhi);
  rhi->Shutdown();
  window_system.Shutdown();
  return status;
}

}  // namespace

int main(int argc, char** argv) {
  const int frame_limit = ParseFrameLimit(argc, argv);
  const Status status = RunTriangleSample(frame_limit);
  if (!status.ok()) {
    std::cerr << "[rhi_triangle_sample] Failed: " << status.ToString() << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}