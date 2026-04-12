#pragma once

#include "rhi/RHITypes.h"

namespace Fabrica::RHI {

class IRHISampler {
 public:
  virtual ~IRHISampler() = default;

  [[nodiscard]] virtual RHISamplerHandle GetHandle() const = 0;
  [[nodiscard]] virtual const RHISamplerDesc& GetDesc() const = 0;
};

}  // namespace Fabrica::RHI
