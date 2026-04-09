#pragma once

namespace Fabrica::Core::Jobs {

/**
 * Defines standard priority levels shared by all job executors.
 */
inline constexpr int kMinimumTaskPriority = -100;
inline constexpr int kLowTaskPriority = -10;
inline constexpr int kNormalTaskPriority = 0;
inline constexpr int kHighTaskPriority = 10;
inline constexpr int kCriticalTaskPriority = 20;
inline constexpr int kMaximumTaskPriority = 100;

}  // namespace Fabrica::Core::Jobs
