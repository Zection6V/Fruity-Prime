# Fruity-Prime C# → C++20 Native Build 修正優先順位
## 2026-09-22 14:37 JST 最新 GitHub / CI 検証版

## 0. 結論

`develop2` は前回版から大きく前進している。

current HEAD:

```text
6bd5341aa494e9fb6688b5e13d44cf5ecd3eb307
Preserve HomeView host worker read order
```

前回 P0 だった `EntityBase.cpp` は既に strict parity commit 群で frontier を通過済み。

current exact SHA の Linux / macOS / Android arm64 / Android x86_64 では、現在 **2つの独立 blocker が同時に露出**している。

```text
P0-A  PlayerDialog.cpp
      stale ../../Paths.hpp include

P0-B  PlayerCollision.cpp
      Scale name collision
      GameState / Features / Cheats / Scene accessor不足
```

1 workerで順番に直す場合:

```text
PlayerDialog include
→ PlayerCollision
→ full Native CI
→ 新しい cross-platform frontier
```

複数 workerなら P0-A / P0-B は file非競合なので並行可能。

current Windows / MSVC は観測時点で exact SHA `6bd5341a...` の build中だが、直前 terminal Windows run `a7146568...` でも:

```text
PlayerCollision
PlayerDialog
↓
PlayerDraw
...
```

が既に露出している。

したがって current priority の判断は十分強い。

---

# 1. 基準 repository state

```text
Repository: Zection6V/Fruity-Prime
Branch: develop2
HEAD: 6bd5341aa494e9fb6688b5e13d44cf5ecd3eb307
Tree: e9bcce15fb29863a378024ff0e4495436b5d0c42
Parent: a7146568f105d81837bd4cfec1fd065564b007b9
Commit: Preserve HomeView host worker read order
```

この MD の current frontier 判定はこの exact SHA を基準にする。

---

# 2. current Native CI

## Linux / GCC

```text
Run: 35690815122
Job: 106627232932
Result: failure
```

current errors:

```text
PlayerCollision.cpp
PlayerDialog.cpp
```

## macOS / Clang

```text
Run: 35690815130
Job: 106627232993
Result: failure
```

current errors:

```text
PlayerDialog.cpp
PlayerCollision.cpp
```

## Android

```text
Run: 35690815144
Contract job: success

arm64-v8a:
  Job 106627267840
  failure

x86_64:
  Job 106627267875
  failure
```

両 ABI とも:

```text
PlayerDialog.cpp
PlayerCollision.cpp
```

で failure。

## Windows / MSVC

```text
Run: 35690815115
Job: 106627233586
Status at observation: in_progress
Step: Build shared native owners
```

current exact-SHA Windows terminal result はまだ authority として使わない。

ただし直前 SHA:

```text
a7146568f105d81837bd4cfec1fd065564b007b9
```

の completed Windows run `35688591020` では:

```text
PlayerCollision.cpp
PlayerDialog.cpp
PlayerDraw.cpp
...
```

が既に確認済み。

---

# 3. 前回版からの大きな進捗

前回の P0:

```text
EntityBase.cpp
```

は通過済み。

その後、少なくとも以下の strict parity work が積まれている。

```text
EntityBase
MorphCameraEntity
NodeDefenseEntity
OctolithFlagEntity
ObjectEntity
PlayerSpawnEntity
PlatformEntity
DynamicLightEntity
HalfturretEntity
PlayerAi
PlayerEntity
PlayerCamera
Selection
GameFiles
Repack*
Map*
Q3*
Updater / Update*
DesktopUpdate
DebugLog
PlayerEntity bot level owner
GUI rows/views
```

EntityBase関連では特に:

```text
1ccf5d83...
136a562f...
ce940293...
7ba742f2...
be87e2d2...
6e8e11dc...
c762ee54...
42b67d42...  Fix EntityBase strict parity
```

まで連続修正されている。

従って旧 MD の:

```text
P0 = EntityBase
```

は現在は stale。

---

