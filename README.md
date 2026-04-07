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

## Build scripts

Windows (PowerShell):
```powershell
.\scripts\build_windows.ps1 -Config Debug
.\scripts\build_windows.ps1 -Config Debug -BuildSamples -FetchGlfw -Target fabrica_runtime_sample
```

Windows (batch wrapper):
```bat
scripts\build_windows.bat -Config Debug
```

Linux:
```bash
chmod +x scripts/build_linux.sh
./scripts/build_linux.sh --config Debug
./scripts/build_linux.sh --config Debug --build-samples --fetch-glfw --target fabrica_runtime_sample
```

## CLion project generation (Windows)

```powershell
.\scripts\generate_clion_windows.ps1
.\scripts\generate_clion_windows.ps1 -Config Debug -BuildDir engine/apps/cmake-build-debug -Build
```

This script configures with Ninja and exports `compile_commands.json` into paths CLion can consume. Use `-Build` to compile (default target: `fabrica_runtime_sample`). For core-only indexing: `-NoSamples -WithTests`.
