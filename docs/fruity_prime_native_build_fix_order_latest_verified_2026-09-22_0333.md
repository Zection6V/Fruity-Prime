# Fruity-Prime C# → C++20 Native Build 修正優先順位
## 2026-09-22 03:33 JST 最新 GitHub / CI 検証版

## 0. 結論

`develop2` は前回版からさらに大きく進んでいる。

current HEAD:

```text
3ccdf0ecc0f9b9924b6f39621addcdb719993615
Fix EnemySpawnEntity C# parity
```

この exact SHA では:

- Linux / GCC: `EntityBase.cpp` で failure
- macOS / Clang: `EntityBase.cpp` で failure
- Android arm64-v8a: `EntityBase.cpp` で failure
- Android x86_64: `EntityBase.cpp` で failure
- Windows / MSVC: 観測時点では build 中

したがって現在の **P0 は `src/MphRead.Native/Entities/EntityBase.cpp`**。

前回 P0/P1 だった:

```text
49_ForceFieldLock
51_CarnivorousPlant
EnemyInstanceEntity
EnemySpawnEntity
```

はすでに frontier を通過している。

現在は Enemy 個別 slice の compiler frontier から、より基盤側の **EntityBase / Model / Scene / Collision ownership-adaptation 層**へ移ったと見てよい。

---

# 1. 基準 repository state

```text
Repository: Zection6V/Fruity-Prime
Branch: develop2
HEAD: 3ccdf0ecc0f9b9924b6f39621addcdb719993615
Tree: 7dcbbc9b3c4f3ba2df590148fa8b7c16f527f150
Parent: cdac847b66db507743a420c6f6fc4a15473639bf
Commit: Fix EnemySpawnEntity C# parity
```

---

# 2. current CI state

## Linux

```text
Run: 35638267439
Job: 106460990563
Result: failure
first source blocker: src/MphRead.Native/Entities/EntityBase.cpp
```

## macOS

```text
Run: 35638267554
Job: 106460991266
Result: failure
first source blocker: src/MphRead.Native/Entities/EntityBase.cpp
```

## Android

```text
Run: 35638267444
Contract: 106460990831 success
arm64-v8a: 106461052815 failure
x86_64: 106461052898 failure
```

両 ABI とも first source blocker:

```text
src/MphRead.Native/Entities/EntityBase.cpp
```

## Windows

```text
Run: 35638267503
Job: 106460990844
Status at observation: in_progress
Step: Build shared native owners
```

current exact-SHA Windows frontier はまだ terminal 確定していない。

ただし1つ前の commit `cdac847b...` の Windows run では:

```text
EnemySpawnEntity.cpp
↓
EntityBase.cpp
↓
大量 downstream errors
```

まで既に露出していた。

current HEAD は EnemySpawnEntity 修正 commitなので、Windows も EntityBaseへ進む可能性は高いが、terminal logが authority。

---

# 3. 前回版から完了済みに移す項目

前回:

```text
P0 49_ForceFieldLock
P1 51_CarnivorousPlant
```

だったが、現在は以下まで進んでいる。

```text
30dc479e  Fix Enemy49 ForceFieldLock native parity
8c6e13c6  Fix Enemy51 CarnivorousPlant native parity
cdac847b  Fix EnemyInstanceEntity C# parity
3ccdf0ec  Fix EnemySpawnEntity C# parity
```

したがって:

```text
49_ForceFieldLock
51_CarnivorousPlant
EnemyInstanceEntity
EnemySpawnEntity
```

は current priority list から外す。

---

# 4. P0 — `EntityBase.cpp`

authoritative C#:

```text
src/MphRead/Entities/EntityBase.cs
```

Native:

```text
src/MphRead.Native/Entities/EntityBase.hpp
src/MphRead.Native/Entities/EntityBase.cpp
```

主要 dependencies:

```text
src/MphRead.Native/Formats/Model.hpp
src/MphRead.Native/Formats/Collision.hpp
src/MphRead.Native/Scene.hpp
src/MphRead.Native/Renderer.hpp
src/MphRead.Native/Read.hpp
```

---

# 5. current EntityBase error clusters

Linuxでは `EntityBase.cpp` だけで多数の diagnostics が出るが、独立 defect 数ではない。

主に次の root に分かれる。

```text
A. Scene incomplete type
B. ModelInstance::Model property → method adaptation
C. shared_ptr<vector<T>> collection adaptation
D. shared_ptr<Node/Material/Mesh/Recolor> element adaptation
E. AnimationInfo shared_ptr adaptation
F. EntityCollision constructor ownership mismatch
G. Scene property → accessor adaptation
H. Render/collision call signature adaptation
```

優先して root 単位で直す。

---

