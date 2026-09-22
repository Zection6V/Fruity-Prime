# Fruity-Prime C# → C++20 Native Build 修正優先順位
## 2026-09-22 19:05 JST 最新 GitHub / CI / Windowsログ検証版

## 0. 結論

current `develop2` HEAD:

```text
5ae6f1c374882685962e1dbe8afbb34608611b5f
Fix NetScoreboard Native parity
```

前回の `PlayerDialog / PlayerCollision / include scan` は通過済み。

最新 completed exact-SHA CI と current HEAD の差分を照合した結果、現在の推奨順は:

```text
P0  PointModuleEntity.cpp
    ↓
P1  RoomEntity.cpp
    ↓
    exact SHA full Native CI
    ↓
P2  Testing exception canonicalization
P3  Extract Version adaptation
P4  RepackCollision Read qualification
P5+ 新しい cross-platform first-error
```

今回提示された Windows の `TestMisc / TestParse / Extract / RepackCollision` エラーは実在する後段 blockerだが、current global first frontier は `PointModuleEntity`、その直後が `RoomEntity`。

---

# 1. current repository state

```text
Repository: Zection6V/Fruity-Prime
Branch: develop2
HEAD: 5ae6f1c374882685962e1dbe8afbb34608611b5f
Tree: cc0cd7df10d59f6f9b8703fb81ec1eedb1fda7cf
Parent: 7f90c7907d4be857700a6fa9fd8ed80ff8f84884
Commit: Fix NetScoreboard Native parity
```

current HEAD CI は観測時点で実行中。

frontier判定は直前 completed SHA:

```text
aff4aff2bfd2dbbbb8a7ce71985e0aa0c3c0a24a
```

の terminal CIを基準とする。

その後の2 commitは:

```text
7f90c790  src/MphRead.Native/Mods/InputSettings.cpp
5ae6f1c3  src/MphRead.Native/Mods/Network/NetScoreboard.cpp
```

のみで、`PointModuleEntity / RoomEntity / Testing / Utility` は未変更。

---

# 2. latest completed CI frontier

## Linux / GCC

Run:

```text
35712843975
```

first blocker:

```text
src/MphRead.Native/Entities/PointModuleEntity.cpp
```

errors:

```text
line 37  invalid use of incomplete type Scene
line 41  invalid use of incomplete type Scene
```

---

## macOS / Clang

Run:

```text
35712843950
```

first:

```text
PointModuleEntity.cpp
```

その次:

```text
RoomEntity.cpp
```

---

## Android

Run:

```text
35712844001
```

arm64 first:

```text
PointModuleEntity.cpp
```

x86_64:

```text
PointModuleEntity.cpp
RoomEntity.cpp
```

---

## Windows / MSVC

Run:

```text
35712843968
```

terminal failure。

first:

```text
PointModuleEntity.cpp
```

next major blocker:

```text
RoomEntity.cpp
```

その後に:

```text
TeleporterEntity
GameState
Renderer
Collision
Effects
Export
Testing
Utility
...
```

が露出。

---

# 3. 前回版から完了済みに移す項目

代表 commit:

```text
fa645d33  Fix PlayerDialog Paths include
3d4770de  Fix PlayerDialog Strings include
0615ec7e  Fix PlayerDialog native API call sites
e8ffcf29  Fix PlayerCollision native build parity
8f152606  Fix remaining Native quoted include paths
a843fdea  Fix PlayerDraw Native parity build frontier
dba3157d  Fix PlayerHud Native parity build frontier
34707d69  Fix remaining PlayerEntity Native parity build errors
bc687d04  Fix PlayerInput Native parity build errors
670d4961  Fix PlayerPause strict parity build errors
ee902da8  Fix PlayerScan native accessor parity
f0504f09  Fix PlayerProcess Native parity frontier
045c44d2  Fix PlayerProcess Room metadata accessor parity
f832df25  Fix PlayerSound Native parity
d6016fbf  Complete PlayerSound Scene accessor parity
aff4aff2  Fix GameSettings Paths parity
7f90c790  Fix InputSettings controls reader construction
5ae6f1c3  Fix NetScoreboard Native parity
```

---

# 4. include mechanical scan status

以前の mechanical scan:

```text
16 unresolved quoted includes
9 affected files
```

は current HEAD で対象9ファイルを再確認し、すべて修正済み。

例:

```text
Mods/InputSettings.cpp
SmoothHudIcon.hpp
NetCheckClient.cpp
MapAudit.hpp/.cpp
ChatBox.cpp
PlayerHud.cpp
PlayerDialog.cpp
PlayerInput.cpp
```

従って:

```text
include-only preflight = COMPLETE
```

---

# 5. P0 — PointModuleEntity

authority:

```text
src/MphRead/Entities/PointModuleEntity.cs
```

Native:

```text
src/MphRead.Native/Entities/PointModuleEntity.hpp
src/MphRead.Native/Entities/PointModuleEntity.cpp
```

## P0-A Scene complete type

current cppは:

```cpp
#include "PointModuleEntity.hpp"
```

だけで、`Scene` は forward declarationのまま。

一方:

```cpp
_scene->TryGetEntity(...)
```

を呼ぶため全 platformで incomplete-type error。

### 修正方向

implementation側に:

```cpp
#include "../Scene.hpp"
```

を追加。

---

## P0-B TryGetEntity shared_ptr out

current `Scene.hpp`:

```cpp
bool TryGetEntity(
    std::int32_t id,
    std::shared_ptr<Entities::EntityBase>& entity) const;
```

current `PointModuleEntity.cpp`:

```cpp
EntityBase* entity = nullptr;
_scene->TryGetEntity(..., entity)
```

なので complete typeを入れると次に ownership mismatchが露出する可能性が高い。

C#:

```csharp
_scene.TryGetEntity(..., out EntityBase? entity)
Next = (PointModuleEntity)entity;
```

Nativeでは:

```cpp
std::shared_ptr<EntityBase> entity{};
```

で outを受け、C# cast behaviorを保って `_next/_prev` の non-owning raw pointerへ identityを移す。

### 禁止

- `Scene::TryGetEntity` に raw-pointer overloadを追加
- failed castを silent null化
- PointModule側で新しい ownershipを持つ

---

## P0-C hidden follow-up

`SetActive()` の:

```cpp
_models[0].Active = Active;
```

も current `_models` surfaceと再照合する。

Scene errorに隠れている可能性があるため、P0 compile後に確認。

---

# 6. P0 acceptance

- [ ] Linux diagnostics 0
- [ ] macOS diagnostics 0
- [ ] Windows diagnostics 0
- [ ] Android arm64 diagnostics 0
- [ ] Android x86_64 diagnostics 0
- [ ] Next/Prev identity一致
- [ ] invalid cast behavior一致
- [ ] TryGetEntity false時 state不変
- [ ] chain traversal最大5件・順序一致

---

# 7. P1 — RoomEntity

macOS / Android x86_64 / Windows で PointModuleの次に既に露出。

100件超の diagnostics があるが、主に repeated translation patterns。

RoomEntityは局所patchではなく:

```text
RoomEntity.cs
→ RoomEntity.hpp/.cpp
```

の file-slice strict auditとして扱う。

---

# 8. P1-A Entities enumerator protocol

current:

```cpp
auto enumerator = scene.Entities();
while (enumerator.MoveNext())
{
    auto entity = enumerator.Current();
}
```

`Scene::Entities()` は `LinkedListIterator<EntityBase>` を返し、`MoveNext/Current` は enumerator側。

修正:

```cpp
auto enumerator = scene.Entities().GetEnumerator();
```

---

# 9. P1-B Portal type/member collision

`PortalNodeRef` は member:

```cpp
const std::shared_ptr<Formats::Collision::Portal> Portal;
```

を持つ。

constructor definitionで:

```cpp
std::shared_ptr<Portal>
```

と書くと nested scopeの member `Portal` と衝突。

修正方向:

```cpp
std::shared_ptr<MphRead::Formats::Collision::Portal>
```

と fully qualify。

---

# 10. P1-C NodeRef type/member collision

RoomEntity内の unqualified:

```cpp
NodeRef(...)
```

が EntityBase由来 `NodeRef` memberと衝突。

current error:

```text
NodeRef does not provide a call operator
```

type constructionは:

```cpp
MphRead::Formats::Culling::NodeRef(...)
```

