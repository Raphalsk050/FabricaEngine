## ADR-007: Stabilize the Renderer Frame Path Around Per-Frame Resources and Batched Queue Submission
**Status**: Accepted

**Context**: The first executable renderer sample exposed a correctness gap between the renderer, the render graph, and the Vulkan backend. The renderer updated a single uniform buffer and a single tone-mapping descriptor set across frames in flight, while the Vulkan backend assumed only one submission per frame and the render graph recorded multiple passes into the same per-queue command buffer. Under validation this produced fence reuse errors, semaphore misuse, command buffer re-recording while pending, and descriptor writes against in-flight GPU work.

**Decision**: Stabilize the frame path in three coordinated changes. First, make renderer-owned frame data explicitly per-frame-slot by allocating one frame uniform buffer and one tone-mapping descriptor set per `kFramesInFlight` slot. Second, batch consecutive render-graph passes that target the same queue into a single command-list recording and submission. Third, change Vulkan frame submission so `SubmitCommandLists()` chains submissions through intermediate semaphores and reserves `frame_fence` plus `render_finished` signaling for the final frame submit performed by `Present()`.

**Alternatives considered**:
- Keep single-buffer and single-descriptor resources and add coarse CPU waits before every update.
- Rework the Vulkan backend immediately to allocate multiple command buffers per queue per frame.
- Skip the render graph for the sample and record both passes manually inside the renderer.

**Consequences**:
- Positive: The renderer sample now runs under Vulkan validation without synchronization or descriptor lifetime errors.
- Positive: Frame resource ownership is clearer and now matches the engine's frames-in-flight contract.
- Positive: The render graph no longer re-records a pending command buffer for consecutive graphics passes.
- Negative: Queue interleaving in the render graph is still not fully supported because the RHI currently exposes only one command list per queue per frame. The implementation now detects that case and aborts execution instead of submitting invalid Vulkan work.
- Neutral: A future industrial pass should still add true per-frame descriptor allocation, transient resource pooling, and multiple command buffers per queue to remove the current remaining structural limits.
