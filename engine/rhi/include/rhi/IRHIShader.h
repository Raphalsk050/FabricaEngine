#pragma once

#include "rhi/RHITypes.h"

namespace Fabrica::RHI {

class IRHIShader {
 public:
  virtual ~IRHIShader() = default;

  [[nodiscard]] virtual RHIShaderHandle GetHandle() const = 0;
  [[nodiscard]] virtual const RHIShaderDesc& GetDesc() const = 0;
};

}  // namespace Fabrica::RHI
