#include <array>
#include <string>
#include <vector>

#include "core/common/test/test_framework.h"
#include "rhi/RenderGraph.h"

namespace {

using namespace Fabrica::RHI;

std::size_t QueueSlot(ERHIQueueType queue) {
  switch (queue) {
    case ERHIQueueType::Graphics:
      return 0;
    case ERHIQueueType::Compute:
      return 1;
    case ERHIQueueType::Transfer:
    default:
      return 2;
  }
}

class FakeCommandList final : public IRHICommandList {
 public:
  FakeCommandList(ERHIQueueType queue_type = ERHIQueueType::Graphics)
      : queue_type_(queue_type) {}

  void Begin() override { ++begin_count; }
  void End() override { ++end_count; }
  void Reset() override { ++reset_count; }
  void BeginRenderPass(RHIRenderPassHandle, const RHIRenderPassBeginDesc&) override {}
  void EndRenderPass() override {}
  void SetViewport(const RHIViewport&) override {}
  void SetScissor(const RHIRect&) override {}
  void BindPipeline(RHIPipelineHandle) override {}
  void BindVertexBuffers(std::uint32_t, std::span<RHIBufferHandle>,
                         std::span<std::uint64_t>) override {}
  void BindIndexBuffer(RHIBufferHandle, std::uint64_t, ERHIIndexType) override {}
  void BindDescriptorSet(RHIDescriptorSetHandle, std::uint32_t) override {}
  void PushConstants(const void*, std::uint32_t, std::uint32_t, ERHIShaderStage) override {}
  void Draw(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) override {}
  void DrawIndexed(std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t,
                   std::uint32_t) override {}
  void Dispatch(std::uint32_t, std::uint32_t, std::uint32_t) override {}
  void TransitionTexture(RHITextureHandle, ERHITextureLayout, ERHITextureLayout,
                         ERHIPipelineStage, ERHIPipelineStage, ERHIAccessFlags,
                         ERHIAccessFlags) override {}
  void TransitionBuffer(RHIBufferHandle, ERHIPipelineStage, ERHIPipelineStage,
                        ERHIAccessFlags, ERHIAccessFlags) override {}
  void CopyBuffer(RHIBufferHandle, RHIBufferHandle,
                  std::span<RHIBufferCopyRegion>) override {}
  void CopyBufferToTexture(RHIBufferHandle, RHITextureHandle,
                           std::span<RHIBufferTextureCopyRegion>) override {}
  void BeginDebugRegion(std::string_view) override {}
  void EndDebugRegion() override {}
  void InsertDebugLabel(std::string_view) override {}

  [[nodiscard]] ERHIQueueType GetQueueType() const { return queue_type_; }

  std::uint32_t begin_count = 0;
  std::uint32_t end_count = 0;
  std::uint32_t reset_count = 0;

 private:
  ERHIQueueType queue_type_ = ERHIQueueType::Graphics;
};

class FakeContext final : public IRHIContext {
 public:
  FakeContext()
      : command_lists_{FakeCommandList(ERHIQueueType::Graphics),
                       FakeCommandList(ERHIQueueType::Compute),
                       FakeCommandList(ERHIQueueType::Transfer)} {}

