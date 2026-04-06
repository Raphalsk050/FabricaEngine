#include "core/async/future_group.h"

#include "core/async/future_impl.h"

namespace Fabrica::Core::Async {

FutureGroup::FutureGroup(int task_priority)
    : impl_(std::make_shared<FutureGroupImpl>()) {
  impl_->cached_priority = task_priority;
}

void FutureGroup::AddFuture(std::shared_ptr<Internal::FutureImpl> future) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  future->UpdatePriority(impl_->cached_priority);
  impl_->futures.emplace(impl_->next_future_id++, std::move(future));
}

void FutureGroup::UpdatePriority(int task_priority) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->cached_priority = task_priority;
  for (auto it = impl_->futures.begin(); it != impl_->futures.end();) {
    if (auto future = it->second.lock()) {
      future->UpdatePriority(task_priority);
      ++it;
    } else {
      it = impl_->futures.erase(it);
    }
  }
}

int FutureGroup::GetTaskPriority() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->cached_priority;
}

}  // namespace Fabrica::Core::Async

