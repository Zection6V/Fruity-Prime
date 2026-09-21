# Fruity-Prime C# → C++20 Native Build 修正優先順位
## latest ZIP / PlayerAi frontier 再検証版

## 目的

C# を唯一の behavioral specification として C++20 Native 側を厳密再現する作業について、以前の Actions runs:

- Desktop: `35551999867`
- Android: `35551999865`

を起点に作成した修正順を、追加提供された以下の資料で再評価した。

- `Fruity-Prime-develop2 (4).zip`
- 添付テキストの Linux / macOS / Windows 診断
- GitHub 上で確認した current `develop2`
- current-head Native CI

この版では、既に直った項目を TODO から外し、**現在実際に全 platform を止めている compile frontier** を最優先にする。

---

# 1. source state

## 添付 ZIP

ZIP 内の source は、少なくとも以下の点で commit `7a2eb8e73cab98367055acbd079dd1ca0cd68778` の状態と一致する。

`Renderer.cpp`:

```cpp
#include "Music.hpp"
```

一方、GitHub current `develop2` はその1 commit 後:

```text
b7e3bb0ca52c1e28b67656839d51844b0f388a6d
Fix Renderer native Music include
```

であり、唯一の差分は:

```diff
-#include "Music.hpp"
+#include "Sound/Music.hpp"
```

だった。

したがって本書では:

- ZIP / 添付診断の基準: `7a2eb8e...`
- current-head 補正: `b7e3bb0...`

として扱う。

---

# 2. current CI frontier

current head `b7e3bb0...` で確認した run:

| Platform | Run | 状態 | 現在の先頭 blocker |
|---|---:|---|---|
| Linux / GCC | `35563011763` | failure | `PlayerAi.hpp` |
| macOS / Clang | `35563011782` | failure | `PlayerAi.hpp` |
| Android | `35563011856` | failure | `PlayerAi.hpp` |
| Windows / MSVC | `35563011897` | build 中に `PlayerAi.hpp` errors を確認 | `PlayerAi.hpp` |

Linux / macOS / Android / Windows の4 platform すべてで、現在の最初の実 source blocker は `PlayerAi.hpp`。

したがって以前の修正順から優先順位を大きく変更する。

---

# 3. 前回版から「解決済み」に移動する項目

## MemoryClasses / GameMode visibility

既に:

```cpp
enum class GameMode : std::uint8_t;
```

が追加済み。

旧 P0 は完了扱い。

---

## RoomEntity cancellation portability

前回は:

```cpp
std::stop_token
std::stop_source
```

が macOS / Android で失敗していた。

最新 ZIP では:

```cpp
void ProcessTransition(std::shared_ptr<const std::atomic_bool> token);
```

へ変更済み。

したがって旧 P1 は現在の blocker ではない。

---

## DoorEntity property → accessor

以前問題だった:

```cpp
GameState::Mode
_scene->Room
_scene->RoomId
_scene->AreaId
_scene->FrameCount
Cheats::UnlockAllDoors
```

は最新 ZIP では:

```cpp
GameState::Mode()
_scene->Room()
_scene->RoomId()
_scene->AreaId()
_scene->FrameCount()
Cheats::UnlockAllDoors()
```

へ修正済み。

`Scale` helper collision も `CreateScale(...)` 等へ整理済み。

旧 P2 は解決済み。

---

## 00_WarWasp

以前問題だった:

```cpp
Fields.S01.WarWasp
```

は:

```cpp
Fields.S01().WarWasp
```

へ修正済み。

`Position` の `VectorProperty` も必要な場所で:

```cpp
static_cast<Vector3>(Position)
```

へ変換されている。

helper `Scale` も namespace qualification が入っている。

旧 P3 は解決済み。

---

## PlayerEntity partial macro closure

前回版では `PlayerPause` / `PlayerProcess` / `PlayerScan` 等の macro 展開漏れを疑っていたが、最新 ZIP では canonical `PlayerEntity` 内に:

