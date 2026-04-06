#include "pal/threading.h"

#include <thread>

#include "core/common/test/test_framework.h"

namespace {

FABRICA_TEST(PALThreadingHardwareConcurrencyIsPositive) {
  FABRICA_EXPECT_TRUE(Fabrica::PAL::GetHardwareConcurrency() > 0);
}

FABRICA_TEST(PALThreadingCanSetThreadName) {
  std::thread thread([]() {});
  Fabrica::PAL::SetThreadName(thread.native_handle(), "fabrica_test_thread");
  thread.join();
  FABRICA_EXPECT_TRUE(true);
}

}  // namespace