  RHIResult Initialize(const RHIContextDesc&) override { return RHIResult::Success; }
  void Shutdown() override {}
  void BeginFrame() override {}
  void EndFrame() override {}
  void Present() override {}
  RHIBufferCreateResult CreateBuffer(const RHIBufferDesc&) override {
    return {.handle = RHIBufferHandle{EncodeHandleId(
                1u, ERHIResourceType::kBuffer, ++buffer_index_)},
            .result = RHIResult::Success};
  }
  RHITextureCreateResult CreateTexture(const RHITextureDesc&) override {
    return {.handle = RHITextureHandle{EncodeHandleId(
                1u, ERHIResourceType::kTexture, ++texture_index_)},
            .result = RHIResult::Success};
  }
  RHISamplerCreateResult CreateSampler(const RHISamplerDesc&) override { return {}; }
  RHIShaderCreateResult CreateShader(const RHIShaderDesc&) override { return {}; }
  RHIRenderPassCreateResult CreateRenderPass(const RHIRenderPassDesc&) override {
    return {};
  }
  RHIPipelineCreateResult CreateGraphicsPipeline(const RHIGraphicsPipelineDesc&) override {
    return {};
  }
  RHIPipelineCreateResult CreateComputePipeline(const RHIComputePipelineDesc&) override {
    return {};
  }
  RHIDescriptorSetCreateResult CreateDescriptorSet(const RHIDescriptorSetDesc&) override {
    return {};
  }
  RHIResult UpdateBuffer(RHIBufferHandle, const void*, std::size_t, std::size_t) override {
    return RHIResult::Success;
  }
  RHIResult UpdateTexture(RHITextureHandle, const RHITextureUpdateDesc&) override {
    return RHIResult::Success;
  }
  RHIResult MapBuffer(RHIBufferHandle, RHIMappedRange& out_range) override {
    out_range = {};
    return RHIResult::ErrorUnsupported;
  }
  void UnmapBuffer(RHIBufferHandle) override {}
  IRHICommandList* GetCommandList(ERHIQueueType queue) override {
    return &command_lists_[QueueSlot(queue)];
  }
  void SubmitCommandLists(std::span<IRHICommandList*> lists, RHIFenceHandle) override {
    ++submit_count;
    if (!lists.empty() && lists.front() != nullptr) {
      submitted_queue_order.push_back(
          static_cast<FakeCommandList*>(lists.front())->GetQueueType());
    }
  }
  RHIFenceCreateResult CreateFence(bool) override { return {}; }
  void WaitFence(RHIFenceHandle) override {}
  void DestroyFence(RHIFenceHandle) override {}
  IRHISwapChain* GetSwapChain() override { return nullptr; }
  void Destroy(RHIBufferHandle handle) override { destroyed_buffers_.push_back(handle.id); }
  void Destroy(RHITextureHandle handle) override { destroyed_textures_.push_back(handle.id); }
  void Destroy(RHISamplerHandle) override {}
  void Destroy(RHIShaderHandle) override {}
  void Destroy(RHIRenderPassHandle) override {}
  void Destroy(RHIPipelineHandle) override {}
  void Destroy(RHIDescriptorSetHandle) override {}
  void SetDebugName(RHIBufferHandle, std::string_view) override {}
  void SetDebugName(RHITextureHandle, std::string_view) override {}
  const RHIDeviceInfo& GetDeviceInfo() const override { return device_info_; }
  bool IsValid(RHIBufferHandle handle) const override { return handle.IsValid(); }
  bool IsValid(RHITextureHandle handle) const override { return handle.IsValid(); }
  bool IsValid(RHISamplerHandle handle) const override { return handle.IsValid(); }
  bool IsValid(RHIShaderHandle handle) const override { return handle.IsValid(); }
  bool IsValid(RHIRenderPassHandle handle) const override { return handle.IsValid(); }
  bool IsValid(RHIPipelineHandle handle) const override { return handle.IsValid(); }
  bool IsValid(RHIDescriptorSetHandle handle) const override { return handle.IsValid(); }
  bool IsValid(RHIFenceHandle handle) const override { return handle.IsValid(); }
  const IRHIBuffer* GetBuffer(RHIBufferHandle) const override { return nullptr; }
  const IRHITexture* GetTexture(RHITextureHandle) const override { return nullptr; }
  const IRHISampler* GetSampler(RHISamplerHandle) const override { return nullptr; }
  const IRHIShader* GetShader(RHIShaderHandle) const override { return nullptr; }
  const IRHIRenderPass* GetRenderPass(RHIRenderPassHandle) const override {
    return nullptr;
  }
  const IRHIPipeline* GetPipeline(RHIPipelineHandle) const override { return nullptr; }
  IRHIDescriptorSet* GetDescriptorSet(RHIDescriptorSetHandle) override { return nullptr; }
  const IRHIDescriptorSet* GetDescriptorSet(RHIDescriptorSetHandle) const override {
    return nullptr;
  }
  const IRHIFence* GetFence(RHIFenceHandle) const override { return nullptr; }