```cpp
MPHREAD_PLAYER_AI_MEMBERS
MPHREAD_PLAYER_ENTITY_ICON_BOUNDS_MEMBERS
MPHREAD_PLAYER_CAMERA_MEMBERS
MPHREAD_PLAYER_COLLISION_MEMBERS
MPHREAD_PLAYER_DIALOG_MEMBERS
MPHREAD_PLAYER_DRAW_MEMBERS
MPHREAD_PLAYER_HUD_MEMBERS
MPHREAD_PLAYER_INPUT_MEMBERS
MPHREAD_PLAYER_PAUSE_MEMBERS
MPHREAD_PLAYER_PROCESS_MEMBERS
MPHREAD_PLAYER_SCAN_MEMBERS
MPHREAD_PLAYER_SOUND_MEMBERS
...
```

が展開済み。

したがって「macro が PlayerEntity に入っていない」という旧診断は撤回する。

現在の問題は **partial materialization そのものではなく、`PlayerAi.hpp` の implementation body が古い C# style API を大量に残していること**。

---

# 4. 新しい推奨修正順

# P0. `PlayerAi.hpp` strict C# → C++20 parity repair

## 対象

C# sole specification:

```text
src/MphRead/Entities/Players/PlayerAi.cs
```

Native:

```text
src/MphRead.Native/Entities/Players/PlayerAi.hpp
src/MphRead.Native/Entities/Players/PlayerAi.cpp
src/MphRead.Native/Entities/Players/PlayerEntity.hpp
```

## なぜ P0 か

current Linux / macOS / Android / Windows の全 platform で最初に失敗している。

代表 current errors:

```text
PlayerAi.hpp:659
Vector3 has no member WithY

PlayerAi.hpp:671
VectorProperty - VectorProperty has no operator-

PlayerAi.hpp:684
Vector3 has no member AddY

PlayerAi.hpp:703
NodeData3 incomplete type

PlayerAi.hpp:708
CameraInfo member function must be called

PlayerAi.hpp:726
shared_ptr<const WeaponList> cannot be indexed directly

PlayerAi.hpp:759
LinkedListIteratorSpecialized<ItemSpawnEntity> has no begin/end

PlayerAi.hpp:786
Rng has not been declared

PlayerAi.hpp:838
BotLevel member function used as a field

PlayerAi.hpp:1009
GameState::PrimeHunter and SlotIndex used as fields
```

Linux は `PlayerAi.hpp` だけで 1000 件超の unique diagnostics を生成しているが、独立した1000 defect ではない。

大半は数種類の移植パターンの反復。

---

## P0-A. complete type / declaration visibility を先に閉じる

現在 `PlayerAi.hpp` 上部は以下を forward declaration しているだけ。

```cpp
class Scene;

class NodeData;
class NodeData3;

class ItemSpawnEntity;
class ItemInstanceEntity;
class OctolithFlagEntity;
class FlagBaseEntity;
class NodeDefenseEntity;
class DoorEntity;
class CamSeqEntity;
class ForceFieldEntity;
class BombEntity;
class BeamProjectileEntity;
class JumpPadEntity;
```

しかし inline implementation 内で member access している。

forward declaration のままでは:

```cpp
node->Position
node->Field4
node->NodeType
item->Item
flag->Carrier
defense->Position
```

等は不可能。

### actual defining headers

ZIP で実在を確認した path:

```text
../../Scene.hpp
../../GameState.hpp
../../Formats/NodeData.hpp
../../Utility/Rng.hpp

../ItemSpawnEntity.hpp
../ItemInstanceEntity.hpp
../OctolithFlagEntity.hpp
../FlagBaseEntity.hpp
../NodeDefenseEntity.hpp
../DoorEntity.hpp
../ForceFieldEntity.hpp
../BombEntity.hpp
../BeamProjectileEntity.hpp
../JumpPadEntity.hpp

../CamSeq/CamSeqEntity.hpp
HalfturretEntity.hpp
```

必要なものだけ、`PlayerEntity.hpp` の canonical definition が完成した後の implementation section に追加する。

### 添付テキストからの修正点

添付案の:

```cpp
#include "../Scene.hpp"
```

は `Entities/Players/PlayerAi.hpp` からは path が1階層不足。

正しくは:

```cpp
#include "../../Scene.hpp"
```

また:

```text
Formats/NodeData3.hpp
```

という別ファイルはない。

`NodeData` と `NodeData3` は両方:

```text
src/MphRead.Native/Formats/NodeData.hpp
```

に定義されている。

RNG も:

```text
Random.hpp
```

ではなく実在 header は:

```text
src/MphRead.Native/Utility/Rng.hpp
```

なので:

