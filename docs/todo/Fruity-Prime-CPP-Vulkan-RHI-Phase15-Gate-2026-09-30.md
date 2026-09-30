# Vulkan RHI Phase 15 validation — in progress

Date: 2026-09-30. Branch: `develop3_rendering`.

## Implementation

BindingLayout maps uniform/storage buffers, sampled/storage images and separate
samplers to Vulkan descriptor declarations, including stages and array counts.
BindingSetEntry now has a backend-neutral arrayElement index. Native Vulkan
handles remain private to the backend. Set descriptions are immutable; every
materialization allocates and updates a fresh descriptor set rather than
overwriting a set already recorded for GPU use. Resource and layout ownership
remains with callers, which must keep referenced objects alive through use.

Each of two frame slots owns a completion fence and descriptor pools. BeginFrame
waits for that slot's prior GPU submission before resetting every pool. EndFrame
submits a fence after prior graphics-queue work. Exhausted pools grow with
additional pages; pages are retained and reused after the frame fence.

Validation rejects duplicate/missing/undeclared array entries, resource type or
device mismatches, incorrect usage, out-of-bounds buffer ranges and offsets
violating minUniformBufferOffsetAlignment/minStorageBufferOffsetAlignment.
Layout creation queries native descriptor layout support.

## Local evidence

MSYS2 MinGW64 Release build passed. CTest passed 5/5 after the shared binding
contract change. RTX 5070 Ti diagnostic `-vulkanresourcecheck -noupdate` passed
with validation=1, errors=0 and live resources=0. Binding coverage includes all
five descriptor types, a uniform array, invalid duplicate/range/type/alignment
rejection, fresh sets, overflow page creation, and eight frames of slot reuse.
Each frame also records vkCmdBindDescriptorSets and submits the command buffer;
its command pool and pipeline layout remain alive until the frame completes.

Logs: `C:/tmp/gp/p15-final-build.log`, `C:/tmp/gp/p15-final-runtime.log`.

The bind diagnostic does not execute a shader or draw geometry. Shader reads
and production pipeline binding are Phase 16 and subsequent rendering gates.
Exact-SHA desktop and Android CI remain required before checking Phase 15
complete. The overall objective remains Phase 26.
