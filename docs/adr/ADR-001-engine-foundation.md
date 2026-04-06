## ADR-001: Foundation for DOD-Oriented Engine Core
**Status**: Accepted

**Context**:  
FabricaEngine needs a C++20 baseline architecture for desktop platforms with strict constraints: no exceptions in engine code, explicit async orchestration, pre-allocated memory patterns, and platform isolation.

**Decision**:  
Adopt a modular core with independent targets for `common`, `memory`, `logging`, `jobs`, `async`, `window`, `assets`, plus a separate `pal` layer.  
Key choices:
- Custom `Status`/`StatusOr<T>` for typed error propagation (no exceptions).
- Custom move-only `Invocable` and `Holdable` for async/lifetime handling.
- `TaskScheduler` + `Executor` abstraction with foreground/background/immediate execution and task reprioritization.
- Custom `Future<T>` with type-erased `FutureImpl`, `WeakFuture`, chaining, cancellation, combine, and merge.
- Async logging with lock-free queue + dedicated writer thread.
- Memory foundations with `LinearAllocator`, `PoolAllocator<T>`, and `StackAllocator`.
- PAL encapsulating thread/file operations and OS includes restricted to PAL `.cc`.

**Alternatives considered**:  
1. Use `std::future` + `std::function` + exceptions.  
Rejected due missing chaining/cancellation model, move-only callback limitations, and weaker control over engine-level error flow.

2. Integrate full third-party task/runtime stack early.  
Rejected to avoid upfront external coupling before core contracts are stable.

**Consequences**:  
- Better control over scheduling, cancellation, and allocation behavior in hot paths.
- Higher implementation complexity in exchange for predictable runtime behavior and clearer ownership boundaries.
- Foundation is ready for ECS/renderer/physics integration with minimal API surface leakage.

