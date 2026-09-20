# Fruity-Prime C# → C++20 Native Build 修正優先順位

## 目的

C# 実装を唯一の仕様として C++20 Native 側を完全再現している途中で発生した GitHub Actions のビルドエラーについて、単純な「ログ出現順」ではなく、以下の基準で修正優先順位を付ける。

1. 多数の translation unit に波及する共有ヘッダの不整合を先に直す。
2. 1つの根本原因から派生したエラーを別々の不具合として数えない。
3. macOS / Android のように MphRead へ到達する前に止まる独立 blocker は別レーンで先に解消する。
4. C# のプロパティ、nullable/reference semantics、enum underlying type、例外、所有権を C++ 側の都合で勝手に変更しない。
5. 共有 blocker を1層直すごとに CI を再実行し、新しく露出した先頭 blocker を次の修正対象にする。
6. 古い Windows ログに大量に残っている leaf error は、上流ヘッダ修正後にも再現することを確認してから触る。

---

## 解析対象

### 基準 run

- Workflow: `Native C++ Desktop`
- Run: `35520110778`
- Branch: `develop2`
- Head: `f8159947ab56e28debb6a8c3eb99652801613867`
- Tree: `77003f2adcb0aa246cba880350b5b8f053bc4249`
- Commit title: `Match native Selection to C# behavior`
- Run conclusion: `failure`

Jobs:

| Platform | Job ID | Result | 主な停止位置 |
|---|---:|---|---|
| Linux / GCC | `106102616498` | failure | `MphRead.Native` |
| Windows / MSVC | `106102616502` | failure | `MphRead.Native` |
| macOS / Clang | `106102616576` | failure | `NcsfPlay.Native/TagList.cpp` |

### 解析時点の現在 head

解析時点で `develop2` は以下まで進んでいる。

- Current head: `107ef07a800dc734a16c5f43e9bb37c123093d10`
- 基準 run の head より 6 commits ahead
- `src/MphRead.Native/MemoryClasses.hpp` に enum 可視性修正が入っている
- Current Desktop run: `35521924802`
- Current Android run: `35521924852`

Current Desktop の Linux/GCC では、基準 run に大量発生していた `MemoryClasses.hpp` の enum 未解決エラーが消えている。したがって `MemoryClasses` は「基準 run では最優先 blocker だったが、現在 head では修正済みで最終検証待ち」と扱う。

---

# 結論: 推奨修正順

## 0. `NcsfPlay/TagList` のクロスプラットフォーム blocker

### 対象

- C# sole specification:
  - `src/NcsfPlay/TagList.cs`
- Native:
  - `src/NcsfPlay.Native/TagList.hpp`
  - `src/NcsfPlay.Native/TagList.cpp`

### なぜ最初か

macOS は MphRead に到達する前にここで停止しているため、この blocker を残したままだと macOS で後続の MphRead エラーを観測できない。

基準 run の macOS:

```text
src/NcsfPlay.Native/TagList.cpp:13:10:
fatal error: 'unicode/ucol.h' file not found
```

現在の Android run `35521924852` でも両 ABI が同じ領域で停止している。

```text
ucol_open          is unavailable: introduced in Android 33
ucol_setStrength   is unavailable: introduced in Android 33
ucol_setAttribute  is unavailable: introduced in Android 33
ucol_close         is unavailable: introduced in Android 33
ucol_strcoll       is unavailable: introduced in Android 33
ucol_getSortKey    is unavailable: introduced in Android 33
```

### 修正方針

単に macOS に ICU header を追加するだけでは不十分。Android の最低 API と C# の文字列比較・ソート semantics を両立する必要がある。

- C# `TagList.cs` の比較 semantics を先に確定する。
- macOS の ICU include/link 問題と Android API 33 制約を同時に考える。
- Android の minSdk を安易に 33 へ上げて回避しない。
- ICU が使えない platform で別アルゴリズムに差し替える場合も、C# observable behavior が一致することを証明する。
- Windows / Linux だけ通る実装で完了扱いにしない。

