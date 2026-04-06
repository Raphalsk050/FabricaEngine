#include <memory>
#include <utility>

#include "core/common/holdable.h"
#include "core/common/test/test_framework.h"

namespace {

using Fabrica::Core::Holdable;

FABRICA_TEST(HoldableCanStoreMoveOnlyObject) {
  Holdable holdable(std::make_unique<int>(15));
  FABRICA_EXPECT_TRUE(static_cast<bool>(holdable));
}

FABRICA_TEST(HoldableMoveConstructionKeepsOwnership) {
  Holdable source(std::make_unique<int>(21));
  Holdable moved(std::move(source));
  FABRICA_EXPECT_TRUE(static_cast<bool>(moved));
}

}  // namespace

