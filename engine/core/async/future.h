#pragma once

#include <mutex>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/async/future_group.h"
#include "core/async/future_impl.h"
#include "core/common/assert.h"
#include "core/common/holdable.h"
#include "core/common/status.h"
#include "core/jobs/executor.h"

namespace Fabrica::Core::Async {

template <typename T>
class Future;

template <typename T>
class WeakFuture;

namespace Internal {

template <typename T>
struct IsFuture : std::false_type {};

template <typename T>
struct IsFuture<Future<T>> : std::true_type {};

template <typename T>
inline constexpr bool IsFutureV = IsFuture<T>::value;

template <typename T>
struct FutureValueFromReturn {
  using Type = std::decay_t<T>;
};

template <>
struct FutureValueFromReturn<void> {
  using Type = Core::Status;
};

template <typename T>
struct FutureValueFromReturn<Core::StatusOr<T>> {
  using Type = T;
};

template <>
struct FutureValueFromReturn<Core::Status> {
  using Type = Core::Status;
};

template <typename T>
struct FutureValueFromReturn<Future<T>> {
  using Type = T;
};

template <typename T>
using FutureValueFromReturnT = typename FutureValueFromReturn<T>::Type;

template <typename>
inline constexpr bool AlwaysFalseV = false;

}  // namespace Internal

/**
 * Composable typed future with continuation chaining and fan-in utilities.
 *
 * Future<T> wraps shared async state and supports continuation scheduling,
 * status propagation, tuple/vector merges, cancellation, and priority updates.
 *
 * @tparam T Success value type (Core::Status represents status-only flows).
 */
template <typename T>
class Future {
 public:
  using Value = T;
  using Result =
      std::conditional_t<std::is_same_v<T, Core::Status>, Core::Status,
                         Core::StatusOr<T>>;
  using ImplWrapper = Internal::FutureImpl::FutureImplWrapper;

  /**
   * Create a pending future with a fresh shared implementation state.
   */
  Future()
      : impl_wrapper_(std::make_shared<ImplWrapper>(
            std::make_shared<Internal::FutureImpl>())) {}

  explicit Future(Result result) : Future() {
    impl_wrapper_->GetImpl()->Return(Internal::ResultHolder(std::move(result)));
  }

  bool Ready() const { return impl_wrapper_->GetImpl()->Ready(); }

  /**
   * Access ready result by const reference.
   *
   * @pre Ready() is true.
   */
  const Result& Get() const {
    FABRICA_ASSERT(Ready(), "Future::Get called before Ready");
    return impl_wrapper_->GetImpl()->Get().GetAs<Result>();
  }

  Result Move() const {
    FABRICA_ASSERT(Ready(), "Future::Move called before Ready");
    auto& holder = impl_wrapper_->GetImpl()->Get();
    if constexpr (std::is_copy_constructible_v<Result>) {
      return holder.GetAs<Result>();
    } else {
      return holder.MoveAs<Result>();
    }
  }

