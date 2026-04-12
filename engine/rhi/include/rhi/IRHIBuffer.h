#pragma once

#include "rhi/RHITypes.h"

namespace Fabrica::RHI {

class IRHIBuffer {
 public:
  virtual ~IRHIBuffer() = default;

  [[nodiscard]] virtual RHIBufferHandle GetHandle() const = 0;
  [[nodiscard]] virtual const RHIBufferDesc& GetDesc() const = 0;
  [[nodiscard]] virtual std::uint64_t GetSize() const = 0;
};

}  // namespace Fabrica::RHI
