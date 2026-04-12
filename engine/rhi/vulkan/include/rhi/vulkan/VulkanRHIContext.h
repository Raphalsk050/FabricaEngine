#pragma once

#include <memory>

#include "rhi/IRHIContext.h"

namespace Fabrica::RHI::Vulkan {

struct VulkanContextState;

class VulkanRHIContext final : public IRHIContext {
 public:
  VulkanRHIContext();
  ~VulkanRHIContext() override;

  RHIResult Initialize(const RHIContextDesc& desc) override;
  void Shutdown() override;

  void BeginFrame() override;
  void EndFrame() override;
  void Present() override;

  [[nodiscard]] RHIBufferCreateResult CreateBuffer(const RHIBufferDesc& desc) override;
  [[nodiscard]] RHITextureCreateResult CreateTexture(const RHITextureDesc& desc) override;
  [[nodiscard]] RHISamplerCreateResult CreateSampler(const RHISamplerDesc& desc) override;
  [[nodiscard]] RHIShaderCreateResult CreateShader(const RHIShaderDesc& desc) override;
  [[nodiscard]] RHIRenderPassCreateResult CreateRenderPass(
      const RHIRenderPassDesc& desc) override;
  [[nodiscard]] RHIPipelineCreateResult CreateGraphicsPipeline(
      const RHIGraphicsPipelineDesc& desc) override;
  [[nodiscard]] RHIPipelineCreateResult CreateComputePipeline(
      const RHIComputePipelineDesc& desc) override;
  [[nodiscard]] RHIDescriptorSetCreateResult CreateDescriptorSet(
      const RHIDescriptorSetDesc& desc) override;

  RHIResult UpdateBuffer(RHIBufferHandle handle,
                         const void* data,
                         std::size_t offset,
                         std::size_t size) override;
  RHIResult UpdateTexture(RHITextureHandle handle,
                          const RHITextureUpdateDesc& desc) override;
  RHIResult MapBuffer(RHIBufferHandle handle, RHIMappedRange& out_range) override;
  void UnmapBuffer(RHIBufferHandle handle) override;

  [[nodiscard]] IRHICommandList* GetCommandList(ERHIQueueType queue) override;
  void SubmitCommandLists(std::span<IRHICommandList*> lists,
                          RHIFenceHandle signal_fence) override;

  [[nodiscard]] RHIFenceCreateResult CreateFence(bool signaled = false) override;
  void WaitFence(RHIFenceHandle handle) override;
  void DestroyFence(RHIFenceHandle handle) override;

  [[nodiscard]] IRHISwapChain* GetSwapChain() override;

  void Destroy(RHIBufferHandle handle) override;
  void Destroy(RHITextureHandle handle) override;
  void Destroy(RHISamplerHandle handle) override;
  void Destroy(RHIShaderHandle handle) override;
  void Destroy(RHIRenderPassHandle handle) override;
  void Destroy(RHIPipelineHandle handle) override;
  void Destroy(RHIDescriptorSetHandle handle) override;

  void SetDebugName(RHIBufferHandle handle, std::string_view name) override;
  void SetDebugName(RHITextureHandle handle, std::string_view name) override;
  [[nodiscard]] const RHIDeviceInfo& GetDeviceInfo() const override;

  [[nodiscard]] bool IsValid(RHIBufferHandle handle) const override;
  [[nodiscard]] bool IsValid(RHITextureHandle handle) const override;
  [[nodiscard]] bool IsValid(RHISamplerHandle handle) const override;
  [[nodiscard]] bool IsValid(RHIShaderHandle handle) const override;
  [[nodiscard]] bool IsValid(RHIRenderPassHandle handle) const override;
  [[nodiscard]] bool IsValid(RHIPipelineHandle handle) const override;
  [[nodiscard]] bool IsValid(RHIDescriptorSetHandle handle) const override;
  [[nodiscard]] bool IsValid(RHIFenceHandle handle) const override;

  [[nodiscard]] const IRHIBuffer* GetBuffer(RHIBufferHandle handle) const override;
  [[nodiscard]] const IRHITexture* GetTexture(RHITextureHandle handle) const override;
  [[nodiscard]] const IRHISampler* GetSampler(RHISamplerHandle handle) const override;
  [[nodiscard]] const IRHIShader* GetShader(RHIShaderHandle handle) const override;
  [[nodiscard]] const IRHIRenderPass* GetRenderPass(
      RHIRenderPassHandle handle) const override;
  [[nodiscard]] const IRHIPipeline* GetPipeline(
      RHIPipelineHandle handle) const override;
  [[nodiscard]] IRHIDescriptorSet* GetDescriptorSet(
      RHIDescriptorSetHandle handle) override;
  [[nodiscard]] const IRHIDescriptorSet* GetDescriptorSet(
      RHIDescriptorSetHandle handle) const override;
  [[nodiscard]] const IRHIFence* GetFence(RHIFenceHandle handle) const override;

 private:
  std::unique_ptr<VulkanContextState> state_;
};

}  // namespace Fabrica::RHI::Vulkan