# 6. P0-A — `Scene` complete type

`EntityBase.hpp` では:

```cpp
class Scene;
```

の forward declarationだけで十分な箇所がある。

しかし `EntityBase.cpp` は `_scene` の memberを大量に直接使用している。

例:

```cpp
_scene->GetNodeRefByName(...)
_scene->FrameCount
_scene->Light1Vector
_scene->TransformRoomNodes
_scene->UpdateMaterials(...)
_scene->ShowCollision
_scene->GetNextPolygonId()
_scene->CameraMode
_scene->IsNodeRefAudible(...)
_scene->AddRenderItem(...)
_scene->FrameTime
```

現在 `EntityBase.cpp` の include listには `Scene.hpp` がなく、compiler は:

```text
invalid use of incomplete type 'class MphRead::Scene'
```

を多数出している。

## 推奨

implementation file側で complete definition を可視化する。

```cpp
#include "../Scene.hpp"
```

を候補とする。

Scene APIを EntityBase.cpp のために再設計しない。

まず complete-type visibility を閉じ、その後残る accessor error を評価する。

---

# 7. P0-B — `ModelInstance::Model()` adaptation

Native `ModelInstance`:

```cpp
class ModelInstance
{
private:
    std::shared_ptr<Model> _model;

public:
    [[nodiscard]] std::shared_ptr<Model> Model() const noexcept;
};
```

C#:

```csharp
inst.Model
```

は Native では getter:

```cpp
inst.Model()
```

。

current `EntityBase.cpp` には:

```cpp
inst->Model->Materials
attach->Model->GetNodeByName(...)
inst.Model->Scale
Model& model = *inst.Model;
inst.Model->FirstHunt
```

が残っている。

compiler:

```text
invalid use of member function ModelInstance::Model()
reference to non-static member function must be called
```

## 推奨

canonical Native getterへ合わせる。

managed-null semantics が必要な箇所では project の既存 `RequireReference(...)` pattern を使う。

### 禁止

全箇所を機械的に `inst.Model()->...` へ置換して null/exception timing を変えること。

---

# 8. P0-C — Model collections are shared_ptr-owned

Native `Model`:

```cpp
const std::shared_ptr<const std::vector<std::shared_ptr<Node>>> Nodes;
const std::shared_ptr<const std::vector<std::shared_ptr<Mesh>>> Meshes;
const std::shared_ptr<const std::vector<std::shared_ptr<Material>>> Materials;
const std::shared_ptr<const std::vector<Matrix4>> TextureMatrices;
const std::shared_ptr<const std::vector<std::shared_ptr<Recolor>>> Recolors;
const std::shared_ptr<const std::vector<std::int32_t>> NodeMatrixIds;
```

current Native は shared_ptr wrapperを外さず:

```cpp
model.Materials.at(...)
model.Meshes.at(...)
model.Nodes.at(...)
model.TextureMatrices.empty()
model.Recolors.at(...)
model.NodeMatrixIds.size()
```

としているため failure。

## 推奨

pointee collectionへアクセスする。

ただし C# managed-null semanticsを保つため、repositoryで既に使われている:

```cpp
RequireReference(...)
ManagedListAt(...)
VectorAt(...)
```

等の canonical helper patternを優先。

---

# 9. P0-D — collection elementも shared_ptr

Model collection elementも:

```text
shared_ptr<Node>
shared_ptr<Mesh>
shared_ptr<Material>
shared_ptr<Recolor>
```

。

従って:

```cpp
Material& material = model.Materials.at(...)
Mesh& mesh = model.Meshes.at(...)
Node& node = model.Nodes.at(...)
```

とはできない。

collectionを dereferenceした後、さらに element shared_ptrの managed reference boundaryを処理する。

### 重要

C# reference identityを守るため object copy を作らない。

---

# 10. P0-E — CollisionInfo::Points / DrawPoints ownership

current code:

```cpp
entCol->DrawPoints.insert(...)
entCol->Collision->Info->Points.begin()
...
```

だが current Native graph は shared_ptrを複数段持つ。

compiler:

```text
shared_ptr<vector<Vector3>> has no insert/end
shared_ptr<const vector<Vector3>> has no begin/end/size
```

## 推奨

以下を各 reference boundaryごとに canonical helperで解く。

```text
DrawPoints
Collision
Info
Points
```

C# の AddRange / foreach 順序を維持する。

### 禁止

shared_ptr collection自体へ STL compatibility shim を追加すること。

---

# 11. P0-F — `EntityCollision` constructor ownership mismatch

Native:

```cpp
EntityCollision(
    std::shared_ptr<CollisionInstance> collision,
    std::shared_ptr<EntityBase> entity);
```

current `EntityBase::SetCollision` は raw:

