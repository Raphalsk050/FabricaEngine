#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "rhi/IRHIContext.h"

namespace Fabrica::RHI {

class RHIFactory {
 public:
  using ContextCreatorFn = std::function<std::unique_ptr<IRHIContext>()>;

  static void RegisterBackend(ERHIBackend backend, ContextCreatorFn creator);
  static void EnsureBackendsRegistered();
  [[nodiscard]] static std::vector<ERHIBackend> GetAvailableBackends();
  [[nodiscard]] static std::unique_ptr<IRHIContext> CreateContext(
      ERHIBackend backend,
      const RHIContextDesc& desc);
};

}  // namespace Fabrica::RHI