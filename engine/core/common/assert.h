#pragma once

#include <cassert>

#include "core/common/status.h"
#include "core/config/config.h"

/**
 * Assert in debug builds and compile out in ship builds.
 *
 * Use this macro for invariant checks that must never fail in development.
 */
#if defined(FABRICA_DEBUG_BUILD) && FABRICA_DEBUG_BUILD
#define FABRICA_ASSERT(condition, message) assert((condition) && (message))
#else
#define FABRICA_ASSERT(condition, message) ((void)sizeof(condition))
#endif

/**
 * Validate a condition and convert failure into `Status`.
 *
 * This macro is intended for boundary checks where caller-visible failures are
 * expected and should be propagated without terminating the process.
 */
#define FABRICA_VERIFY(condition, message)                                             \
  ((condition)                                                                         \
       ? ::Fabrica::Core::Status::Ok()                                                 \
       : ::Fabrica::Core::Status(::Fabrica::Core::ErrorCode::kFailedPrecondition,     \
                                 (message)))
