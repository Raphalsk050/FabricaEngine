#pragma once

#include <cstdint>
#include <string_view>
#include <thread>

#include "core/common/status.h"

namespace Fabrica::PAL {

using ThreadHandle = std::thread::native_handle_type;

void SetThreadName(ThreadHandle handle, std::string_view name);
Core::Status SetThreadAffinity(ThreadHandle handle, std::uint64_t affinity_mask);
std::uint64_t GetThreadId();
std::uint32_t GetHardwareConcurrency();

}  // namespace Fabrica::PAL

