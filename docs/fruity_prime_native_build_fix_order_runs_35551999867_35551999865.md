# Fruity-Prime C# → C++20 Native Build 修正優先順位
## Actions runs 35551999867 / 35551999865 基準

## 目的

C# 実装を sole specification として C++20 Native 側を厳密再現している途中で発生しているビルドエラーについて、GitHub Actions の以下2 runを同一スナップショットとして解析し、**最短で compile frontier を前進させる修正順**を定義する。

- Native C++ Desktop: `35551999867`
- Native C++ Android: `35551999865`

単純な error 件数順ではなく、以下を優先する。

1. 全 platform / 多数 translation unit に波及する shared declaration error
2. platform gate になって後続エラーを隠す error
3. C# property → C++ accessor 移植時の system-wide surface mismatch
4. 複数ファイルが依存する shared Native API closure
5. 最後に個別 implementation / leaf errors

---

# 1. 基準スナップショット

両 run は完全に同じ commit / tree をビルドしている。

- Branch: `develop2`
- Commit: `80f565a153b2a355776d5a20ab45ab219b602809`
- Tree: `32ef14ae8e5ab32e8fb30683d9e87b3d3b4b94cc`
- Commit title: `Fix TagList invariant ignore-case parity`
- Run attempt: `1`

## Desktop

| Platform | Job ID | Result |
|---|---:|---|
| macOS / Clang | `106188329195` | failure |
| Linux / GCC | `106188329364` | failure |
| Windows / MSVC | `106188329378` | failure |

## Android

| Platform | Job ID | Result |
|---|---:|---|
| Android build contract | `106188329178` | success |
| Android NDK / arm64-v8a | `106188436265` | failure |
| Android NDK / x86_64 | `106188436270` | failure |

## 最新 ZIP による source-state 再確認

追加提供された `Fruity-Prime-develop2 (3).zip` の Native source を Actions log と照合した。

ZIP 内で確認できた重要点:

- `MemoryClasses.hpp` には `enum class GameMode : std::uint8_t;` が既に追加済み
- `RoomEntity.hpp` は依然として `<stop_token>` / `std::stop_token` / `std::stop_source` を使用
- `DoorEntity.cpp` は依然として C# property-style の呼び出しが多数残存
- `00_WarWasp.cpp` は `Fields.S01.WarWasp` のままで、Native `EnumSpawnUnion::S01()` accessor と不一致
- `PlayerAi.hpp` は canonical `PlayerEntity.hpp` に partial surface を注入せず、独立した縮小版 `class PlayerEntity` を定義している
- `PlayerPause.hpp` / `PlayerProcess.hpp` / `PlayerScan.hpp` は `MPHREAD_PLAYER_*_MEMBERS` macro を定義しているが、ZIP の `PlayerEntity.hpp` では include / 展開されていない
- `PlayerSound.hpp` の `DoorUnlockSfxTimer` / `DoorChimeSfxTimer` は public field として既に canonical PlayerEntity surface に展開されている

このため、Actions log から推定した P1～P4 は ZIP の実ソースでも再現でき、優先順位の大枠は変更不要。

ただし ZIP によって P2～P4 の root cause をより具体化できたため、以下では修正指針を更新している。

---

# 2. 重要な変化: TagList は今回の先頭 blocker ではない

以前の run では `NcsfPlay.Native/TagList.cpp` の ICU / Android API 問題が platform gate だったが、今回の SHA では状況が変わっている。

今回の macOS / Android ログでは `TagList.cpp` 自体の compilation command まで進み、**TagList-specific compiler error は出ていない**。

したがって、今回の修正順では `TagList` を先頭に置かない。

今回の actual frontier は `MphRead.Native` 側。

---

# 3. 結論: 推奨修正順

# P0. `MemoryClasses.hpp` の `GameMode` visibility

## 対象

- C# sole specification:
  - `src/MphRead/MemoryClasses.cs`
- Native:
  - `src/MphRead.Native/MemoryClasses.hpp`
  - `src/MphRead.Native/MemoryClasses.cpp`

## 全 platform 共通の最上流 blocker

Linux / GCC:

```text
MemoryClasses.hpp:2344:19:
'MphRead::GameMode' does not name a type

MemoryClasses.hpp:2463:19:
'MphRead::GameMode' does not name a type
```

macOS / Clang:

```text
MemoryClasses.hpp:2344:19:
no type named 'GameMode' in namespace 'MphRead'

MemoryClasses.hpp:2463:19:
no type named 'GameMode' in namespace 'MphRead'
```

Android arm64 / x86_64 でも同一。

