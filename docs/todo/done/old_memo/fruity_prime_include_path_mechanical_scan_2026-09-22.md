# Fruity-Prime Native include path mechanical scan
## 2026-09-22 / ZIP `Fruity-Prime-develop2 (5).zip`

## Summary

Scanned source owners:

- `src/MphRead.Native`
- `src/NcsfPlay.Native`
- `src/MphRead.Native.Android`

Resolution rules followed the repository `CMakeLists.txt` include roots and normal quoted-include lookup:

1. directory of the including file
2. target include directories
3. transitive native include root where applicable

Results:

- C/C++ source/header files scanned: **713**
- local quoted includes checked: **2,605**
- unresolved quoted includes: **16**
  - missing/wrong path: **15**
  - case-only mismatch: **1**
- repository-local angle-bracket includes with resolution problems: **0**
- affected files: **9**

The same 9 affected files were spot-checked against current `develop2` HEAD
`6bd5341aa494e9fb6688b5e13d44cf5ecd3eb307`; all 16 directives are still present there.

---

# Exact findings

| File | Line | Current include | Mechanical replacement | Classification |
|---|---:|---|---|---|
| `src/MphRead.Native/Mods/InputSettings.cpp` | 3 | `"PadBindings.hpp"` | `"Input/PadBindings.hpp"` | missing path |
| `src/MphRead.Native/Mods/InputSettings.cpp` | 4 | `"PointerInput.hpp"` | `"Input/PointerInput.hpp"` | missing path |
| `src/MphRead.Native/Mods/InputSettings.cpp` | 5 | `"StylusZone.hpp"` | `"Input/StylusZone.hpp"` | missing path |
| `src/MphRead.Native/Mods/InputSettings.cpp` | 6 | `"TouchSettings.hpp"` | `"Input/TouchSettings.hpp"` | missing path |
| `src/MphRead.Native/Mods/InputSettings.cpp` | 7 | `"../Network/DemoClip.hpp"` | `"Network/DemoClip.hpp"` | wrong parent depth |
| `src/MphRead.Native/Mods/InputSettings.cpp` | 8 | `"../Branding.hpp"` | `"Branding.hpp"` | wrong parent depth |
| `src/MphRead.Native/Mods/InputSettings.cpp` | 9 | `"../../Entities/Players/PlayerEntity.hpp"` | `"../Entities/Players/PlayerEntity.hpp"` | wrong parent depth |
| `src/MphRead.Native/Mods/InputSettings.cpp` | 10 | `"../Launcher/Portable/LauncherPrefs.hpp"` | `"Launcher/Portable/LauncherPrefs.hpp"` | wrong parent depth |
| `src/MphRead.Native/Mods/Render/SmoothHudIcon.hpp` | 3 | `"../../Hud/HudInfo.hpp"` | `"../../HUD/HudInfo.hpp"` | **case mismatch** |
| `src/MphRead.Native/Mods/Network/NetCheckClient.cpp` | 16 | `"SpectatorMode.hpp"` | `"../SpectatorMode.hpp"` | missing parent |
| `src/MphRead.Native/Mods/Network/MapAudit.hpp` | 8 | `"../../Entities/PlayerEntity.hpp"` | `"../../Entities/Players/PlayerEntity.hpp"` | moved into `Players/` |
| `src/MphRead.Native/Mods/Network/MapAudit.cpp` | 11 | `"../../Entities/PlayerEntity.hpp"` | `"../../Entities/Players/PlayerEntity.hpp"` | moved into `Players/` |
| `src/MphRead.Native/Mods/Chat/ChatBox.cpp` | 4 | `"../Input/InputSettings.hpp"` | `"../InputSettings.hpp"` | InputSettings is not under `Input/` |
| `src/MphRead.Native/Entities/Players/PlayerHud.cpp` | 6 | `"../../Paths.hpp"` | `"../../Formats/Formats.hpp"` | stale removed header |
| `src/MphRead.Native/Entities/Players/PlayerDialog.cpp` | 9 | `"../../Text/Strings.hpp"` | `"../../Strings.hpp"` | stale old directory |
| `src/MphRead.Native/Entities/Players/PlayerInput.cpp` | 17 | `"../../Mods/Input/InputSettings.hpp"` | `"../../Mods/InputSettings.hpp"` | InputSettings is not under `Input/` |

