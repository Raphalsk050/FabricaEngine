#pragma once

#include <cstdint>
#include <string_view>
#include <thread>

#include "core/common/status.h"

namespace Fabrica::PAL {

using ThreadHandle = std::thread::native_handle_type;

/**
 * Assign a human-readable name to a native thread handle when supported.
 */
void SetThreadName(ThreadHandle handle, std::string_view name);

/**
 * Apply CPU affinity mask to a native thread handle.
 *
 * @return `Status::Ok()` on success, or platform-specific failure.
 */
Core::Status SetThreadAffinity(ThreadHandle handle, std::uint64_t affinity_mask);

/**
 * Return the calling thread id as a stable integer token.
 */
std::uint64_t GetThreadId();

/**
 * Return the number of hardware threads reported by the platform.
 */
std::uint32_t GetHardwareConcurrency();

}  // namespace Fabrica::PAL