```cpp
#include "../../Utility/Rng.hpp"
```

が候補。

### 原則

include を増やす前に cyclic dependency を確認する。

「compile error が出た entity header を全部追加」ではなく、inline implementation が complete type を要求するものだけ追加する。

---

## P0-B. C# property → Native method mapping

最新 Native `PlayerEntity.hpp` では、例えば:

```cpp
BotLevel()
CurrentWeapon()
SlotIndex()
IsBot()
AvailableWeapons()
Values()
CameraInfo()
IsAltForm()
Hunter()
Flags1()
TeamIndex()
Players()
```

等は method/accessor。

しかし `PlayerAi.hpp` ではまだ:

```cpp
_player->BotLevel
_player->CurrentWeapon
_player->SlotIndex
_player->IsBot
_player->AvailableWeapons[beam]
_player->Values.AltColYPos
_player->CameraInfo.Facing
_player->IsAltForm
_player->Hunter
_player->Flags1.TestFlag(...)
_player->TeamIndex
PlayerEntity::Players.at(...)
```

という C# property syntax が大量に残っている。

### 修正原則

Native declaration に従い:

```cpp
_player->BotLevel()
_player->CurrentWeapon()
_player->SlotIndex()
_player->IsBot()
_player->AvailableWeapons()[...]
_player->Values().AltColYPos
_player->IsAltForm()
_player->Hunter()
_player->Flags1()
_player->TeamIndex()
PlayerEntity::Players()
```

へ変換する。

ただし一括 regex 置換はしない。

同名 field が実在する箇所もあるため、各 declaration を照合する。

---

## P0-C. EntityBase::VectorProperty を Vector3 として直接演算しない

`EntityBase.hpp` の:

```cpp
class VectorProperty
{
public:
    operator ::OpenTK::Mathematics::Vector3() const noexcept;
    ...
};

VectorProperty Position;
```

が canonical Native adaptation。

従って:

```cpp
_targetPlayer->Position - _player->Position
```

のように wrapper 同士へ直接 binary operator をかけるのではなく、値へ明示変換する。

例えば:

```cpp
const Vector3 targetPos
    = static_cast<Vector3>(_targetPlayer->Position);
const Vector3 playerPos
    = static_cast<Vector3>(_player->Position);

Vector3 between = targetPos - playerPos;
```

### 重要

添付テキストにある候補:

```cpp
Position()
```

はこの repository の `EntityBase::Position` には当てはまらない。

`Position` は accessor method ではなく `VectorProperty`。

現 repository では `static_cast<Vector3>(Position)` が既存 Native code でも使われている。

---

## P0-D. OpenTK Vector3 の C# extension-style API を Native helper 化

Native `Formats/Types.hpp` の `OpenTK::Mathematics::Vector3` は:

```cpp
Normalized()
Cross(...)
Dot(...)
Distance(...)
```

等しか持たず、以下は存在しない。

```text
WithY
AddY
AddZ
LengthSquared property
Length property
operator==
operator!=
DistanceSquared
```

`PlayerAi.hpp` にはこれら C# style usage が大量に残存。

### 推奨

`PlayerAi` 専用の file-local / class-private helper として必要最小限を定義する。

概念的には:

```cpp
Vector3 WithY(Vector3 value, float y)
Vector3 AddY(Vector3 value, float y)
Vector3 AddZ(Vector3 value, float z)
float LengthSquared(Vector3 value)
bool Equal(Vector3 left, Vector3 right)
```

### `DistanceSquared`

Native core `Vector3` には `DistanceSquared` がない。

計算順を崩さない形で:

```cpp
LengthSquared(left - right)
```

へ変換する。

### 注意

`Formats/Types.hpp` の `Vector3` 全体へ安易に大量の convenience method を追加すると、他 module の observable numeric behavior や overload resolution に波及する。

まず `PlayerAi` 側の限定 helper を推奨。

---

## P0-E. `CameraInfo` / `shared_ptr` adaptation

Native:

```cpp
std::shared_ptr<CameraInfo> CameraInfo() const;
```

なら:

```cpp
_player->CameraInfo.Facing
```

ではなく:

```cpp
_player->CameraInfo()->Facing
```

等、実型に合わせて pointer dereference が必要。

同様に `Weapons::Current` は:

```cpp
extern std::shared_ptr<const WeaponList> Current;
```