---

# Important hidden blockers

## 1. `PlayerDialog.cpp` has two include problems, not one

Current CI already exposes:

```cpp
#include "../../Paths.hpp"
```

as missing.

But the same file also contains:

```cpp
#include "../../Text/Strings.hpp"
```

and there is no:

```text
src/MphRead.Native/Text/Strings.hpp
```

The actual header is:

```text
src/MphRead.Native/Strings.hpp
```

Therefore fixing only the `Paths.hpp` include will likely expose `Text/Strings.hpp` immediately afterward.

`Paths` itself is currently declared in:

```text
src/MphRead.Native/Formats/Formats.hpp
```

so the mechanical target is:

```cpp
#include "../../Formats/Formats.hpp"
```

for `Paths`, and:

```cpp
#include "../../Strings.hpp"
```

for `Strings`.

---

## 2. `PlayerHud.cpp` has the same stale `Paths.hpp`

Current:

```cpp
#include "../../Paths.hpp"
```

Mechanical target:

```cpp
#include "../../Formats/Formats.hpp"
```

This is likely to become another hard compile blocker once compilation reaches this translation unit.

---

## 3. `Mods/InputSettings.cpp` contains eight broken includes

This is the largest concentration found by the scan.

Current block:

```cpp
#include "PadBindings.hpp"
#include "PointerInput.hpp"
#include "StylusZone.hpp"
#include "TouchSettings.hpp"
#include "../Network/DemoClip.hpp"
#include "../Branding.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../Launcher/Portable/LauncherPrefs.hpp"
```

Mechanical corrected paths:

```cpp
#include "Input/PadBindings.hpp"
#include "Input/PointerInput.hpp"
#include "Input/StylusZone.hpp"
#include "Input/TouchSettings.hpp"
#include "Network/DemoClip.hpp"
#include "Branding.hpp"
#include "../Entities/Players/PlayerEntity.hpp"
#include "Launcher/Portable/LauncherPrefs.hpp"
```

These are path-only findings. After fixing them, normal C++ API/parity errors may still appear.

---

## 4. Linux/Android case-sensitive failure in `SmoothHudIcon.hpp`

Current:

```cpp
#include "../../Hud/HudInfo.hpp"
```

Actual directory:

```text
HUD
```

Correct:

```cpp
#include "../../HUD/HudInfo.hpp"
```

This can be masked on case-insensitive filesystems but is invalid on case-sensitive environments.

---

# Suggested include-only repair order

If repairing include paths mechanically before continuing semantic parity work:

```text
I0  PlayerDialog.cpp
    ├─ ../../Paths.hpp
    └─ ../../Text/Strings.hpp

I1  PlayerHud.cpp
    └─ ../../Paths.hpp

I2  Mods/InputSettings.cpp
    └─ 8 broken directives

I3  SmoothHudIcon.hpp
    └─ Hud → HUD

I4  PlayerInput.cpp
I5  ChatBox.cpp
I6  NetCheckClient.cpp
I7  MapAudit.hpp / MapAudit.cpp
```

The order is based on current compile visibility plus concentration of hard include errors, not on behavioral importance.

---

# Scope / limitations

This was a mechanical path-resolution scan.

It can reliably detect:

- nonexistent quoted include paths
- wrong `../` depth
- stale directory moves
- case mismatch
- references to deleted/moved headers

It does **not** prove that every resolving include is semantically the correct header.

For example, an include can resolve to an existing file with the same or compatible declarations while still being architecturally stale. Detecting that requires declaration/use analysis or compilation.

Likewise, fixing these 16 directives does not guarantee build success; it removes the include-path layer so the next semantic C++ errors become visible.