Windows/MSVC では同じ1原因が macro expansion によって大量の二次 error に展開される。

```text
C2039 GameMode is not a member of MphRead
C3646 unknown override specifier
C2059 syntax error
C2334 unexpected tokens
C2061 syntax error: GameMode
C2065 value undeclared
```

使用箇所:

```cpp
MPH_MEM_ENUM8(GameMode, GameMode, 0x0);
```

`MemoryClasses.hpp` には他 enum の forward declaration はあるが、基準 SHA では `MphRead::GameMode` が宣言されていない。

## 状態

**現在の develop2 では修正済み。**

run の直後の commit:

```text
e5638299459950863b6d8940e5971224fbfe953c
Fix MemoryClasses GameMode visibility
```

追加内容:

```cpp
enum class GameMode : std::uint8_t;
```

## 判定

- 基準 run 上の Priority: **P0**
- Current status: **implemented**
- 次の作業: current-head CI で消滅確認

実際、current macOS job では `GameMode` error が消えているため、この修正は compile frontier を前進させている。

---

# P1. `RoomEntity` cancellation portability

## 対象

- C# sole specification:
  - `src/MphRead/Entities/RoomEntity.cs`
- Native:
  - `src/MphRead.Native/Entities/RoomEntity.hpp`
  - `src/MphRead.Native/Entities/RoomEntity.cpp`

## macOS / Android 共通 blocker

```text
RoomEntity.hpp:108:
no type named 'stop_token' in namespace 'std'

RoomEntity.hpp:176:
no type named 'stop_source' in namespace 'std'
```

Native header は既に:

```cpp
#include <stop_token>
```

を include しているため、単なる include 忘れではない。

実際の CI:

- macOS: AppleClang 17 / C++20
- Android: NDK 27.2 / Clang / `-std=c++20`
- Android target: API 24

この build matrix では `std::stop_token` / `std::stop_source` を利用できない。

## C# specification

C# は:

```csharp
private readonly CancellationTokenSource _cts = new CancellationTokenSource();

Task.Run(() => ProcessTransition(_cts.Token), _cts.Token);

public void CancelTransition()
{
    _cts.Cancel();
}
```

`ProcessTransition()` 中では複数地点で:

```csharp
if (token.IsCancellationRequested)
```

を確認する。

したがって Native 側で必要なのは「`std::stop_token` を使うこと」ではなく、**C# CancellationToken の observable behavior を再現すること**。

## 推奨方針

platform 非依存の小さな Native cancellation abstraction に置き換える。

必要 semantics:

- source と token が shared cancellation state を見る
- `Cancel()` 後は以後の `IsCancellationRequested` が true
- `CancellationToken.None` 相当を表現可能
- cancellation point の位置と順序を C# と一致
- cancellation による early-return timing を変更しない
- thread 起動順を変えない
- exception policy を勝手に追加しない

### 避けること

- macOS/Androidだけ cancellation check を無効化
- Android API level を上げて逃げる
- C# に存在しない blocking/wait semantics を追加
- `volatile bool` だけを雑に共有して data race を作る

## ZIP 確認

最新 ZIP でも以下がそのまま残っている。

```cpp
#include <stop_token>
void ProcessTransition(std::stop_token token);
std::stop_source _cts{};
```

`RoomEntity.cpp` 側にも:

```cpp
ProcessTransition(std::stop_token{});
const std::stop_token token = _cts.get_token();
```

が残存している。

したがって P1 は「log 上だけの候補」ではなく、**最新 ZIP でも未修正の確定 blocker**。

## Priority

**P1**

理由: macOS / Android で shared header gate。DoorEntity より先。

---

# P2. `DoorEntity` strict parity / property-accessor migration

## 対象

- C#:
  - `src/MphRead/Entities/DoorEntity.cs`
- Native:
  - `src/MphRead.Native/Entities/DoorEntity.hpp`
  - `src/MphRead.Native/Entities/DoorEntity.cpp`

## macOS / Android で同じ error 群

代表例:

```text
reference to non-static member function must be called
reference to overloaded function could not be resolved
VectorProperty does not provide a call operator
assigning to overloaded function type
expression is not assignable
```

Windows でも同じ root pattern:

```text
Scene::RoomId: non-standard syntax
Scene::Room: non-standard syntax
Scene::AreaId: non-standard syntax
Scene::FrameCount: non-standard syntax
function as left operand
```

## 典型的な原因

C# の property syntax を C++ へ port した際、Native API は accessor function 化されているのに call-site が C# syntax のまま残っている。

### 例

C#:

```csharp
_scene.RoomId
_scene.Room
_scene.AreaId
_scene.FrameCount
GameState.TransitionState = TransitionState.Start
GameState.TransitionRoomId = TargetRoomId
Cheats.UnlockAllDoors
```

Native call-site に C# style が残っている。

```cpp
_scene->RoomId
_scene->Room
_scene->AreaId
_scene->FrameCount
GameState::TransitionState = ...
GameState::TransitionRoomId = ...
Cheats::UnlockAllDoors
```

最新 ZIP では canonical Native declaration も確認できる。

`Scene`:

```cpp
std::int32_t RoomId() const noexcept;
std::int32_t AreaId() const noexcept;
std::uint64_t FrameCount() const noexcept;
std::shared_ptr<Entities::RoomEntity> Room() const noexcept;
```

`GameState`:

```cpp
static GameMode Mode() noexcept;
static void Mode(GameMode value) noexcept;
static TransitionStateValue TransitionState() noexcept;
static void TransitionState(TransitionStateValue value) noexcept;
static std::int32_t TransitionRoomId() noexcept;
static void TransitionRoomId(std::int32_t value) noexcept;
```

`Cheats`:

```cpp
static bool UnlockAllDoors() noexcept;
```

したがって DoorEntity では少なくとも次の adaptation が必要。

```text
GameState::Mode
    → GameState::Mode()

Cheats::UnlockAllDoors
    → Cheats::UnlockAllDoors()

_scene->RoomId
    → _scene->RoomId()

_scene->AreaId
    → _scene->AreaId()

_scene->FrameCount
    → _scene->FrameCount()

_scene->Room
    → _scene->Room()

GameState::TransitionState = x
    → GameState::TransitionState(x)

GameState::TransitionRoomId = x
    → GameState::TransitionRoomId(x)
```

ただし一括置換ではなく、各 call-site で C# の getter/setter semantics と評価順を確認する。

## `Scale` name lookup collision

DoorEntity の:

```text
DoorEntity.cpp:490
DoorEntity.cpp:491
VectorProperty does not provide a call operator
```

は、helper `Scale(...)` と Entity property/member `Scale` の name lookup collision の可能性が高い。

C# の:

```csharp
Matrix4.CreateScale(inst.Model.Scale)
```

等の意味を維持し、Native helper を明示的に qualified / renamed local helper 化する。

**C# member surface を変更して解決しない。**

## Door sound timer

C#:

```csharp
PlayerEntity.Main.DoorChimeSfxTimer = 2 / 30f;
PlayerEntity.Main.DoorUnlockSfxTimer = 2 / 30f;
```

Windows では:

```text
SetDoorChimeSfxTimer is not a member of PlayerEntity
SetDoorUnlockSfxTimer is not a member of PlayerEntity
```

最新 ZIP の Native `PlayerSound.hpp` では:

```cpp
public:
    float DoorUnlockSfxTimer = 0.0F;
    float DoorChimeSfxTimer = 0.0F;
```

であり、`MPHREAD_PLAYER_SOUND_MEMBERS` は canonical `PlayerEntity.hpp` に既に展開されている。

したがって `SetDoorChimeSfxTimer()` / `SetDoorUnlockSfxTimer()` を新設するのは不要で、C# readonly/reference surface を崩さない範囲で既存 public field を使うのが現在の Native surface に一致する。

この箇所の build-fix は「missing setter を追加」ではなく、**DoorEntity 側の誤った setter 想定を修正**する。

## ZIP 確認

最新 ZIP の `DoorEntity.cpp` でも以下が未修正のまま残る。

```cpp
GameState::Mode
Cheats::UnlockAllDoors
_scene->Room
_scene->RoomId
_scene->AreaId
_scene->FrameCount
GameState::TransitionState = ...
GameState::TransitionRoomId = ...
main.SetDoorChimeSfxTimer(...)
main.SetDoorUnlockSfxTimer(...)
```

よって P2 も **最新 ZIP で未修正の確定 blocker**。

## Priority

**P2**

P1 完了後に macOS / Android の frontier を最も大きく前進させる。

---

# P3. `00_WarWasp` accessor / member translation

## 対象

- C#:
  - `src/MphRead/Entities/Enemies/00_WarWasp.cs`
- Native:
  - `src/MphRead.Native/Entities/Enemies/00_WarWasp.hpp`
  - `src/MphRead.Native/Entities/Enemies/00_WarWasp.cpp`

基準 run の Windows に加え、`GameMode` 修正後の current macOS でも WarWasp error が露出している。

Current macOS 代表例:

```text
00_WarWasp.cpp:145:
reference to non-static member function must be called

00_WarWasp.cpp:151
00_WarWasp.cpp:152
00_WarWasp.cpp:183
00_WarWasp.cpp:188

00_WarWasp.cpp:210:
VectorProperty does not provide a call operator
```

## C# で重要な surface

```csharp
_spawner.Data.Fields.S01.WarWasp.Volume2
_spawner.Data.Fields.S01.WarWasp.Volume1
_spawner.Data.Fields.S01.WarWasp.PositionCount
_spawner.Data.Fields.S01.WarWasp.MovementVectors[i]
PlayerEntity.Main.Position
Position
```

最新 ZIP では root cause を具体的に確認できる。

`EnemySpawnEntity::Data` 自体は reference field だが、`Data.Fields` の Native union view は:

```cpp
EnemySpawnFields01 S01() const noexcept;
```

という accessor。

一方 `00_WarWasp.cpp` は:

```cpp
_spawner->Data.Fields.S01.WarWasp
```

のままなので、少なくとも:

```text
Fields.S01.WarWasp
    → Fields.S01().WarWasp
```

という C++ adaptation が必要。

`WarWasp` 自体は `EnemySpawnFields01` の readonly field なので、`WarWasp()` のような架空 accessor は作らない。

また line 210 付近の `Scale(...)` は DoorEntity と同種の inherited `Scale` property / helper name-lookup collision と考えられるため、C# の vector scaling semantics を保った local helper 名へ明示的に分離する。

## ZIP 確認

最新 ZIP でも:

```cpp
_spawner->Data.Fields.S01.WarWasp
```

および unqualified `Scale(...)` が残っているため、P3 は **未修正**。

## Priority

**P3**

理由:

- GameMode 修正後の macOS で実際に露出
- DoorEntity と同じ「C# property → C++ accessor」class の問題
- 修正パターンを DoorEntity から再利用可能

---

# P4. `PlayerEntity` partial surface closure

Windows の raw error 数で最大クラス。

代表:

```text
PlayerAi.hpp
'_scene' is not a member of PlayerEntity

PlayerPause.cpp
'_drawPauseState' is not a member of PlayerEntity
'_navTextTimer' is not a member of PlayerEntity
'_navLoading' is not a member of PlayerEntity

PlayerProcess.cpp
'BombCountCheck' is not a member of PlayerEntity

PlayerScan.cpp
'SetCombatVisor' is not a member of PlayerEntity

PlayerDraw.cpp
'DrawScanModels' identifier not found
CameraInfo accessor mismatch

PlayerEntityEndScreen.cpp
'_endPanel' is not a member of PlayerEntity
...
```

## 重要

Windows の error occurrence:

```text
PlayerAi.hpp                   404
PlayerDraw.cpp                 296
PlayerCamera.cpp               204
PlayerPause.cpp                204
PlayerProcess.cpp              204
PlayerScan.cpp                 204
PlayerEntityEndScreen.cpp      182
PlayerEntity.cpp               172
```

これを個別に8ファイル直すのではなく、まず C# partial `PlayerEntity` 全体と Native macro/header closure を比較する。

## 最新 ZIP で確認できた structural root cause

### 1. `PlayerAi.hpp` が canonical `PlayerEntity` partial seam になっていない

他の Player partial は `MPHREAD_PLAYER_*_MEMBERS` macro で canonical `PlayerEntity.hpp` に member を注入する形式が多い。

しかし ZIP の `PlayerAi.hpp` は:

```cpp
class PlayerEntity
{
public:
    class PlayerAiData;
    std::shared_ptr<PlayerAiData> AiData{};
    ...
};
```

という**別の縮小版 `PlayerEntity` class definition**を持つ。

その `PlayerAiData` constructor は:

```cpp
: _player(player), _scene(*player->_scene)
```

を使うが、この縮小版 class には `_scene` がない。

Windows の:

```text
PlayerAi.hpp: '_scene' is not a member of PlayerEntity
```

はこの構造と直接一致する。

したがって PlayerAi の修正では、compile を通すために shadow class へ `_scene` を足すのではなく、**C# partial class と同様に canonical Native `PlayerEntity` surface に統合すること**が必要。

### 2. Pause / Process / Scan macro が定義されているが canonical class に展開されていない

ZIP では:

```text
PlayerPause.hpp
  MPHREAD_PLAYER_PAUSE_MEMBERS

PlayerProcess.hpp
  MPHREAD_PLAYER_PROCESS_MEMBERS

PlayerScan.hpp
  MPHREAD_PLAYER_SCAN_MEMBERS
```

が存在する。

