# Vulkan RHI Phase 14 validation — in progress

Date: 2026-09-30. Branch: `develop3_rendering`.

## Working-tree implementation

VMA owns buffer/image allocations behind the Vulkan backend boundary. Buffer
usage maps Vertex, Index, Uniform, Storage and transfer flags; host upload and
readback use CpuToGpu/GpuToCpu memory. GPU-only uploads use staging buffers.
Texture images and image views are separate objects, and resizing rebuilds
native views while preserving the RHI texture/view objects. Resource states
map to synchronization2 stages, access masks and layouts in one helper.
Transfer command lists currently submit synchronously and wait their fence.
Graphics commands, descriptors and pipelines remain later-phase work.

`BufferTextureCopy::aspect` selects colour/depth/stencil without leaking Vulkan
types. Automatic selects colour or depth. Copy bounds include mip, layer,
extent, row pitch and buffer range. Buffer copies support arbitrary byte
offsets/sizes and reject overlapping source/destination ranges.

## Local evidence

MSYS2 MinGW64 Release build passed. RTX 5070 Ti Vulkan resource diagnostic
passed with validation=1, errors=0 and live resource counts=0. Coverage includes:

- Vertex/Index/Uniform initial-state barriers and GPU upload restoring that state.
- A 127-byte GPU upload/copy/readback with an unaligned source offset.
- Storage read/write and transfer state transitions.
- RGBA8 upload/readback, upload-triggered resize and explicit resize with retained views.
- RG16Float transfer and separate D24S8 depth/stencil aspect transfers.
- Color attachment and depth attachment state transitions.
- Resource release and device/allocator teardown.

CTest passed 5/5 after the shared copy-contract change. The final subsequent
buffer-copy adjustment changed only Vulkan sources; the rebuilt resource
diagnostic passed again. `git diff --check` passed.

Build log: `C:/tmp/gp/p14-bytecopy-build.log`.

## Pending

- Final architecture audit must retain the extension-less final-shutdown
  synchronization limitation documented in the Phase 13 gate record.
- Commit only task-owned changes; preserve the pre-existing VCPKG_ROOT workflow edits.
- Validate the final implementation SHA in the relevant build/runtime CI jobs.
- Update Phase 14 completion conditions only after those gates pass.

Phase 14 is not yet marked complete. The overall goal remains Phase 26.