  /**
   * Attach a continuation and return a child future.
   *
   * Continuation errors propagate automatically unless callback explicitly
   * accepts the parent result.
   */
  template <typename Fn>
  auto Then(Fn&& fn, Jobs::Executor::Type executor_type =
                        Jobs::Executor::Type::kForeground) const {
    using ThenReturnT =
        decltype(InvokeThenFunctor(std::declval<Fn&>(), std::declval<const Result&>()));
    using ChildValue = Internal::FutureValueFromReturnT<ThenReturnT>;

    Future<ChildValue> child;
    auto parent_impl = impl_wrapper_->GetImpl();
    auto child_impl_wrapper = child.impl_wrapper_;
    child_impl_wrapper->GetImpl()->SetParentFuture(impl_wrapper_);
    child.DependsOn(*this);

    parent_impl->OnReady(Core::Invocable<void(Internal::ResultHolder&)>(
        [parent_impl, child_impl_wrapper, executor_type,
         callback = std::forward<Fn>(fn)](Internal::ResultHolder&) mutable {
          auto run_chain = Core::Invocable<void()>(
              [parent_impl, child_impl_wrapper,
               callback = std::move(callback)]() mutable {
                auto child_impl = child_impl_wrapper->GetImpl();
                if (child_impl->Ready()) {
                  return;
                }

                const Result& parent_result = parent_impl->Get().GetAs<Result>();
                if (ShouldPropagateParentError<Fn>(parent_result)) {
                  child_impl->Return(
                      Internal::ResultHolder(Future<ChildValue>::FromStatus(
                          ExtractStatus(parent_result))));
                  return;
                }

                if constexpr (std::is_void_v<ThenReturnT>) {
                  InvokeThenFunctor(callback, parent_result);
                  child_impl->Return(Internal::ResultHolder(
                      Future<ChildValue>::FromStatus(Core::Status::Ok())));
                } else {
                  auto produced = InvokeThenFunctor(callback, parent_result);
                  Future<ChildValue>::template ReturnProducedResult<ThenReturnT>(
                      child_impl_wrapper, std::move(produced));
                }
              });

          Future<ChildValue>::ScheduleOnExecutor(child_impl_wrapper->GetImpl(),
                                                 executor_type,
                                                 std::move(run_chain));
        }));

    return child;
  }

  /**
   * Schedule a callable on an executor and return its future.
   */
  template <typename Fn>
  static Future<T> Schedule(Fn&& fn,
                            Jobs::Executor::Type executor_type =
                                Jobs::Executor::Type::kForeground) {
    Future<T> future;
    auto impl = future.impl_wrapper_->GetImpl();
    auto impl_wrapper = future.impl_wrapper_;

    auto task = Core::Invocable<void()>(
        [impl_wrapper, callback = std::forward<Fn>(fn)]() mutable {
      auto impl = impl_wrapper->GetImpl();
      if (impl == nullptr || impl->Ready()) {
        return;
      }

      using ScheduleReturn = std::invoke_result_t<Fn>;
      if constexpr (std::is_void_v<ScheduleReturn>) {
        callback();
        impl->Return(Internal::ResultHolder(FromStatus(Core::Status::Ok())));
      } else {
        auto produced = callback();
        ReturnProducedResult<ScheduleReturn>(impl_wrapper, std::move(produced));
      }
    });

    ScheduleOnExecutor(impl, executor_type, std::move(task));
    return future;
  }

  /**
   * Combine this future with others and fail fast on first error.
   */
  template <typename... FutureTypes>
  Future<Core::Status> Combine(FutureTypes... futures) const {
    std::vector<Future<Core::Status>> status_futures;
    status_futures.reserve(sizeof...(futures) + 1);
    status_futures.push_back(AsStatusFuture(*this));
    (status_futures.push_back(AsStatusFuture(futures)), ...);
    return CombineStatusFutures(status_futures, false);
  }

  /**
   * Combine this future with others and wait for all to complete.
   */
  template <typename... FutureTypes>
  Future<Core::Status> CombineWaitForAll(FutureTypes... futures) const {
    std::vector<Future<Core::Status>> status_futures;
    status_futures.reserve(sizeof...(futures) + 1);
    status_futures.push_back(AsStatusFuture(*this));
    (status_futures.push_back(AsStatusFuture(futures)), ...);
    return CombineStatusFutures(status_futures, true);
  }

  /**
   * Merge values from this future and additional futures into one tuple.
   */
  template <typename... FutureTypes>
  auto Merge(FutureTypes... futures) const
      -> Future<std::tuple<T, typename std::decay_t<FutureTypes>::Value...>> {
    using TupleValue = std::tuple<T, typename std::decay_t<FutureTypes>::Value...>;

    auto combined = CombineWaitForAll(futures...);
    const auto futures_tuple = std::make_tuple(*this, futures...);
    return combined.Then(
        [futures_tuple](const Core::Status& status) mutable
            -> Core::StatusOr<TupleValue> {
          if (!status.ok()) {
            return status;
          }
          return BuildTupleValues(futures_tuple,
                                  std::make_index_sequence<sizeof...(FutureTypes) + 1>{});
        },
        Jobs::Executor::Type::kImmediate);
  }

