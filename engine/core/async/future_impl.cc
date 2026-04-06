#include "core/async/future_impl.h"

#include <cassert>

namespace Fabrica::Core::Async::Internal {

FutureImpl::FutureImpl() = default;

FutureImpl::FutureImpl(ResultHolder result_holder) {
  Return(std::move(result_holder));
}

FutureImpl::FutureImplWrapper::FutureImplWrapper(std::shared_ptr<FutureImpl> impl)
    : impl_(std::move(impl)) {}

FutureImpl::FutureImplWrapper::~FutureImplWrapper() {
  if (impl_ != nullptr && !impl_->Ready()) {
    impl_->Cancel(Core::Status::Cancelled("Future destroyed before completion"));
  }
}

std::shared_ptr<FutureImpl>& FutureImpl::FutureImplWrapper::GetImpl() {
  return impl_;
}

bool FutureImpl::Ready() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return ready_;
}

ResultHolder& FutureImpl::Get() {
  std::lock_guard<std::mutex> lock(mutex_);
  assert(ready_ && "Future is not ready");
  return result_;
}

void FutureImpl::OnReady(OnReadyCallback callback) {
  bool call_now = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ready_) {
      call_now = true;
    } else {
      on_ready_.push_back(std::move(callback));
    }
  }

  if (call_now) {
    callback(Get());
  }
}

void FutureImpl::DependsOn(Core::Holdable holdable) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (ready_) {
    return;
  }
  depends_on_.push_back(std::move(holdable));
}

void FutureImpl::ClearDependencies() {
  std::lock_guard<std::mutex> lock(mutex_);
  depends_on_.clear();
  parent_future_.reset();
  nested_future_.reset();
}

void FutureImpl::SetParentFuture(
    std::shared_ptr<FutureImplWrapper> parent_future) {
  std::lock_guard<std::mutex> lock(mutex_);
  parent_future_ = std::move(parent_future);
  if (parent_future_ != nullptr) {
    depth_ = parent_future_->GetImpl()->GetDepth() + 1;
  }
}

void FutureImpl::SetNestedFuture(
    std::shared_ptr<FutureImplWrapper> nested_future) {
  std::lock_guard<std::mutex> lock(mutex_);
  nested_future_ = std::move(nested_future);
}

void FutureImpl::Return(ResultHolder result_holder) {
  std::vector<OnReadyCallback> callbacks;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ReturnInternal(std::move(result_holder))) {
      return;
    }
    callbacks = std::move(on_ready_);
    depends_on_.clear();
    parent_future_.reset();
    nested_future_.reset();
  }

  for (auto& callback : callbacks) {
    callback(Get());
  }
}

void FutureImpl::Cancel(const Core::Status& reason) {
  Return(ResultHolder(reason));
}

void FutureImpl::SetTaskContext(Jobs::Executor* executor, Jobs::TaskId task_id,
                                int task_priority) {
  std::lock_guard<std::mutex> lock(mutex_);
  executor_ = executor;
  task_id_ = task_id;
  task_priority_ = task_priority;
}

void FutureImpl::UpdatePriority(int priority) {
  std::lock_guard<std::mutex> lock(mutex_);
  task_priority_ = priority;
  if (executor_ != nullptr && task_id_.IsValid()) {
    executor_->UpdateTaskPriority(task_id_, priority);
  }
}

int FutureImpl::GetDepth() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return depth_;
}

bool FutureImpl::ReturnInternal(ResultHolder result_holder) {
  if (ready_) {
    return false;
  }
  result_ = std::move(result_holder);
  ready_ = true;
  return true;
}

}  // namespace Fabrica::Core::Async::Internal