```cpp
SetCollision(CollisionInstance* collision, ...)
```

から:

```cpp
std::make_shared<EntityCollision>(collision, this)
```

としている。

compiler:

```text
no matching constructor for EntityCollision(
    CollisionInstance*,
    EntityBase*)
```

## 方針

これは単純 `.get()` / raw-pointer patchでは解決しない。

必要なのは:

```text
CollisionInstance* → canonical shared ownership
EntityBase* this → canonical entity shared ownership
```

の確立。

既存 Scene ownership / identity lookup precedentを探して再利用する。

`shared_from_this` をこの1箇所のためだけに安易に導入しない。

---

# 12. P0-G — AnimationInfo shared_ptr adaptation

`ModelInstance`:

```cpp
const std::shared_ptr<AnimationInfo> AnimInfo;
```

current code:

```cpp
inst.AnimInfo.Texcoord.Group
inst.AnimInfo.TexcoordFrame()
```

は不正。

さらに Texcoord groupの animation dictionaryも shared_ptr-owned。

current errors:

```text
AnimationInfo shared_ptr has no Texcoord
shared_ptr<TexcoordAnimationDictionary> has no find/end
AnimationInfo shared_ptr has no TexcoordFrame
```

## 推奨

managed object graphを段階的に解く。

```text
inst.AnimInfo
→ AnimationInfo
→ Texcoord
→ TexcoordAnimationInfo
→ Group
→ TexcoordAnimationGroup
→ Animations
→ dictionary
```

C# null behavior / TryGetValue semanticsを保持する。

---

# 13. P0-H — Scene property → accessor

complete typeを入れた後、以下の property-style usageが残る可能性が高い。

```cpp
_scene->FrameCount
_scene->CameraMode
_scene->ShowCollision
_scene->ShowInvisibleEntities
_scene->ShowAllEntities
_scene->ColEntDisplay
_scene->CameraPosition
_scene->FrameTime
```

current Native Scene/Renderer APIが getter methodなら:

```cpp
FrameCount()
CameraMode()
ShowCollision()
...
```

へ適合する。

## 修正順

Scene complete typeを先に直してから compilerを再評価する。

---

# 14. P0-I — render/model call signatures

current codeには:

```cpp
model.AnimateNodes(...)
model.AnimateTexcoords(...)
_scene->AddRenderItem(...)
```

の signature mismatchも見える。

ただし前段の:

```text
Model()
AnimInfo
shared_ptr collections
Scene complete type
```

の parse failureに起因する cascading diagnosticsが含まれる。

P0-A～Hを先に閉じ、残った genuine mismatchだけ修正する。

---

# 15. EntityBase 修正順

```text
P0-1  EntityBase.cpp で Scene complete type可視化
P0-2  ModelInstance::Model() adaptation
P0-3  Model shared_ptr collections pointee access
P0-4  Node/Mesh/Material/Recolor element shared_ptr adaptation
P0-5  Collision.Info.Points / DrawPoints ownership
P0-6  AnimInfo / Texcoord group/dictionary adaptation
P0-7  Scene property → accessor
P0-8  EntityCollision constructor ownership
P0-9  residual AnimateNodes / AddRenderItem signatures
P0-10 C# EntityBase.cs 全体 strict audit
P0-11 full Native CI
```

---

# 16. P0 acceptance gate

- [ ] Linux / GCC EntityBase diagnostics 0
- [ ] macOS / Clang EntityBase diagnostics 0
- [ ] Android arm64-v8a EntityBase diagnostics 0
- [ ] Android x86_64 EntityBase diagnostics 0
- [ ] Windows terminal後 EntityBase diagnostics 0
- [ ] ModelInstance.Model null/reference semantics一致
- [ ] Model collection bounds behavior一致
- [ ] Node/Mesh/Material/Recolor identity一致
- [ ] Collision ownership identity一致
- [ ] collision point iteration/order一致
- [ ] Scene property read order一致
- [ ] animation group null handling一致
- [ ] render recursion order一致
- [ ] GetDrawItems node traversal order一致
- [ ] AddVolumeItem behavior一致
- [ ] ConstantAcceleration / Drag / ExponentialDecay operation order一致

---

# 17. Windows status handling

current exact-SHA Windows jobは観測時点で:

```text
in_progress
```

なので、最新版MDでは Windowsについて EntityBaseを確定 frontierとは断言しない。

ただし parent SHA `cdac847b...` の terminal Windows runでは:

```text
EnemySpawnEntity
↓
EntityBase
↓
Player / GameState / Renderer等
```

まで確認済み。

current HEADで EnemySpawnEntityは修正済みなので、EntityBaseが次になることは合理的な期待だが terminal resultが authority。

