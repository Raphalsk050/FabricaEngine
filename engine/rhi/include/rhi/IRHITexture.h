#pragma once

#include "rhi/RHITypes.h"

namespace Fabrica::RHI {

class IRHITexture {
 public:
  virtual ~IRHITexture() = default;

  [[nodiscard]] virtual RHITextureHandle GetHandle() const = 0;
  [[nodiscard]] virtual const RHITextureDesc& GetDesc() const = 0;
  [[nodiscard]] virtual ERHITextureLayout GetLayout() const = 0;
};

}  // namespace Fabrica::RHI