`WeaponList` は:

```cpp
using WeaponList = std::vector<std::shared_ptr<WeaponInfo>>;
```

なので:

```cpp
Weapons::Current[index]
```

は無効。

必要な semantic は:

```cpp
(*Weapons::Current)[index]
```

で element `shared_ptr<WeaponInfo>` を取得し、その後 C# の reference semantics に対応して dereference する。

---

## P0-F. WeaponFlags は member `.TestFlag()` ではない

`WeaponFlags` は enum class。

従って:

```cpp
info.Flags.TestFlag(WeaponFlags::CanCharge)
```

という C# extension style は無効。

repository 内の canonical enum-bit helper / operator を用いる。

例えば equivalent が既に定義されているなら:

```cpp
(info.Flags & WeaponFlags::CanCharge) != WeaponFlags::None
```

等。

既存 `TypeExtensions::TestFlag(...)` が当該型に対応しているなら、その既存 helper を優先する。

---

## P0-G. Scene entity collection を range-for に変更しない

これは添付テキスト中でも提案が揺れている箇所。

現在 Native `Scene`:

```cpp
LinkedListIteratorSpecialized<T> Get...Entities() const;
```

`LinkedListIteratorSpecialized<T>` は:

```cpp
GetEnumerator()
```

を持ち、その enumerator が:

```cpp
MoveNext()
Current()
```

を持つ。

一方 `begin()` / `end()` は持たない。

従って:

```cpp
for (const auto& entity : _scene.GetItemSpawnEntities())
```

は失敗する。

### 推奨

既存 Native code と同じ:

```cpp
auto enumerator = _scene.GetItemSpawnEntities().GetEnumerator();
while (enumerator.MoveNext())
{
    auto entity = enumerator.Current();
    ...
}
```

形式へ変換する。

### 不採用

以下はこの work item のためだけには行わない。

- `Scene::Get...Entities()` を `vector` return に変更
- `LinkedListIteratorSpecialized` へ場当たり的 `begin/end` を追加
- Scene 全体の collection API を再設計

C# `Scene.cs` 自体も `LinkedListIteratorSpecialized<T>` を返しているため、enumerator protocol を維持する方が parity に近い。

---

## P0-H. GameState property-style access

Native `GameState.hpp`:

```cpp
GameMode Mode()
bool SinglePlayer()
IntSlots& EncounterState()
std::int32_t PrimeHunter()
bool RadarPlayers()
```

等。

`PlayerAi.hpp` に残る:

```cpp
GameState::Mode
GameState::SinglePlayer
GameState::EncounterState
GameState::PrimeHunter
GameState::RadarPlayers
```

は:

```cpp
GameState::Mode()
GameState::SinglePlayer()
GameState::EncounterState()
GameState::PrimeHunter()
GameState::RadarPlayers()
```

へ変換する。

setter が必要な property は対応する setter function を使用。

---

## P0-I. PlayerAi の修正順

一度に巨大 diff にせず、以下の順で compile frontier を前進させる。

1. complete-type include / RNG visibility
2. `WithY/AddY/AddZ/LengthSquared/DistanceSquared/equality` helper
3. `VectorProperty` → `Vector3` extraction
4. `PlayerEntity` property → accessor
5. `GameState` property → accessor
6. `CameraInfo` / `shared_ptr` dereference
7. `Weapons::Current` / `WeaponFlags`
8. Scene iteratorを `GetEnumerator()` protocol に修正
9. incomplete entity type member access
10. rebuildして次の first-error line から継続

C# の関数順・RNG call順・branch順を変更しない。

---

# P1. `SceneSetup.hpp` enum underlying type consistency

## 現状

`Formats/Formats.hpp`:

```cpp
enum class GameMode : std::uint8_t
enum class BossFlags : std::int32_t
```

C#:

```csharp
public enum GameMode : byte
public enum BossFlags
```

C# default enum underlying type は `int` なので:

```text
GameMode  = uint8_t
BossFlags = int32_t
```

が parity。

しかし最新 ZIP の `SceneSetup.hpp` は:

```cpp
enum class BossFlags : std::uint32_t;
enum class GameMode : std::int32_t;
```

で両方不一致。

`GameState.hpp` は:

```cpp
enum class BossFlags : std::int32_t;
enum class GameMode : std::uint8_t;
```

