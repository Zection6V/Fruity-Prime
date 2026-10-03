# Native Qt Quick migration gate — 2026-10-03

The C++ desktop and Android launchers now use Qt Quick. The C# projects and
`namespace MphRead` are unchanged. Native Avalonia, Skia, the old launcher
controls, CPU Android UI compositor and the compatibility check stubs were
removed. Shared asset loading, country lookup and shell interfaces live outside
the removed toolkit directories.

## Build and deployment

- Qt 6.11.2 kits: Windows `msvc2022_64`, Android `android_arm64_v8a` and
  `android_x86_64`; Linux `gcc_64` and macOS `clang_64` are installed by CI.
- `tools/qt/install-qt.py` reads official Qt repository metadata, verifies each
  archive checksum and extracts the selected kit. This supports the nested
  repository layout that the installed aqtinstall 3.3 could not read.
- Desktop CMake requires Qt Quick, Quick Shapes and Quick Effects. MSVC and
  MinGW require matching Qt kits. `fruity_package` deploys Qt libraries, QML
  imports, plugins and application dependencies into `dist`.
- Android embeds the QML resources in each application library. QtQuickView
  owns the launcher view, input and lifecycle; the native match owns its game
  surface. Both ABI deployments are merged before Gradle packages the APK.
  Qt uses a TextureView container so transparent menus compose above the
  game's SurfaceView instead of competing with its separate surface layer.
- Native Android requires API 28, targets API 35 and retains the shared C# API
  26 contract. The package copies of the application libraries are stripped;
  the native build outputs retain their diagnostic symbols.
- C++ workflows install Qt and package deployment directories, including Qt
  plugins/imports. Desktop archives on Unix preserve executable permissions.

## Local evidence

| Check | Result |
| --- | --- |
| Windows MSVC Release build | Passed |
| CTest | 21/21 passed |
| Native UI capture | 27 Qt screens generated |
| Tap/drag discrimination | Passed with Qt touch input |
| Gamepad UI/binding checks | Passed, 286 checks |
| Saved window geometry / lit frame | OpenGL and Vulkan passed |
| Renderer switching | Front screen switch plus three in-match switches passed; same scene, continued simulation, visibility, geometry and return to front verified |
| Vulkan resources / presentation conformance | Passed |
| Qt UI benchmark | Completed GPU frames for settings, pause menu and phone front screen |
| Windows packaged executable | UI capture passed with only System32 on PATH and Qt import/plugin environment overrides removed |
| Android arm64-v8a / x86_64 | Native builds and signed release APK passed |
| APK signature / alignment / metadata | v2 signature verified; 16 KiB page alignment passed; min 28 / target 35 / both ABIs |
| Renderer static audits | Legacy GL, shader interface, frontend GL, GL boundaries and RHI isolation passed |

Two runtime defects found by the switching check were fixed: Vulkan texture
identities no longer restart when a device is recreated, and initializing Qt's
OpenGL render control restores the game window surface before compositing.

`-uishot`, `-shellshot`, `-tapcheck`, `-gamepadcheck`, `-windowcheck` and
`-uibench` now execute Qt paths. The benchmark measures completed GPU frames
and excludes PNG writes. Historical CPU rasterizer options and the experimental
six-layout `-uidesign` command fail explicitly with guidance to the current
capture/benchmark commands; they do not return a stubbed success.

No Android device was attached to ADB. Android view composition, touch/gamepad
operation and live hunter previews still need physical-device runtime testing.
Linux/macOS build and deployment validation is performed separately in CI;
the Windows checks do not establish their runtime behavior.
