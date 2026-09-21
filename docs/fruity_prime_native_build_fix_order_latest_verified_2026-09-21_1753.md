# Fruity-Prime C# → C++20 Native Build 修正優先順位
## 2026-09-21 17:53 JST 最新 GitHub / CI 検証版

## 0. 結論

現在の `develop2` では、以前の最優先 blocker だった `PlayerAi.hpp`、`SceneSetup` enum、`BeamProjectileArray`、Music include、portable file-time、Enemy include path、Gorea/EquipInfo shared API、`AiPersonality` は既に修正が進んでいる。

**現在、Linux / macOS / Windows / Android arm64 / Android x86_64 の全 Native build で最上流に一致している blocker は `src/MphRead.Native/Entities/Enemies/24_Gorea1A.cpp`。**

したがって現時点の推奨順は:

```text
P0  24_Gorea1A strict parity / ownership-accessor closure
    ↓
    全 Native CI を同一 SHA で再実行
    ↓
P1+ 再観測

Windows で既に見えている provisional candidates:
    27_GoreaLeg
    28_Gorea1B
    30_Trocra
    31_Gorea2
    ↓
    その後 shared/high-fanout modules を再順位付け
```

**P0 修正前に Windows の raw error count だけを見て `GameState.cpp` や Player modules へ飛ばない。**

---

# 1. 基準 repository state

Repository:

```text
Zection6V/Fruity-Prime
```

Branch:

```text
develop2
```

Observed HEAD:

```text
1e2d46b082b97b1048adefcc61c5cb62e784352a
```

Commit:

```text
Fix PsychoBit weapon ownership parity
```

Parent:

```text
e4d83a211f87ef4ec736dccf9d88d74e91d80edd
```

Tree:

```text
10903f4a0becdd08a025dfbdad7932a6f068b1e3
```

この文書の優先順位は **この exact SHA の CI** を基準にしている。

---

# 2. current Native CI

HEAD `1e2d46b0...` に対する最新 run:

| Platform | Run ID | Job ID | Result | first source blocker |
|---|---:|---:|---|---|
| Linux / GCC | `35578843292` | `106266759851` | failure | `24_Gorea1A.cpp` |
| macOS / Clang | `35578843608` | `106266760773` | failure | `24_Gorea1A.cpp` |
| Windows / MSVC | `35578843530` | `106266760424` | failure | `24_Gorea1A.cpp` |
| Android x86_64 | `35578843531` | `106266808068` | failure | `24_Gorea1A.cpp` |
| Android arm64-v8a | `35578843531` | `106266808105` | failure | `24_Gorea1A.cpp` |
| Android build contract | `35578843531` | `106266760624` | success | — |

5つの compiler/ABI job が **同じ translation unit に収束している**ため、現在の P0 はかなり明確。

---

# 3. 前回版から完了済みに移動する項目

| 旧項目 | 主な commit | 現状 |
|---|---|---|
| MemoryClasses `GameMode` visibility | `e5638299...` | resolved |
| RoomEntity portable cancellation | `4c07e84d...`, `26bbc40e...` | frontier passed |
| DoorEntity native blockers | `531e614f...` | frontier passed |
| WarWasp native blockers | `cac55909...` | frontier passed |
| PlayerEntity canonical partial closure | `39794d85...`, `34d8f872...`, `7da8e582...` | advanced |
| PlayerAi first frontier | `c2028eb7...` | advanced |
| PlayerAi main frontier closure | `27a69a88...` | advanced |
| PlayerAi speed-decay indexed access | `e298fb7b...` | advanced |
| SceneSetup enum declarations | `4241c9e2...` | resolved |
| BeamProjectileArray canonical identity | `efcb5cb4...`, `0f018774...` | resolved |
| BeamProjectileArray consumers | `fb3cb9c4...` | advanced |
| Music `Paths` include | `19f7fa15...` | resolved |
| Music metadata ownership | `74f29547...` | advanced |
| portable file-time | `c145d54f...` | frontier passed |
| Enemy include paths | `c6febc3c...` | frontier passed |
| Gorea `EquipInfo` shared API | `d71277d0...` | advanced |
| Gorea vector scale parity | `41cde65d...` | advanced |
| canonical weapon implementation ownership | `6b2b2a3c...` | advanced |
| `AiPersonality` GameMode underlying type | `5b39245b...` | resolved |
| Cretaphid EquipInfo qualification | `e4d83a21...` | advanced |
| PsychoBit weapon ownership | `1e2d46b0...` | current HEAD |

