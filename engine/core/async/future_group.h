#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>

#include "core/jobs/task_priority.h"

namespace Fabrica::Core::Async::Internal {
class FutureImpl;
}

namespace Fabrica::Core::Async {

class FutureGroup {
 public:
  explicit FutureGroup(int task_priority = Jobs::kNormalTaskPriority);

  void AddFuture(std::shared_ptr<Internal::FutureImpl> future);
  void UpdatePriority(int task_priority);
  int GetTaskPriority() const;

 private:
  struct FutureGroupImpl {
    mutable std::mutex mutex;
    int cached_priority = Jobs::kNormalTaskPriority;
    std::unordered_map<int, std::weak_ptr<Internal::FutureImpl>> futures;
    int next_future_id = 0;
  };

  std::shared_ptr<FutureGroupImpl> impl_;
};

}  // namespace Fabrica::Core::Async

