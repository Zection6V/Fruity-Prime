# Fruity-Prime C# → C++20 Native Build 修正優先順位
## 2026-09-22 01:55 JST 最新 GitHub / CI / 添付ログ検証版

## 0. 結論

current `develop2` HEAD:

```text
a71b01ac1d7e964f559371fb4d4846c45ddf6c39
Fix Enemy46 LesserIthrak native parity
```

この exact SHA の最新 Native CI では、Linux / macOS / Windows / Android arm64 / Android x86_64 の全 compiler/ABI で共通する最上流 blocker は:

```text
src/MphRead.Native/Entities/Enemies/49_ForceFieldLock.cpp
```

したがって現時点の推奨順は:

```text
P0  49_ForceFieldLock
    ↓
    exact pushed SHA の全 Native CI
    ↓
P1  51_CarnivorousPlant（Linux / Windows で既に露出）
    ↓
P2+ 新しい cross-platform first-error を再観測
```

添付ログの `PlayerScan` / `PlayerPause` / `PlayerProcess` は実在する downstream blocker だが、Windows が並列コンパイルで先まで見せているものなので、今は P0 に上げない。

---

# 1. current repository state

```text
Repository: Zection6V/Fruity-Prime
Branch: develop2
HEAD: a71b01ac1d7e964f559371fb4d4846c45ddf6c39
Tree: 397fdd6e26e24663ee1bd6ade0d4b6a97d5eae8c
Parent: e7254d8ac1b80b64c6b5543ae0aef08783d54ff7
Commit: Fix Enemy46 LesserIthrak native parity
```

---

# 2. current Native CI

| Platform | Run ID | Job ID | Result | first source blocker |
|---|---:|---:|---|---|
| Linux / GCC | `35626643262` | `106422518888` | failure | `49_ForceFieldLock.cpp` |
| macOS / Clang | `35626643261` | `106422518855` | failure | `49_ForceFieldLock.cpp` |
| Windows / MSVC | `35626643269` | `106422519196` | failure | `49_ForceFieldLock.cpp` |
| Android arm64-v8a | `35626643255` | `106422575755` | failure | `49_ForceFieldLock.cpp` |
| Android x86_64 | `35626643255` | `106422575783` | failure | `49_ForceFieldLock.cpp` |
| Android build contract | `35626643255` | `106422519261` | success | — |

5 compiler/ABI job が同じ source file に収束しているため、現在の P0 は明確。

---

# 3. 前回 MD からの進捗

前回 P0 だった `24_Gorea1A` は既に通過済み。その後も:

```text
Gorea1A
GoreaLeg
Gorea1B
Trocra
Gorea2
GoreaSealSphere2
GoreaMeteor
Voldrum
Quadtroid
Enemy38
FireSpawn
Slench
SlenchNest
SlenchTurret
LesserIthrak
```

まで compiler frontier / strict parity correction が進んでいる。

直近 commit:

```text
a71b01ac  Fix Enemy46 LesserIthrak native parity
e7254d8a  Fix Enemy45 SlenchTurret native parity
30210805  Fix Enemy43 SlenchNest native parity
a0626fbe  Fix Enemy41 Slench native parity
53001a87  Fix Enemy39 FireSpawn native parity
ced8c327  Fix Enemy38 native accessor parity
80187224  Fix Enemy37 UnitY parity
b9009e18  Fix Enemy37 Quadtroid parity
9038f47a  Fix Enemy36 Voldrum parity
```

旧 MD の priority は更新必須。

---

# 4. P0 — `49_ForceFieldLock.cpp`

authoritative C#:

```text
src/MphRead/Entities/Enemies/49_ForceFieldLock.cs
```

Native:

```text
src/MphRead.Native/Entities/Enemies/49_ForceFieldLock.cpp
src/MphRead.Native/Entities/Enemies/49_ForceFieldLock.hpp
```

current CI の主な error:

