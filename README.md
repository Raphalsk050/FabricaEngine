# FabricaEngine

Initial engine architecture focused on DOD/ECS, including:
- Core (`common`, `memory`, `logging`, `jobs`, `async`, `window`, `assets`, `ecs`, `runtime`)
- PAL (`threading`, `file_system`)

## Build + tests

```bash
cd engine
cmake -S . -B build -DFABRICA_BUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Runtime sample (GLFW)

The runtime sample uses the GLFW backend and can be resolved in two ways:
- system package (`find_package(glfw3 CONFIG)`)
- CMake download (`FetchContent`) with `-DFABRICA_FETCH_GLFW=ON`

```bash
cd engine
cmake -S . -B build \
  -DFABRICA_BUILD_SAMPLES=ON \
  -DFABRICA_ENABLE_GLFW_WINDOW_BACKEND=ON \
  -DFABRICA_FETCH_GLFW=ON
cmake --build build --config Debug --target fabrica_runtime_sample
```

If the GLFW backend is unavailable, CMake emits explicit guidance on how to enable the sample target.