等へ明示。

---

# 11. P1-D Scene / GameState accessor closure

current RoomEntityにはまだ:

```cpp
GameState::TransitionState
GameState::TransitionRoomId
GameState::PausePrevented
GameState::EscapeTimer
GameState::Mode

scene.RoomId
scene.AreaId
```

等の C# property syntaxが残る。

canonical Native getter/setterへ変換。

例:

```cpp
GameState::TransitionState()
GameState::TransitionState(value)

GameState::TransitionRoomId()
GameState::TransitionRoomId(value)

scene.RoomId()
scene.RoomId(value)
scene.AreaId()
```

generic compatibility shimは追加しない。

---

# 12. P1-E Player shared_ptr ownership

current:

```cpp
PlayerEntity* player = PlayerEntity::Main();
```

canonical:

```cpp
static std::shared_ptr<PlayerEntity> Main();
static std::shared_ptr<PlayerEntity> Create(...);
```

さらに:

```cpp
scene.InsertEntity(...)
scene.InitEntity(...)
```

も shared_ptrを要求。

Room load/transitionの間は shared_ptrを保持し、raw pointerはidentity比較が必要な箇所だけに限定する。

---

# 13. P1-F NetRoomChange / AiPersonality visibility

actual files:

```text
Mods/Network/NetRoomChange.hpp
Formats/AiPersonality.hpp
```

current RoomEntityには declaration visibility不足がある。

current errors:

```text
NetRoomChange not found
AiPersonality not found
```

actual owner headerを implementation側へ追加し、shimは作らない。

---

# 14. P1-G Music / CameraSequence / Metadata

current errorsに:

```text
Music
CameraSequence
Metadata::GetAreaInfo
Metadata::GetRoomById
Fields.S05
PlayerEntity::Players
```

等が残る。

current declarationに合わせ:

```text
namespace
getter/setter
shared_ptr
Fields.S05()
Players()
```

を個別に修正。

---

# 15. P1-H SetRoomValues reference mismatch

current:

```cpp
const RoomMetadata* roomMeta = ...
scene.SetRoomValues(roomMeta);
```

canonical Scene APIが referenceなら、C# null/assert順序を保って pointeeを渡す。

---

# 16. RoomEntity修正順

```text
P1-1  Entities().GetEnumerator()
P1-2  Portal type qualification
P1-3  NodeRef type qualification
P1-4  NetRoomChange / AiPersonality visibility
P1-5  Scene accessor conversion
P1-6  GameState accessor conversion
P1-7  Player shared_ptr ownership
P1-8  Music / CameraSequence API
P1-9  Metadata / Fields accessors
P1-10 SetRoomValues等 pointer/reference
P1-11 full RoomEntity.cs audit
P1-12 full Native CI
```

---

# 17. ユーザー提供 Windows Testing/Utility errors

今回提示された:

```text
Testing/TestMisc.cpp
Testing/TestParse.hpp/.cpp
Utility/Extract.cpp
Utility/RepackCollision.cpp
```

は current Windows terminalでも後段に残る。

ただし PointModule/RoomEntityより後。

---

# 18. P2 candidate — Testing exception canonicalization

`Formats/Types.hpp` には既に:

```cpp
System::ArgumentException
System::ArgumentOutOfRangeException
System::ArgumentNullException
```

が canonical definitionとして存在。

一方 `TestParse.hpp` は `Types.hpp` include後に:

```cpp
class System::ArgumentException
```

を再定義。

MSVC:

```text
C2011 class type redefinition
```

## 方針

duplicate TestParse-local exceptionを削除し、canonical ownerへ統一。

ただし C#:

```csharp
throw new ArgumentException(nameof(values));
```

の message constructor semanticsが必要。

current canonical `ArgumentException` は default constructorのみなので、必要なら **canonical shared exception ownerに最小の constructor surfaceを追加**。

TestParse専用 duplicate classを残さない。

---

# 19. TestMisc ArgumentOutOfRangeException

current code:

```cpp
throw System::ArgumentOutOfRangeException("source");
```

canonical class自体は `Formats/Types.hpp` にあるが current constructorは引数なし。

従って:

- visibility
- constructor signature
- parameter/message semantics

を C# sourceと照合して canonical ownerで閉じる。

