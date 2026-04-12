#pragma once

#include "rhi/RHITypes.h"

namespace Fabrica::RHI {

class IRHIRenderPass {
 public:
  virtual ~IRHIRenderPass() = default;

  [[nodiscard]] virtual RHIRenderPassHandle GetHandle() const = 0;
  [[nodiscard]] virtual const RHIRenderPassDesc& GetDesc() const = 0;
};

}  // namespace Fabrica::RHI
