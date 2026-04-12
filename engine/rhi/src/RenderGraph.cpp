#include "rhi/RenderGraph.h"

#include <algorithm>
#include <array>
#include <queue>

#include "core/config/config.h"
#include "core/logging/logger.h"

namespace Fabrica::RHI {
namespace {

template <typename Container>
bool ContainsPass(const Container& container, std::size_t value) {
  return std::find(container.begin(), container.end(), value) != container.end();
}

ERHIQueueType QueueTypeFromPassType(RenderGraph::PassType type) {
  switch (type) {
    case RenderGraph::PassType::Compute:
      return ERHIQueueType::Compute;
    case RenderGraph::PassType::Transfer:
      return ERHIQueueType::Transfer;
    case RenderGraph::PassType::Graphics:
    default:
      return ERHIQueueType::Graphics;
  }
}

std::size_t QueueSlotFromType(ERHIQueueType type) {
  switch (type) {
    case ERHIQueueType::Graphics:
      return 0;
    case ERHIQueueType::Compute:
      return 1;
    case ERHIQueueType::Transfer:
    default:
      return 2;
  }
}

}  // namespace

void RenderGraph::PassBuilder::Read(RenderGraphTextureHandle handle,
                                    ERHITextureLayout expected_layout,
                                    ERHIPipelineStage stage,
                                    ERHIAccessFlags access) {
  pass_.read_textures.push_back(
      {.handle = handle, .layout = expected_layout, .stage = stage, .access = access});
}

void RenderGraph::PassBuilder::Write(RenderGraphTextureHandle handle,
                                     ERHITextureLayout target_layout,
                                     ERHIPipelineStage stage,
                                     ERHIAccessFlags access) {
  pass_.write_textures.push_back(
      {.handle = handle, .layout = target_layout, .stage = stage, .access = access});
}

void RenderGraph::PassBuilder::Read(RenderGraphBufferHandle handle,
                                    ERHIPipelineStage stage,
                                    ERHIAccessFlags access) {
  pass_.read_buffers.push_back({.handle = handle, .stage = stage, .access = access});
}

void RenderGraph::PassBuilder::Write(RenderGraphBufferHandle handle,
                                     ERHIPipelineStage stage,
                                     ERHIAccessFlags access) {
  pass_.write_buffers.push_back({.handle = handle, .stage = stage, .access = access});
}

RenderGraphTextureHandle RenderGraph::ImportTexture(RHITextureHandle handle,
                                                    const RHITextureDesc& desc,
                                                    ERHITextureLayout initial_layout,
                                                    std::string name) {
  textures_.push_back({.name = std::move(name),
                       .desc = desc,
                       .imported_handle = handle,
                       .resolved_handle = handle,
                       .initial_layout = initial_layout,
                       .imported = true,
                       .output = false});
  return RenderGraphTextureHandle{static_cast<std::uint32_t>(textures_.size())};
}

RenderGraphBufferHandle RenderGraph::ImportBuffer(RHIBufferHandle handle,
                                                  const RHIBufferDesc& desc,
                                                  std::string name) {
  buffers_.push_back({.name = std::move(name),
                      .desc = desc,
                      .imported_handle = handle,
                      .resolved_handle = handle,
                      .imported = true,
                      .output = false});
  return RenderGraphBufferHandle{static_cast<std::uint32_t>(buffers_.size())};
}

RenderGraphTextureHandle RenderGraph::CreateTexture(const RHITextureDesc& desc,
                                                    std::string name) {
  textures_.push_back({.name = std::move(name), .desc = desc});
  return RenderGraphTextureHandle{static_cast<std::uint32_t>(textures_.size())};
}

RenderGraphBufferHandle RenderGraph::CreateBuffer(const RHIBufferDesc& desc,
                                                  std::string name) {
  buffers_.push_back({.name = std::move(name), .desc = desc});
  return RenderGraphBufferHandle{static_cast<std::uint32_t>(buffers_.size())};
}

void RenderGraph::MarkOutput(RenderGraphTextureHandle handle) {
  if (!handle.IsValid()) {
    return;
  }
  textures_[handle.id - 1u].output = true;
}

void RenderGraph::MarkOutput(RenderGraphBufferHandle handle) {
  if (!handle.IsValid()) {
    return;
  }
  buffers_[handle.id - 1u].output = true;
}

void RenderGraph::Compile(IRHIContext&) {
  execution_order_.clear();
  compiled_ = true;

  std::vector<int> texture_producers(textures_.size(), -1);
  std::vector<int> buffer_producers(buffers_.size(), -1);
  for (std::size_t pass_index = 0; pass_index < passes_.size(); ++pass_index) {
    PassNode& pass = passes_[pass_index];
    pass.culled = true;
    for (const TextureUsageRef& write : pass.write_textures) {
      texture_producers[write.handle.id - 1u] = static_cast<int>(pass_index);
    }
    for (const BufferUsageRef& write : pass.write_buffers) {
      buffer_producers[write.handle.id - 1u] = static_cast<int>(pass_index);
    }
  }

  std::queue<int> live_pass_queue;
  for (std::size_t texture_index = 0; texture_index < textures_.size(); ++texture_index) {
    if (!textures_[texture_index].output && !textures_[texture_index].imported) {
      continue;
    }
    const int producer = texture_producers[texture_index];
    if (producer >= 0 && passes_[producer].culled) {
      passes_[producer].culled = false;
      live_pass_queue.push(producer);
    }
  }
  for (std::size_t buffer_index = 0; buffer_index < buffers_.size(); ++buffer_index) {
    if (!buffers_[buffer_index].output && !buffers_[buffer_index].imported) {
      continue;
    }
    const int producer = buffer_producers[buffer_index];
    if (producer >= 0 && passes_[producer].culled) {
      passes_[producer].culled = false;
      live_pass_queue.push(producer);
    }
  }

  while (!live_pass_queue.empty()) {
    const int pass_index = live_pass_queue.front();
    live_pass_queue.pop();
    const PassNode& pass = passes_[pass_index];
    for (const TextureUsageRef& read : pass.read_textures) {
      const int producer = texture_producers[read.handle.id - 1u];
      if (producer >= 0 && passes_[producer].culled) {
        passes_[producer].culled = false;
        live_pass_queue.push(producer);
      }
    }
    for (const BufferUsageRef& read : pass.read_buffers) {
      const int producer = buffer_producers[read.handle.id - 1u];
      if (producer >= 0 && passes_[producer].culled) {
        passes_[producer].culled = false;
        live_pass_queue.push(producer);
      }
    }
  }

  std::vector<std::vector<std::size_t>> adjacency(passes_.size());
  std::vector<std::size_t> indegree(passes_.size(), 0);
  std::vector<int> latest_texture_writer(textures_.size(), -1);
  std::vector<int> latest_buffer_writer(buffers_.size(), -1);

  for (std::size_t pass_index = 0; pass_index < passes_.size(); ++pass_index) {
    const PassNode& pass = passes_[pass_index];
    if (pass.culled) {
      continue;
    }

    auto add_dependency = [&](int dependency) {
      if (dependency < 0 || dependency == static_cast<int>(pass_index) ||
          passes_[dependency].culled || ContainsPass(adjacency[dependency], pass_index)) {
        return;
      }
      adjacency[dependency].push_back(pass_index);
      ++indegree[pass_index];
    };

    for (const TextureUsageRef& read : pass.read_textures) {
      add_dependency(latest_texture_writer[read.handle.id - 1u]);
    }
    for (const BufferUsageRef& read : pass.read_buffers) {
      add_dependency(latest_buffer_writer[read.handle.id - 1u]);
    }
    for (const TextureUsageRef& write : pass.write_textures) {
      add_dependency(latest_texture_writer[write.handle.id - 1u]);
      latest_texture_writer[write.handle.id - 1u] = static_cast<int>(pass_index);
    }
    for (const BufferUsageRef& write : pass.write_buffers) {
      add_dependency(latest_buffer_writer[write.handle.id - 1u]);
      latest_buffer_writer[write.handle.id - 1u] = static_cast<int>(pass_index);
    }
  }

  std::queue<std::size_t> ready;
  for (std::size_t pass_index = 0; pass_index < passes_.size(); ++pass_index) {
    if (!passes_[pass_index].culled && indegree[pass_index] == 0) {
      ready.push(pass_index);
    }
  }

  while (!ready.empty()) {
    const std::size_t pass_index = ready.front();
    ready.pop();
    execution_order_.push_back(pass_index);
    for (const std::size_t next : adjacency[pass_index]) {
      if (--indegree[next] == 0) {
        ready.push(next);
      }
    }
  }

  if (execution_order_.empty()) {
    for (std::size_t pass_index = 0; pass_index < passes_.size(); ++pass_index) {
      if (!passes_[pass_index].culled) {
        execution_order_.push_back(pass_index);
      }
    }
  }
}

void RenderGraph::Execute(IRHIContext& rhi) {
  if (!compiled_) {
    Compile(rhi);
  }

  for (TextureResourceNode& texture : textures_) {
    if (!texture.imported && !texture.resolved_handle.IsValid()) {
      texture.resolved_handle = rhi.CreateTexture(texture.desc).handle;
    }
  }
  for (BufferResourceNode& buffer : buffers_) {
    if (!buffer.imported && !buffer.resolved_handle.IsValid()) {
      buffer.resolved_handle = rhi.CreateBuffer(buffer.desc).handle;
    }
  }

  const auto destroy_transient_resources = [&]() {
    for (TextureResourceNode& texture : textures_) {
      if (!texture.imported && texture.resolved_handle.IsValid()) {
        rhi.Destroy(texture.resolved_handle);
        texture.resolved_handle = {};
      }
    }
    for (BufferResourceNode& buffer : buffers_) {
      if (!buffer.imported && buffer.resolved_handle.IsValid()) {
        rhi.Destroy(buffer.resolved_handle);
        buffer.resolved_handle = {};
      }
    }
  };

  std::vector<ERHITextureLayout> texture_layouts(textures_.size(),
                                                 ERHITextureLayout::Undefined);
  std::vector<ERHIPipelineStage> texture_stages(textures_.size(), ERHIPipelineStage::None);
  std::vector<ERHIAccessFlags> texture_access(textures_.size(), ERHIAccessFlags::None);
  std::vector<ERHIPipelineStage> buffer_stages(buffers_.size(), ERHIPipelineStage::None);
  std::vector<ERHIAccessFlags> buffer_access(buffers_.size(), ERHIAccessFlags::None);
  for (std::size_t index = 0; index < textures_.size(); ++index) {
    texture_layouts[index] = textures_[index].initial_layout;
  }

  std::array<bool, 3> submitted_queue_batches{false, false, false};
  ERHIQueueType active_queue = ERHIQueueType::Graphics;
  std::size_t active_queue_slot = 0;
  IRHICommandList* active_command_list = nullptr;
  bool batch_open = false;

  const auto submit_batch = [&]() {
    if (!batch_open || active_command_list == nullptr) {
      return;
    }

    active_command_list->End();
    std::array<IRHICommandList*, 1> submit_list{active_command_list};
    rhi.SubmitCommandLists(submit_list, {});
    submitted_queue_batches[active_queue_slot] = true;
    active_command_list = nullptr;
    batch_open = false;
  };

  for (const std::size_t pass_index : execution_order_) {
    PassNode& pass = passes_[pass_index];
    if (pass.culled) {
      continue;
    }

    const ERHIQueueType queue = QueueTypeFromPassType(pass.type);
    const std::size_t queue_slot = QueueSlotFromType(queue);
    if (!batch_open || queue != active_queue) {
      submit_batch();
      if (submitted_queue_batches[queue_slot]) {
        if constexpr (::Fabrica::Config::kRHIVerboseLogging) {
          FABRICA_LOG(Render, Error)
              << "[RHI] RenderGraph queue interleaving requires multiple command lists per queue; queue="
              << static_cast<int>(queue);
        }
        destroy_transient_resources();
        return;
      }

      active_command_list = rhi.GetCommandList(queue);
      if (active_command_list == nullptr) {
        continue;
      }

      active_command_list->Reset();
      active_command_list->Begin();
      active_queue = queue;
      active_queue_slot = queue_slot;
      batch_open = true;
    }

    for (const TextureUsageRef& read : pass.read_textures) {
      const std::size_t texture_index = read.handle.id - 1u;
      if (texture_layouts[texture_index] != read.layout ||
          texture_access[texture_index] != read.access ||
          texture_stages[texture_index] != read.stage) {
        active_command_list->TransitionTexture(textures_[texture_index].resolved_handle,
                                               texture_layouts[texture_index], read.layout,
                                               texture_stages[texture_index], read.stage,
                                               texture_access[texture_index], read.access);
        texture_layouts[texture_index] = read.layout;
        texture_stages[texture_index] = read.stage;
        texture_access[texture_index] = read.access;
      }
    }
    for (const TextureUsageRef& write : pass.write_textures) {
      const std::size_t texture_index = write.handle.id - 1u;
      if (texture_layouts[texture_index] != write.layout ||
          texture_access[texture_index] != write.access ||
          texture_stages[texture_index] != write.stage) {
        active_command_list->TransitionTexture(textures_[texture_index].resolved_handle,
                                               texture_layouts[texture_index], write.layout,
                                               texture_stages[texture_index], write.stage,
                                               texture_access[texture_index], write.access);
        texture_layouts[texture_index] = write.layout;
        texture_stages[texture_index] = write.stage;
        texture_access[texture_index] = write.access;
      }
    }
    for (const BufferUsageRef& read : pass.read_buffers) {
      const std::size_t buffer_index = read.handle.id - 1u;
      if (buffer_access[buffer_index] != read.access ||
          buffer_stages[buffer_index] != read.stage) {
        active_command_list->TransitionBuffer(buffers_[buffer_index].resolved_handle,
                                              buffer_stages[buffer_index], read.stage,
                                              buffer_access[buffer_index], read.access);
        buffer_stages[buffer_index] = read.stage;
        buffer_access[buffer_index] = read.access;
      }
    }
    for (const BufferUsageRef& write : pass.write_buffers) {
      const std::size_t buffer_index = write.handle.id - 1u;
      if (buffer_access[buffer_index] != write.access ||
          buffer_stages[buffer_index] != write.stage) {
        active_command_list->TransitionBuffer(buffers_[buffer_index].resolved_handle,
                                              buffer_stages[buffer_index], write.stage,
                                              buffer_access[buffer_index], write.access);
        buffer_stages[buffer_index] = write.stage;
        buffer_access[buffer_index] = write.access;
      }
    }

    if (pass.execute) {
      pass.execute({.rhi = rhi, .command_list = *active_command_list, .graph = *this});
    }
  }

  submit_batch();
  destroy_transient_resources();
}

void RenderGraph::Reset() {
  passes_.clear();
  textures_.clear();
  buffers_.clear();
  execution_order_.clear();
  compiled_ = false;
}

RHITextureHandle RenderGraph::ResolveTexture(RenderGraphTextureHandle handle) const {
  return handle.IsValid() ? textures_[handle.id - 1u].resolved_handle : RHITextureHandle{};
}

RHIBufferHandle RenderGraph::ResolveBuffer(RenderGraphBufferHandle handle) const {
  return handle.IsValid() ? buffers_[handle.id - 1u].resolved_handle : RHIBufferHandle{};
}

}  // namespace Fabrica::RHI