### 優先度

**P0 / 独立クロスプラットフォーム gate**

MphRead 修正と並行作業可能。

---

## 1. `MemoryClasses` enum/type visibility

### 対象

- C# sole specification:
  - `src/MphRead/MemoryClasses.cs`
- Native:
  - `src/MphRead.Native/MemoryClasses.hpp`
  - `src/MphRead.Native/MemoryClasses.cpp`

### 基準 run での症状

Linux/GCC では `MemoryClasses.hpp` から以下が未解決。

```text
PlatformFlags
PlatStateFlags
PlatAnimFlags
PlatformState
SpawnerFlags
TriggerFlags
AiFlags2
AiFlags3
AiFlags4
BeamFlags
EquipFlags
```

例:

```text
MemoryClasses.hpp:455:21: error:
'PlatformFlags' in namespace 'MphRead' does not name a type
```

Windows/MSVC では同じ共有ヘッダが多数の translation unit から include されるため、`MemoryClasses.hpp` だけで数千件の raw error occurrence に増幅していた。

これは数千個の独立バグではなく、少数の型可視性・namespace 不整合が fan-out したもの。

### 現在 head の状態

`107ef07a...` では forward declaration と namespace qualification が追加されている。

例:

```cpp
namespace MphRead
{
enum class EquipFlags : std::uint8_t;

namespace Entities
{
enum class PlatformFlags : std::uint32_t;
enum class PlatStateFlags : std::uint32_t;
enum class PlatAnimFlags : std::uint16_t;
enum class PlatformState : std::uint8_t;
enum class SpawnerFlags : std::uint8_t;
enum class TriggerFlags : std::uint32_t;
enum class AiFlags2 : std::uint32_t;
enum class AiFlags3 : std::uint32_t;
enum class AiFlags4 : std::uint8_t;
enum class BeamFlags : std::uint16_t;
}
}
```

使用側も `Entities::PlatformFlags` などへ修正されている。

Current Linux/GCC run では、基準 run の `MemoryClasses.hpp` enum error は消えている。

### 判定

**P1 / 基準 run の最大 blocker。現在 head では実装済み。**

残作業は Windows/MSVC を含めた current-head CI での確認。ここを再実装するのではなく、次の blocker へ進む。

---

## 2. `IconBounds` の incomplete type を解消

### 関連対象

- C#:
  - `src/MphRead/Mods/Render/PlayerEntityIconBounds.cs`
- Native:
  - `src/MphRead.Native/Mods/Render/PlayerEntityIconBounds.hpp`
  - `src/MphRead.Native/Mods/Render/PlayerEntityIconBounds.cpp`
- include/use site:
  - `src/MphRead.Native/Entities/Players/PlayerHud.hpp`
  - `src/MphRead.Native/Entities/Players/PlayerEntity.hpp`

### Current Linux/GCC

```text
std::array<MphRead::Entities::IconBounds, 9>
...
error: std::array<...>::_M_elems has incomplete type
```

原因位置:

```text
PlayerHud.hpp:31
struct IconBounds;
```

一方で `PlayerHud.hpp` の macro 内では実体サイズが必要な:

```cpp
std::array<::MphRead::Entities::IconBounds, 9>
```

をメンバとして保持している。

`std::array<T, N>` の `T` は完全型である必要があり、forward declaration だけでは不足する。

### 修正方針

- `IconBounds` の正規定義を一箇所に保つ。
- `std::array<IconBounds, 9>` を宣言する時点で完全型を見せる。
- duplicate struct を作らない。
- 循環 include を作らない。
- C# 側の `IconBounds` の value semantics と初期値を維持する。

### 優先度

**P2 / 共有ヘッダ blocker**

AreaVolume / Artifact の両方に波及するため、Entity 本体より先に直す。

---

## 3. `BeamProjectileEntity.hpp` の `Effectiveness` 型可視性

