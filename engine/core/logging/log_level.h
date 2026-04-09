#pragma once

namespace Fabrica::Core::Logging {

/**
 * Defines log severity levels in ascending order.
 */
enum class LogLevel : int {
  kDebug = 0,
  kInfo = 1,
  kWarning = 2,
  kError = 3,
  kFatal = 4,
};

}  // namespace Fabrica::Core::Logging