一方 `PlayerEntity.hpp` が展開しているのは:

```text
CAMERA
COLLISION
DIALOG
DRAW
HUD
INPUT
SOUND
CHAT_HUD
```

であり、`PAUSE` / `PROCESS` / `SCAN` は展開されていない。

これは Windows の:

```text
_drawPauseState missing
_navTextTimer missing
BombCountCheck missing
SetCombatVisor missing
```

等と整合する。

よって P4 は単なる call-site 修正ではなく、**Native partial declaration closure の構造修正**として扱う。

## 推奨作業

1. C# `PlayerEntity` partial files の member inventory
2. Native:
   - `PlayerEntity.hpp`
   - `PlayerAi.hpp`
   - `PlayerCamera.hpp`
   - `PlayerCollision.hpp`
   - `PlayerDialog.hpp`
   - `PlayerDraw.hpp`
   - `PlayerHud.hpp`
   - `PlayerInput.hpp`
   - `PlayerSound.hpp`
   - `PlayerPause.hpp`
   - `PlayerProcess.hpp`
   - `PlayerScan.hpp`
   - related Mods partial headers
3. `PlayerAi.hpp` の shadow `PlayerEntity` definition を canonical partial seam としてどう materialize すべきか、C# `partial class PlayerEntity` を sole specification に決定
4. `MPHREAD_PLAYER_PAUSE_MEMBERS` / `PROCESS` / `SCAN` の canonical `PlayerEntity.hpp` への inclusion / expansion を確認
5. 以下を分類
   - field
   - property
   - getter/setter
   - method
   - static
   - instance
6. macro 展開後の `PlayerEntity` class surface に漏れがないか確認

### 避けること

- 各 `.cpp` を通すためだけに同名 field/method を重複追加しない
- `PlayerAi.hpp` の shadow class に missing member を継ぎ足して延命しない
- Pause / Process / Scan の member を call-site 側ローカル state に逃がさない
- C# partial class に存在する1つの state を Native 側で複数 state に分裂させない

## Priority

**P4 / high fan-out shared API closure**

---

# P5. `GameState.cpp` managed-array helper ambiguity

Windows:

```text
GameState.cpp:594:
ManagedAt: ambiguous call to overloaded function
```

同 file だけで raw occurrence 約 `372`。

## 方針

- C# indexer semantics を確認
- `ManagedAt` overload set の型を明示
- const / non-const
- shared_ptr / pointee
- integer index type
- returned reference/value semantics

を整理する。

テンプレート側を雑に1 overloadへ潰すのではなく、C# で実際に必要な mutation semantics を保つ。

## Priority

**P5**

PlayerEntity / GameState は多数 implementation の上流。

---

# P6. `RoomEntity.cpp` 自身の C++ name lookup / type closure

P1 の `stop_token` 問題を通した後、Windows では RoomEntity implementation 自体にも大量の error がある。

代表:

```text
RoomEntity.cpp:367:
RoomEntity::PortalNodeRef::Portal is not a type name

Formats::Collision::Portal expected an expression instead of a type
```

原因候補:

```cpp
struct PortalNodeRef
{
    const std::shared_ptr<Formats::Collision::Portal> Portal;
};
```

C# の property/member naming を直接 port した結果、C++ では type `Portal` と member `Portal` が name lookup 上衝突している可能性がある。

## 方針

- C# public observable member naming は保持
- Native 内部型参照は fully-qualified にする
- type/member name collision を C++ adaptation で解消
- ABI/layout を変える必要があるか確認
- P1 cancellation 修正と同時に behavior を混ぜない

## Priority

**P6**

---

# P7. Weapons / `EquipInfo` shared API closure

複数 Enemy file に共通して出ている。

## 主な affected files

```text
23_PsychoBit.cpp
26_GoreaArm.cpp
39_FireSpawn.cpp
その他 weapon-consuming enemies
```

代表:

```text
shared_ptr<const WeaponList> has no member size
operator[] unavailable on shared_ptr

EquipInfo has no member:
SetWeapon
SetBeams
SetGetAmmo
SetSetAmmo
SetUnchargedDamage
SetSplashDamage
SetHeadshotDamage
```

## root class

これは各 Enemy の個別バグというより、

- C# object/property mutation
- Native `EquipInfo`
- `WeaponList` ownership
- `shared_ptr<vector<...>>`

の interface adaptation が統一されていない問題。

## 推奨

`Metadata/Weapons` / `EquipInfo` の canonical Native API を先に確定し、その後 consumer を修正する。

### 禁止

Enemy ごとに独自 setter shim を追加しない。

## Priority