### 対象

- C# sole specification:
  - `src/MphRead/Entities/BeamProjectileEntity.cs`
- Native:
  - `src/MphRead.Native/Entities/BeamProjectileEntity.hpp`
  - `src/MphRead.Native/Entities/BeamProjectileEntity.cpp`

### Current Linux/GCC

```text
BeamProjectileEntity.hpp:217:32:
error: 'Effectiveness' has not been declared

void SpawnDamageEffect(Effectiveness effectiveness);
```

### 修正方針

- C# 側で `Effectiveness` がどの namespace/type を指すか確認する。
- Native 既存型の正しい namespace を使う。
- include で完全型が必要か、enum forward declaration で十分かを判断する。
- 「ビルドを通すためだけ」の新規 enum/alias を作らない。
- underlying type も C# と既存 Native 宣言に一致させる。

### 優先度

**P3 / 共有 declaration blocker**

AreaVolume が `BeamProjectileEntity.hpp` を include しているため、AreaVolume 本体修正より先。

---

## 4. `Formats/EntityClass` の GCC name-hiding エラー

### 対象

- C# sole specification:
  - `src/MphRead/Formats/EntityClass.cs`
- Native:
  - `src/MphRead.Native/Formats/EntityClass.hpp`
  - `src/MphRead.Native/Formats/EntityClass.cpp`

### Current Linux/GCC

```text
EntityClass.hpp:81:
declaration of 'MphRead::ItemType ...::ItemType' changes meaning of 'ItemType'

EntityClass.hpp:179:
declaration of 'MphRead::DoorType ...::DoorType' changes meaning of 'DoorType'

EntityClass.hpp:209:
declaration of 'MphRead::ItemType ...::ItemType' changes meaning of 'ItemType'
```

C# では type 名と property/field 名が同じでも問題ないが、C++ の lookup では曖昧さを生む。

### 修正方針

C# 側の member 名を変えず、Native の型名を明示的に修飾する方向を優先する。

例として考える形:

```cpp
MphRead::ItemType ItemType{};
MphRead::DoorType DoorType{};
```

ただし実際の変更時には `.cpp` 側、aggregate/constructor、ABI/interop layout も確認する。

### 優先度

**P4 / 共有 format header blocker**

Artifact の include chain を先に通すため、Artifact 本体より先。

---

## 5. `AreaVolumeEntity` strict parity correction

### 対象

- C# sole specification:
  - `src/MphRead/Entities/AreaVolumeEntity.cs`
- Native:
  - `src/MphRead.Native/Entities/AreaVolumeEntity.hpp`
  - `src/MphRead.Native/Entities/AreaVolumeEntity.cpp`

### Current Linux/GCC の代表例

C# property を C++ accessor function に移植した後、呼び出し側が field syntax のまま残っているパターンが多い。

```text
return scene->RoomId;
```

Native `Scene` 側が accessor の場合は function object として解釈され、以下になる。

```text
cannot resolve overloaded function 'RoomId'
```

同様に:

```text
GameState::Mode == GameMode::SinglePlayer
_scene->ShowVolumes == VolumeDisplay::AreaInside
```

が accessor 呼び出しになっていない。

また:

```text
StorySave* storySave = GameState::StorySave;
```

に対して Native 側は:

```cpp
std::shared_ptr<StorySave>
```

であり ownership model が一致していない。

その他 current Linux で確認できる問題:

```text
PlayerEntity* と std::shared_ptr<PlayerEntity> の比較
std::shared_ptr<EquipInfo> に対する .Beams
```

### 修正方針

C# observable behavior を保ちながら、Native API の実際の形に合わせる。

確認対象:

- `Scene.RoomId`
- `GameState.Mode`
- `GameState.StorySave`
- `Scene.ShowVolumes`
- local/raw pointer と `shared_ptr` の境界
- `EquipInfo` の dereference
- null behavior
- C# reference equality / object identity
- message side effects
- room-state update ordering

