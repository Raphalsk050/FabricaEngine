# FabricaEngine

Base arquitetural inicial da engine com foco em DOD/ECS, incluindo:
- Core (`common`, `memory`, `logging`, `jobs`, `async`, `window`, `assets`)
- PAL (`threading`, `file_system`)

## Build

```bash
cd engine
cmake -S . -B build -DFABRICA_BUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```
