#include "core/common/test/test_framework.h"
#include "rhi/RHITypes.h"

namespace {

using namespace Fabrica::RHI;

FABRICA_TEST(RHIHandleEncodingRoundTripsTypeGenerationAndIndex) {
  const std::uint64_t encoded = EncodeHandleId(42u, ERHIResourceType::kTexture, 123456u);
  FABRICA_EXPECT_EQ(DecodeHandleGeneration(encoded), 42u);
  FABRICA_EXPECT_EQ(DecodeHandleType(encoded), ERHIResourceType::kTexture);
  FABRICA_EXPECT_EQ(DecodeHandleIndex(encoded), 123456u);
}

FABRICA_TEST(RHIHandleValidityMatchesZeroSentinel) {
  RHIBufferHandle invalid{};
  RHIBufferHandle valid{EncodeHandleId(1u, ERHIResourceType::kBuffer, 7u)};
  FABRICA_EXPECT_TRUE(!invalid.IsValid());
  FABRICA_EXPECT_TRUE(valid.IsValid());
}

}  // namespace

int main() { return Fabrica::Core::Test::Registry::Instance().RunAll(); }