# 4. P0-A — `PlayerDialog.cpp` stale Paths include

## current failure

全 non-Windows platform:

```text
src/MphRead.Native/Entities/Players/PlayerDialog.cpp:5

fatal error:
../../Paths.hpp: No such file or directory
```

直前 Windows terminal run:

```text
error C1083:
Cannot open include file: '../../Paths.hpp'
```

## current source

```cpp
#include "PlayerDialog.hpp"

#include "../../GameState.hpp"
#include "../../Metadata/SoundMeta.hpp"
#include "../../Paths.hpp"
#include "../../Program.hpp"
#include "../../Scene.hpp"
...
```

しかし repository に:

```text
src/MphRead.Native/Paths.hpp
```

は存在しない。

## canonical `Paths` location

current sourceでは `Paths` は:

```text
src/MphRead.Native/Formats/Formats.hpp
```

に定義される。

```cpp
class Paths final
{
public:
    static std::string MphKey;
    static std::string FhKey;

    [[nodiscard]] static bool IsMphAmericas() noexcept;
    [[nodiscard]] static bool IsMphEurope() noexcept;
    [[nodiscard]] static bool IsMphJapan() noexcept;
    [[nodiscard]] static bool IsMphKorea() noexcept;
    ...
};
```

C# `PlayerDialog.cs` の使用箇所:

```csharp
if (Paths.IsMphJapan || Paths.IsMphKorea)
{
    maxWidth = 160;
}
```

Native accessor surface:

```cpp
Paths::IsMphJapan()
Paths::IsMphKorea()
```

## 推奨修正

stale include:

```cpp
#include "../../Paths.hpp"
```

を actual defining header:

```cpp
#include "../../Formats/Formats.hpp"
```

へ変更する方向。

### 注意

include を直した後、`PlayerDialog.cpp` 内の:

```text
Paths::IsMphJapan
Paths::IsMphKorea
```

等がまだ C# property syntax で残っていれば:

```cpp
Paths::IsMphJapan()
Paths::IsMphKorea()
```

へ current Native accessorに適合させる。

fatal include が先に止めているため、現在の CI ではその後の PlayerDialog errors はまだ見えていない。

---

# 5. P0-A acceptance gate

- [ ] Linux で PlayerDialog TU が include phaseを通過
- [ ] macOS で通過
- [ ] Android arm64で通過
- [ ] Android x86_64で通過
- [ ] Windows MSVCで C1083消滅
- [ ] Paths accessor usageが current Native declarationと一致
- [ ] Japan/Korea branching orderが C# と一致
- [ ] maxWidth assignment timing不変

---

# 6. P0-B — `PlayerCollision.cpp`

authoritative C#:

```text
src/MphRead/Entities/Players/PlayerCollision.cs
```

Native:

```text
src/MphRead.Native/Entities/Players/PlayerCollision.hpp
src/MphRead.Native/Entities/Players/PlayerCollision.cpp
src/MphRead.Native/Entities/Players/PlayerEntity.hpp
```

current Linux では PlayerCollisionだけで約20件の primary diagnostics が見える。

ただし root cause は少数。

---

# 7. P0-B1 — file-local `Scale` と `EntityBase::Scale` の name collision

`PlayerCollision.cpp` anonymous namespace:

```cpp
[[nodiscard]] constexpr Vector3 Scale(
    Vector3 value, float scale) noexcept;

[[nodiscard]] constexpr Vector4 Scale(
    Vector4 value, float scale) noexcept;
```

一方 `PlayerEntity` は `EntityBase` 由来の:

```cpp
VectorProperty Scale;
```

も持つ。

member function内の unqualified:

```cpp
Scale(toTurret, 0.45F)
Scale(between, dot)
Scale(edge, div)
Scale(result.Plane.Xyz(), dot)
Scale(ffResult.Plane, -1.0F)
...
```

が inherited member `Scale` と衝突。

current errors:

```text
type 'VectorProperty' does not provide a call operator
term does not evaluate to a function taking 2 arguments
```

