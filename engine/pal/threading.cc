#include "pal/threading.h"

#include <algorithm>
#include <string>

#include "core/config/config.h"
#include "core/logging/logger.h"

#if FABRICA_PLATFORM(WINDOWS)
#include <windows.h>
#elif FABRICA_PLATFORM(LINUX)
#include <pthread.h>
#include <sched.h>
#endif

namespace Fabrica::PAL {

void SetThreadName(ThreadHandle handle, std::string_view name) {
#if FABRICA_PLATFORM(WINDOWS)
  if (handle == nullptr) {
    return;
  }
  const std::wstring wname(name.begin(), name.end());
  SetThreadDescription(handle, wname.c_str());
#elif FABRICA_PLATFORM(LINUX)
  std::string truncated(name.substr(0, 15));
  pthread_setname_np(handle, truncated.c_str());
#else
  (void)handle;
  (void)name;
#endif

#if FABRICA_PAL_VERBOSE_LOG
  FABRICA_LOG(PAL, Debug) << "[PAL][Thread] Setting name '" << name
                          << "' for thread " << GetThreadId();
#endif
}

Core::Status SetThreadAffinity(ThreadHandle handle, std::uint64_t affinity_mask) {
#if FABRICA_PLATFORM(WINDOWS)
  if (SetThreadAffinityMask(handle, static_cast<DWORD_PTR>(affinity_mask)) == 0) {
    return Core::Status::Internal("SetThreadAffinityMask failed");
  }
#elif FABRICA_PLATFORM(LINUX)
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  for (int i = 0; i < 64; ++i) {
    if ((affinity_mask & (1ull << i)) != 0) {
      CPU_SET(i, &cpu_set);
    }
  }
  if (pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpu_set) != 0) {
    return Core::Status::Internal("pthread_setaffinity_np failed");
  }
#else
  (void)handle;
  (void)affinity_mask;
#endif

#if FABRICA_PAL_VERBOSE_LOG
  FABRICA_LOG(PAL, Debug) << "[PAL][Thread] Set affinity mask=0x" << std::hex
                          << affinity_mask;
#endif
  return Core::Status::Ok();
}

std::uint64_t GetThreadId() {
  return static_cast<std::uint64_t>(
      std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

std::uint32_t GetHardwareConcurrency() {
  return std::max<std::uint32_t>(std::thread::hardware_concurrency(), 1);
}

}  // namespace Fabrica::PAL

