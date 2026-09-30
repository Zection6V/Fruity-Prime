# Phase 12 — Vulkan foundation gate

Status: implementation, Windows Vulkan runtime, OpenGL parity and runtime
verified; exact-source remote CI remains pending. Do not advance to Phase 13 yet.

`-vulkancheck -noupdate` creates a hidden NoApi window, enumerates physical
GPUs, selects a Vulkan 1.3 device with dynamic rendering and synchronization2,
checks graphics/presentation queues, swapchain extension, RGBA8 sampled/color/
transfer and D32 depth formats, and device-local memory. It translates limits
and supported features to common RHI Capabilities. It enables validation when
available, attaches a debug messenger, names backend objects, allocates/names
a command buffer, waits idle and destroys device/instance before reporting.
Raw Vulkan objects and function pointers stay inside the backend implementation.

Vulkan entry points load through GLFW only on explicit initialization. The
ordinary OpenGL executable has no Vulkan loader DLL import. Optional development
support can be required with `FRUITY_REQUIRE_VULKAN=ON`; unavailable builds fail
the explicit diagnostic rather than supplying a dummy backend.

Initial Windows run (`C:/tmp/gp/p12-vulkan.log`, `.err`) exits zero:
RTX 5070 Ti, API 1.4, graphics/present family 0, Khronos validation enabled,
no validation errors, clean shutdown. Bandicam and OBS implicit layers each
warn about advertising an older API version; these warnings are retained.
Executable import inspection confirms no Vulkan DLL dependency.
The final command routing also passes without game assets or paths.txt in an
isolated runtime folder, exit zero (`C:/tmp/gp/p12-no-game.log`, `.err`).
MinGW Release builds and CTest passes 5/5; Phase 4/5/9/11 audits pass.
Exact implementation commit: `5403e1eabd94d1b16cdc936f43508c39ba3114f1`.
Its explicit Vulkan check exits zero with validation enabled
(`p12-head-vulkan.log`, `.err`). OpenGL shellshot exits zero with all 28 PNGs
(`p12-shell` under `C:/tmp/gp/`). Pointing VK_DRIVER_FILES at a nonexistent
driver makes the Vulkan diagnostic exit 1 with a clear unavailable-WSI reason;
under that same condition OpenGL still completes shellshot, exit zero, 28 PNGs
(`p12-missing-driver` and `p12-shell-no-vulkan-driver` logs/artifacts).

The first Golden attempt is retained as rejected provenance: invoking MSYS
bash without its login environment makes Python normalize path separators
differently (`out-p12verified`, `p12-validation.log`). This is not a pixel
PASS and the validator is unchanged. The replacement uses the same MSYS
login environment as Phase 3, a fresh build directory and new output
(`out-p12canonical`, `p12-canonical-validation.log`).
The replacement passes all seven exact decoded RGB comparisons, including
unchanged harness and all four runtime-input identities against Phase 3.
Executable SHA-256 is recorded in its `golden-parity-runtime.json`.

CI requires Vulkan headers/loader on Windows and Linux. Linux additionally
runs the diagnostic under Xvfb with Mesa and Khronos validation layers and
requires the validation-enabled PASS message.
Linux job `109745710099` in run `36670960931` succeeds. Its downloaded job
log (`C:/tmp/gp/p12-linux-ci.log`) confirms llvmpipe, API 1.4,
graphics/present family 0, validation=1 and clean-shutdown foundation PASS.
Windows/MSVC remains in progress; the whole run is not yet claimed green.

References: [GLFW Vulkan integration](https://www.glfw.org/docs/latest/vulkan_guide.html),
[Vulkan 1.3 feature contract](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceVulkan13Features.html),
[portability enumeration](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_portability_enumeration.html).

Scope is native C++; C# sources and existing TRANSFER LOCK light geometry are
unchanged. Phase 12 does not render a game scene or present a Vulkan image.
