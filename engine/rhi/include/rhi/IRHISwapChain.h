#pragma once

#include "rhi/RHITypes.h"

namespace Fabrica::RHI {

class IRHISwapChain {
 public:
  virtual ~IRHISwapChain() = default;

  virtual bool AcquireNextImage() = 0;
  [[nodiscard]] virtual RHITextureHandle GetCurrentBackBuffer() const = 0;
  [[nodiscard]] virtual std::uint32_t GetCurrentImageIndex() const = 0;
  [[nodiscard]] virtual RHIExtent2D GetExtent() const = 0;
  virtual bool Resize(std::uint32_t width, std::uint32_t height) = 0;
  virtual void SetVSync(bool enabled) = 0;
};

}  // namespace Fabrica::RHI
