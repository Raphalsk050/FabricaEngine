#pragma once

#include "rhi/RHITypes.h"

namespace Fabrica::RHI {

class IRHIPipeline {
 public:
  virtual ~IRHIPipeline() = default;

  [[nodiscard]] virtual RHIPipelineHandle GetHandle() const = 0;
  [[nodiscard]] virtual ERHIPipelineType GetType() const = 0;
  [[nodiscard]] virtual const RHIGraphicsPipelineDesc* GetGraphicsDesc() const = 0;
  [[nodiscard]] virtual const RHIComputePipelineDesc* GetComputeDesc() const = 0;
};

}  // namespace Fabrica::RHI