旧 MD の `PlayerAi` を P0 のまま残すのは現在の CI と一致しない。

---

# 4. P0 — `24_Gorea1A.cpp`

## 対象

C# sole specification:

```text
src/MphRead/Entities/Enemies/24_Gorea1A.cs
```

Native:

```text
src/MphRead.Native/Entities/Enemies/24_Gorea1A.hpp
src/MphRead.Native/Entities/Enemies/24_Gorea1A.cpp
```

関連 canonical declarations:

```text
src/MphRead.Native/Metadata/Metadata.hpp
src/MphRead.Native/Metadata/Weapons.cpp
src/MphRead.Native/Entities/Players/PlayerEntity.hpp
src/MphRead.Native/Entities/Enemies/28_Gorea1B.hpp
src/MphRead.Native/Entities/Enemies/29_GoreaSealSphere1.hpp
```

## current cross-platform errors

```text
24_Gorea1A.cpp:769
VectorAt(shared_ptr<const WeaponList>, index) no matching overload

24_Gorea1A.cpp:780
Enemy29Entity incomplete type

24_Gorea1A.cpp:875
PlayerEntity::Speed member function used as property

24_Gorea1A.cpp:1229
PlayerEntity::CameraInfo member function used as property

24_Gorea1A.cpp:1286
PlayerEntity::CameraInfo member function used as property

24_Gorea1A.cpp:1422
VectorAt(shared_ptr<const WeaponList>, index) no matching overload

24_Gorea1A.cpp:1503
VectorAt(shared_ptr<const WeaponList>, index) no matching overload

24_Gorea1A.cpp:1522
VectorAt(shared_ptr<const WeaponList>, index) no matching overload

24_Gorea1A.cpp:1633
PlayerEntity::Flags1 member function used as property

24_Gorea1A.cpp:1650
PlayerEntity::Flags1 member function used as property
```

macOS / Android もほぼ同一。Windows は同じ root cause を複数 diagnostic に展開している。

---

# 5. P0-A — `Weapons::GoreaWeapons` ownership adaptation

`Metadata.hpp` の canonical declaration:

```cpp
namespace Weapons
{
    using WeaponList = std::vector<std::shared_ptr<WeaponInfo>>;
    extern const std::shared_ptr<const WeaponList> GoreaWeapons;
}
```

つまり `Weapons::GoreaWeapons` は vector そのものではなく:

```text
shared_ptr<const vector<shared_ptr<WeaponInfo>>>
```

。

一方 `24_Gorea1A.cpp` は:

```cpp
VectorAt(Weapons::GoreaWeapons, index)
```

としており、`VectorAt(const std::vector<T>&, ...)` に一致しない。

## 既に確立済みの canonical pattern

PsychoBit:

```cpp
const Weapons::WeaponList& enemyWeapons
    = RequireReference(Weapons::EnemyWeapons);
```

Cretaphid Crystal:

```cpp
const Weapons::WeaponList& bossWeapons
    = RequireReference(Weapons::BossWeapons);
const std::shared_ptr<WeaponInfo> weapon
    = VectorAt(bossWeapons, 0);
```

GoreaArm:

```cpp
ManagedListAt(RequireReference(Weapons::GoreaWeapons), 0);
```

## 推奨

必要な scope で:

