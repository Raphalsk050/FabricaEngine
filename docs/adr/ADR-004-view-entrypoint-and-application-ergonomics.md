## ADR-004: View-Based Application Entry Point and Ergonomics API
**Status**: Accepted

**Context**:
Applications integrating FabricaEngine were still required to wire runtime callbacks manually, call engine initialization explicitly in app code, and use verbose namespace-qualified types. The ECS API also forced direct `World` access for component mutation, and input actions lacked a dedicated multi-key binding model.

**Decision**:
Adopt an application-facing API layer centered on `BaseView` and `ViewApplication`:

1. `BaseView` lifecycle hooks
- `ConfigureWindow`, `Awake`, `Start`, `Update`, `Render`, `OnEvent`, `OnInputAction`, `Shutdown`.
- Runtime maps these hooks automatically through `ViewApplication`.

2. Entry point simplification
- Add `core/app/fabrica.h` with ergonomic aliases and `FABRICA_DEFINE_VIEW_MAIN(ViewType)`.
- Applications now declare a view class and use a single macro instead of manually wiring `EngineRuntime` callbacks.

3. ECS entity ergonomics
- Add `Core::ECS::EntityHandle` with `AddComponent`, `RemoveComponent`, `HasComponent`, and `GetComponent`.
- Keep `World` API backward compatible while enabling `entity.AddComponent<T>(...)` flow.

4. Input actions API
- Add `Core::Input::InputActionMap` with `Bind`, `Unbind`, and `UnbindAll`.
- Support multiple keys per action and lifecycle-friendly action dispatch (`Started`, `Repeated`, `Completed`).

5. Runtime-managed initialization and async usability
- Extend runtime config with logger lifecycle control and initialize logger from runtime by default.
- `Future` scheduling now falls back to the immediate executor when requested executors are not configured, reducing boilerplate for basic usage.

**Alternatives considered**:
1. Keep callback-based runtime API only and improve docs.
- Rejected: does not address verbosity and repeated setup costs.

2. Add a separate code generator for app bootstrap.
- Rejected: adds tooling complexity and does not improve core runtime contracts.

3. Keep strict failure when executor is unavailable.
- Rejected: preserves friction for simple app-level async tasks where immediate execution is acceptable.

**Consequences**:
- Faster app onboarding with less boilerplate and clearer lifecycle structure.
- Improved ECS and input ergonomics without breaking existing runtime and world APIs.
- Async behavior becomes safer by default in non-bootstrapped contexts via immediate fallback, with explicit executor routing still available for production pipelines.
