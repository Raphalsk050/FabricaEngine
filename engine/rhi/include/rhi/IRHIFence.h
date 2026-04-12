#pragma once

#include "rhi/RHITypes.h"

namespace Fabrica::RHI {

class IRHIFence {
 public:
  virtual ~IRHIFence() = default;

  [[nodiscard]] virtual RHIFenceHandle GetHandle() const = 0;
  [[nodiscard]] virtual bool IsSignaled() const = 0;
};

}  // namespace Fabrica::RHI
