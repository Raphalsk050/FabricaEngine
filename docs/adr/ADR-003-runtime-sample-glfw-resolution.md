## ADR-003: Runtime Sample GLFW Dependency Resolution
**Status**: Accepted

**Context**:
The first runtime sample depended on a preinstalled GLFW package. In environments without a configured `glfw3` package, CMake silently skipped the sample target, which made sample readiness ambiguous and slowed onboarding.

**Decision**:
Adopt a deterministic GLFW resolution order for the sample and window backend:

1. Try system package discovery via `find_package(glfw3 CONFIG)`.
2. If unavailable and explicitly enabled, fetch GLFW through CMake `FetchContent`.
3. If still unavailable, keep core build/test path valid and emit explicit status messages with next steps.

New CMake controls:
- `FABRICA_FETCH_GLFW` (default OFF): enables FetchContent fallback.
- `FABRICA_GLFW_FETCH_TAG` (default `3.4`): controls the fetched GLFW tag.

Also expose backend source telemetry (`FABRICA_GLFW_BACKEND_SOURCE`) to report whether the sample uses system package or fetched dependency.

**Alternatives considered**:
1. Keep silent skip behavior.
- Rejected: unclear diagnostics and poor developer UX.

2. Hard-fail configuration when GLFW is unavailable.
- Rejected: breaks CI and core-only workflows that do not require samples.

3. Vendor GLFW permanently in-repo.
- Rejected for now: increases repository weight and third-party maintenance overhead.

**Consequences**:
- First sample path is explicit and reproducible.
- Core architecture remains decoupled from mandatory third-party requirements.
- Teams can opt into automatic dependency fetch only when needed.