### 優先度

**P5 / current Linux の最初の concrete implementation blocker**

共有ヘッダ P2～P4 の後に着手。

---

## 6. `ArtifactEntity` strict parity correction

### 対象

- C# sole specification:
  - `src/MphRead/Entities/ArtifactEntity.cs`
- Native:
  - `src/MphRead.Native/Entities/ArtifactEntity.hpp`
  - `src/MphRead.Native/Entities/ArtifactEntity.cpp`

### Current Linux/GCC の代表例

```text
scene->RoomId
Formats::CameraSequence::Current
GameState::PausePrevented = true
_scene->StartMovie(...)
System::InvalidCastException
_scene->CameraMode
_scene->CameraPosition
```

具体的には:

```text
cannot resolve overloaded function 'RoomId'
unable to deduce 'auto' from CameraSequence::Current
Scene has no member named 'StartMovie'
InvalidCastException is not a member of System
invalid use of non-static member function Scene::CameraMode()
invalid use of non-static member function Scene::CameraPosition()
```

さらに property 名と helper function 名の衝突により:

```text
Scale(Scale(direction, 0.1F), 0.5F)
```

の内側 `Scale` が想定した helper ではなく member/property と競合している形のエラーも出ている。

### 修正方針

このファイルは単純な括弧追加だけでは終わらせない。

C# と照合する対象:

- `CameraSequence.Current`
- pause-prevention state
- movie/cutscene start API
- room ID
- camera mode / camera position
- enemy spawn reference identity
- `InvalidCastException` 相当の例外
- Vector/Scale 演算
- artifact/octolith save-state update
- side-effect ordering
- null/reference behavior

### 優先度

**P6 / current Linux の2番目の concrete implementation blocker**

AreaVolume と同じ API migration mistake が含まれるため、AreaVolume で確立した property→accessor 変換方針を再利用する。

---

# ここで必ず再ビルド

P2～P6 をまとめて大量修正してから確認するのではなく、最低でも以下の区切りで再ビルドする。

1. `IconBounds` + `BeamProjectileEntity` + `EntityClass`
2. `AreaVolumeEntity`
3. `ArtifactEntity`

理由は、current Linux/GCC が現在 AreaVolume と Artifact 付近で停止しており、その先の translation unit の真の先頭エラーがまだ隠れているため。

---

# 次に出る可能性が高い Windows/MSVC blocker

以下は基準 run `35520110778` で確認できたが、`MemoryClasses` 修正前のログなので、**現在 head でも再現することを確認してから修正する**。

推奨順位は shared declaration / API surface を leaf implementation より先にする。

## 7. `Renderer.hpp` / `Scene.hpp` の `MetaDir` / `Movie` declaration closure

基準 run 例:

```text
Renderer.hpp(389): syntax error: identifier 'MetaDir'
Scene.hpp(907): 'MetaDir' is not a member of 'MphRead'
Scene.hpp(907): 'Movie' is not a member of 'MphRead'
```

### 方針

- header self-containment を確認する。
- include order に偶然依存して型が見える状態を禁止する。
- C# の型を Native で新造せず、既存 canonical declaration を参照する。
- forward declaration で足りるか完全定義が必要かを分ける。

**候補優先度: P7**

---

## 8. `Read.hpp` の `MphRead::Paths` symbol-kind collision

基準 run:

```text
Read.hpp(29):
'MphRead::Paths': a symbol with this name already exists and therefore
this name cannot be used as a namespace name
```

### 方針

`Paths` が別 translation unit/header で class/variable/namespace のどれとして canonical に定義されているかを確定し、C# の `Paths` surface と一致させる。

安易な rename は、他の Native 呼び出しと C# parity を壊す可能性がある。

**候補優先度: P8**

---

## 9. `SceneSetup.hpp` / enum underlying type consistency

基準 run:

```text
SceneSetup.hpp:
BossFlags declarations disagree on underlying type
```