**P7 / multi-file shared API**

---

# P8. Scene / GameState property-accessor migration family

Windows では DoorEntity 以外にも同一 root pattern が広範囲。

例:

```text
Scene::RoomId
Scene::AreaId
Scene::FrameTime
Scene::FrameCount
PlayerEntity::Speed
PlayerEntity::Flags1
GameState property access
StorySave shared_ptr/raw pointer
```

affected examples:

- `TriggerVolumeEntity.cpp`
- `ItemInstanceEntity.cpp`
- `ItemSpawnEntity.cpp`
- `DynamicLightEntity.cpp`
- `EnemySpawnEntity.cpp`
- Gorea files
- その他 Entity

## 推奨順

一括 search/replace はしない。

1. C# source 1ファイル
2. Native counterpart 1ファイル
3. property/accessor mapping を確認
4. ownership/null/reference semantics を確認
5. build
6. 次ファイル

という strict one-file parity 方式を維持する。

## Priority

**P8**

DoorEntity / WarWasp で確立した変換ルールをここへ展開する。

---

# P9. Gorea / Enemy implementation cluster

Windows で明確に残るもの。

## `24_Gorea1A.cpp`

```text
Cannot open include file:
../HalfturretEntity.hpp
```

まず include path / actual hierarchy を修正。

## `25_GoreaHead.cpp`

```text
shared_ptr<Node> → Node* conversion
VectorProperty call collision
```

## `26_GoreaArm.cpp`

```text
ManagedListAt no matching overload
EquipInfo setters missing
```

P7 の後。

## `27_GoreaLeg.cpp`

```text
PlayerEntity::Speed accessor misuse
VectorProperty collision
```

## `28_Gorea1B.cpp`

```text
PlayerEntity::Speed accessor misuse
CollisionResult undeclared
CollisionDetection namespace/type resolution
PlayerEntity::Flags1 accessor misuse
ExitAltForm missing
GameState name resolution
```

## Priority

**P9**

shared API P4 / P7 / P8 後に処理。

---

# P10. Core formats / utility / export closure

Windows で並列に露出しているが、上記 shared layer より後でよい。

## `Formats/Collision.cpp`

```text
use of undefined type MphRead::Scene
```

header dependency / complete type 問題。

## `Formats/CollisionDetection.cpp`

同様に Scene complete type。

## `Formats/Effects.cpp`

```text
System::Buffers::ArrayPool
```

C# BCL construct の Native adaptation 未完。

## `Read.cpp`

```text
RoomMetadata type resolution
Effects namespace misuse
structured binding source type failure
```

## `Export/Collada.cpp`

`Paths` property/function adaptation。

## `Export/Scripting.cpp`

```text
System::Version missing
```

C# BCL type adaptation。

## `Menu.cpp`

```text
Sound::Music::UserVolume
```

namespace/type/property mapping。

## `Mods/PauseMenu.cpp`

MSVC-specific token collision:

```text
__leave
```

identifier / macro / keyword collision を確認。

## Priority

**P10**

---

# 4. Platform 別 frontier

## Linux / GCC

基準 runではほぼ `MemoryClasses::GameMode` だけで停止。

```text
GameMode visibility
↓
現在修正済み
↓
次の Linux frontier は current CI で再観測
```

Linux は一番「上流 blocker を正確に見る」platform として使いやすい。

---

## macOS / Clang

基準 run:

```text
MemoryClasses GameMode
RoomEntity stop_token
RoomEntity stop_source
DoorEntity
```

GameMode 修正後の current macOS:

```text
RoomEntity stop_token
RoomEntity stop_source
DoorEntity
00_WarWasp
```

したがって current confirmed order は:

```text
RoomEntity cancellation
→ DoorEntity
→ WarWasp
```

---

## Android arm64 / x86_64

両 ABI で同一 error frontier。

```text
MemoryClasses GameMode
RoomEntity stop_token/stop_source
DoorEntity
```

arm64 と x86_64 で同一なので architecture-specific issue ではない。

---

## Windows / MSVC

Windows は parallel compilation により大量の downstream error を同時に露出する。

raw count を優先順位にしてはいけない。

例:

| File | Raw occurrences |
|---|---:|
| `PlayerAi.hpp` | 404 |
| `GameState.cpp` | 372 |
| `PlayerDraw.cpp` | 296 |
| `RoomEntity.cpp` | 238 |
| `Export/Collada.cpp` | 216 |
| `PlayerCamera.cpp` | 204 |
| `PlayerPause.cpp` | 204 |
| `PlayerProcess.cpp` | 204 |
| `PlayerScan.cpp` | 204 |
| `Formats/Collision.cpp` | 204 |
| `Formats/Effects.cpp` | 204 |