```cpp
const Weapons::WeaponList& goreaWeapons
    = RequireReference(Weapons::GoreaWeapons);
```

を取得し、既存 `VectorAt` には pointee list を渡す。

`shared_ptr` 専用の場当たり的 `VectorAt` overload は追加しない。

### parity注意

- index評価順を変えない
- listをcloneしない
- `WeaponInfo` identityをcopy object化しない
- null behaviorを silent fallback に変えない

---

# 6. P0-B — `Enemy29Entity` complete type

`24_Gorea1A.cpp` は `28_Gorea1B.hpp` を include しているが、seal sphere の実型 `Enemy29Entity` は:

```text
src/MphRead.Native/Entities/Enemies/29_GoreaSealSphere1.hpp
```

で定義される。

current failure:

```text
invalid use of incomplete type Enemy29Entity
```

は、`SealSphere` 経由で member accessしているため。

## 推奨

implementation sideで:

```cpp
#include "29_GoreaSealSphere1.hpp"
```

を可視化。

complete type が `.cpp` だけで必要なら `.hpp` の dependency は増やさない。

Windows の `LoadEffectiveness` overload error はこの incomplete-type failure による cascading diagnostic の可能性が高いため、complete type 修正後に再判定する。

---

# 7. P0-C — `PlayerEntity::Speed`

C#:

```csharp
PlayerEntity.Main.Speed += between / 4;
```

Native canonical API:

```cpp
Vector3 Speed() const noexcept;
void SetSpeed(Vector3 value) noexcept;
```

## semantic mapping

概念的には:

```cpp
player.SetSpeed(
    player.Speed()
    + ScaleVector(between, 1.0F / 4.0F));
```

C# compound assignment と同じ observable order を保持する。

---

# 8. P0-D — `CameraInfo`

C#:

```csharp
PlayerEntity.Main.CameraInfo.SetShake(0.75f);
```

Native:

```cpp
std::shared_ptr<CameraInfo> CameraInfo() const;
```

current Native の:

```cpp
MainPlayer().CameraInfo.SetShake(...)
```

は不正。

## 推奨

repository の reference/null policy に合わせて:

```cpp
RequireReference(MainPlayer().CameraInfo()).SetShake(0.75F);
```

等。

対象:

```text
State08
State12
```

---

# 9. P0-E — `Flags1`

C#:

```csharp
PlayerEntity.Main.Flags1.TestFlag(PlayerFlags1.AltForm)
```

Native:

```cpp
PlayerFlags1 Flags1() const noexcept;
```

## 推奨

```cpp
TypeExtensions::TestFlag(
    MainPlayer().Flags1(),
    PlayerFlags1::AltForm)
```

等、現在の canonical helper を使用。

対象:

```text
24_Gorea1A.cpp:1633
24_Gorea1A.cpp:1650
```

---

# 10. P0 内の修正順

```text
P0-1  include 29_GoreaSealSphere1.hpp
P0-2  GoreaWeapons shared_ptr → const WeaponList& adaptation
P0-3  Speed() / SetSpeed(...)
P0-4  CameraInfo() shared_ptr dereference
P0-5  Flags1()
P0-6  24_Gorea1A.cs と全 observable behavior を再監査
P0-7  full Native CI
```

---

# 11. P0 acceptance gate

- [ ] Linux / GCC で `24_Gorea1A.cpp` diagnostics消滅
- [ ] macOS / Clang で消滅
- [ ] Windows / MSVC で消滅
- [ ] Android arm64-v8a で消滅
- [ ] Android x86_64 で消滅
- [ ] `ChangeWeapon()` weapon identity/order一致
- [ ] `CheckPlayerCollision()` Speed compound update一致
- [ ] `State08()` camera shake timing一致
- [ ] `State12()` camera shake timing一致
- [ ] `Behavior05()` charge lookup/order一致
- [ ] `Behavior09()` / `Behavior10()` Flags1 condition一致
- [ ] `Enemy29Entity` ownership/lifetimeを変更していない

