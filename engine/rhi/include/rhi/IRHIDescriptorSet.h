#pragma once

#include "rhi/RHITypes.h"

namespace Fabrica::RHI {

class IRHIDescriptorSet {
 public:
  virtual ~IRHIDescriptorSet() = default;

  [[nodiscard]] virtual RHIDescriptorSetHandle GetHandle() const = 0;
  [[nodiscard]] virtual const RHIDescriptorSetDesc& GetDesc() const = 0;

  virtual void BindBuffer(std::uint32_t binding,
                          const RHIDescriptorBufferWriteDesc& write) = 0;
  virtual void BindTexture(std::uint32_t binding,
                           const RHIDescriptorTextureWriteDesc& write) = 0;
  virtual void BindSampler(std::uint32_t binding,
                           const RHIDescriptorSamplerWriteDesc& write) = 0;
  virtual void FlushWrites() = 0;
};

}  // namespace Fabrica::RHI