これは defect count ではない。

共有 class surface / template error が何十 translation unit でも再報告される。

---

# 5. 推奨実行シーケンス

## Gate A

- [x] `MemoryClasses.hpp` `GameMode` forward declaration
- [ ] Desktop current SHA で Linux / Windows disappearance 確認
- [ ] Android current SHA で disappearance 確認

## Gate B

- [ ] `RoomEntity` cancellation abstraction
- [ ] macOS build
- [ ] Android arm64 build
- [ ] Android x86_64 build

ここで macOS/Android の `stop_token` / `stop_source` が消えることを確認。

## Gate C

- [ ] `DoorEntity.cs` vs Native strict audit
- [ ] property → accessor call-site correction
- [ ] GameState setter adaptation
- [ ] Scene getter adaptation
- [ ] `Scale` name lookup collision
- [ ] Player door SFX timer surface
- [ ] build

## Gate D

- [ ] `00_WarWasp.cs` vs Native strict audit
- [ ] nested memory wrapper accessor correction
- [ ] PlayerEntity.Main property adaptation
- [ ] VectorProperty/helper collision
- [ ] build

## Gate E

- [ ] PlayerEntity partial surface inventory
- [ ] `PlayerAi.hpp` shadow-class seam を canonical PlayerEntity partial へ統合
- [ ] `PlayerPause.hpp` include + `MPHREAD_PLAYER_PAUSE_MEMBERS` 展開
- [ ] `PlayerProcess.hpp` include + `MPHREAD_PLAYER_PROCESS_MEMBERS` 展開
- [ ] `PlayerScan.hpp` include + `MPHREAD_PLAYER_SCAN_MEMBERS` 展開
- [ ] macro/header closure 後に PlayerAi diagnostics 再確認
- [ ] PlayerCamera
- [ ] PlayerDraw
- [ ] PlayerPause
- [ ] PlayerProcess
- [ ] PlayerScan
- [ ] PlayerSound
- [ ] EndScreen partial
- [ ] build

## Gate F

- [ ] `GameState.cpp` ManagedAt ambiguity
- [ ] `RoomEntity.cpp` Portal name/type collision
- [ ] build

## Gate G

- [ ] `EquipInfo` / Weapons canonical Native API
- [ ] PsychoBit
- [ ] GoreaArm
- [ ] FireSpawn
- [ ] other weapon consumers
- [ ] build

## Gate H

- [ ] remaining Scene/GameState accessor consumers
- [ ] Gorea cluster
- [ ] remaining enemies
- [ ] build

## Gate I

- [ ] Collision / CollisionDetection
- [ ] Effects
- [ ] Read
- [ ] Export
- [ ] Menu
- [ ] PauseMenu
- [ ] remaining leaf modules
- [ ] full build

---

# 6. 修正時の strict parity checklist

各 task は「build が通った」で完了しない。

必ず C# と比較する。

- [ ] public/internal-equivalent API
- [ ] enum underlying type
- [ ] default values
- [ ] constructor initialization order
- [ ] static initialization order
- [ ] property getter semantics
- [ ] property setter semantics
- [ ] null behavior
- [ ] reference identity
- [ ] collection ownership
- [ ] collection ordering
- [ ] exception type
- [ ] exception timing
- [ ] integer signedness
- [ ] numeric conversion
- [ ] overflow behavior
- [ ] RNG call count
- [ ] RNG call ordering
- [ ] side-effect ordering
- [ ] message ordering
- [ ] update ordering
- [ ] draw ordering
- [ ] cancellation check positions
- [ ] threading/task semantics
- [ ] boundary conditions

---

# 7. C# property → C++ accessor の共通ルール

今回の Windows/macOS error の大部分は同じ migration pattern。

C#:

```csharp
obj.Value
obj.Value = x
```

Native を:

```cpp
T Value() const;
void Value(T value);
```

と設計したなら call-site は:

```cpp
obj.Value()
obj.Value(x)
```

へ変換する必要がある。

ただし getter/setter が本当に canonical Native adaptation かは C# source と Native class definition を毎回確認する。

### やってはいけない例

ビルド error を消すためだけに:

```cpp
public:
    T Value;
```

を追加し、既存 getter/setter と二重状態にすること。

これは compile は通っても C# parity を壊す。

---

# 8. `shared_ptr` migration の共通ルール

Windows では以下が頻出。

```text
shared_ptr<T> を T* として扱う
shared_ptr<vector<T>> に .size() する
shared_ptr<vector<T>> に [] する
shared_ptr<T> に .Member する
```