  template <typename ListT>
  static Future<Core::Status> CombineList(const ListT& futures) {
    std::vector<Future<Core::Status>> status_futures;
    for (const auto& future : futures) {
      status_futures.push_back(AsStatusFuture(future));
    }
    return CombineStatusFutures(status_futures, false);
  }

  template <typename ListT>
  static auto MergeList(const ListT& futures) -> Future<std::vector<T>> {
    std::vector<Future<T>> typed_futures;
    for (const auto& future : futures) {
      typed_futures.push_back(future);
    }

    std::vector<Future<Core::Status>> status_futures;
    status_futures.reserve(typed_futures.size());
    for (const auto& future : typed_futures) {
      status_futures.push_back(AsStatusFuture(future));
    }

    auto combined = CombineStatusFutures(status_futures, true);
    return combined.Then(
        [typed_futures](const Core::Status& status) mutable
            -> Core::StatusOr<std::vector<T>> {
          if (!status.ok()) {
            return status;
          }
          std::vector<T> values;
          values.reserve(typed_futures.size());
          for (const auto& future : typed_futures) {
            values.push_back(ExtractValue(future.Get()));
          }
          return values;
        },
        Jobs::Executor::Type::kImmediate);
  }

  /// Cancel the future with default cancelled status.
  void Cancel() { impl_wrapper_->GetImpl()->Cancel(); }

  /**
   * Attach dependency tokens that must remain alive until completion.
   */
  template <typename... Args>
  void DependsOn(Args&&... args) const {
    (impl_wrapper_->GetImpl()->DependsOn(
         Core::Holdable(std::forward<Args>(args))),
     ...);
  }

  /**
   * Store this future in an external owner using callable/Remember protocol.
   */
  template <typename Rememberer>
  void KeptBy(Rememberer rememberer) const {
    if constexpr (requires { rememberer(*this); }) {
      rememberer(*this);
    } else if constexpr (requires { rememberer.Remember(*this); }) {
      rememberer.Remember(*this);
    } else {
      static_assert(Internal::AlwaysFalseV<Rememberer>,
                    "Rememberer must be callable or implement Remember(Future)");
    }
  }

  /// Update task priority when underlying executor supports reprioritization.
  void UpdatePriority(int priority) { impl_wrapper_->GetImpl()->UpdatePriority(priority); }

 private:
  friend class WeakFuture<T>;
  template <typename>
  friend class Future;

  explicit Future(std::shared_ptr<ImplWrapper> impl_wrapper)
      : impl_wrapper_(std::move(impl_wrapper)) {}

  template <typename Fn>
  static auto InvokeThenFunctor(Fn& fn, const Result& parent_result) {
    if constexpr (std::is_same_v<T, Core::Status>) {
      if constexpr (std::is_invocable_v<Fn, const Result&>) {
        return fn(parent_result);
      } else if constexpr (std::is_invocable_v<Fn>) {
        return fn();
      } else {
        static_assert(Internal::AlwaysFalseV<Fn>,
                      "Unsupported Then callback signature");
      }
    } else {
      if constexpr (std::is_invocable_v<Fn, const Result&>) {
        return fn(parent_result);
      } else if constexpr (std::is_invocable_v<Fn, const T&>) {
        return fn(parent_result.value());
      } else if constexpr (std::is_invocable_v<Fn>) {
        return fn();
      } else {
        static_assert(Internal::AlwaysFalseV<Fn>,
                      "Unsupported Then callback signature");
      }
    }
  }

  template <typename Fn>
  static bool ShouldPropagateParentError(const Result& parent_result) {
    constexpr bool callback_accepts_result =
        std::is_invocable_v<Fn, const Result&>;
    return !ExtractStatus(parent_result).ok() && !callback_accepts_result;
  }