```text
line 158  VectorProperty does not provide a call operator
line 205  Weapons1P shared_ptr has no .size()
line 210  Weapons1P shared_ptr cannot be indexed directly
line 211  BeamProjectileArray value cannot convert to shared_ptr<BeamProjectileArray>
line 245  CameraInfo shared_ptr has no .Position
line 248  VectorProperty does not provide a call operator
line 250  VectorProperty does not provide a call operator
line 265  VectorProperty does not provide a call operator
line 292  VectorProperty does not provide a call operator
line 294  VectorProperty does not provide a call operator
line 340  VectorProperty does not provide a call operator
line 343  VectorProperty does not provide a call operator
line 370  raw EntityBase* vs shared_ptr<PlayerEntity> equality mismatch
```

主因は5系統。

---

# 5. P0-A — `Scale` helper collision

`49_ForceFieldLock.cpp` には:

```cpp
Vector3 Scale(Vector3 value, float factor)
```

という anonymous namespace helper がある。

一方 `EntityBase` には member `Scale` があるため、member function 内での unqualified `Scale(...)` が C++ name lookup で衝突している。

## 推奨

file-local helperだけを:

```cpp
ScaleVector(...)
```

等へ renameし、該当 call sites だけ追従。

### 禁止

- `EntityBase::Scale` の API変更
- `VectorProperty::operator()` 追加
- global Vector3 API変更

---

# 6. P0-B — `Weapons::Weapons1P` ownership adaptation

canonical declaration:

```cpp
using WeaponList = std::vector<std::shared_ptr<WeaponInfo>>;
extern const std::shared_ptr<const WeaponList> Weapons1P;
```

C#:

```csharp
Weapons.Weapons1P[(int)_forceField.Data.Type]
```

current Native は:

```cpp
Weapons::Weapons1P.size()
Weapons::Weapons1P[index]
```

としており不正。

## 推奨

既存 pattern:

```cpp
const Weapons::WeaponList& weapons
    = RequireReference(Weapons::Weapons1P);
```

で pointee list を取得し、既存 bounds/index helper を使う。

shared_ptr 専用の場当たり的 `VectorAt` overload は増やさない。

---

# 7. P0-C — `_beams` ownership

`EnemyInstanceEntity.hpp`:

```cpp
static std::shared_ptr<MphRead::BeamProjectileArray> _beams;
```

`EquipInfo`:

```cpp
void SetBeams(std::shared_ptr<BeamProjectileArray> value) noexcept;
```

current Native:

```cpp
_equipInfo->SetBeams(RequireReference(_beams));
```

は `_beams` を dereference してしまっている。

C#:

```csharp
new EquipInfo(..., _beams)
```

と同じ reference identity を保つなら、ownership object 自体を渡す方向が正しい。

概念:

```cpp
_equipInfo->SetBeams(_beams);
```

---

# 8. P0-D — `CameraInfo()` shared_ptr

C#:

```csharp
PlayerEntity.Main.CameraInfo.Position
```

Native:

```cpp
std::shared_ptr<CameraInfo> CameraInfo() const noexcept;
```

current Native:

```cpp
MainPlayer().CameraInfo().Position
```

は shared_ptr に `.` accessしている。

## 推奨

project の null/reference policy に合わせ:

```cpp
RequireReference(MainPlayer().CameraInfo()).Position
```

等。

---

# 9. P0-E — reference identity

C#:

```csharp
beam.Owner == PlayerEntity.Main
```

reference equality。

current Native:

```cpp
owner.get() == PlayerEntity::Main()
```

は raw pointer vs shared_ptr。

## 推奨

reference identity を保つ形で:

```cpp
owner.get() == PlayerEntity::Main().get()
```

または shared_ptr 同士の equality。

C# equality は nullでも例外にしないため、不要な `RequireReference` は入れない。

---

# 10. P0 修正順

