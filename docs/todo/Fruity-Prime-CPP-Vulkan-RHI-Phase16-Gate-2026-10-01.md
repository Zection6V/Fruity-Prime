# Vulkan RHI Phase 16 validation — in progress

Phase 15 is complete. Phase 16 remains incomplete until its final exact-source
Desktop CI finishes successfully.

Final audited implementation: `cebd532c2d75714d67139a640e96a8ab81ec7899`.
User-selected primary CI is again exact-SHA Desktop `36746271237`, Windows
job `109993145867`, following the user's corrected link. It remains in the
dependency installation stage. The previously selected integration run was
[build_cpp 36743419689](https://github.com/Zection6V/Fruity-Prime/actions/runs/36743419689).
Its Windows job `109983770120` is installing dependencies; Linux, macOS and
Android builds succeeded. Linux runtime checks pass with validation=1, errors=0
and live=0. Its checkout is PR merge `297d160` incorporating the audited SHA,
so these results describe merge integration rather than an exact standalone SHA.
Evidence: `C:/tmp/gp/p16-user-selected-linux-ci.log`. No Phase 11 audit was rerun.

Explicit checkout_ref was used for both final runs:

- [Desktop 36746271237](https://github.com/Zection6V/Fruity-Prime/actions/runs/36746271237):
  macOS/Clang PASS; Linux/GCC PASS; Windows/MSVC is installing dependencies.
- [Android 36744453750](https://github.com/Zection6V/Fruity-Prime/actions/runs/36744453750):
  4/4 PASS, including both ABIs and APK packaging.

Original implementation `9d612df1a6c94bbdee55bf1998d32814aa7728e5` runs:
[Desktop 36742852274](https://github.com/Zection6V/Fruity-Prime/actions/runs/36742852274),
[Android 36742858600](https://github.com/Zection6V/Fruity-Prime/actions/runs/36742858600).
Original Android is 4/4 PASS. The original desktop run remains independently
active; the final run above is the completion gate.

Current local evidence: native build, CTest 5/5, eight reproducible SPIR-V stages,
shader/pipeline/binding diagnostics and normal/fallback presentation checks PASS.
Validation errors and post-diagnostic live shaders/programs/resources are zero.
Invalid pipeline descriptions and incompatible descriptor layouts are rejected.
No shader draws have executed; actual drawing belongs to Phase 17 and scene
image parity remains a later gate.

Final Linux job `109993146058` confirms checkout of the full audited SHA,
compiles all eight SPIR-V stages and passes foundation, four graphics pipelines,
eight shader modules, descriptor allocation/GPU bind submission and resource
checks (`live=0`, `validation=1`, `errors=0`). Normal and fallback presentation
also pass with clean shutdown and zero errors. Downloaded evidence:
`C:/tmp/gp/p16-final-linux-ci.log`. This is runtime evidence on Linux; the
remaining Windows/MSVC job is still required before Phase 16 completion.

The following implementation notes retain intermediate failures and repairs as
history. Their statements that a build/runtime revision is pending describe
that intermediate point, not the current verification state above.

## Shader contract audit

The existing `VulkanShaderInterface.hpp` material block used implicit std140
offsets after its last vec3, while the C++ mirror explicitly pads each vec3
to sixteen bytes. The GLSL alpha/mode/light members now explicitly use offsets
64/68/72, matching the C++ mirror's layout.

An isolated vertex shader containing the actual ConstantBlocksGlsl was compiled
with Vulkan SDK 1.4.357.0 glslc, `--target-env=vulkan1.3 -O0`. spirv-dis confirms
MaterialBlock offsets 0,16,32,48,64,68,72; spirv-val passed for Vulkan 1.3.
Evidence: `C:/tmp/gp/p16-interface.vert`, `p16-interface.spv` and
`p16-interface.spvasm` in the same directory.

Further contract work is required: the draft uses a combined sampler although
Phase 15 implements separate image/sampler descriptors, and GraphicsPipelineDesc
currently names one layout while the draft declares four descriptor sets.
The runtime constants required by the frozen OpenGL shaders also exceed the
draft blocks (texgen, palette overrides, flat color and alpha test among them).
These must be reconciled in the production shader contract before claiming parity.

## Remaining gates

The production generator `tools/generate-vulkan-scene-shaders.py` now extracts
the main/composite/cel/shift desktop shader bodies, preserving their algorithms,
and emits eight Vulkan GLSL stages plus a deterministic binding manifest.
Each program uses one set with a std140 uniform block and separate image/sampler
descriptors, matching the current RHI layout shape. Uniform offsets and array
strides are emitted explicitly in the manifest. All eight generated stages
compiled using glslc for Vulkan 1.3 and passed spirv-val locally.

This compilation is not rendering parity evidence: Vulkan clip depth,
framebuffer/texture Y conventions and gl_FragCoord-dependent effects still need
explicit adaptation and image verification. The manifest must be integrated
with C++ packing/layout creation and CMake before production use.

- Reproducible CMake SPIR-V generation using the same explicit compiler in CI.
- Shader modules and GraphicsPipelineDesc to Vulkan pipeline/layout mapping.
- Pipeline caching and dynamic rendering attachment format validation.
- Production binding layout agreement and validation-clean pipeline execution.
- Document every required shader semantic and its OpenGL correspondence.
- Local and exact-SHA CI verification.

## CMake integration evidence

`cmake/FruityVulkanShaders.cmake` selects glslc explicitly, requires Python3 for
source generation, and creates the `fruity_vulkan_shaders` dependency of the
native library whenever the Vulkan backend is enabled. Eight SPIR-V outputs
use `--target-env=vulkan1.3 -O0`; source and generator changes invalidate outputs.
The local CMake target passed. Deleting all eight SPIR-V outputs and rebuilding
produced byte-identical binaries (8/8). Log: `C:/tmp/gp/p16-cmake-shaders.log`.
`tools/embed-vulkan-scene-shaders.py` now emits a generated C++ header containing
all eight SPIR-V word arrays and uniform/texture binding metadata. CMake owns
the embedding dependency and exposes the header privately to the native library;
runtime shader files are unnecessary. The generated header passed standalone
C++20 syntax compilation with magic, alignment and texture-count assertions.
Log: `C:/tmp/gp/p16-embed-build.log`.
CI compiler installation and backend module loading remain outstanding.

## Shader module implementation — validation pending

Vulkan CreateShader now creates a native module, retains its immutable RHI
description and destroys the module with its held device state. Input checks
cover supported single stages, SPIR-V header/word length, instruction bounds,
null-terminated entry-point names and matching execution model/name.
The device loader now includes create/destroy shader module functions.
Native build is running (`C:/tmp/gp/p16-module-build.log`); module runtime
creation and release diagnostics have not yet been executed.

The resource diagnostic now includes creation/release of all eight embedded
modules, live shader count checks, and rejection of missing entry points,
incorrect stages and truncated code. Shader statistics include native modules.
Build session remains active; these runtime checks are implemented but unrun.
Because diagnostic sources changed during the ongoing full rebuild, a final
incremental build must complete before runtime validation of this revision.

The final incremental build passed. Runtime creation/release diagnostics passed
for all eight modules, but the resource gate failed validation: glslc's default
SPIR-V 1.6 output used DemoteToHelperInvocation, which the device did not enable.
Compilation now explicitly targets SPIR-V 1.5 within Vulkan 1.3, avoiding that
additional feature requirement. The repaired build/runtime remain pending.
CI dependency installation now includes glslc (Linux) and shaderc (Windows);
the compiler search includes the Windows vcpkg tools directory. CI is not run yet.

The SPIR-V 1.5 repair built successfully and resource diagnostics passed on
RTX 5070 Ti: eight module create/release checks, binding checks and resource
checks, validation=1, errors=0, live=0. Evidence:
`C:/tmp/gp/p16-spv15-build.log`, `C:/tmp/gp/p16-spv15-runtime.log`.

## Pipeline implementation — in progress

CreateGraphicsPipeline now maps native shaders/layout, vertex input, topology,
rasterization, depth/stencil, blending, sample count and dynamic rendering formats.
Viewport and scissor are dynamic. Input checks cover device/stage consistency,
attachment count, duplicate vertex declarations and required rasterization features.
The initial implementation is not yet runtime validated or cached. Further
validation of enum values, device limits, attachment format capabilities, shader
layout compatibility and non-unit line widths remains necessary before completion.
Build log: `C:/tmp/gp/p16-pipeline-build.log`.

All four production programs now have pipeline creation/release diagnostics
using layouts derived from embedded binding metadata. The build passed, but
runtime validation found unused unmatched legacy `color` fragment inputs in
composite/shift. The generator now removes varying declarations whose symbols
are never used, preserving shader calculations. Repair build/runtime pending.
Initial evidence: `C:/tmp/gp/p16-pipeline-runtime.log`.

The varying repair built and passed runtime validation: main/composite/cel/shift
pipeline creation, module lifecycle, bindings and resources all PASS with
validation=1, errors=0 and live=0. Logs: `C:/tmp/gp/p16-varying-build.log`,
`C:/tmp/gp/p16-varying-runtime.log`. Pipeline drawing/cache and image parity
remain unverified; creation-only diagnostics do not prove those gates.

Pipeline validation now covers physical device vertex declaration limits,
vertex/color/depth format capabilities, sample counts, state enums, stencil
format requirements and independent blend availability. Supported wide lines
and independent blend are enabled at device creation. Non-unit widths are
accepted only within device limits with wideLines support. The first limits
build passed; the final depth-format check rebuild is pending.

The final limits build and runtime resource gate passed. Pipeline caching now
hashes every pipeline state field plus shader bytecode/entry points and copied
binding declarations; full key equality resolves collisions. Shader/layout
addresses are excluded, so equivalent replacement resources reuse native
pipelines without retaining dangling references in cache keys. Returned RHI
wrappers preserve the current caller's description. Cached native objects live
until device teardown after WaitIdle; outstanding wrappers hold shared ownership.
Four-program diagnostics verify identical and content-equivalent descriptions
reuse native handles, while changed culling creates a distinct pipeline.
Cache build/runtime passed with validation=1, errors=0 and live resources=0.
Evidence: `C:/tmp/gp/p16-cache-diagnostic-build.log`, `C:/tmp/gp/p16-cache-runtime.log`.

CommandList now binds graphics pipelines and compatible set-0 binding sets,
rejecting device/layout mismatches. Diagnostics submit pipeline binds for all
four programs. Vertex generation remaps GL clip Z into Vulkan clip Z; shader
semantics and the pending coordinate/image gates are documented in
`docs/app_design/Fruity-Prime-CPP-Vulkan-Shader-Semantics.md`.
The clip/bind revision built and resource diagnostics passed with validation=1,
errors=0, live=0 (`C:/tmp/gp/p16-clip-build.log`, `p16-clip-runtime.log`).
No shader draws have been executed yet; Phase 17 introduces actual triangle drawing.

Production SetBindingSet submission now passes for all four programs with real
uniform/image/sampler descriptors derived from their manifests; incompatible
layouts are rejected. Evidence: `C:/tmp/gp/p16-binding-submit-runtime.log`.
Pipeline cache retention is now visible as Programs in resource statistics.
The diagnostic clears its cache only after WaitIdle and verifies zero native
programs; the final resource leak check includes shaders/programs as well.
This accounting revision is building; runtime verification remains pending.

Accounting build/runtime passed (`C:/tmp/gp/p16-program-count-runtime.log`).
Further audit added rejection diagnostics for invalid sample count, missing
vertex bindings, attachment count mismatch, null layout, non-finite depth bias
and color formats used as depth. The negative-case build/runtime passed with
validation=1, errors=0 and live=0 (`C:/tmp/gp/p16-negative-pipeline-build.log`,
`p16-negative-pipeline-runtime.log`). These follow-up changes are not yet committed
and require exact-SHA CI before Phase 16 completion.

An independent output-directory regeneration using the same glslc produced
byte-identical eight SPIR-V binaries and the embedded C++ header compared with
the CMake outputs; spirv-val passed 8/8. Evidence directory:
`C:/tmp/gp/p16-repro-independent`. This proves local path-independent
reproducibility with that compiler, not cross-version compiler equivalence.
