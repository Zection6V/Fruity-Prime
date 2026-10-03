# macOS builds and startup

Both workflows publish on actual Macs: `macos-latest` for `osx-arm64`,
`macos-15-intel` for `osx-x64`. Each artifact must execute on its matching
runner; structural verification is not counted as a successful launch.

`MphReadAudioRid` uses the explicit RID when provided, otherwise the Mac SDK's
RID. Running an x64 SDK under Rosetta therefore gets x64 audio, matching the
process. Windows/Linux local defaults are unchanged.

The build pipeline cooks maps, publishes, sets executable mode, signs, checks
architecture and signatures, and executes `-smoketest`. The release pipeline
resolves one tag first; the Ubuntu and Mac jobs build that tag independently.
The final draft release waits for all packages to pass before uploading them.

## Signing and packaging

- `tools/sign-macos.sh DIRECTORY` signs every Mach-O file recursively, then
  framework containers inside out, then FruityPrime. Only the apphost gets
  `com.apple.security.cs.allow-jit`; no additional runtime entitlements are used.
- `tools/check-macos-build.sh DIRECTORY RID` checks executable mode, requires
  OpenAL, uses `lipo -verify_arch` on every Mach-O (universal files are allowed),
  prints `otool -L`, verifies signatures, and checks the JIT entitlement's value.
- `tools/package-macos.sh DIRECTORY DIST RID VERSION` preserves the publish
  native layout inside `Fruity Prime.app/Contents/MacOS`; maps go in
  `Contents/Resources/maps` because Apple's signer treats subdirectories of
  MacOS as nested code. It versions Info.plist from
  the build version, generates an ICNS from the existing project mark, signs
  components and the app, then verifies the bundle. `--deep` is verification
  only. It runs the existing asset/map guards over the staged package.
- The resulting tar.gz preserves executable permissions. The script extracts
  it again, verifies the resource seal and executes the extracted apphost.

When present in a publish, `gamecontrollerdb.txt` and `gamecontrollerdb.LICENSE`
are also moved into `Contents/Resources` before signing. They are data, not
nested code; controller feature branches must load the database from
`AppPaths.ResourceDirectory`. The packaging fixture checks both files and
proves that changing the database invalidates the app's resource seal.

These are ad-hoc signatures, not Developer ID signatures or notarization.
Gatekeeper can still require the user's approval for a downloaded archive.
See `tools/macos-README.txt`; quarantine removal is scoped to the app only.

## Native (C++/Qt) build

`tools/build/build-cpp-macos.sh [config] [deps] [vulkan] [run]` builds
`tools/build/out/macos-<config>/FruityPrime.app`; double-clicking
`build-cpp-and-log.command` runs `deps vulkan run` and logs to
`tools/build/out/build-cpp-macos.log`. Qt is `~/Qt/6.*` or Homebrew's `qt`.
VMA and `glslc` are not in Homebrew (and `shaderc` does not build on a tier-3
Homebrew), so `vulkan` takes them from vcpkg (`VCPKG_ROOT`, default
`~/vcpkg`), as the Linux and Android CI do. `deps` installs only what is
missing, one formula at a time, so one that fails to build does not stop the
rest. Verified on an Intel MacBook Pro (Iris Plus 655, macOS 15.7):
21/21 ctest, and `-vulkancheck`, `-vulkanresourcecheck`,
`-vulkanpresentcheck`, `-vulkanpresentfallbackcheck` and
`-presentconformance` (both backends) all pass with validation and zero
validation errors.

**OpenGL runs on the legacy 2.1 context**, the only one with immediate mode
on a Mac, while the RHI is written against GL 3.3. `OpenTK/GlFeatures.hpp`
reads the version and extension string, because macOS resolves every GL 4.1
symbol whatever the context can do -- `glGenSamplers` resolves in 2.1 and then
fails. `OpenGlNative` uses it to load the extension forms
(`APPLE_vertex_array_object`, `ARB_instanced_arrays`, `ARB_draw_instanced`,
`EXT_gpu_shader4`), to null what is absent so existing null checks decide
(sync, timer queries, debug labels, base instance, image load/store), and
to fall back where there is no extension: sampler state is written onto the
bound texture (`ApplySampler`), readback is `glGetBufferSubData`, buffer
copies read back and re-upload, and allocation and upload go through
ARRAY_BUFFER (COPY_READ/COPY_WRITE are 3.1).