---

# 12. P1+ — provisional next candidates

ここから先は **P0後のCIで再順位付けする**。

current Windows では以下が既に露出している。

## provisional P1 — `27_GoreaLeg.cpp`

```text
line 151  function/call mismatch
line 154  function/call mismatch
line 181  PlayerEntity::Speed property-style access
line 182  function/call mismatch
```

P0で確立する PlayerEntity accessor修正パターンを再利用できる可能性が高い。

---

## provisional P2 — `28_Gorea1B.cpp`

主な cluster:

```text
PlayerEntity::Speed property-style access
CollisionResult undeclared
CollisionDetection unresolved
TestFlags unresolved
PlayerEntity::Flags1 property-style access
StorySave shared_ptr adaptation
Cheats visibility
Scene movie API mismatch
PrevPosition property-style access
string_view → std::string mismatch
```

単一 typo ではなく strict file-level parity audit が必要。

---

## provisional P3 — `30_Trocra.cpp`

```text
CollisionResult unresolved
CollisionDetection unresolved
TestFlags unresolved
CheckBetweenPoints unresolved
GetCandidatesForLimits unresolved
Vector3 arithmetic mismatch
Vector3::UnitY unavailable
```

Collision API + Vector3 translation cluster。

---

## provisional P4 — `31_Gorea2.cpp`

```text
PlayerEntity::Speed property-style access
CollisionResult unresolved
```

Trocra / Gorea1B と同じ shared pattern を含む。

---

# 13. Windows raw-count high-fanout modules

current Windows run の上位 raw diagnostic occurrences:

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

これは priority ranking ではない。

1つの shared API/header mismatch が多数 translation unit で再報告されるため、P0後に first-error を再取得して分類する。

---

# 14. なぜ `GameState.cpp` を今P0にしないか

Windowsだけなら `GameState.cpp` の raw count は大きいが:

```text
Linux   → 24_Gorea1A
macOS   → 24_Gorea1A
Windows → 24_Gorea1A が先頭
Android arm64 → 24_Gorea1A
Android x86_64 → 24_Gorea1A
```

と consensus がある。

優先規則:

```text
cross-platform first frontier
>
Windows raw count
```

---

# 15. 現時点で再作業しないもの

同じ error が current CI に再出現しない限り、以下へ戻らない。

```text
PlayerAi old frontier
SceneSetup enum
BeamProjectileArray identity
Music Paths include
portable file-time
old Enemy hard include paths
Gorea EquipInfo shared API
AiPersonality GameMode
PsychoBit ownership
Cretaphid EquipInfo qualification
```

---

# 16. strict parity workflow

```text
1. current develop2 SHAを確認
2. C#をsole specificationとして読む
3. canonical Native declarationsを確認
4. current first-errorを分類
5. narrow root correction
6. unrelated redesignをしない
7. commit/push
8. exact pushed SHAの全 Native CIを見る
9. same-file diagnosticsが残れば同task内で閉じる
10. 新しい first-errorを次 priorityへ昇格
```

---

# 17. 共通 adaptation rules

## C# property → Native getter

```csharp
obj.Value
```

Native:

```cpp
Value()
```

なら:

```cpp
obj.Value()
```

。

## writable/compound property

```csharp
obj.Value += x;
```

Native:

```cpp
Value()
SetValue(...)
```

なら evaluation semantics を保持して:

```cpp
obj.SetValue(obj.Value() + x);
```

。

## shared_ptr collection

C#:

```csharp
Weapons.GoreaWeapons[index]
```

Native:

```cpp
shared_ptr<const vector<shared_ptr<WeaponInfo>>>
```

なら:

```cpp
const WeaponList& list = RequireReference(Weapons::GoreaWeapons);
const auto& item = VectorAt(list, index);
```

