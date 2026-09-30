# Native OpenGL dependency boundaries (Phase 11)

`tools/check-phase11-gl-boundaries.py` scans every native `.cpp` and `.hpp`,
including qualified references inside macros. Comments and C++ literals are
masked while preserving line numbers. A newly introduced owner fails the gate
unless explicitly classified. `tools/test-phase11-gl-boundaries.py` verifies
the masking and rejection of unknown owners. Both run in `build_cpp` CI.

| Class | Owners | Reason |
|---|---|---|
| A: OpenGL backend | `NativeRuntime/Rhi/OpenGL/` | Device, resource, pipeline, geometry, shader, readback and launcher-background implementation; frontend rendering uses semantic operations. |
| A: GL API bindings | `NativeRuntime/OpenTK/GL.cpp`, `GL.hpp`, `GLAndroid.cpp` | Desktop/ES binding layer used by the backend. |
| B: Skia interoperability | `NativeRuntime/Skia/SkiaGpu.cpp`, `Mods/Render/UiOverlay.cpp` | Ganesh GPU surfaces, texture adoption and composition of the launcher into the game window. This is an explicit backend-specific interop boundary. |
| C: diagnostics | `Mods/Diagnostics/LauncherWindowCheck.cpp`, `ThumbnailWindowCheck.cpp`, `GpuLifetimeCheck.cpp` | GL state assertions and context/device lifetime measurement. |
| C: diagnostics | `Mods/MapGen/AltFormProbe.cpp`, `Mods/Network/MapAudit.cpp`, `NetCheckClient.cpp`, `WeaponDps.cpp` | Test-window error/state observations, outside production draw submission. |
| C: diagnostics | `Mods/ScreenCapture.cpp`, `Mods/ThumbnailCapture.cpp`, `ThumbnailCapture.hpp` | Screenshot buffer selection/readback and framebuffer diagnostic status. |
| D: invalid | Any other file | No unclassified qualified GL dependency is allowed. |

After the Phase 11 migration, the qualified-call counts are A=601, B=250,
C=122, D=0. Counts are informational; the executable gate always rescans the
checkout. Frontend `LauncherPhoto` and `LauncherNoise` retain semantic facades;
their texture/program ownership and GL operations live in the OpenGL backend.
`Export/Images` accepts an RHI command list for tightly packed RGB readback.

The Phase 4, 5 and 9 audits remain separate gates for immediate submission,
display lists, shader built-ins and direct scene-renderer GL calls. Moving an
owner into class A does not waive those contracts.

The shellshot sequence checks actual minimized/normal window states and captures
the restored launcher and match. It also writes real Screenshot/Record PNGs from
an odd-width RGB color-band pattern and checks every decoded pixel against the
known colors, orientation and dimensions. These diagnostics run only when
explicitly requested by `-shellshot`.

Runtime evidence and exact source revisions are recorded separately in
`docs/todo/Fruity-Prime-CPP-OpenGL-RHI-Phase10-11-Gate-2026-09-30.md`.
