#pragma once

#include <cstddef>

namespace Fabrica::Core::Logging {

/**
 * Enumerates structured logging channels used by the engine.
 */
enum class LogChannel : size_t {
  kCore = 0,
  kAsync,
  kJobs,
  kAssets,
  kRender,
  kECS,
  kPAL,
  kWindow,
  kMemory,
  kGame,
  kCount,
};

constexpr size_t kLogChannelCount = static_cast<size_t>(LogChannel::kCount);

}  // namespace Fabrica::Core::Logging