## 推奨

anonymous helperを:

```cpp
ScaleVector(...)
```

など non-colliding nameへ rename。

Vector3 / Vector4 overloadの両方を同じ helper familyとして保持。

### 禁止

- `EntityBase::Scale` の surface変更
- `VectorProperty::operator()` 追加
- global Vector API redesign

これは file-local C++ name lookup adaptation。

---

# 8. P0-B2 — `GameState::EncounterState()` / `SinglePlayer()`

current code:

```cpp
if (source.IsBot() && GameState::SinglePlayer)
{
    std::int32_t encounter
        = ManagedAt(GameState::EncounterState, source.SlotIndex());
}
```

canonical `GameState.hpp`:

```cpp
[[nodiscard]] static bool SinglePlayer();
[[nodiscard]] static IntSlots& EncounterState() noexcept;
```

従って Nativeでは:

```cpp
GameState::SinglePlayer()
GameState::EncounterState()
```

current diagnostics:

```text
std::array<int, 8>&() noexcept
subscript requires array or pointer
ManagedAt no matching overload
```

は `EncounterState` の function object自体を containerとして渡しているため。

## 推奨

該当箇所すべて:

```cpp
GameState::SinglePlayer()
ManagedAt(GameState::EncounterState(), source.SlotIndex())
```

へ。

`ManagedAt` helper自体を変更して function pointerを受けられるようにはしない。

---

# 9. P0-B3 — `GameState::TransitionState()`

current:

```cpp
bool includeEntities
    = GameState::TransitionState == MphRead::TransitionState::None;
```

canonical:

```cpp
[[nodiscard]] static TransitionStateValue TransitionState() noexcept;
```

従って:

```cpp
GameState::TransitionState()
    == MphRead::TransitionState::None
```

へ。

---

# 10. P0-B4 — `Features::BoostOpensDoors()`

current:

```cpp
Features::BoostOpensDoors
```

canonical `Features.hpp`:

```cpp
[[nodiscard]] static bool BoostOpensDoors() noexcept;
```

従って:

```cpp
Features::BoostOpensDoors()
```

へ。

---

# 11. P0-B5 — `Cheats::WalkThroughWalls()`

current:

```cpp
Cheats::WalkThroughWalls
```

canonical:

```cpp
[[nodiscard]] static bool WalkThroughWalls() noexcept;
```

従って:

```cpp
Cheats::WalkThroughWalls()
```

へ。

---

# 12. P0-B6 — `Scene::RoomId()`

current:

```cpp
RequireReference(_scene).RoomId == 30
RequireReference(_scene).RoomId == 67
RequireReference(_scene).RoomId == 80
```

current Native Scene surfaceでは `RoomId` は method/accessor。

従って:

```cpp
RequireReference(_scene).RoomId()
```

へ。

current errors:

```text
reference to non-static member function must be called
unable to resolve function overload
```

---

# 13. P0-B7 — `ManagedAt` helperは原則そのまま

current `std::size(storage)` / subscript errorsは helper設計そのものより:

```cpp
GameState::EncounterState
```

を `()` なしで渡したことによる function type混入が原因。

まず call siteを:

```cpp
GameState::EncounterState()
```

に直す。

その後 residual errorがなければ helperを変更しない。

---

# 14. PlayerCollision修正順

```text
P0-B1  Scale(Vector3/Vector4) helper rename
P0-B2  GameState::SinglePlayer()
P0-B3  GameState::EncounterState()
P0-B4  GameState::TransitionState()
P0-B5  Features::BoostOpensDoors()
P0-B6  Cheats::WalkThroughWalls()
P0-B7  Scene::RoomId()
P0-B8  residual compiler errorsだけ再確認
P0-B9  PlayerCollision.cs 全体 strict audit
P0-B10 full Native CI
```

この順なら cascading template errorを早期に消せる。

---

# 15. PlayerCollision strict parity acceptance

