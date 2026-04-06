#include <memory>
#include <string>

#include "core/common/assert.h"
#include "core/common/status.h"
#include "core/common/test/test_framework.h"
#include "core/common/typed_id.h"

namespace {

using Fabrica::Core::ErrorCode;
using Fabrica::Core::Status;
using Fabrica::Core::StatusOr;

struct EntityTag {};
using EntityId = Fabrica::Core::TypedId<EntityTag>;

FABRICA_TEST(StatusOkByDefault) {
  Status status;
  FABRICA_EXPECT_TRUE(status.ok());
  FABRICA_EXPECT_EQ(status.code(), ErrorCode::kOk);
}

FABRICA_TEST(StatusOrCarriesValue) {
  StatusOr<int> result(42);
  FABRICA_EXPECT_TRUE(result.ok());
  FABRICA_EXPECT_EQ(result.value(), 42);
}

FABRICA_TEST(StatusOrCarriesError) {
  StatusOr<int> result(Status::NotFound("asset"));
  FABRICA_EXPECT_TRUE(!result.ok());
  FABRICA_EXPECT_EQ(result.status().code(), ErrorCode::kNotFound);
}

FABRICA_TEST(TypedIdStrongTypingAndValidity) {
  EntityId invalid = EntityId::Invalid();
  EntityId id(EntityId::UnderlyingType{7});
  FABRICA_EXPECT_TRUE(!invalid.IsValid());
  FABRICA_EXPECT_TRUE(id.IsValid());
  FABRICA_EXPECT_EQ(id.Value(), 7u);
}

FABRICA_TEST(VerifyReturnsFailedPrecondition) {
  Status status = FABRICA_VERIFY(false, "must fail");
  FABRICA_EXPECT_TRUE(!status.ok());
  FABRICA_EXPECT_EQ(status.code(), ErrorCode::kFailedPrecondition);
}

}  // namespace

int main() { return Fabrica::Core::Test::Registry::Instance().RunAll(); }