C# reference semantics を C++ へ移すとき:

```cpp
ptr->Member
(*ptr)[i]
ptr->size()
ptr.get()
```

のどれが正しいかは用途次第。

**機械的 `.get()` 化は禁止。**

C# で:

- null が許されるか
- object identity を比較しているか
- lifetime を延長する必要があるか
- child が owner を持つか
- local borrow だけか

を確認して決定する。

---

# 9. 今回の2 runから除外してよい旧 blocker

前回の修正順に入っていた以下は、今回の two-run frontier では先頭 blockerではない。

- TagList ICU header
- TagList Android ICU availability
- old `MemoryClasses` entity enum visibility:
  - PlatformFlags
  - PlatStateFlags
  - PlatAnimFlags
  - PlatformState
  - SpawnerFlags
  - TriggerFlags
  - AiFlags2/3/4
  - BeamFlags
  - EquipFlags

これらを今回の新しい TODO の先頭へ戻さない。

---

# 10. 推奨 parallel worker 分割

| Worker | Scope | Dependency |
|---|---|---|
| A | RoomEntity cancellation | P0 後 |
| B | DoorEntity | P0 後、Aとファイル非競合 |
| C | WarWasp | P0 後 |
| D | PlayerEntity partial surface inventory | 独立調査可 |
| E | GameState ManagedAt | P0 後 |
| F | Weapons / EquipInfo API | 独立調査可 |
| G | Windows remaining error classification | 読み取り専用なら常時可 |

### 注意

`PlayerEntity.hpp` / Player partial headers を複数 worker が同時に書き換えない。

`GameState.hpp`、`Scene.hpp`、`Metadata.hpp` のような shared header も単一 owner にする。

---

# 10.1. 最新 ZIP 反映後の status matrix

| Priority | Item | ZIP status | 根拠 |
|---|---|---|---|
| P0 | MemoryClasses GameMode | 修正済み | `GameMode : std::uint8_t` forward declaration を確認 |
| P1 | RoomEntity cancellation | 未修正 | `std::stop_token` / `std::stop_source` が残存 |
| P2 | DoorEntity | 未修正 | Scene/GameState/Cheats の property-style call-site が残存 |
| P3 | WarWasp | 未修正 | `Fields.S01.WarWasp` と unqualified `Scale` が残存 |
| P4 | PlayerEntity partial closure | 未修正 | PlayerAi shadow class、Pause/Process/Scan macro 未展開 |
| P5+ | downstream | 未確定/未修正 | P1～P4 後の CI で再順位付け |

したがって、ZIPを加味しても immediate next action は:

```text
RoomEntity
→ DoorEntity
→ WarWasp
→ PlayerEntity partial closure
```

で変わらない。

ただし P4 は ZIP により root cause がより明確になったため、以前より「個別 Player file 修正」ではなく **partial-class materialization の修正を先に行う**べきと確定した。

---

# 11. 最終推奨順まとめ

```text
P0  MemoryClasses GameMode visibility
    └─ 現在 develop2 では修正済み

P1  RoomEntity cancellation portability
    └─ macOS + Android gate

P2  DoorEntity strict parity
    ├─ Scene accessor
    ├─ GameState accessor/setter
    ├─ Scale name lookup
    └─ Player door SFX timer surface

P3  00_WarWasp strict parity
    └─ current macOS で確認済み next frontier

P4  PlayerEntity partial API closure
    └─ Windows の最大 shared surface blocker

P5  GameState ManagedAt overload ambiguity

P6  RoomEntity implementation / Portal name lookup

P7  Weapons / EquipInfo shared API closure

P8  Scene/GameState accessor migration consumers

P9  Gorea + remaining Enemy cluster

P10 Collision / Effects / Read / Export / Menu / utility leaf closure

→ Full Desktop + Android build
→ exact C# semantic audit
→ runtime/regression validation
```

---

# 12. 最重要ルール

今回の Windows log に数千件 error があるからといって、数千個の独立 task に分解しない。

正しい進め方:

```text
shared root cause を1つ直す
↓
同一 SHA の全 CI を見る
↓
消えた error 群を確認する
↓
新しく最上流に出た blocker を1つ直す
↓
繰り返す
```

特に今回、

```text
MemoryClasses GameMode
```

を1行 forward-declare しただけで current macOS からその error 群が消え、

```text
RoomEntity
DoorEntity
WarWasp
```

まで frontier が前進している。

この方式が、C# strict parity を壊さずに Native 全体を compile-clean に近づける最短ルート。