- [ ] Linux diagnostics 0
- [ ] macOS diagnostics 0
- [ ] Android arm64 diagnostics 0
- [ ] Android x86_64 diagnostics 0
- [ ] Windows exact-SHA terminal後 diagnostics 0
- [ ] collision branch order一致
- [ ] player iteration order一致
- [ ] damage calculation order一致
- [ ] bot encounter damage rules一致
- [ ] door alt-attack condition一致
- [ ] transition-state entity inclusion一致
- [ ] ForceField collision plane inversion一致
- [ ] Spire climbing room exceptions一致
- [ ] CameraSequence gating一致
- [ ] Vector3 / Vector4 float multiplication order一致
- [ ] no extra state reads where C# observable order matters

---

# 16. current P0 implementation strategy

## 1 workerの場合

```text
1. PlayerDialog stale include
2. PlayerDialog Paths accessor follow-up if exposed
3. PlayerCollision helper/accessor closure
4. full Native CI
```

PlayerDialogはhard include failureで1 TU全体を遮断しており、修正が狭いため先に片付ける。

## parallel workerの場合

### Worker A

```text
PlayerDialog.cs
PlayerDialog.hpp
PlayerDialog.cpp
Formats/Formats.hpp read-only
```

### Worker B

```text
PlayerCollision.cs
PlayerCollision.hpp
PlayerCollision.cpp
PlayerEntity.hpp read-only
GameState.hpp read-only
Features.hpp read-only
Scene.hpp read-only
```

write fileが重ならないので並行しやすい。

---

# 17. provisional next — `PlayerDraw.cpp`

current exact-SHA non-Windowsは P0で停止しているため、P1として固定はしない。

ただし直前 Windows terminal runでは PlayerCollision / PlayerDialogの直後に `PlayerDraw.cpp` が露出。

代表 errors:

```text
CameraInfo accessor
Features::MaxPlayerDetail declaration/definition mismatch
shared_ptr<ModelInstance> → ModelInstance&
ModNodeUnresolved accessor
CameraSequence::Current()
shared_ptr<Node> → Node&
Scale helper / row helper name lookup
Node.Animation shared_ptr access
```

したがって P0後に `PlayerDraw` が cross-platform frontierへ上がる可能性は高い。

ただし full CI確認前に P1固定しない。

---

# 18. downstream Player cluster

直近 Windows terminal runで継続して見える。

```text
PlayerDraw
PlayerProcess
PlayerPause
PlayerScan
PlayerSound
PlayerEntityVoteHud
PlayerEntityEndScreen
```

現在は Player系 partial migrationの build frontierに入ったと見てよい。

一方 current branchでは:

```text
Complete PlayerEntity strict parity
Complete PlayerCamera strict parity
Complete PlayerAi strict parity
```

等も既に入っている。

従って各 fileで古い diagnosticsをそのまま前提にせず、P0後の exact SHA CIを authorityにする。

---

# 19. Windows raw-count backlogについて

直前 Windows terminal runでは依然として:

```text
GameState.cpp
PlayerDraw.cpp
RoomEntity.cpp
Renderer.cpp
PlayerProcess.cpp
Export/Collada.cpp
PlayerEntity.cpp
Collision.cpp
Effects.cpp
Menu.cpp
...
```

に大量 diagnosticsがある。

しかし raw diagnostic countは defect countではない。

今後も:

```text
current cross-platform first blockers
>
shared root/API mismatch
>
Windows raw count
```

の順で扱う。

---

# 20. 完了済み frontier history

少なくとも以下は以前の compiler frontierとして通過済み。

```text
MemoryClasses
PlayerAi old frontier
SceneSetup enum
BeamProjectileArray
Music
file-time
Gorea cluster
Voldrum
Quadtroid
FireSpawn
Slench family
LesserIthrak
ForceFieldLock
CarnivorousPlant
EnemyInstanceEntity
EnemySpawnEntity
EntityBase
ObjectEntity
PlayerSpawnEntity
PlatformEntity
DynamicLightEntity
HalfturretEntity
PlayerEntity main strict parity
PlayerCamera
Selection
```

