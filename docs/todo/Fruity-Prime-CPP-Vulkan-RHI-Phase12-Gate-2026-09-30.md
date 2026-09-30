# Phase 12 — Vulkan foundation gate

Status: implementation and initial Windows runtime verified; final-source
OpenGL parity and remote CI remain pending. Do not advance to Phase 13 yet.

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

CI requires Vulkan headers/loader on Windows and Linux. Linux additionally
runs the diagnostic under Xvfb with Mesa and Khronos validation layers and
requires the validation-enabled PASS message.

References: [GLFW Vulkan integration](https://www.glfw.org/docs/latest/vulkan_guide.html),
[Vulkan 1.3 feature contract](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceVulkan13Features.html),
[portability enumeration](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_portability_enumeration.html).

Scope is native C++; C# sources and existing TRANSFER LOCK light geometry are
unchanged. Phase 12 does not render a game scene or present a Vulkan image.