```text
P0-1  Scale helper rename
P0-2  Weapons1P shared_ptr → const WeaponList& adaptation
P0-3  SetBeams に _beams ownership objectを渡す
P0-4  CameraInfo() pointee access
P0-5  Owner/Main reference identity
P0-6  C# 49_ForceFieldLock.cs 全体との strict parity audit
P0-7  full Native CI
```

---

# 11. P0 acceptance gate

- [ ] Linux `49_ForceFieldLock.cpp` diagnostics 0
- [ ] macOS diagnostics 0
- [ ] Windows diagnostics 0
- [ ] Android arm64-v8a diagnostics 0
- [ ] Android x86_64 diagnostics 0
- [ ] Scale arithmetic order一致
- [ ] Weapons1P index/bounds semantics一致
- [ ] `_beams` reference identity一致
- [ ] CameraInfo null/reference behavior一致
- [ ] owner/reference equality一致
- [ ] RNG call count/order不変
- [ ] `_shotFrames` state transition不変

---

# 12. provisional P1 — `51_CarnivorousPlant.cpp`

Linux / Windows で既に露出。

current errors:

```text
S07 property-style access
ObjectMetadata undeclared
Metadata::GetObjectById declaration visibility不足
```

C#:

```csharp
_spawner.Data.Fields.S07.EnemyHealth
Metadata.GetObjectById(...)
_spawner.Data.Fields.S07.EnemyDamage
```

Native union surface は accessor 化されているため:

```cpp
Fields.S07()
```

が必要。

current Native metadata には実際に:

```cpp
class ObjectMetadata
Metadata::GetObjectById(int)
Metadata::GetObjectById(uint32_t)
```

が存在する。

主因候補:

```text
1. S07 → S07()
2. Metadata.hpp 等の canonical declaration header が未include
```

P0後の full CI で cross-platform first frontier になれば P1確定。

---

# 13. 添付 Windows ログの扱い

添付ログでは:

```text
PlayerScan
PlayerPause
PlayerProcess
```

が大量に出ている。

これらは **実在する downstream backlog**。

ただし current global P0 ではない。

Windows/MSVC は多数 TU を並列ビルドするため、共通 first frontier より後の defect も大量に報告する。

---

# 14. downstream cluster A — `PlayerScan.cpp`

添付で確認できる代表:

```text
line 514
RequireReference / function-call mismatch

line 567, 612, 617, 622, 627
Scene::DrawHudObject ownership mismatch
```

`DrawHudObject` は:

```cpp
const std::shared_ptr<HudObjectInstance>&
```

を要求する一方、call site は `HudObjectInstance` value/reference を渡している。

## 方針

HUD object の C# reference identity と current Native ownership を照合。

`Scene::DrawHudObject` に value overload を追加して逃げない。

---

# 15. downstream cluster B — `PlayerPause.cpp`

添付で確認:

```text
room reference未初期化
GetDrawItems argument count mismatch
GetDrawItems arg2 int → Node& mismatch
Scene::AddRenderItem overload mismatch
```

method overload / Node reference / render ownership の cluster。

個別1行 patchより `PlayerPause.cs` vs Native pair の file-level audit向き。

---

# 16. downstream cluster C — `PlayerProcess.cpp`

添付エラーは多数だが、主に以下。

## C1. Weapons shared_ptr collection

```text
std::size(shared_ptr<const WeaponList>) failure
shared_ptr subscript failure
```

Enemyで確立した:

```cpp
const Weapons::WeaponList& list
    = RequireReference(...);
```

patternを再利用。

## C2. property → accessor

例:

```text
GameState::PointGoal
Scene::Room
Scene::FrameCount
EquipInfo::Ammo
PlayerEntity::EquipInfo
```

current Native declarationに従い accessor化。

## C3. CameraSequence visibility

```text
CameraSequence undeclared
CameraSequence::Current unresolved
```

canonical declaration/headerを確認。

## C4. shared_ptr WeaponInfo

```text
AmmoCost is not a member of shared_ptr<WeaponInfo>
```

pointee accessとnull boundaryを合わせる。

## C5. Features/Bugfixes signature mismatch

