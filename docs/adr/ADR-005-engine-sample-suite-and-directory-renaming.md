## ADR-005: Engine Sample Suite and Directory Renaming
**Status**: Accepted

**Context**: The repository had a single runtime sample under `engine/apps`, which made onboarding hard for new contributors who need focused examples for each subsystem (common, memory, logging, jobs, async, assets, ecs, input, window, runtime, app, and PAL). The directory name `apps` also did not match current intent: these binaries are educational samples, not production application shells.

**Decision**: Rename `engine/apps` to `engine/samples` and introduce one sample target per major subsystem, each in its own subdirectory with documented C++ entry points. Keep the GLFW runtime sample (`fabrica_runtime_sample`) conditional on backend availability, and add headless samples for runtime/view flows so all core concepts remain buildable without GLFW.

**Alternatives considered**:
- Keep a single monolithic sample and expand it with feature toggles.
- Keep `engine/apps` and add markdown-only walkthroughs without executable examples.
- Publish samples outside the engine tree in a separate repository.

**Consequences**:
- Positive: Onboarding improves through isolated, executable examples aligned with subsystem boundaries.
- Positive: CI/local environments without GLFW can still compile and run most samples.
- Negative: More sample targets increase maintenance surface and require periodic updates when APIs evolve.
- Neutral: Existing scripts/documentation referencing `engine/apps` had to be updated to `engine/samples`.