  static Core::Status ExtractStatus(const Result& result) {
    if constexpr (std::is_same_v<T, Core::Status>) {
      return result;
    } else {
      return result.status();
    }
  }

  static T ExtractValue(const Result& result) {
    if constexpr (std::is_same_v<T, Core::Status>) {
      return result;
    } else {
      return result.value();
    }
  }

  static Result FromStatus(const Core::Status& status) {
    if constexpr (std::is_same_v<T, Core::Status>) {
      return status;
    } else {
      return Result(status);
    }
  }

  template <typename Produced>
  static Result ConvertProduced(Produced&& produced) {
    using ProducedType = std::decay_t<Produced>;
    if constexpr (std::is_same_v<ProducedType, Result>) {
      return std::forward<Produced>(produced);
    } else if constexpr (std::is_same_v<T, Core::Status> &&
                         std::is_same_v<ProducedType, Core::Status>) {
      return std::forward<Produced>(produced);
    } else if constexpr (!std::is_same_v<T, Core::Status> &&
                         std::is_same_v<ProducedType, T>) {
      return Result(std::forward<Produced>(produced));
    } else if constexpr (!std::is_same_v<T, Core::Status> &&
                         std::is_same_v<ProducedType, Core::StatusOr<T>>) {
      return std::forward<Produced>(produced);
    } else if constexpr (!std::is_same_v<T, Core::Status> &&
                         std::is_same_v<ProducedType, Core::Status>) {
      return Result(std::forward<Produced>(produced));
    } else {
      static_assert(Internal::AlwaysFalseV<ProducedType>,
                    "Unsupported return type for Future conversion");
    }
  }

  template <typename Produced>
  static void ReturnProducedResult(
      const std::shared_ptr<ImplWrapper>& target_impl_wrapper, Produced&& produced) {
    using ProducedType = std::decay_t<Produced>;
    auto target_impl = target_impl_wrapper->GetImpl();

    if constexpr (Internal::IsFutureV<ProducedType>) {
      ProducedType nested_future = std::forward<Produced>(produced);
      target_impl->SetNestedFuture(nested_future.impl_wrapper_);
      nested_future.impl_wrapper_->GetImpl()->OnReady(
          Core::Invocable<void(Internal::ResultHolder&)>(
              [target_impl, nested_future](Internal::ResultHolder&) mutable {
                if (target_impl->Ready()) {
                  return;
                }
                target_impl->Return(Internal::ResultHolder(nested_future.Get()));
              }));
      return;
    }

    target_impl->Return(
        Internal::ResultHolder(ConvertProduced(std::forward<Produced>(produced))));
  }

  static bool ScheduleOnExecutor(std::shared_ptr<Internal::FutureImpl> impl,
                                 Jobs::Executor::Type executor_type,
                                 Core::Invocable<void()> task,
                                 int priority = Jobs::kNormalTaskPriority) {
    Jobs::Executor* executor = Jobs::Executor::Get(executor_type);
    if (executor == nullptr && executor_type != Jobs::Executor::Type::kImmediate) {
      executor = Jobs::Executor::Get(Jobs::Executor::Type::kImmediate);
    }
    if (executor == nullptr) {
      impl->Return(Internal::ResultHolder(
          FromStatus(Core::Status::Unavailable("Executor not configured"))));
      return false;
    }

    const Jobs::TaskId reserved_task_id = executor->ReserveTaskId();
    impl->SetTaskContext(executor, reserved_task_id, priority);

    bool scheduled = false;
    if (reserved_task_id == Jobs::kGenericTaskId) {
      scheduled = executor->ScheduleInvocable(std::move(task), priority).IsValid();
    } else {
      scheduled = executor->ScheduleWithReservedTaskId(reserved_task_id,
                                                       std::move(task), priority);
    }

    if (!scheduled) {
      impl->Return(Internal::ResultHolder(
          FromStatus(Core::Status::Unavailable("Failed to schedule task"))));
    }
    return scheduled;
  }

