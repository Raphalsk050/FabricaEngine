#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "core/common/holdable.h"
#include "core/common/invocable.h"
#include "core/common/status.h"
#include "core/jobs/executor.h"

namespace Fabrica::Core::Async::Internal {

class ResultHolder {
 public:
  ResultHolder() : held_result_(nullptr, nullptr) {}

  template <typename T>
  explicit ResultHolder(T result)
      : held_result_(
            new T(std::move(result)), +[](void const* ptr) {
              const auto* typed_ptr = static_cast<const T*>(ptr);
              delete typed_ptr;
            }) {}

  ResultHolder(const ResultHolder&) = delete;
  ResultHolder& operator=(const ResultHolder&) = delete;
  ResultHolder(ResultHolder&&) noexcept = default;
  ResultHolder& operator=(ResultHolder&&) noexcept = default;

  bool HasResult() const { return held_result_ != nullptr; }

  template <typename T>
  const T& GetAs() const {
    return *static_cast<T*>(held_result_.get());
  }

  template <typename T>
  T MoveAs() {
    return std::move(*static_cast<T*>(held_result_.get()));
  }

 private:
  std::unique_ptr<void, void (*)(void const*)> held_result_;
};

class FutureImpl : public std::enable_shared_from_this<FutureImpl> {
 public:
  using OnReadyCallback = Core::Invocable<void(ResultHolder&)>;

  class FutureImplWrapper {
   public:
    explicit FutureImplWrapper(std::shared_ptr<FutureImpl> impl);
    ~FutureImplWrapper();

    std::shared_ptr<FutureImpl>& GetImpl();

   private:
    std::shared_ptr<FutureImpl> impl_;
  };

  FutureImpl();
  explicit FutureImpl(ResultHolder result_holder);

  bool Ready() const;
  ResultHolder& Get();
  void OnReady(OnReadyCallback callback);
  void DependsOn(Core::Holdable holdable);
  void ClearDependencies();

  void SetParentFuture(std::shared_ptr<FutureImplWrapper> parent_future);
  void SetNestedFuture(std::shared_ptr<FutureImplWrapper> nested_future);

  void Return(ResultHolder result_holder);
  void Cancel(const Core::Status& reason =
                  Core::Status::Cancelled("Future cancelled"));

  void SetTaskContext(Jobs::Executor* executor, Jobs::TaskId task_id,
                      int task_priority);
  void UpdatePriority(int priority);
  int GetDepth() const;

 private:
  bool ReturnInternal(ResultHolder result_holder);

  mutable std::mutex mutex_;
  bool ready_ = false;
  ResultHolder result_;
  std::vector<OnReadyCallback> on_ready_;
  std::vector<Core::Holdable> depends_on_;
  std::shared_ptr<FutureImplWrapper> parent_future_;
  std::shared_ptr<FutureImplWrapper> nested_future_;
  Jobs::Executor* executor_ = nullptr;
  Jobs::TaskId task_id_ = Jobs::kInvalidTaskId;
  int task_priority_ = Jobs::kNormalTaskPriority;
  int depth_ = 0;
};

}  // namespace Fabrica::Core::Async::Internal