で正しい。

## 修正

`SceneSetup.hpp` の forward declaration を canonical definition と一致させる。

```cpp
enum class BossFlags : std::int32_t;
enum class GameMode : std::uint8_t;
```

または include graph を確認した上で duplicate forward declaration 自体を除去。

## Priority

**P1**

current all-platform first gate は PlayerAi なので P0 の後。

ただし shared-header integrity 問題なので、PlayerAi worker と非競合なら並行修正可能。

---

# P2. `BeamProjectileArray` canonical type identity

## 現状

`SceneSetup.hpp`:

```cpp
class BeamProjectileArray final
{
public:
    std::int32_t Length() const noexcept;
    std::shared_ptr<BeamProjectileEntity>& operator[](std::int32_t index);
    ...
};
```

一方 `Metadata/Metadata.hpp`:

```cpp
using BeamProjectileArray
    = std::vector<std::shared_ptr<Entities::BeamProjectileEntity>>;
```

同 namespace で同名を class と alias の2種類として定義している。

これは canonical type identity が壊れている。

## C# sole specification

C#:

```csharp
public BeamProjectileEntity[] Beams { get; set; }
```

`SceneSetup.CreateBeamList`:

```csharp
var beams = new BeamProjectileEntity[size];
for (int i = 0; i < size; i++)
{
    beams[i] = new BeamProjectileEntity(scene);
}
```

C# array は fixed-length かつ index bounds semantics を持つ。

Native `SceneSetup.hpp` の class wrapper は:

```cpp
const std::int32_t _length;
unique_ptr<shared_ptr<BeamProjectileEntity>[]>
CheckIndex(...)
Length()
operator[]
```

を持っており、単なる `std::vector` alias より C# array semantics を意図している。

## 推奨

**class `BeamProjectileArray` を canonical adaptation として残し、Metadata 側の vector alias を統合する方向を優先。**

ただし最終決定は:

- `EquipInfo`
- `SceneSetup::CreateBeamList`
- `BeamProjectileEntity::Spawn`
- `AreaVolumeEntity`
- `PlayerEntity`
- enemy consumers

をまとめて audit して決める。

### 禁止

その場で:

```cpp
using BeamProjectileArray = vector<...>;
```

へ全面統一し、C# fixed-array semantics / bounds behavior を失わない。

## Priority

**P2**

---

# P3. stale include paths

## Renderer.cpp

ZIP:

```cpp
#include "Music.hpp"
```

だが `Music.hpp` は:

```text
src/MphRead.Native/Sound/Music.hpp
```

current head `b7e3bb0...` で:

```cpp
#include "Sound/Music.hpp"
```

へ修正済み。

### 状態

**resolved on current develop2**

---

## Sound/Music.cpp

最新 ZIP:

```cpp
#include "../Paths.hpp"
```

しかし:

```text
src/MphRead.Native/Paths.hpp
```

は存在しない。

`Paths` の current definition は:

```text
src/MphRead.Native/Formats/Formats.hpp
```

内の:

```cpp
class Paths final
```

。

## 推奨

`Music.cpp` が本当に必要とする declaration を提供する actual header へ修正する。

現 source layout では候補は:

```cpp
#include "../Formats/Formats.hpp"
```

ただし include-cost と循環を確認する。

`Paths` を今後独立 header へ切り出すなら、その変更は別の deliberate refactor として行う。

## Priority

**P3**

hard missing-header なので PlayerAi / shared type blockers 後すぐ。

---

# P4. portable file-time conversion

添付診断は `ThumbnailLog.cpp` の:

```cpp
std::filesystem::file_time_type::clock::to_sys(value)
```

を指摘。

最新 ZIP を追加検索すると:

```text
src/MphRead.Native/Mods/ThumbnailLog.cpp
src/MphRead.Native/Mods/LogShare.cpp
```

の2箇所に `file_clock::to_sys` / `clock::to_sys` usage がある。

## 推奨

片方だけ修正せず、同一 portable helper に寄せる。

概念:

```cpp
system_clock::now()
    + (fileTime - file_time_type::clock::now())
```

等。

ただし rounding / duration cast / timestamp formatting の C# semantics を比較する。

C# source で `LastWriteTimeUtc` / `DateTimeOffset` 等のどの semantics を再現しているかを確認してから固定する。

## Priority

**P4**