  template <typename U>
  static Future<Core::Status> AsStatusFuture(const Future<U>& future) {
    if constexpr (std::is_same_v<U, Core::Status>) {
      return future;
    } else {
      return future.Then(
          [](const typename Future<U>::Result& result) { return result.status(); },
          Jobs::Executor::Type::kImmediate);
    }
  }

  static Future<Core::Status> CombineStatusFutures(
      const std::vector<Future<Core::Status>>& futures, bool wait_for_all) {
    Future<Core::Status> result;
    if (futures.empty()) {
      result.impl_wrapper_->GetImpl()->Return(Internal::ResultHolder(Core::Status::Ok()));
      return result;
    }

    struct SharedState {
      explicit SharedState(size_t count, bool wait_for_all)
          : remaining(count), wait_all(wait_for_all) {}
      std::mutex mutex;
      size_t remaining = 0;
      bool wait_all = false;
      bool done = false;
      bool has_error = false;
      Core::Status first_error = Core::Status::Ok();
      std::shared_ptr<ImplWrapper> result_impl;
      std::vector<Future<Core::Status>> keep_alive;
    };

    auto state = std::make_shared<SharedState>(futures.size(), wait_for_all);
    state->result_impl = result.impl_wrapper_;
    state->keep_alive = futures;

    for (const auto& status_future : futures) {
      status_future.impl_wrapper_->GetImpl()->OnReady(
          Core::Invocable<void(Internal::ResultHolder&)>(
              [state, status_future](Internal::ResultHolder&) mutable {
                const Core::Status status = status_future.Get();

                bool should_return = false;
                Core::Status final_status = Core::Status::Ok();

                {
                  std::lock_guard<std::mutex> lock(state->mutex);
                  if (state->done) {
                    return;
                  }

                  if (!status.ok() && !state->has_error) {
                    state->has_error = true;
                    state->first_error = status;
                    if (!state->wait_all) {
                      state->done = true;
                      should_return = true;
                      final_status = status;
                    }
                  }

                  if (state->remaining > 0) {
                    --state->remaining;
                  }

                  if (!state->done && state->remaining == 0) {
                    state->done = true;
                    should_return = true;
                    final_status = state->has_error ? state->first_error
                                                    : Core::Status::Ok();
                  }
                }

                if (should_return) {
                  state->result_impl->GetImpl()->Return(
                      Internal::ResultHolder(final_status));
                }
              }));
    }

    return result;
  }

  template <typename TupleT, size_t... I>
  static auto BuildTupleValues(const TupleT& futures_tuple,
                               std::index_sequence<I...>) {
    return std::make_tuple(ExtractFutureValue(std::get<I>(futures_tuple))...);
  }

  template <typename FutureT>
  static auto ExtractFutureValue(const FutureT& future) {
    using FutureValueT = typename FutureT::Value;
    const auto& result = future.Get();
    if constexpr (std::is_same_v<FutureValueT, Core::Status>) {
      return result;
    } else {
      return result.value();
    }
  }

  std::shared_ptr<ImplWrapper> impl_wrapper_;
};

/**
 * Weak non-owning reference to a Future<T> shared state.
 */
template <typename T>
class WeakFuture {
 public:
  WeakFuture() = default;
  WeakFuture(const Future<T>& future) : impl_wrapper_(future.impl_wrapper_) {}

  /**
   * Lock and promote to a strong future when state is still alive.
   */
  std::optional<Future<T>> Lock() const {
    if (auto impl_wrapper = impl_wrapper_.lock()) {
      return Future<T>(std::move(impl_wrapper));
    }
    return std::nullopt;
  }

 private:
  std::weak_ptr<typename Future<T>::ImplWrapper> impl_wrapper_;
};

/**
 * Create a weak reference from a strong future.
 */
template <typename T>
WeakFuture<T> MakeWeak(Future<T> future) {
  return WeakFuture<T>(future);
}

}  // namespace Fabrica::Core::Async


