#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "core/common/holdable.h"
#include "core/common/invocable.h"
#include "core/common/status.h"
#include "core/jobs/executor.h"

namespace Fabrica::Core::Async::Internal {

/**
 * Type-erased container for future result values.
 */
class ResultHolder {
 public:
  ResultHolder() : held_result_(nullptr, nullptr) {}

  /**
   * Store one result value by move.
   */
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

  /// Return true when holder contains a value.
  bool HasResult() const { return held_result_ != nullptr; }

  /// Access stored result as type `T`.
  template <typename T>
  const T& GetAs() const {
    return *static_cast<T*>(held_result_.get());
  }

  /// Move stored result as type `T`.
  template <typename T>
  T MoveAs() {
    return std::move(*static_cast<T*>(held_result_.get()));
  }

 private:
  std::unique_ptr<void, void (*)(void const*)> held_result_;
};

/**
 * Shared state backing `Future<T>` values and continuations.
 *
 * Stores readiness, dependencies, parent/nested links, and optional executor
 * task context for reprioritization.
 */
class FutureImpl : public std::enable_shared_from_this<FutureImpl> {
 public:
  using OnReadyCallback = Core::Invocable<void(ResultHolder&)>;

  /**
   * RAII holder that keeps `FutureImpl` alive across async boundaries.
   */
  class FutureImplWrapper {
   public:
    explicit FutureImplWrapper(std::shared_ptr<FutureImpl> impl);
    ~FutureImplWrapper();

    /// Return shared implementation pointer.
    std::shared_ptr<FutureImpl>& GetImpl();

   private:
    std::shared_ptr<FutureImpl> impl_;
  };

  FutureImpl();
  explicit FutureImpl(ResultHolder result_holder);

  /// Return true when future already has a terminal result.
  bool Ready() const;

  /// Access terminal result holder.
  ResultHolder& Get();

  /**
   * Register callback executed when future transitions to ready state.
   */
  void OnReady(OnReadyCallback callback);

  /**
   * Attach an owned dependency token to keep related objects alive.
   */
  void DependsOn(Core::Holdable holdable);

  /// Remove all dependency tokens.
  void ClearDependencies();

  /// Link parent continuation future.
  void SetParentFuture(std::shared_ptr<FutureImplWrapper> parent_future);

  /// Link nested future produced by continuation flattening.
  void SetNestedFuture(std::shared_ptr<FutureImplWrapper> nested_future);

  /// Resolve future with result value.
  void Return(ResultHolder result_holder);

  /// Resolve future with cancellation status.
  void Cancel(const Core::Status& reason =
                  Core::Status::Cancelled("Future cancelled"));

  /**
   * Bind executor/task identity for later reprioritization.
   */
  void SetTaskContext(Jobs::Executor* executor, Jobs::TaskId task_id,
                      int task_priority);

  /// Update task priority when bound executor supports reprioritization.
  void UpdatePriority(int priority);

  /// Return continuation depth from root future.
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
