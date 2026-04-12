#pragma once

#include <memory>
#include <span>
#include <string_view>

#include "rhi/IRHIBuffer.h"
#include "rhi/IRHICommandList.h"
#include "rhi/IRHIDescriptorSet.h"
#include "rhi/IRHIFence.h"
#include "rhi/IRHIPipeline.h"
#include "rhi/IRHIRenderPass.h"
#include "rhi/IRHISampler.h"
#include "rhi/IRHIShader.h"
#include "rhi/IRHISwapChain.h"
#include "rhi/IRHITexture.h"
#include "rhi/RHITypes.h"

namespace Fabrica::RHI {

class IRHIContext {
 public:
  virtual ~IRHIContext() = default;

  virtual RHIResult Initialize(const RHIContextDesc& desc) = 0;
  virtual void Shutdown() = 0;

  virtual void BeginFrame() = 0;
  virtual void EndFrame() = 0;
  virtual void Present() = 0;

  [[nodiscard]] virtual RHIBufferCreateResult CreateBuffer(const RHIBufferDesc& desc) = 0;
  [[nodiscard]] virtual RHITextureCreateResult CreateTexture(const RHITextureDesc& desc) = 0;
  [[nodiscard]] virtual RHISamplerCreateResult CreateSampler(const RHISamplerDesc& desc) = 0;
  [[nodiscard]] virtual RHIShaderCreateResult CreateShader(const RHIShaderDesc& desc) = 0;
  [[nodiscard]] virtual RHIRenderPassCreateResult CreateRenderPass(
      const RHIRenderPassDesc& desc) = 0;
  [[nodiscard]] virtual RHIPipelineCreateResult CreateGraphicsPipeline(
      const RHIGraphicsPipelineDesc& desc) = 0;
  [[nodiscard]] virtual RHIPipelineCreateResult CreateComputePipeline(
      const RHIComputePipelineDesc& desc) = 0;
  [[nodiscard]] virtual RHIDescriptorSetCreateResult CreateDescriptorSet(
      const RHIDescriptorSetDesc& desc) = 0;

  virtual RHIResult UpdateBuffer(RHIBufferHandle handle,
                                 const void* data,
                                 std::size_t offset,
                                 std::size_t size) = 0;
  virtual RHIResult UpdateTexture(RHITextureHandle handle,
                                  const RHITextureUpdateDesc& desc) = 0;
  virtual RHIResult MapBuffer(RHIBufferHandle handle, RHIMappedRange& out_range) = 0;
  virtual void UnmapBuffer(RHIBufferHandle handle) = 0;

  [[nodiscard]] virtual IRHICommandList* GetCommandList(ERHIQueueType queue) = 0;
  virtual void SubmitCommandLists(std::span<IRHICommandList*> lists,
                                  RHIFenceHandle signal_fence) = 0;

  [[nodiscard]] virtual RHIFenceCreateResult CreateFence(bool signaled = false) = 0;
  virtual void WaitFence(RHIFenceHandle handle) = 0;
  virtual void DestroyFence(RHIFenceHandle handle) = 0;

  [[nodiscard]] virtual IRHISwapChain* GetSwapChain() = 0;

  virtual void Destroy(RHIBufferHandle handle) = 0;
  virtual void Destroy(RHITextureHandle handle) = 0;
  virtual void Destroy(RHISamplerHandle handle) = 0;
  virtual void Destroy(RHIShaderHandle handle) = 0;
  virtual void Destroy(RHIRenderPassHandle handle) = 0;
  virtual void Destroy(RHIPipelineHandle handle) = 0;
  virtual void Destroy(RHIDescriptorSetHandle handle) = 0;

  virtual void SetDebugName(RHIBufferHandle handle, std::string_view name) = 0;
  virtual void SetDebugName(RHITextureHandle handle, std::string_view name) = 0;
  [[nodiscard]] virtual const RHIDeviceInfo& GetDeviceInfo() const = 0;

  [[nodiscard]] virtual bool IsValid(RHIBufferHandle handle) const = 0;
  [[nodiscard]] virtual bool IsValid(RHITextureHandle handle) const = 0;
  [[nodiscard]] virtual bool IsValid(RHISamplerHandle handle) const = 0;
  [[nodiscard]] virtual bool IsValid(RHIShaderHandle handle) const = 0;
  [[nodiscard]] virtual bool IsValid(RHIRenderPassHandle handle) const = 0;
  [[nodiscard]] virtual bool IsValid(RHIPipelineHandle handle) const = 0;
  [[nodiscard]] virtual bool IsValid(RHIDescriptorSetHandle handle) const = 0;
  [[nodiscard]] virtual bool IsValid(RHIFenceHandle handle) const = 0;

  [[nodiscard]] virtual const IRHIBuffer* GetBuffer(RHIBufferHandle handle) const = 0;
  [[nodiscard]] virtual const IRHITexture* GetTexture(RHITextureHandle handle) const = 0;
  [[nodiscard]] virtual const IRHISampler* GetSampler(RHISamplerHandle handle) const = 0;
  [[nodiscard]] virtual const IRHIShader* GetShader(RHIShaderHandle handle) const = 0;
  [[nodiscard]] virtual const IRHIRenderPass* GetRenderPass(
      RHIRenderPassHandle handle) const = 0;
  [[nodiscard]] virtual const IRHIPipeline* GetPipeline(
      RHIPipelineHandle handle) const = 0;
  [[nodiscard]] virtual IRHIDescriptorSet* GetDescriptorSet(
      RHIDescriptorSetHandle handle) = 0;
  [[nodiscard]] virtual const IRHIDescriptorSet* GetDescriptorSet(
      RHIDescriptorSetHandle handle) const = 0;
  [[nodiscard]] virtual const IRHIFence* GetFence(RHIFenceHandle handle) const = 0;
};

}  // namespace Fabrica::RHI