  std::vector<std::string> executed_passes;
  std::vector<ERHIQueueType> submitted_queue_order;
  std::vector<std::uint64_t> destroyed_buffers_;
  std::vector<std::uint64_t> destroyed_textures_;
  std::uint32_t submit_count = 0;
  std::array<FakeCommandList, 3> command_lists_{};

 private:
  RHIDeviceInfo device_info_{};
  std::uint64_t buffer_index_ = 0;
  std::uint64_t texture_index_ = 0;
};

FABRICA_TEST(RenderGraphCompilesDependenciesAndCullsUnusedPasses) {
  FakeContext context;
  RenderGraph graph;

  RHITextureDesc texture_desc{};
  texture_desc.width = 1280;
  texture_desc.height = 720;
  texture_desc.format = ERHITextureFormat::B8G8R8A8_UNORM;
  texture_desc.usage = ERHITextureUsage::ColorAttachment | ERHITextureUsage::Sampled;

  RHIBufferDesc buffer_desc{};
  buffer_desc.size = 256;
  buffer_desc.usage = ERHIBufferUsage::StorageBuffer;

  const RenderGraphTextureHandle transient = graph.CreateTexture(texture_desc, "lighting");
  const RenderGraphBufferHandle payload = graph.CreateBuffer(buffer_desc, "payload");
  const RenderGraphTextureHandle back_buffer = graph.ImportTexture(
      RHITextureHandle{EncodeHandleId(1u, ERHIResourceType::kTexture, 99u)}, texture_desc,
      ERHITextureLayout::Present, "backbuffer");
  graph.MarkOutput(back_buffer);

  graph.AddComputePass(
      "prepare",
      [&](RenderGraph::PassBuilder& builder) { builder.Write(payload); },
      [&](const RenderGraph::PassContext&) { context.executed_passes.push_back("prepare"); });

  graph.AddGraphicsPass(
      "lighting",
      [&](RenderGraph::PassBuilder& builder) {
        builder.Read(payload);
        builder.Write(transient, ERHITextureLayout::ColorAttachment);
      },
      [&](const RenderGraph::PassContext&) { context.executed_passes.push_back("lighting"); });

  graph.AddGraphicsPass(
      "present",
      [&](RenderGraph::PassBuilder& builder) {
        builder.Read(transient, ERHITextureLayout::ShaderReadOnly);
        builder.Write(back_buffer, ERHITextureLayout::ColorAttachment);
      },
      [&](const RenderGraph::PassContext&) { context.executed_passes.push_back("present"); });

  graph.AddTransferPass(
      "dead-pass",
      [&](RenderGraph::PassBuilder& builder) { builder.Write(graph.CreateBuffer(buffer_desc)); },
      [&](const RenderGraph::PassContext&) { context.executed_passes.push_back("dead-pass"); });

  graph.Compile(context);
  graph.Execute(context);

  FABRICA_EXPECT_EQ(context.executed_passes.size(), 3u);
  FABRICA_EXPECT_EQ(context.executed_passes[0], std::string("prepare"));
  FABRICA_EXPECT_EQ(context.executed_passes[1], std::string("lighting"));
  FABRICA_EXPECT_EQ(context.executed_passes[2], std::string("present"));
  FABRICA_EXPECT_EQ(context.submit_count, 2u);
  FABRICA_EXPECT_EQ(context.submitted_queue_order.size(), 2u);
  FABRICA_EXPECT_EQ(context.submitted_queue_order[0], ERHIQueueType::Compute);
  FABRICA_EXPECT_EQ(context.submitted_queue_order[1], ERHIQueueType::Graphics);
  FABRICA_EXPECT_EQ(context.command_lists_[QueueSlot(ERHIQueueType::Graphics)].begin_count, 1u);
  FABRICA_EXPECT_EQ(context.command_lists_[QueueSlot(ERHIQueueType::Graphics)].end_count, 1u);
  FABRICA_EXPECT_EQ(context.command_lists_[QueueSlot(ERHIQueueType::Graphics)].reset_count, 1u);
  FABRICA_EXPECT_TRUE(!context.destroyed_buffers_.empty());
  FABRICA_EXPECT_TRUE(!context.destroyed_textures_.empty());
}

}  // namespace

