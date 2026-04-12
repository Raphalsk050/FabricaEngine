# Renderer Industrial Task Board

This task board is the execution contract for the FabricaEngine renderer program.
It follows the architecture workflow from `$fabrica-cpp-performance-architecture`
and the material / lighting / imaging equations documented in `filament_docs.md`.

## Non-Negotiable Rules

- All GPU work must flow through `IRHIContext`, `IRHICommandList`, and `RenderGraph`.
- Material, lighting, camera, scene submission, and post-process must remain separate modules.
- No backend-native types outside backend implementation files.
- No invented BRDF or light equations. Follow `filament_docs.md` listings and equations.
- Final hot path target: zero avoidable allocations during frame submission and execution.

## Stage 0 - Architecture Freeze and Gates

Status: In Progress

Tasks:
- [x] Introduce `engine/renderer` as a standalone target above the RHI.
- [x] Keep camera, material, light, and renderer entrypoint data separate.
- [ ] Split renderer runtime into reusable modules: shader, frame, passes, scene, lighting.
- [ ] Define ownership and lifecycle boundaries for per-frame resources, pipelines, and materials.
- [ ] Add module-level build structure so future features do not grow `Renderer.cpp` into a god unit.

Acceptance:
- No cyclic dependencies.
- Renderer public API remains minimal.
- No Vulkan headers leak into renderer headers.

## Stage 1 - Visual Harness

Status: Completed

Tasks:
- [x] Add `fabrica_renderer_sample` with a native window and Vulkan RHI context.
- [x] Generate procedural geometry in code for initial validation.
- [x] Render one visible PBR object with one directional light.
- [x] Keep all rendering routed through `Renderer`.

Acceptance:
- Window opens.
- Render graph executes HDR + tone mapping passes.
- Manual smoke test shows visible geometry on screen.

## Stage 2 - Standard Material Model

Status: Pending

Filament references:
- Section 4.4.1 Listing 2
- Section 4.4.2 Listing 4
- Section 4.4.3 Listing 6
- Section 4.5 Listing 7
- Section 4.6 Listing 9
- Section 4.7.5 Equation 30 / Listing 10
- Section 4.8.8 Listings 11 and 12 / Equation 37

Tasks:
- [ ] Move BRDF helpers into a dedicated material shading module.
- [ ] Implement standard parameter remapping exactly as documented.
- [ ] Add energy compensation path once DFG integration is available.
- [ ] Wire `ShaderKey` into real shader permutation selection.
- [ ] Add shader validation tests for metallic, roughness, and reflectance sweeps.

Acceptance:
- White furnace behavior is validated.
- Roughness clamp is fp16-safe.
- Metallic and dielectric paths match Filament expectations.

## Stage 3 - Real Material System

Status: Pending

Tasks:
- [ ] Add `MaterialInstance` with descriptor ownership.
- [ ] Bind base color, normal, metallic-roughness, AO, and emissive textures.
- [ ] Add pipeline cache keyed by `ShaderKey` + render state.
- [ ] Keep unused textures out of material bindings and permutations.

Acceptance:
- All material texture handles are exercised by rendering code.
- No declared material feature is dead or ignored.

## Stage 4 - Direct Lighting with Physical Units

Status: Pending

Filament references:
- Section 5.2.2 Equation 56 / Listing 20
- Section 5.2.3 Equations 59, 64, 65, 66 / Listing 21
- Section 5.2.7 Listings 24 and 25

Tasks:
- [ ] Implement directional, point, and spot light evaluation modules.
- [ ] Implement square falloff and spot angle attenuation from Filament.
- [ ] Add pre-exposed punctual light path.
- [ ] Add Kelvin temperature conversion.

Acceptance:
- Inverse-square falloff matches expected numeric behavior.
- Spot light cone attenuation is correct.
- Directional illuminance path is physically scaled.

## Stage 5 - Correct IBL

Status: Pending

Filament references:
- Section 5.3
- Section 5.3.11.8
- Section 5.3.11.9
- Listing 26

Tasks:
- [ ] Generate DFG LUT.
- [ ] Generate or load prefiltered environment cubemap.
- [ ] Add irradiance SH evaluation.
- [ ] Add diffuse occlusion, specular occlusion, and horizon occlusion.

Acceptance:
- Roughness controls reflection LOD correctly.
- Indirect diffuse and specular match Filament split-sum behavior.

## Stage 6 - Imaging Pipeline

Status: Pending

Filament references:
- Section 8.1
- Equations 100, 102, 115

Tasks:
- [ ] Keep physically based camera settings as reusable component data.
- [ ] Add auto-exposure.
- [ ] Add bloom on HDR scene color.
- [ ] Keep ACES tone mapping as a dedicated post-process pass.

Acceptance:
- Exposure responds correctly to aperture, shutter speed, and ISO.
- Adaptation is smooth and deterministic.

## Stage 7 - Shadows

Status: Pending

Tasks:
- [ ] Add modular directional shadow pass.
- [ ] Implement cascaded shadow maps and atlas management.
- [ ] Implement PCF and bias controls.

Acceptance:
- Stable cascades.
- No severe acne or peter-panning.

## Stage 8 - Scene Submission and ECS Integration

Status: Pending

Tasks:
- [ ] Add `RenderableSystem` over ECS.
- [ ] Generate draw packets from Transform + Mesh + Material.
- [ ] Sort opaque and transparent draws separately.
- [ ] Keep ECS iteration and render packet building cache-friendly.

Acceptance:
- Passes consume packets, not ECS directly.
- No ECS logic leaks into shader/pipeline/pass modules.

## Stage 9 - Clustered Forward

Status: Pending

Filament references:
- Section 8.4.12
- Section 8.4.13
- Equations 117 and 118

Tasks:
- [ ] Add froxel grid configuration and depth slicing.
- [ ] Add light assignment buffers.
- [ ] Integrate froxel lookup in lighting shader path.

Acceptance:
- Many-light scenes scale without linear per-pixel light loops.

## Stage 10 - Industrial Hardening

Status: Pending

Tasks:
- [ ] Add transient resource pool.
- [ ] Add per-frame descriptor allocator.
- [ ] Add upload ring / staging strategy.
- [ ] Remove steady-state frame allocations.
- [ ] Add regression, numerical, and smoke coverage.

Acceptance:
- Zero warnings.
- Green tests.
- No avoidable frame allocations.
- Frame time and memory behavior are stable.

