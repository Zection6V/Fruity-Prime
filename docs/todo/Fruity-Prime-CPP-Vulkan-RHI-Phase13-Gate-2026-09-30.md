# Vulkan RHI Phase 13 validation

Date: 2026-09-30 (Asia/Tokyo). Branch: `develop3_rendering`.

## Result

Phase 13 is complete at implementation commit
`8c6f2d2a044544c5975515fa838730271a02f003` (`Add Vulkan swapchain and presentation`).
The gate is intentionally clear-only; Vulkan game rendering and image parity
belong to later phases.

The desktop backend creates a `GLFW_NO_API` window surface, chooses a supported
surface format, present mode, extent, and image count, wraps swapchain-owned
images without destroying them, and owns their image views. It uses FIFO as the
fallback present mode; Mailbox and Immediate are selected when requested and
supported. Two frame slots own command buffers, acquire semaphores, and frame
fences. Each swapchain image owns its render-finished semaphore and, when
`VK_EXT_swapchain_maintenance1` is available, a present-completion fence.
Acquire/present handle out-of-date and suboptimal results, resize, and zero-sized
minimized windows. The clear path records synchronization2 transitions around
dynamic rendering.

## Local validation

MSYS2 MinGW64 Release build succeeded:

```text
cmake --build tools/build/out/msys2-mingw64-Release --config Release --target fruity_prime --parallel 8
ctest --test-dir tools/build/out/msys2-mingw64-Release -C Release --output-on-failure
```

CTest passed 5/5. On Windows 11 with an NVIDIA GeForce RTX 5070 Ti (Vulkan API
1.4), `FruityPrime.exe -vulkanpresentcheck -noupdate` passed clear presentation,
resize, fullscreen/windowed transitions, minimize/restore, FIFO/Mailbox changes,
and shutdown with validation enabled and zero errors. The device reported
`swapchainMaintenance1=1`, so this run exercised the present-fence cleanup path.
The validation layer also reported API-version warnings from the Bandicam and
OBS implicit layers (1.2 versus the application's 1.3); these were warnings,
not validation errors. `-vulkancheck -noupdate` passed the foundation control,
and `-thumbnailwindowcheck -noupdate` passed the OpenGL control.

Logs: `C:/tmp/gp/p13-build.log`, `C:/tmp/gp/p13-foundation.log`,
`C:/tmp/gp/p13-present.log`, and `C:/tmp/gp/p13-opengl.log`.

## Exact-SHA CI

[CI run 36684051768](https://github.com/Zection6V/Fruity-Prime/actions/runs/36684051768)
completed successfully on exact implementation SHA
`8c6f2d2a044544c5975515fa838730271a02f003`: 11/11 jobs passed. This includes
Windows/MSVC, Linux/GCC, macOS/Clang, Android build contract, Android NDK
arm64-v8a and x86_64, Android APK packaging, and the Phase 4/5/9/11 static
audits. Linux/GCC passed both the Vulkan foundation and the Xvfb/Openbox Vulkan
presentation runtime gates.

## Scope and lifecycle note

The presentation diagnostic submits several clear colors but does not read back
pixels or draw the production game renderer. This phase proves swapchain
creation, presentation, window transitions, validation-clean operation on the
tested devices, and orderly shutdown; it does not claim game-scene parity.

The local device supports `VK_EXT_swapchain_maintenance1`, whose present fence
was used to prove present completion before destroying swapchain resources. On
devices without that extension, the implementation uses per-image
`renderFinished` semaphores and waits for device work at shutdown; that
extension-less shutdown path was not exercised on hardware in this gate. The
[Khronos Vulkan Guide](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html)
documents image reacquisition as the portable synchronization point for reusing
present wait semaphores, and notes that ordinary queue/device idle waits alone
do not provide a specification-level proof that a pending presentation has
released swapchain resources. The
[Khronos swapchain recreation sample](https://docs.vulkan.org/samples/latest/samples/api/swapchain_recreation/README.html)
describes the corresponding deferred-retirement workaround before
`VK_EXT_swapchain_maintenance1`.
