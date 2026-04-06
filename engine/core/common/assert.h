#pragma once

#include <cassert>

#include "core/common/status.h"
#include "core/config/config.h"

#if defined(FABRICA_DEBUG_BUILD) && FABRICA_DEBUG_BUILD
#define FABRICA_ASSERT(condition, message) assert((condition) && (message))
#else
#define FABRICA_ASSERT(condition, message) ((void)sizeof(condition))
#endif

#define FABRICA_VERIFY(condition, message)                                             \
  ((condition)                                                                         \
       ? ::Fabrica::Core::Status::Ok()                                                 \
       : ::Fabrica::Core::Status(::Fabrica::Core::ErrorCode::kFailedPrecondition,     \
                                 (message)))

