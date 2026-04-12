#include <algorithm>

#include "core/common/test/test_framework.h"
#include "rhi/RHIFactory.h"

namespace {

using namespace Fabrica::RHI;

FABRICA_TEST(RHIFactoryRegistersVulkanBackendExplicitly) {
  const auto backends = RHIFactory::GetAvailableBackends();
  const auto it = std::find(backends.begin(), backends.end(), ERHIBackend::Vulkan);
  FABRICA_EXPECT_TRUE(it != backends.end());
}

}  // namespace