Windows portability blocker。

---

# P5. Enemy include-path blockers

current Windows log で PlayerAi の error-limit 後に既に見えている。

## 24_Gorea1A.cpp

```text
Cannot open include file:
../HalfturretEntity.hpp
```

actual file:

```text
Entities/Players/HalfturretEntity.hpp
```

Enemy file:

```text
Entities/Enemies/24_Gorea1A.cpp
```

から actual relative path を再計算する。

---

## 30_Trocra.cpp / 31_Gorea2.cpp

```text
Cannot open include file:
../../CollisionDetection.hpp
```

actual `CollisionDetection` locationを source tree から確認して修正。

include-path error は behavioral logic に触れず修正できるが、**推測 path ではなく actual defining path を使う**。

## Priority

**P5**

---

# P6. Gorea / EquipInfo shared API cluster

current Windows で確認済み。

例:

```text
25_GoreaHead.cpp:
shared_ptr<Node> → Node*

26_GoreaArm.cpp:
ManagedListAt no matching overload
EquipInfo::SetWeapon missing
EquipInfo::SetBeams missing
EquipInfo::SetGetAmmo missing
EquipInfo::SetSetAmmo missing
```

これは以前からの:

```text
EquipInfo / Weapons ownership model
shared_ptr adaptation
property → setter API
```

問題。

P2 の `BeamProjectileArray` identity を先に確定してから修正する。

## Priority

**P6**

---

# P7. remaining shared API / C# BCL adaptation

PlayerAi と include/type identity を通した後に再分類する。

候補:

- `GameState.cpp` ManagedAt overload ambiguity
- Collision / CollisionDetection complete-type errors
- Effects / ArrayPool adaptation
- Export / Paths adaptation
- Scripting / System::Version adaptation
- Menu / Sound namespace/property mapping
- remaining Scene / GameState property-style consumers
- remaining Enemy files

ここは旧ログの raw count だけで先回り修正しない。

current CI の first errors を再取得して順番を決める。

---

# 5. 添付テキストの診断: 採否

| 指摘 | 判定 | 理由 |
|---|---|---|
| PlayerAi incomplete types | 採用 | current全platformログと一致 |
| property-style API mismatch | 採用 | source / current CI と一致 |
| `WithY` / `AddY` 不在 | 採用 | Native Vector3 declaration と一致 |
| `DistanceSquared` 不在 | 採用 | Native Vector3 declaration と一致 |
| `Rng` declaration 不可視 | 採用 | actual `Utility/Rng.hpp` を確認 |
| `Weapons::Current` shared_ptr indexing | 採用 | Metadata.hpp declaration と一致 |
| `WeaponFlags.TestFlag` | 採用 | enum class surface と不一致 |
| `../Scene.hpp` include | **path修正して採用** | actual path は `../../Scene.hpp` |
| `NodeData3.hpp` | **修正して採用** | actual definition は `Formats/NodeData.hpp` |
| `Random.hpp` | **修正して採用** | actual RNG header は `Utility/Rng.hpp` |
| Scene getter を vector return に変更 | **不採用** | C# / Native とも custom iterator設計 |
| iterator に即 `begin/end` を追加 | 原則不採用 | ownership/sentinel semantics変更を避ける |
| existing `GetEnumerator()` を使う | 採用 | repository canonical pattern |
| enum forward declaration mismatch | 採用 | ZIP sourceとC# underlying typeで確認 |
| BeamProjectileArray duplicate | 採用 | class vs alias を実際に確認 |
| Renderer `Music.hpp` path | current headでは解決済み | `b7e3bb0...` |
| Music `../Paths.hpp` | 採用 | actual Paths.hppなし |
| `file_clock::to_sys` | 採用 | 2ファイルに残存 |

---

# 6. revised worker split

## Worker A — PlayerAi only

対象:

```text
PlayerAi.cs
PlayerAi.hpp
PlayerAi.cpp
```

必要な declaration を読むだけなら:

```text
PlayerEntity.hpp
EntityBase.hpp
Scene.hpp
GameState.hpp
Formats/NodeData.hpp
Metadata/Metadata.hpp
Utility/Rng.hpp
関連 entity headers
```

### 禁止

- Scene collection API の redesign
- unrelated Player modules の書き換え
- GameState implementation の変更
- Metadata ownership model の変更