local duplicate typeを作らない。

---

# 20. P3 candidate — Extract Version

current `Program.hpp`:

```cpp
static const Mods::Update::Version Version;
```

current `Extract.cpp`:

```cpp
std::is_trivially_copyable_v<System::Version>
sizeof(System::Version)
memcpy(Program::Version)
```

Native canonical typeは:

```text
MphRead::Mods::Update::Version
```

で、API:

```cpp
Major()
Minor()
Build()
Revision()
ToString()
ToString(fieldCount)
```

を持つ。

## 推奨

raw layout memcpy/static_assertを捨て、canonical Version APIで C# Version.ToString semanticsを再現。

fake `System::Version` typedefは追加しない。

---

# 21. P4 candidate — RepackCollision Read qualification

`RepackCollision.cpp` は既に:

```cpp
#include "../Read.hpp"
```

済み。

canonical:

```cpp
namespace MphRead
{
    class Read final
}
```

しかし anonymous namespaceでは unqualified:

```cpp
Read::ReadStruct(...)
Read::DoOffsets(...)
```

と呼んでいる。

MSVC:

```text
Read is not a class or namespace name
```

## 修正方向

either:

```cpp
MphRead::Read::...
```

または:

```cpp
using MphRead::Read;
```

。

後続 `portals` / `ManagedAt` errorsはこの parse failureの cascadeである可能性が高いので、まず qualification後に再評価。

---

# 22. Windows high-fanout backlog

current completed Windows raw diagnostics上位:

```text
GameState.cpp         372
RoomEntity.cpp        244
Renderer.cpp          224
Export/Collada.cpp    216
Collision.cpp         204
Effects.cpp           204
Menu.cpp              204
Export/Scripting.cpp  186
...
```

raw countはpriorityではない。

---

# 23. current recommended order

```text
P0  PointModuleEntity
    ├─ Scene complete type
    ├─ TryGetEntity shared_ptr out
    └─ Next/Prev cast identity

P1  RoomEntity
    ├─ Entities().GetEnumerator()
    ├─ Portal type/member collision
    ├─ NodeRef type/member collision
    ├─ NetRoomChange / AiPersonality visibility
    ├─ Scene accessor API
    ├─ GameState accessor API
    ├─ Player shared_ptr ownership
    ├─ Music / CameraSequence
    ├─ Metadata / Fields
    └─ residual strict audit

    ↓ exact SHA full Native CI

P2  Testing exception canonicalization
P3  Extract Version adaptation
P4  RepackCollision Read qualification

P5+ new cross-platform first-error
```

---

# 24. worker split

## Worker A

```text
PointModuleEntity.cs
PointModuleEntity.hpp/.cpp
Scene.hpp read-only
```

## Worker B

```text
RoomEntity.cs
RoomEntity.hpp/.cpp
Scene/GameState/NetRoomChange/Music/CameraSequence/AiPersonality/Metadata read-only
```

## Worker C

read-only triage:

```text
TestMisc
TestParse
Extract
RepackCollision
Formats/Types.hpp
BuildVersion.hpp
Read.hpp
```

---

# 25. strict parity rules

- [ ] C# sole specification
- [ ] canonical declaration owner
- [ ] reference identity
- [ ] null behavior
- [ ] exception type/message/timing
- [ ] shared_ptr/raw ownership
- [ ] getter/setter read count
- [ ] collection iteration order
- [ ] unchecked integer behavior
- [ ] float operation order
- [ ] static state
- [ ] side-effect order
- [ ] no convenience shim solely for compiler closure

---

# 26. 最重要ポイント

前回の:

```text
PlayerDialog
PlayerCollision
include scan
```

は current HEADでは通過済み。

今の最上流:

```text
PointModuleEntity
```

その直後:

```text
RoomEntity
```

。

今回の Testing/Utility errorsは正しく、後で必ず処理する必要があるが、今の最短ルートは:

```text
PointModuleEntity
→ RoomEntity
→ exact SHA full CI
→ Testing/Utility または新しい共通 frontier
```

。

RoomEntityは error数の多さではなく:

```text
enumerator protocol
type/member name collision
property→accessor
shared_ptr ownership
missing canonical declaration visibility
```

のroot単位で閉じる。