のように pointee collection を既存 indexed helperへ渡す。

## complete type

forward declarationだけでよいのは pointer/reference宣言まで。

```cpp
ptr->Member
sizeof(T)
base class knowledge
```

が必要なら implementation側で defining header を includeする。

---

# 18. 推奨 worker split

## Worker A — P0 implementation

write scope:

```text
24_Gorea1A.hpp
24_Gorea1A.cpp
```

C# authority:

```text
24_Gorea1A.cs
```

read-only dependencies:

```text
Metadata.hpp
Weapons.cpp
PlayerEntity.hpp
28_Gorea1B.hpp
29_GoreaSealSphere1.hpp
```

## Worker B — next-frontier triage

read-only:

```text
27_GoreaLeg
28_Gorea1B
30_Trocra
31_Gorea2
```

P0後に即着手できる narrow candidate を準備。

## Worker C — Windows shared-error classification

read-only:

```text
GameState
PlayerDraw
RoomEntity
Renderer
PlayerProcess
Collision
Effects
Menu
```

raw count順に書き換えない。

---

# 19. mandatory rebuild

P0後は必ず:

```text
Linux / GCC
macOS / Clang
Windows / MSVC
Android arm64-v8a
Android x86_64
```

を確認。

次 priority は:

```text
Linux first-error
macOS first-error
Android first-error
Windows first-error / independent TU errors
```

を比較して決める。

複数 platform が一致する blocker を優先。

---

# 20. current status matrix

| Item | Status |
|---|---|
| MemoryClasses GameMode | COMPLETE/frontier passed |
| RoomEntity cancellation | COMPLETE/frontier passed |
| DoorEntity blocker pass | COMPLETE/frontier passed |
| WarWasp blocker pass | COMPLETE/frontier passed |
| PlayerEntity canonical partial surface | advanced |
| PlayerAi main frontier | COMPLETE/frontier passed |
| SceneSetup enum | COMPLETE |
| BeamProjectileArray identity | COMPLETE |
| Music Paths include | COMPLETE |
| portable file-time | COMPLETE/frontier passed |
| Enemy include paths | COMPLETE/frontier passed |
| Gorea EquipInfo shared API | advanced |
| AiPersonality GameMode | COMPLETE |
| Cretaphid EquipInfo | advanced |
| PsychoBit ownership | COMPLETE/current head |
| **24_Gorea1A** | **CURRENT P0 / BLOCKED** |
| 27_GoreaLeg | provisional |
| 28_Gorea1B | provisional |
| 30_Trocra | provisional |
| 31_Gorea2 | provisional |
| Windows shared high-fanout cluster | deferred until P0 rerun |

---

# 21. 最新推奨順

```text
P0  24_Gorea1A
    ├─ GoreaWeapons pointee-list adaptation
    ├─ Enemy29Entity complete definition
    ├─ Speed()/SetSpeed()
    ├─ CameraInfo()
    └─ Flags1()

    ↓ full Native CI

P1  新しい cross-platform first-error
    └─ current Windows candidate: 27_GoreaLeg

P2  candidate: 28_Gorea1B

P3  candidate: 30_Trocra

P4  candidate: 31_Gorea2

P5+ 再観測した shared/high-fanout roots
    ├─ GameState
    ├─ Player*
    ├─ RoomEntity
    ├─ Renderer
    ├─ Collision / Effects
    ├─ Export
    └─ Menu
```

P1～P4 は固定順ではない。

**P0後の exact SHA CI が新しい authority。**

---

# 22. 最重要ルール

この migration では:

```text
エラー件数が多い file
```

より:

```text
現在 compile を最初に止めている root cause
```

を優先する。

現在それは明確に:

```text
src/MphRead.Native/Entities/Enemies/24_Gorea1A.cpp
```

。

これを strict C# parity で閉じ、同一 SHA の5 Native compile jobs を再確認してから次へ進むのが現在の最短経路。
