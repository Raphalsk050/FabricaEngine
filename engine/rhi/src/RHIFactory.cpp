#include "rhi/RHIFactory.h"

#include <mutex>
#include <unordered_map>

namespace Fabrica::RHI::Vulkan {

void EnsureVulkanBackendRegistered();

}  // namespace Fabrica::RHI::Vulkan

namespace Fabrica::RHI {
namespace {

using RegistryMap = std::unordered_map<ERHIBackend, RHIFactory::ContextCreatorFn>;

RegistryMap& GetRegistry() {
  static RegistryMap registry;
  return registry;
}

std::mutex& GetRegistryMutex() {
  static std::mutex mutex;
  return mutex;
}

}  // namespace

void RHIFactory::RegisterBackend(ERHIBackend backend, ContextCreatorFn creator) {
  std::scoped_lock lock(GetRegistryMutex());
  GetRegistry()[backend] = std::move(creator);
}

void RHIFactory::EnsureBackendsRegistered() {
  Vulkan::EnsureVulkanBackendRegistered();
}

std::vector<ERHIBackend> RHIFactory::GetAvailableBackends() {
  EnsureBackendsRegistered();
  std::scoped_lock lock(GetRegistryMutex());

  std::vector<ERHIBackend> backends;
  backends.reserve(GetRegistry().size());
  for (const auto& [backend, _] : GetRegistry()) {
    backends.push_back(backend);
  }
  return backends;
}

std::unique_ptr<IRHIContext> RHIFactory::CreateContext(ERHIBackend backend,
                                                       const RHIContextDesc& desc) {
  EnsureBackendsRegistered();
  std::scoped_lock lock(GetRegistryMutex());

  const auto it = GetRegistry().find(backend);
  if (it == GetRegistry().end() || !it->second) {
    return nullptr;
  }

  std::unique_ptr<IRHIContext> context = it->second();
  if (context == nullptr || context->Initialize(desc) != RHIResult::Success) {
    return nullptr;
  }

  return context;
}

}  // namespace Fabrica::RHI
