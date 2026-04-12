# FabricaEngine Samples

This directory contains one focused sample per subsystem so new contributors can
learn the engine progressively.

## Build

```bash
cd engine
cmake -S . -B build -DFABRICA_BUILD_SAMPLES=ON -DFABRICA_BUILD_TESTS=OFF
cmake --build build --config Debug
```

## Sample Targets

- `fabrica_common_sample`: `Status`, `StatusOr`, `TypedId`, and boundary validation.
- `fabrica_memory_sample`: `LinearAllocator`, `StackAllocator`, and `PoolAllocator`.
- `fabrica_logging_sample`: asynchronous logger setup, channel filters, and flushing.
- `fabrica_jobs_sample`: foreground + thread-pool scheduling.
- `fabrica_async_sample`: `Future<T>` schedule/then/merge workflows.
- `fabrica_assets_sample`: `ResourceManager` async loading and in-flight deduplication.
- `fabrica_ecs_sample`: ECS registration, archetype migration, and runtime sealing.
- `fabrica_input_sample`: input binding and action phase transitions.
- `fabrica_window_sample`: `WindowSystem` integration with a scripted backend.
- `fabrica_runtime_headless_sample`: runtime loop callbacks without GLFW dependency.
- `fabrica_view_application_sample`: `BaseView` + `ViewApplication` lifecycle.
- `fabrica_pal_sample`: PAL threading and async filesystem adapters.
- `fabrica_runtime_sample`: full GLFW-backed runtime sample (only when GLFW is available).
- `fabrica_rhi_triangle_sample`: minimal swapchain + command-list triangle sample on top of the RHI (only when GLFW is available).

## Run

```bash
# Generic form
./build/samples/<target>

# Example
./build/samples/fabrica_ecs_sample
```

On multi-config generators (Visual Studio), binaries are under
`build/samples/Debug/` or similar configuration folders.