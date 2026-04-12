## ADR-006: Introduce a Renderer Module as an RHI-Only PBR Foundation
**Status**: Accepted

**Context**: The engine had a functional RHI, ECS, jobs, and runtime stack, but no renderer layer above the RHI. That left no place to express material models, camera exposure, lighting data, shader compilation, or frame orchestration without coupling high-level rendering policy directly to the Vulkan backend. The implementation also needed to follow the Filament PBR reference already stored in the repository root while preserving the project's abstraction boundary: all GPU work must flow through `IRHIContext`, `IRHICommandList`, and `RenderGraph`.

**Decision**: Introduce a new `engine/renderer` module as the first rendering layer above the RHI. The initial slice includes minimal math types, physically based camera settings, standard PBR material/light data, offline GLSL-to-SPIR-V compilation with `glslc`, and a small forward pipeline composed of two passes: an HDR PBR color pass and a tone-mapping pass. The renderer owns no backend-native types and interacts with the GPU exclusively through RHI handles, descriptor sets, pipelines, render passes, and the existing render graph.

**Alternatives considered**:
- Implement rendering logic directly inside the Vulkan backend for a faster first image.
- Introduce a third-party math library such as GLM before proving the renderer contract.
- Delay shader infrastructure until a larger asset pipeline or sample application was ready.

**Consequences**:
- Positive: Rendering policy now lives in a dedicated module and depends only on engine abstractions, which keeps backend portability intact.
- Positive: The codebase gains a validated vertical slice for Filament-inspired PBR, exposure, shader compilation, and post-process orchestration.
- Negative: The current renderer slice is intentionally minimal and does not yet cover IBL, shadows, clustered light culling, or runtime asset ingestion.
- Neutral: A small local math layer was introduced to unblock the renderer without committing the project to an external dependency yet.