C++ は同じ enum の forward declaration と定義で underlying type が一致している必要がある。

### 方針

- canonical `BossFlags` 宣言を1つ決める。
- C# enum の実体サイズ/符号を確認する。
- forward declaration を canonical 定義と完全一致させる。

**候補優先度: P9**

---

## 10. `GameState.cpp` の overload / managed-array helper ambiguity

基準 run では:

```text
GameState.cpp:
ManagedAt: ambiguous call to overloaded function
```

が大量に発生。

ただし `GameState` は共有 API の影響を強く受けるため、P7～P9 と current Windows の再確認後に扱う。

**候補優先度: P10**

---

## 11. `PlayerEntity` partial/header declaration closure

基準 Windows/MSVC では以下のような「実装側にはある前提だが class declaration に見えない」エラーが多数あった。

例:

```text
PlayerAi.hpp:
'_scene' is not a member of PlayerEntity

PlayerDraw.cpp:
'DrawScanModels' identifier not found

PlayerPause.cpp:
'_drawPauseState' is not a member of PlayerEntity

PlayerProcess.cpp:
'BombCountCheck' is not a member of PlayerEntity

PlayerScan.cpp:
'SetCombatVisor' is not a member of PlayerEntity
```

### 方針

これは C# partial class を複数 Native header/macro に分割した際の closure 不備の可能性が高い。

- C# `PlayerEntity` partial 全体の member surface を一度 inventory 化する。
- Native の `Player*.hpp` macro 群と `PlayerEntity.hpp` の展開結果を照合する。
- 「その cpp を通すためだけ」の重複 member を個別追加しない。
- field、property accessor、method、static/instance の区別を維持する。

**候補優先度: P11**

---

## 12. Leaf implementation errors

上記 shared/core 層が通ってから、以下へ進む。

基準 Windows/MSVC で確認された主な領域:

- `Entities/DoorEntity.cpp`
- `Entities/EnemySpawnEntity.cpp`
- `Entities/Enemies/*`
- `Formats/Collision.cpp`
- `Formats/Effects.cpp`
- `Menu.cpp`
- `Export/Collada.cpp`
- `Export/Scripting.cpp`
- `Mods/Render/PlayerEntityEndScreen.cpp`
- その他 Player partial 実装

ここでは共通して、C# property を C++ method として実装した後の call-site migration 漏れ、`shared_ptr` と raw/reference semantics の不一致、namespace/type visibility の不整合が多い。

**候補優先度: P12 以降**

---

# 旧 Windows エラー件数をそのまま優先順位にしない理由

基準 run の Windows/MSVC では raw error occurrence が非常に多い。

例:

| File | Raw compiler error occurrences |
|---|---:|
| `MemoryClasses.hpp` | 4408 |
| `Scene.hpp` | 734 |
| `PlayerAi.hpp` | 404 |
| `GameState.cpp` | 382 |
| `PlayerDraw.cpp` | 296 |
| `RoomEntity.cpp` | 232 |
| `Export/Collada.cpp` | 214 |
| `Formats/Collision.cpp` | 204 |
| `PlayerPause.cpp` | 202 |
| `Formats/Effects.cpp` | 202 |
| `Menu.cpp` | 198 |

これは defect 数ではない。

共有ヘッダの1つの declaration error が、多数の `.cpp` から include されて何百～何千回も再報告されているため。

したがって:

```text
4408 errors → 4408個直す
```

ではなく:

```text
共有 declaration 1個を直す
→ 再ビルド
→ 数千件の派生エラーを消す
→ 次の真の blocker を観測
```

という進め方にする。

---

# 基準 run と current-head の差から分かること

`MemoryClasses` の enum visibility 修正後、Current Linux/GCC では基準 run に存在した以下が消えた。

```text
PlatformFlags
PlatStateFlags
PlatAnimFlags
PlatformState
SpawnerFlags
TriggerFlags
AiFlags2
AiFlags3
AiFlags4
BeamFlags
EquipFlags
```