---

# 18. P1+ は EntityBase後に再決定

P1を今固定しない。

EntityBaseは基底クラスであり、修正すると大量の downstream diagnosticsが消える可能性が高い。

従って現在の Windows raw counts をそのまま priority にしない。

---

# 19. downstream backlog

EntityBase後に残る可能性が高い cluster。

## PlayerProcess

```text
WeaponList shared_ptr
GameState property/accessor
Scene Room/FrameCount
EquipInfo
CameraSequence
Feature/Bugfix signature
```

## PlayerScan

```text
RequireReference / accessor mismatch
DrawHudObject shared_ptr ownership mismatch
```

## PlayerPause

```text
GetDrawItems signature mismatch
Node ownership
AddRenderItem overload mismatch
```

## Other

```text
GameState
Renderer
RoomEntity
Collision
Effects
Export
Menu
```

EntityBase後に再順位付けする。

---

# 20. 完了済み frontier history

少なくとも以下は旧 compiler frontierとして通過済み。

```text
MemoryClasses
PlayerAi
SceneSetup enum
BeamProjectileArray
Music
file-time
Gorea cluster
Voldrum
Quadtroid
CrashPillar
FireSpawn
Slench
SlenchNest
SlenchTurret
LesserIthrak
ForceFieldLock
CarnivorousPlant
EnemyInstanceEntity
EnemySpawnEntity
```

同じ errorが current CIに再出現しない限り戻らない。

---

# 21. worker split

## Worker A — EntityBase strict audit / implementation

write scope:

```text
src/MphRead.Native/Entities/EntityBase.cpp
```

authority:

```text
src/MphRead/Entities/EntityBase.cs
```

read dependencies:

```text
Formats/Model.hpp
Formats/Collision.hpp
Scene.hpp
Renderer.hpp
Read.hpp
```

## Worker B — EntityCollision ownership research

read-only推奨。

確認:

```text
EntityCollision constructors
CollisionInstance ownership
Scene entity shared ownership
raw/shared identity precedents
```

## Worker C — downstream triage

read-only:

```text
PlayerProcess
PlayerScan
PlayerPause
GameState
Renderer
RoomEntity
```

EntityBase fix後に staleになる可能性があるため writeしない。

---

# 22. strict parity rules

EntityBaseは基盤なので特に厳格に見る。

- [ ] public/protected API
- [ ] constructors
- [ ] defaults
- [ ] virtual dispatch
- [ ] ModelInstance identity
- [ ] Node/Mesh/Material/Recolor identity
- [ ] null behavior
- [ ] exception type/timing
- [ ] collection bounds
- [ ] recursion order
- [ ] side-effect order
- [ ] animation update order
- [ ] render item order
- [ ] collision point order
- [ ] Scene call order
- [ ] float operation order
- [ ] frame timing reads
- [ ] no hidden copies of managed-reference objects

---

# 23. priority policy

```text
cross-platform common first-error
>
base/shared ownership root
>
multi-platform implementation blocker
>
Windows downstream raw count
>
warning cleanup
```

EntityBaseは:

```text
cross-platform
+
base class
+
shared ownership root
```

なので優先度が非常に高い。

---

# 24. 最新推奨順

```text
P0  EntityBase.cpp
    ├─ Scene complete type
    ├─ ModelInstance::Model()
    ├─ Model shared_ptr collections
    ├─ Node/Mesh/Material/Recolor shared_ptr elements
    ├─ Collision Info/Points/DrawPoints
    ├─ AnimationInfo / Texcoord graph
    ├─ Scene property/accessor
    ├─ EntityCollision constructor ownership
    └─ residual render signatures

    ↓ exact SHA full Native CI

P1  新しい cross-platform first-error
    └─ EntityBase修正後に決定

P2+ shared/high-fanout downstream cluster
    ├─ PlayerProcess
    ├─ PlayerScan
    ├─ PlayerPause
    ├─ GameState
    ├─ Renderer
    ├─ RoomEntity
    ├─ Collision / Effects
    └─ Export / Menu
```

---

# 25. 最重要ポイント

前回の:

```text
P0 = 49_ForceFieldLock
P1 = 51_CarnivorousPlant
```

は現在は古い。

今の non-Windows 4 jobはすべて:

```text
EntityBase.cpp
```

で止まっている。

さらに parent Windows terminal logでも EnemySpawnEntity の次に EntityBase が既に見えている。

したがって現在は個別 Enemyを追い続ける段階から一度離れ:

```text
EntityBase / Model / Scene / Collision ownership adaptation
```

を strict C# parity で閉じるのが最短経路。

EntityBase修正後に必ず exact SHA の全 Native CIを取り直し、その結果だけで次 priorityを決める。