**Vulkan is MoltenVK through `VK_EXT_metal_surface`.** The Qt window's view
gets a hosted CAMetalLayer (Homebrew's Qt backs it with a plain CALayer)
with an explicit frame and autoresizing: a hosted layer is otherwise 0x0
until the view next changes size. The loader is searched in the bundle,
`$VULKAN_SDK/lib` and both Homebrew prefixes, then `libMoltenVK.dylib`
alone, because a program started from Finder has no DYLD path; the file
found is handed to Qt as `QT_VULKAN_LIB` so Qt Quick's `QVulkanInstance`
adopts the renderer's instance through the same loader. The depth/stencil
format is a device capability (`Capabilities::depthStencilFormat`):
Vulkan guarantees D24S8 *or* D32S8 and Metal here has only the second.

Traps found on the way, each now handled:
- The app opts out of macOS window restoration (`ApplePersistenceIgnoreState`,
  registration domain only). After a crash AppKit otherwise opens a modal
  "reopen windows?" alert inside Qt's first `processEvents`, and a scripted
  run waits on it for ever.
- A `QGuiApplication` still alive at static destruction (every diagnostic
  command leaves through `exit()`) is not deleted: `~QGuiApplication` then
  reads Qt's already-destroyed thread storage and segfaults.
- The scene's renderer session gets the same rule (`SceneBackend.cpp`):
  macOS destroys the main thread's thread-locals before statics, so an
  OpenGL device torn down at static destruction asked Qt for the current
  context and segfaulted -- every `-maptest` on OpenGL exited 139 after a
  passing run. Orderly paths still release it explicitly.
- Game files: passing a ROM as the only argument (`FruityPrime ROM.nds`)
  checks its MD5 and extracts into the user-data directory; verified with a
  Japan 1.0 (AMHJ0) dump, then `-shellshot` (switchcheck PASS) and an
  8-bot `-maptest` on both renderers.
- Minimise and restore are asynchronous on macOS; a restore asked for
  during the minimise animation is dropped. The presentation check keeps
  asking and waits for sustained presentation, not for Qt's state flag.
- Homebrew's validation layer names its dylib bare, so the checks that turn
  validation on need `DYLD_FALLBACK_LIBRARY_PATH=/usr/local/lib`
  (`/opt/homebrew/lib` on Apple Silicon). The game itself runs without it.

## OpenGL startup

macOS uses an OpenGL 2.1 context with no profile hint. Its OpenGL 3.2+
contexts are core-only; the renderer's GLSL 1.20, immediate mode and fixed
function UI require legacy GL. Requesting 3.2 compatibility aborted inside
`_glfwCreateContextNSGL` before any launcher frame on Apple Silicon.
Windows/Linux retain the existing 3.2 compatibility request. A startup error
callback is installed before GLFW initialization so window-creation failures
report their native reason and return to OpenTK's managed failure check instead
of throwing through a native callback and aborting without a useful message.

GLFW initialization must set `CocoaChdirResources=false` before creating any
window or polling controllers. Its default changes cwd to the bundle Resources
directory: the extraction child still writes to Application Support, but the
launcher then reads paths.txt from the wrong directory and reports missing files.
The init hint preserves the writable working directory for settings and saves too.

## Smoke coverage and limits

`-smoketest` exits before game setup and checks configuration access, real
Avalonia headless initialization, Skia rasterization, embedded launcher assets,
map discovery, and macOS native loads/exports for OpenAL, GLFW, miniaudio,
Skia, HarfBuzz and AvaloniaNative. It also calls GLFW's version binding.
Each failure returns a nonzero exit code. No audio device or game files are
required. `run-macos-smoke.sh` runs with a fresh HOME and unrelated working
directory, with a 120-second process timeout. On a graphics-capable host it also
runs `-windowcheck`: the real launcher renders, compiles world/composite/cel/
disruption shaders, checks non-black readback and GL errors, resizes, and closes.
The packaged and re-extracted app receive the same check.

`-glfwpathcheck` is mandatory even without a GPU. In a temporary writable
fixture it creates an empty extraction directory and relative paths.txt entry,
initializes the actual GLFW/monitor path, and verifies cwd and launcher readiness
are preserved. Staged and re-extracted .app bundles run it too; no game data is used.

`check-macos-graphics.sh` queries CGL's renderer list before attempting graphics.
Exit 77 means no accelerated renderer (GLFW requires one), so rendered checks
are explicitly reported as **NOT RUN**. Native library/startup and signature
checks remain mandatory. Query errors and application failures on hosts with
an accelerated renderer still fail. Hosted virtual Macs can report only a
software renderer; a green build on those hosts leaves hardware rendering
acceptance pending.

This does not test a rendered game, an audio device, or Finder/Gatekeeper's
handling of a quarantined Internet download. Those require manual Mac checks.

## Paths and writable state

`Mods/Platform/AppPaths.cs` owns installation and desktop user-data roots.
Native libraries resolve from `AppContext.BaseDirectory`; maps resolve from
Contents/Resources in an app bundle and beside the executable otherwise. macOS
preferences, controls, paths.txt, extracted files, generated maps, saves,
thumbnails and logs use `~/Library/Application Support/Fruity Prime/`.
ConsoleSetup retains the caller's directory for explicit relative CLI inputs,
then sets cwd to user data so upstream's relative writes cannot alter the app.
Windows/Linux keep their portable behavior; Android keeps its activity overrides.

Existing portable Mac data is not moved or deleted automatically. Copy it into
the user-data directory or rerun extraction as described in the installation
note. Custom map sources still resolve from the bundle; `-mapdir` explicitly
overrides that location. The application bundle's dependencies are not moved
to Frameworks/ or Resources/, where native probing would need separate testing.

macOS disables the file-copy desktop updater: changing signed bundle resources
in place invalidates their seal. The update badge uses the existing release-page
fallback, and replacing the whole app leaves user data intact.

Every Mac startup records platform, process/runtime architecture, paths and
native library locations in `logs/platform-startup.log` under user data.
Native loader failures record their full exception and `file` architecture
description when available; audio retains its existing silent fallback.

## Background thumbnail workers

The launcher's automatic preview generation starts separate processes. Their
hidden windows use `DesktopGlContext` too: fixing only `RenderWindow` leaves
the worker requesting unsupported 3.2 compatibility on macOS, producing a crash
popup while the main app stays open. Shared settings preserve early error
logging, GL 2.1, default context flags and the writable working directory.

`-glfwpathcheck` checks the actual thumbnail settings without requiring a GPU.
On accelerated hosts, `-thumbnailwindowcheck` creates a separate hidden context
with those settings, draws a legacy primitive into an offscreen framebuffer,
and checks pixel readback and GL errors without using cartridge data. The Mac
packaging wrapper runs it against flat, staged and re-extracted packages.

Preview diagnostics must check GL 4.3 or `GL_KHR_debug` support and a nonzero
`glDebugMessageCallback` address before enabling debug output. The Mac legacy
context lacks that optional feature; calling its null pointer causes SIGSEGV,
which a managed exception handler cannot catch. Context descriptions query
flags only on GL 3.0+ and profile masks only on 3.2+ to avoid legacy GL errors.
The hidden-thumbnail test executes these diagnostics before drawing. Linux CI
also runs it on Mesa forced to GL 2.1 with KHR_debug disabled, verifying both
safe diagnostic skipping and successful rendering even without a Mac GPU runner.

Mac thumbnail batches use one background worker for the whole room list, even
when a higher job count is requested. They disable GLFW's Cocoa menu creation,
drain both redirected output streams with capped logging, and terminate workers
after five minutes or when the parent app exits. Nonzero exits/timeouts stop the batch and suppress further
worker launches for that app session; they are never retried automatically.
Concurrent callers serialize and recheck the preview cache before spawning.
`tools/thumbnail-batch-check` uses synthetic child processes to verify output
flooding, crash suppression, timeouts, process cleanup, and overlapping requests;
Mac CI also verifies the single-worker cap without requiring a GPU or game data.
