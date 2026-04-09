# FabricaEngine

Initial engine architecture focused on DOD/ECS, including:
- Core (`common`, `memory`, `logging`, `jobs`, `async`, `window`, `assets`, `ecs`, `runtime`, `app`)
- PAL (`threading`, `file_system`)
- Samples (`engine/samples`, one target per subsystem)

## Build + tests

```bash
cd engine
cmake -S . -B build -DFABRICA_BUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Samples

The sample suite lives in `engine/samples` and covers every engine subsystem.

```bash
cd engine
cmake -S . -B build \
  -DFABRICA_BUILD_SAMPLES=ON \
  -DFABRICA_ENABLE_GLFW_WINDOW_BACKEND=ON \
  -DFABRICA_FETCH_GLFW=ON
cmake --build build --config Debug
```

Core sample targets:
- `fabrica_common_sample`
- `fabrica_memory_sample`
- `fabrica_logging_sample`
- `fabrica_jobs_sample`
- `fabrica_async_sample`
- `fabrica_assets_sample`
- `fabrica_ecs_sample`
- `fabrica_input_sample`
- `fabrica_window_sample`
- `fabrica_runtime_headless_sample`
- `fabrica_view_application_sample`
- `fabrica_pal_sample`

GLFW-dependent sample target:
- `fabrica_runtime_sample`

If the GLFW backend is unavailable, CMake emits explicit guidance on enabling the target.

## View entry point (recommended)

Use `core/app/fabrica.h` to build applications with a lifecycle-oriented view API instead of manual runtime callback wiring.

```cpp
#include "core/app/fabrica.h"

struct PlayerComponent {
  float speed = 0.0f;
};

class MyView final : public Fabrica::Engine::BaseView {
 public:
  Status Awake() override {
    RegisterComponent<PlayerComponent>();
    auto player = CreateEntity();
    player.AddComponent<PlayerComponent>(6.0f);
    BindInputAction("quit", 256);
    BindInputAction("quit", 81);
    return Status::Ok();
  }

  void OnInputAction(const InputActionEvent& event) override {
    if (event.action == "quit" && event.phase == InputActionPhase::kStarted) {
      RequestStop();
    }
  }
};

FABRICA_DEFINE_VIEW_MAIN(MyView)
```

This flow also avoids explicit logger bootstrap calls in app code (`Logger::Instance().Initialize()`), because runtime now manages logger startup by default.

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
.\scripts\generate_clion_windows.ps1 -Config Debug -BuildDir engine/samples/cmake-build-debug -Build
```

This script configures with Ninja and exports `compile_commands.json` into paths CLion can consume. Use `-Build` to compile (default target: `fabrica_runtime_sample`). For core-only indexing: `-NoSamples -WithTests`.