current CIに同じ root errorが再出現しない限り優先順位を戻さない。

---

# 21. current status matrix

| Item | Status |
|---|---|
| EntityBase previous P0 | COMPLETE / frontier passed |
| ForceFieldLock | COMPLETE / frontier passed |
| CarnivorousPlant | COMPLETE / frontier passed |
| EnemyInstanceEntity | COMPLETE / frontier passed |
| EnemySpawnEntity | COMPLETE / frontier passed |
| PlayerAi main strict parity | advanced / frontier passed |
| PlayerEntity strict parity | advanced |
| PlayerCamera strict parity | advanced |
| **PlayerDialog stale Paths include** | **CURRENT P0-A** |
| **PlayerCollision native API closure** | **CURRENT P0-B** |
| PlayerDraw | provisional next |
| PlayerProcess | downstream backlog |
| PlayerPause | downstream backlog |
| PlayerScan | downstream backlog |
| GameState / Renderer / RoomEntity | deferred rerank |

---

# 22. mandatory rebuild strategy

P0-A / P0-B 修正後は必ず同一 pushed SHAで:

```text
Linux / GCC
macOS / Clang
Windows / MSVC
Android arm64-v8a
Android x86_64
```

を確認。

Android contract job successだけでは compile closure判定に使わない。

---

# 23. next priorityの決め方

P0後:

```text
Linux first errors
macOS first errors
Android arm64 first errors
Android x86_64 first errors
Windows terminal errors
```

を比較。

優先順位:

```text
1. 複数platform共通 hard blocker
2. shared/base API root
3. single translation unit root cluster
4. Windows-only downstream
5. warning cleanup
```

---

# 24. strict parity rules

PlayerCollision / PlayerDialogとも compileだけでは完了にしない。

- [ ] C# sole specification
- [ ] property → getter mapping
- [ ] writable property semantics
- [ ] reference/null behavior
- [ ] exception timing
- [ ] integer conversion
- [ ] float operation order
- [ ] branch order
- [ ] state read order
- [ ] collision iteration order
- [ ] message/render side effects
- [ ] no duplicate compatibility state
- [ ] no API redesign solely to satisfy compiler

---

# 25. 最新推奨順

```text
P0-A  PlayerDialog.cpp
      ├─ ../../Paths.hpp を actual defining headerへ
      └─ Paths::IsMphJapan()/IsMphKorea() 等を確認

P0-B  PlayerCollision.cpp
      ├─ Scale helper → ScaleVector等へ
      ├─ GameState::SinglePlayer()
      ├─ GameState::EncounterState()
      ├─ GameState::TransitionState()
      ├─ Features::BoostOpensDoors()
      ├─ Cheats::WalkThroughWalls()
      ├─ Scene::RoomId()
      └─ residual strict parity audit

      ↓ exact SHA full Native CI

P1  新しい cross-platform first-error
    └─ provisional candidate: PlayerDraw

P2+ downstream:
    ├─ PlayerProcess
    ├─ PlayerPause
    ├─ PlayerScan
    ├─ PlayerSound
    ├─ GameState
    ├─ RoomEntity
    ├─ Renderer
    ├─ Collision / Effects
    └─ Export / Menu
```

---

# 26. 最重要ポイント

前回の:

```text
P0 = EntityBase.cpp
```

は現在は古い。

現在は:

```text
PlayerDialog.cpp
PlayerCollision.cpp
```

の2 TUが non-Windows4 jobで共通 blocker。

特に `PlayerDialog` は1本の stale includeで完全停止しているため、単一workerなら最初に修正する価値が高い。

`PlayerCollision` は大量に見えるが、現在確認できる primary errorsの大半は:

```text
Scale name collision
+
C# property → Native getter の () 抜け
```

へ縮約できる。

従って今は大規模 redesignより:

```text
PlayerDialog include
→ PlayerCollision narrow API closure
→ exact SHA full CI
```

が最短ルート。