Current Linux の observable error は、主に次へ縮小している。

- `BeamProjectileEntity.hpp`: `Effectiveness`
- `EntityClass.hpp`: type/member name collision
- `PlayerEntity.hpp`: incomplete `IconBounds`
- `AreaVolumeEntity.cpp`
- `ArtifactEntity.cpp`

これは「共有 blocker を先に直す」戦略が正しいことを実ビルドで確認できている。

---

# 推奨ワーカー分割

並行作業する場合は、同じ共有 header を複数 worker が触らないようにする。

| Worker | Scope | 並行可否 |
|---|---|---|
| A | `NcsfPlay/TagList` | 他と並行可 |
| B | `PlayerEntityIconBounds` + required include closure | C/D と並行は注意 |
| C | `BeamProjectileEntity` | B/D と基本並行可 |
| D | `Formats/EntityClass` | B/C と基本並行可 |
| E | `AreaVolumeEntity` | B/C 完了後推奨 |
| F | `ArtifactEntity` | B/D 完了後推奨 |
| G | Windows shared-header next blocker | current Windows 再確認後 |

同一 branch に直接 push する場合、各 worker は書き込み直前に `develop2` を refresh し、他 worker の非競合変更を保持する。

---

# 各修正の合格条件

各 C# → C++ parity task は、単にコンパイル成功ではなく以下を満たす。

- C# が唯一の behavioral specification。
- public/internal-equivalent surface が一致。
- enum underlying type が一致。
- default 値が一致。
- property getter/setter semantics が一致。
- static / instance が一致。
- nullable/null behavior が一致。
- reference identity が一致。
- exception type/timing が可能な限り一致。
- side-effect ordering が一致。
- integer overflow / signedness が一致。
- collection ordering が一致。
- `shared_ptr` 化によって C# の参照 semantics を変えない。
- platform workaround を C# behavior の変更として混入させない。
- Windows/MSVC、Linux/GCC、macOS/Clang、Android NDK の実コンパイルで確認する。

---

# 実際の作業順チェックリスト

- [ ] P0 `NcsfPlay/TagList` の macOS/Android compatibility を解決
- [x] P1 `MemoryClasses` enum/type visibility を修正
- [ ] P1 current-head Windows/MSVC で `MemoryClasses` 消滅を確認
- [ ] P2 `PlayerEntityIconBounds` / `IconBounds` complete-type closure
- [ ] P3 `BeamProjectileEntity` / `Effectiveness` declaration
- [ ] P4 `Formats/EntityClass` GCC name-hiding
- [ ] 再ビルド
- [ ] P5 `AreaVolumeEntity` strict parity correction
- [ ] 再ビルド
- [ ] P6 `ArtifactEntity` strict parity correction
- [ ] 再ビルド
- [ ] P7 `Renderer/Scene` shared type closure を current Windows で再確認
- [ ] P8 `Read.hpp` / `Paths` collision を current Windows で再確認
- [ ] P9 `SceneSetup` enum underlying type を current Windows で再確認
- [ ] P10 `GameState` ambiguity を current Windows で再確認
- [ ] P11 `PlayerEntity` partial surface closure
- [ ] 再ビルド
- [ ] P12 以降の leaf implementation errors を新しいログ順に処理
- [ ] 全 platform compile clean
- [ ] strict C# semantic audit
- [ ] runtime / regression validation

---

# 最重要ルール

**古い run の末端エラーを一括修正しない。**

この migration では、共有ヘッダや C# property → C++ accessor の surface mismatch が1つあるだけで、数十～数千件の二次エラーが生成される。

常に:

```text
最上流の共有 blocker
→ 修正
→ CI
→ 新しく露出した最上流 blocker
→ 修正
→ CI
```

の順に進める。

この方法が、C# 完全再現を維持しながら最短で Native 全体を compile-clean にする修正順となる。