PlayerAi call-site adaptation に集中。

---

## Worker B — SceneSetup shared declarations

対象:

```text
SceneSetup.hpp
SceneSetup.cpp
Formats/Formats.cs
Formats/Formats.hpp
```

作業:

- BossFlags underlying type
- GameMode underlying type

BeamProjectileArray は Worker C と重なるため触らない。

---

## Worker C — BeamProjectileArray type identity

対象:

```text
SceneSetup.hpp/.cpp
Metadata/Metadata.hpp/.cpp
Metadata/Weapons.*
C# Weapons.cs
C# SceneSetup.cs
```

class vs alias の canonicalization 専任。

---

## Worker D — include portability

対象:

```text
Sound/Music.cpp
Gorea/Trocra include errors
```

Renderer は current headで修正済みのため再編集しない。

---

## Worker E — file time portability

対象:

```text
Mods/ThumbnailLog.cpp
Mods/LogShare.cpp
対応 C# source
```

---

# 7. mandatory rebuild points

## Rebuild 1

`PlayerAi` complete-type + early API conversion後。

目的:

- 全 platform の `PlayerAi.hpp:659` frontier が何行まで進むか確認
- 巨大一括修正の前に mapping が正しいか確認

---

## Rebuild 2

PlayerAi full parity pass 後。

必ず:

- Linux
- macOS
- Windows
- Android arm64
- Android x86_64

を見る。

---

## Rebuild 3

SceneSetup enum + BeamProjectileArray identity 後。

shared header change のため全 platform mandatory。

---

## Rebuild 4

include/file-time portability 後。

その時点の first-error set を新しい優先順位にする。

---

# 8. current recommended order

```text
P0  PlayerAi.hpp strict parity repair
    ├─ complete types
    ├─ Native method/accessor mapping
    ├─ VectorProperty extraction
    ├─ Vector3 helper adaptation
    ├─ shared_ptr / Weapons
    ├─ Rng
    ├─ GameState accessors
    └─ GetEnumerator traversal

P1  SceneSetup enum underlying type consistency
    ├─ BossFlags = int32_t
    └─ GameMode  = uint8_t

P2  BeamProjectileArray canonical type identity
    └─ C# fixed BeamProjectileEntity[] semantics を保持

P3  stale include paths
    ├─ Renderer Music include = current headで解決済み
    └─ Sound/Music.cpp Paths include

P4  portable file-time conversion
    ├─ ThumbnailLog.cpp
    └─ LogShare.cpp

P5  Enemy hard include-path blockers
    ├─ 24_Gorea1A
    ├─ 30_Trocra
    └─ 31_Gorea2

P6  Gorea / EquipInfo / Weapons shared API cluster

P7  current CIで新たに露出した remaining shared blockers
```

---

# 9. strict parity acceptance checklist

各修正は compile success だけでは完了しない。

- [ ] C# public/internal-equivalent API
- [ ] field/property/method classification
- [ ] static / instance classification
- [ ] enum underlying type
- [ ] defaults
- [ ] initialization order
- [ ] nullability
- [ ] reference identity
- [ ] shared ownership / borrowed reference
- [ ] fixed array vs dynamic collection semantics
- [ ] bounds behavior
- [ ] exception type
- [ ] exception timing
- [ ] integer signedness
- [ ] unchecked conversion
- [ ] float calculation order
- [ ] RNG call count
- [ ] RNG call order
- [ ] branch order
- [ ] side-effect order
- [ ] iterator/enumerator order
- [ ] cancellation timing
- [ ] update/draw order

---

# 10. 最重要ポイント

前回版で先頭だった:

```text
MemoryClasses
RoomEntity cancellation
DoorEntity
WarWasp
PlayerEntity macro closure
```

は現在の source state では既に前進済み。

今は:

```text
PlayerAi.hpp
```

が明確な all-platform compile gate。

また、添付テキストの方向性は概ね正しいが、そのままコード指示に使うと:

```text
../Scene.hpp
NodeData3.hpp
Random.hpp
Scene getterをvector化
```

のように current repository と合わない箇所がある。

したがって今後は:

```text
C# sole specification
+
current Native declarations
+
current CI first errors
```

の3点を毎回照合し、**古い C# syntax を current Native adaptation へ機械的ではなく型ごとに変換する**。

これが現在の最短修正経路。