```text
Features::MaxPlayerDetail
Bugfixes::NoStrayRespawnText
Features::NoIdleSway
```

definition/declarationの namespace / return type / parameter mismatch を確認。

---

# 17. 添付 Android warnings

添付には:

```text
CMake compatibility < 3.10 deprecation warning
offsetof non-standard-layout warning
```

がある。

これらは現時点の build failure root ではない。

warning cleanup を `49_ForceFieldLock` より前にしない。

---

# 18. current Windows high-fanout backlog

最新 Windows run の raw diagnostic上位:

| File | raw count |
|---|---:|
| `GameState.cpp` | 372 |
| `PlayerDraw.cpp` | 290 |
| `RoomEntity.cpp` | 244 |
| `Renderer.cpp` | 228 |
| `PlayerProcess.cpp` | 218 |
| `Export/Collada.cpp` | 216 |
| `PlayerCamera.cpp` | 204 |
| `Formats/Collision.cpp` | 204 |
| `Formats/Effects.cpp` | 204 |
| `Menu.cpp` | 204 |
| `EntityBase.cpp` | 188 |
| `Export/Scripting.cpp` | 186 |
| `PlayerEntity.cpp` | 172 |
| `PlayerAi.cpp` | 122 |
| `PlayerPause.cpp` | 90 |
| `PlayerSound.cpp` | 68 |
| `PlayerCollision.cpp` | 62 |
| `PlayerScan.cpp` | 60 |

raw countは priority ranking ではない。

---

# 19. 最新 recommended order

```text
P0  49_ForceFieldLock
    ├─ Scale helper collision
    ├─ Weapons1P shared_ptr list
    ├─ _beams ownership
    ├─ CameraInfo shared_ptr
    └─ Owner/Main reference identity

    ↓ exact SHA full Native CI

P1  51_CarnivorousPlant
    ├─ S07()
    └─ Metadata/ObjectMetadata declaration visibility

    ↓ exact SHA full Native CI

P2+ cross-platform first-error再観測

Downstream Windows backlog:
    PlayerScan
    PlayerPause
    PlayerProcess
    EnemyInstanceEntity
    EnemySpawnEntity
    EntityBase
    GameState
    PlayerDraw
    RoomEntity
    Renderer
    Collision / Effects
    Export / Menu
```

---

# 20. worker split

## Worker A — P0 write

```text
49_ForceFieldLock.cpp
必要なら 49_ForceFieldLock.hpp
```

authority:

```text
49_ForceFieldLock.cs
```

## Worker B — P1 read-only audit

```text
51_CarnivorousPlant.cs
51_CarnivorousPlant.cpp/.hpp
Metadata.hpp
EnemySpawnEntity declarations
```

## Worker C — Player downstream classification

read-only:

```text
PlayerScan
PlayerPause
PlayerProcess
```

目的は P0/P1後に使う mismatch ledger 作成。

---

# 21. strict parity rules

- [ ] API
- [ ] defaults
- [ ] constructors
- [ ] null/reference behavior
- [ ] exception type/timing
- [ ] ownership/identity
- [ ] bounds
- [ ] integer signedness/unchecked behavior
- [ ] float operation order
- [ ] RNG call count/order
- [ ] branch/side-effect order
- [ ] timers/state transitions
- [ ] message/update/draw ordering

---

# 22. priority policy

```text
cross-platform common first-error
>
shared root API/header
>
multi-platform source blocker
>
Windows-only downstream raw count
>
warning cleanup
```

---

# 23. 最重要ポイント

前回の:

```text
P0 = 24_Gorea1A
```

は現在は古い。

今は:

```text
P0 = 49_ForceFieldLock
```

が Linux / macOS / Windows / Android で一致。

添付ログの `PlayerScan / PlayerPause / PlayerProcess` は重要な未来 backlog として追加したが、優先順位は:

```text
49
→ full CI
→ 51
→ full CI
→ 新しい共通 first-error
```

を基本とする。
