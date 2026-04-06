#include <memory>
#include <string>
#include <utility>

#include "core/common/invocable.h"
#include "core/common/test/test_framework.h"

namespace {

using Fabrica::Core::Invocable;

FABRICA_TEST(InvocableRunsLambda) {
  Invocable<int(int, int)> invocable = [](int lhs, int rhs) { return lhs + rhs; };
  FABRICA_EXPECT_EQ(invocable(2, 3), 5);
}

FABRICA_TEST(InvocableSupportsMoveOnlyCapture) {
  auto value = std::make_unique<int>(9);
  Invocable<int()> invocable = [moved = std::move(value)]() { return *moved; };
  FABRICA_EXPECT_EQ(invocable(), 9);
}

FABRICA_TEST(InvocableBooleanState) {
  Invocable<void()> empty;
  FABRICA_EXPECT_TRUE(!static_cast<bool>(empty));
}

}  // namespace

