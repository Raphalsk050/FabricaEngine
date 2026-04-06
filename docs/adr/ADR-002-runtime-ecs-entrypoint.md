## ADR-002: Archetype ECS and Runtime Entry Point Foundation
**Status**: Accepted

**Context**:
FabricaEngine already had async/jobs/window/memory foundations, but lacked a robust runtime entry point and an ECS storage model aligned with DOD/SoA. Without these two pieces, applications could not share a stable engine lifecycle and core gameplay data model.

**Decision**:
Adopt two foundational modules:

1. `Core::ECS::World`
- Archetype-based storage keyed by component bitmasks.
- Components restricted to trivially-copyable/destructible data to guarantee POD-like SoA moves.
- Entity IDs encoded with index + generation to reject stale handles safely.
- Query API (`ForEach`) iterates only matching archetypes with linear contiguous access.

2. `Core::Runtime::EngineRuntime`
- Unified lifecycle: `Initialize -> Tick/Run -> Shutdown`.
- Main-thread foreground executor + background thread pool configured automatically.
- Window event pump integrated into the runtime loop with callback hooks.
- Typed `Status` failure propagation and explicit stop requests.

Also introduced:
- GLFW backend detection in CMake (`FABRICA_ENABLE_GLFW_WINDOW_BACKEND`) with graceful fallback when unavailable.
- Optional sample app target (`fabrica_runtime_sample`) only when GLFW is available.

**Alternatives considered**:
1. Build runtime first without ECS.
- Rejected: would delay core data-layout decisions and create rework when systems are added.

2. Use object-oriented ECS with polymorphic component containers.
- Rejected: weaker cache locality and more indirection in hot loops.

3. Hard-depend on GLFW in all configurations.
- Rejected: hurts portability of CI/dev environments without GLFW.

**Consequences**:
- Engine now has a reusable entry point contract for host applications.
- Core data model is aligned with archetype SoA iteration from day one.
- Additional complexity introduced in entity migration and archetype management, compensated by tests and explicit API constraints.
