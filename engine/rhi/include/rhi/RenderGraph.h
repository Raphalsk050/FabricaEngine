#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "rhi/IRHIContext.h"

namespace Fabrica::RHI {

struct RenderGraphTextureHandle final {
  std::uint32_t id = 0;
  constexpr bool IsValid() const { return id != 0; }
  constexpr bool operator==(const RenderGraphTextureHandle&) const = default;
};

struct RenderGraphBufferHandle final {
  std::uint32_t id = 0;
  constexpr bool IsValid() const { return id != 0; }
  constexpr bool operator==(const RenderGraphBufferHandle&) const = default;
};

class RenderGraph final {
  struct PassNode;

 public:
  enum class PassType : std::uint8_t {
    Graphics = 0,
    Compute,
    Transfer,
  };

  struct PassContext {
    IRHIContext& rhi;
    IRHICommandList& command_list;
    const RenderGraph& graph;
  };

  class PassBuilder {
   public:
    void Read(RenderGraphTextureHandle handle,
              ERHITextureLayout expected_layout,
              ERHIPipelineStage stage = ERHIPipelineStage::AllCommands,
              ERHIAccessFlags access = ERHIAccessFlags::ShaderRead);
    void Write(RenderGraphTextureHandle handle,
               ERHITextureLayout target_layout,
               ERHIPipelineStage stage = ERHIPipelineStage::AllCommands,
               ERHIAccessFlags access = ERHIAccessFlags::ShaderWrite);
    void Read(RenderGraphBufferHandle handle,
              ERHIPipelineStage stage = ERHIPipelineStage::AllCommands,
              ERHIAccessFlags access = ERHIAccessFlags::ShaderRead);
    void Write(RenderGraphBufferHandle handle,
               ERHIPipelineStage stage = ERHIPipelineStage::AllCommands,
               ERHIAccessFlags access = ERHIAccessFlags::ShaderWrite);

   private:
    friend class RenderGraph;
    explicit PassBuilder(PassNode& pass) : pass_(pass) {}

    PassNode& pass_;
  };

  using ExecuteCallback = std::function<void(const PassContext& context)>;

  RenderGraphTextureHandle ImportTexture(RHITextureHandle handle,
                                         const RHITextureDesc& desc,
                                         ERHITextureLayout initial_layout,
                                         std::string name = {});
  RenderGraphBufferHandle ImportBuffer(RHIBufferHandle handle,
                                       const RHIBufferDesc& desc,
                                       std::string name = {});
  RenderGraphTextureHandle CreateTexture(const RHITextureDesc& desc,
                                         std::string name = {});
  RenderGraphBufferHandle CreateBuffer(const RHIBufferDesc& desc,
                                       std::string name = {});

  void MarkOutput(RenderGraphTextureHandle handle);
  void MarkOutput(RenderGraphBufferHandle handle);

  template <typename BuildFn>
  void AddGraphicsPass(std::string name, BuildFn&& build, ExecuteCallback execute) {
    AddPass(PassType::Graphics, std::move(name), std::forward<BuildFn>(build),
            std::move(execute));
  }

  template <typename BuildFn>
  void AddComputePass(std::string name, BuildFn&& build, ExecuteCallback execute) {
    AddPass(PassType::Compute, std::move(name), std::forward<BuildFn>(build),
            std::move(execute));
  }

  template <typename BuildFn>
  void AddTransferPass(std::string name, BuildFn&& build, ExecuteCallback execute) {
    AddPass(PassType::Transfer, std::move(name), std::forward<BuildFn>(build),
            std::move(execute));
  }

  void Compile(IRHIContext& rhi);
  void Execute(IRHIContext& rhi);
  void Reset();

  [[nodiscard]] RHITextureHandle ResolveTexture(RenderGraphTextureHandle handle) const;
  [[nodiscard]] RHIBufferHandle ResolveBuffer(RenderGraphBufferHandle handle) const;

 private:
  enum class ResourceKind : std::uint8_t {
    Texture = 0,
    Buffer,
  };

  struct TextureUsageRef {
    RenderGraphTextureHandle handle = {};
    ERHITextureLayout layout = ERHITextureLayout::Undefined;
    ERHIPipelineStage stage = ERHIPipelineStage::AllCommands;
    ERHIAccessFlags access = ERHIAccessFlags::None;
  };

  struct BufferUsageRef {
    RenderGraphBufferHandle handle = {};
    ERHIPipelineStage stage = ERHIPipelineStage::AllCommands;
    ERHIAccessFlags access = ERHIAccessFlags::None;
  };

  struct PassNode {
    PassType type = PassType::Graphics;
    std::string name;
    std::vector<TextureUsageRef> read_textures;
    std::vector<TextureUsageRef> write_textures;
    std::vector<BufferUsageRef> read_buffers;
    std::vector<BufferUsageRef> write_buffers;
    ExecuteCallback execute;
    bool culled = false;
  };

  struct TextureResourceNode {
    std::string name;
    RHITextureDesc desc{};
    RHITextureHandle imported_handle = {};
    RHITextureHandle resolved_handle = {};
    ERHITextureLayout initial_layout = ERHITextureLayout::Undefined;
    bool imported = false;
    bool output = false;
  };

  struct BufferResourceNode {
    std::string name;
    RHIBufferDesc desc{};
    RHIBufferHandle imported_handle = {};
    RHIBufferHandle resolved_handle = {};
    bool imported = false;
    bool output = false;
  };

  template <typename BuildFn>
  void AddPass(PassType type,
               std::string name,
               BuildFn&& build,
               ExecuteCallback execute) {
    passes_.push_back(PassNode{});
    PassNode& pass = passes_.back();
    pass.type = type;
    pass.name = std::move(name);
    pass.execute = std::move(execute);

    PassBuilder builder(pass);
    build(builder);
  }

  std::vector<PassNode> passes_;
  std::vector<TextureResourceNode> textures_;
  std::vector<BufferResourceNode> buffers_;
  std::vector<std::size_t> execution_order_;
  bool compiled_ = false;
};

}  // namespace Fabrica::RHI




