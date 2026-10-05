# Fruity-Prime C++ モーフボール マウス操作＋Shift連続Boost 実装修正指示

作成日: 2026-10-05  
対象: `Zection6V/Fruity-Prime` `develop3_rendering` C++版  
監査基準Fruity-Prime HEAD: `edf9bb3547069916cce0f81899836bbf7e8afe23`  
比較基準melonPrimeDS HEAD: `c4165c87416902bb13b670e3147ecf05988017ed`

参照:

- Fruity-Prime: https://github.com/Zection6V/Fruity-Prime/tree/develop3_rendering
- melonPrimeDS: https://github.com/ag-advania/melonPrimeDS
- melonPrimeDS Morph Ball Boost資料: `docs/features/morph-ball-boost.md`
- melonPrimeDS Morph Ball mouse-mode資料: `docs/features/gameplay/morph-ball-boost-assist-sensitivity.md`
- melonPrimeDS設定資料: `docs/features/melonprime-settings/morph-ball-boost.md`

---

## 1. 目的

Fruity-Prime C++版のSamus Morph Ball操作を、melonPrimeDSの操作感に近づける。

今回の必須項目は次の3点。

1. Morph Ball中にマウス移動で継続的にステアリングできること。
2. 既存のMouse Flick Boostを維持し、フリック方向へBoostできること。
3. **Shiftを押し続けるだけで、チャージ → 発動 → 再チャージ → 再発動を自動で繰り返す連続Morph Ball Boost**を追加すること。

既存の通常Boost操作は削除しない。  
Shift連続Boostは通常Boostを置き換える機能ではなく、melonPrimeDSと同様に独立した追加操作とする。

---

## 2. 現状確認

### 2.1 melonPrimeDS

melonPrimeDSでは以下が確認できる。

`src/frontend/qt_sdl/Config.cpp`

```cpp
{"Instance*.Keyboard.HK_MetroidHoldMorphBallBoost", Qt::Key_Shift},
```

Shiftは独立した `HK_MetroidHoldMorphBallBoost` として定義されている。

`src/frontend/qt_sdl/MelonPrime.h`

```cpp
IB_MORPH_BOOST = 1ULL << 4,
```

通常Zoom/Boost系とは別の入力bitを持つ。

`src/frontend/qt_sdl/MelonPrimeInGame.cpp`

```cpp
const bool shiftAutoCycle = IsDown(IB_MORPH_BOOST);
```

Shift長押し時は `HandleMorphBallBoost()` が毎フレーム状態を確認し、Morph Ball Boostのチャージと解放を自動化する。

現在の重要な判定は以下。

```cpp
const uint8_t boostGauge = *m_ptrs.boostGauge;
const bool boostCooldownActive = (*m_ptrs.isBoosting) != 0x00;
const bool gaugeEnough = boostGauge > 0x0A;
```

そして、概念的には以下を繰り返す。

```text
Shift押下
  ↓
Boostゲージ不足
  ↓
Boost入力を押した状態にしてチャージ
  ↓
必要量まで溜まる
  ↓
busy/cooldownが空いていればBoost入力を解放
  ↓
Boost発動
  ↓
cooldown中に次のチャージ
  ↓
cooldown解除
  ↓
再度Boost発動
```

melonPrimeDSの資料でも、受入条件として以下が明記されている。

- Samus Morph BallでShift長押しによる連続Boost
- Morph直後でもShift長押しでBoost可能
- Shift長押し中もマウスステアリング可能
- 通常Boost経路とShift高速Boost経路は別
- 非Samusには適用しない

さらに重要なのは、Boost処理中でもmouse aimを止めないことである。

`HandleMorphBallBoost()` の後に `ProcessAimInputMouse()` が走るため、Boost中も方向更新が続く。

---

### 2.2 Fruity-Prime C++版

Fruity-Prime C++版には既に必要な土台の多くが存在する。

#### Mouse delta

`src/MphRead.Native/Entities/Players/PlayerInput.cpp`

```cpp
std::tie(_mouseDeltaX, _mouseDeltaY)
    = active
    ? Mods::Input::PointerDevice::TakeDelta()
    : Mods::Input::PointerInput::Filter(delta.first, delta.second);
```

ゲームロジック側は、

```cpp
_input.MouseDeltaX()
_input.MouseDeltaY()
```

から現在フレームのrelative mouse deltaを取得できる。

Windows Raw Input実装も最終的にこの共通入力経路へ入る設計にすること。  
Morph BallコードからWin32 Raw InputやQt native eventを直接読んではならない。

#### Mouse Flick Boost

既に以下がある。

- `src/MphRead.Native/Mods/Input/MouseFlick.cpp`
- `src/MphRead.Native/Mods/Input/MouseFlick.hpp`
- `src/MphRead.Native/Mods/Input/PlayerEntityMouseFlick.cpp`
- `src/MphRead.Native/Mods/Input/PlayerEntityMouseFlick.hpp`

`PlayerEntity::ProcessAlt()` では、

```cpp
ModCheckMouseFlick();
```

からMouse Flickを検出し、

```cpp
const float forward = -_swipeBoostY;
const float left = -_swipeBoostX;

const float dirX = _altRollFbX * forward
    + _altRollLrX * left;
const float dirZ = _altRollFbZ * forward
    + _altRollLrZ * left;
```

として、画面上のマウス方向をMorph Ballのcamera-relative world directionへ変換している。

つまり、

```text
Mouse delta
  ↓
screen-space direction
  ↓
_altRollFb / _altRollLr
  ↓
world-space Morph Ball Boost direction
```

までは既に存在する。

#### 通常Morph Ball移動

Samusは、

```cpp
_values.AltFormStrafe == 0
```

であり、現在は以下のRoll keyだけで移動する。

```cpp
RollUp
RollDown
RolltLeft
RollRight
```

この分岐では通常のmouse deltaをMorph Ball steeringへ使用していない。

したがって、**Mouse Flick Boostは存在するが、melonPrimeDS型の継続マウスステアリングは未実装**である。

#### Boost

現在の通常Boost既定キーはSpace。

```cpp
std::shared_ptr<Keybind> boost = key(Keys::Space);
```

処理は、

```cpp
if (_controls.Boost().IsDown() && !swipeBoost)
{
    // charge
}
else
{
    // release / fire
}
```

であり、通常の「押してチャージ、離して発動」である。

Shift長押しによる自動チャージ・自動解放は存在しない。

#### Shift競合

現在、

```cpp
std::shared_ptr<Keybind> hudOverlay = key(Keys::LeftShift);
```

となっている。

よってmelonPrimeDSと同じShift連続Boostを追加する場合、**現状のHudOverlay既定値と競合する**。

これは必ず解決すること。

---

## 3. 必須完成仕様

完成後はSamus Morph Ballで以下が同時に成立すること。

| 操作 | 動作 |
|---|---|
| WASD | 既存Roll移動 |
| マウス移動 | Morph Ballのcamera-relative steering |
| Space | 既存の通常Boost。押してチャージ、離して発動 |
| Mouse Flick | 現在のFlick方向Boost |
| Shift長押し | 自動チャージ、自動発動、再チャージを繰り返す連続Boost |
| Shift長押し＋マウス | 連続Boost中もマウスで進行方向を修正可能 |

Shift連続BoostはSamus Morph Ball専用。

Kanden、Trace、Sylux、Noxus、Spire、Weavelなど他HunterのAlt Form挙動を変更しないこと。

---

## 4. 設計原則

### 4.1 Rendererには触らない

この機能は入力・Player physics・Alt Form処理のみ。

以下は変更不要。

- Vulkan
- OpenGL
- RHI
- Skia
- Swapchain
- Reflex
- renderer frame pacing

描画バックエンド依存コードを追加しないこと。

### 4.2 Platform Raw InputをPlayerEntityから直接読まない

Morph Ball処理で使う入力は必ず、

```cpp
_input.MouseDeltaX()
_input.MouseDeltaY()
```

経由にする。

これにより、

- Windows Raw Input
- Qt fallback
- PointerDevice
- 将来のLinux/macOS relative input

を同一ロジックで扱える。

### 4.3 Hot pathでallocationしない

`ProcessAlt()` はゲームプレイhot path。

以下は禁止。

- `std::vector`の生成
- `std::string`の生成
- heap allocation
- platform API呼び出し
- config file I/O
- mutex追加

入力判定はstack/local scalarのみで処理する。

### 4.4 Mouse sampleをconsumeしない

同じフレームのmouse deltaは、

- continuous steering
- Mouse Flick判定

の両方から参照可能にする。

どちらかが `_mouseDeltaX/Y` を0にしたり、別フレームへqueueしてはならない。

melonPrimeDSと同様に、同一current-frame sampleを複数の判定がread-onlyで利用する。

---

# 5. 修正1: Shift専用Hold Morph Boost actionを追加

## 5.1 既存Boostを流用しない

現在の `Boost` は通常チャージBoostであり、既定値Space。

これをShiftへ変更してはならない。

新規actionを追加する。

推奨名:

```text
HoldMorphBoost
```

または内部名:

```text
FastMorphBoost
```

UI表示は、

```text
Hold to fast morph boost
```

相当とする。

### 5.2 PlayerControls

対象:

- `src/MphRead.Native/Entities/Players/PlayerInput.hpp`
- `src/MphRead.Native/Entities/Players/PlayerInput.cpp`

追加:

```cpp
std::shared_ptr<Keybind> _holdMorphBoost{};
```

accessor:

```cpp
HoldMorphBoost()
```

constructor parameterにも追加する。

現在 `PlayerControls::_all` は35固定。

新action追加後は36へ更新する。

```cpp
std::array<std::shared_ptr<Keybind>, 36>
```

固定値35を残さないこと。

### 5.3 Default key

`PlayerControls::CreateDefault()` で、

```cpp
holdMorphBoost = key(Keys::LeftShift);
```

とする。

既存:

```cpp
boost = key(Keys::Space);
```

はそのまま。

---

# 6. 修正2: HudOverlayのLeftShift競合を解消

現在:

```cpp
hudOverlay = key(Keys::LeftShift);
```

なので、そのまま `HoldMorphBoost = LeftShift` を追加してはならない。

推奨:

```cpp
hudOverlay = key(Keys::Unknown);
holdMorphBoost = key(Keys::LeftShift);
```

HudOverlayは任意bindingとして残し、必要なユーザーは設定から再割り当てできるようにする。

## 6.1 既存設定ファイルmigration

古い設定ファイルは全bindingを書き出しているため、

```text
HudOverlay=Key:LeftShift
```

を持っている可能性が高い。

新しい `HoldMorphBoost` keyが存在しない古い設定を読み込んだ場合のみmigrationする。

条件:

```text
HoldMorphBoostの保存行が存在しない
AND
HudOverlay == LeftShift
```

なら、

```text
HoldMorphBoost = LeftShift
HudOverlay = Unknown
```

へmigrationする。

既に `HoldMorphBoost` 行がある新形式設定ではmigrationを再実行しない。

既存ユーザーがHudOverlayを別キーへ変更している場合、その設定は維持する。

保存後は新形式として両actionを書き出す。

---

# 7. 修正3: InputSettingsへ新actionを完全登録

対象:

- `src/MphRead.Native/Mods/InputSettings.hpp`
- `src/MphRead.Native/Mods/InputSettings.cpp`

現在以下が35固定。

```cpp
std::array<InputBindingProperty, 35>
```

すべて36へ更新する。

対象例:

```cpp
_bindings
Bindings()
FindBindings()
```

`FindBindings()` に追加:

```cpp
{"HoldMorphBoost",
 [](Entities::PlayerControls& c) -> Entities::Keybind&
 {
     return c.HoldMorphBoost();
 }}
```

`_order` にも追加する。

推奨位置:

```text
Jump
Boost
HoldMorphBoost
Shoot
Zoom
Morph
...
```

`_order` の要素数も16から17へ更新する。

`ActionName()` は少なくとも、

```text
Hold morph boost
```

または、

```text
Fast morph boost
```

として自然に表示する。

Save/Load/Reset/Rebind/Apply/ApplyToPlayersの全経路で新actionが欠落しないこと。

---

# 8. 修正4: Shift長押し連続Boost

Fruity-Primeでは既にnativeな `_boostCharge` と `_altAttackCooldown` があるため、melonPrimeDSのRAM patch方式をコピーする必要はない。

Fruityのnative stateを使って同じ動作を実現する。

## 8.1 状態判定

Samus Boost処理内で以下を算出する。

```cpp
const bool manualBoostDown = _controls.Boost().IsDown();
const bool fastBoostHeld = _controls.HoldMorphBoost().IsDown();

const bool boostCooldownActive = _altAttackCooldown > 0;

const bool gaugeEnough =
    _boostCharge > _values.BoostChargeMin * 2;
```

melonPrimeDSの `boostGauge > threshold` と `cooldown/busy == 0` に対応するnative側の判定として、既存FruityのBoost thresholdと `_altAttackCooldown` を利用する。

`PlayerFlags1::Boosting` は速度低下でも解除されるため、auto-cycleの主busy判定には使わない。

## 8.2 auto release

```cpp
const bool autoRelease =
    fastBoostHeld
    && !manualBoostDown
    && !boostCooldownActive
    && gaugeEnough;
```

そして、

```cpp
const bool effectiveBoostDown =
    manualBoostDown
    || (fastBoostHeld && !autoRelease);
```

というvirtual button状態を作る。

既存Boost処理の、

```cpp
_controls.Boost().IsDown()
```

を直接増殖させず、通常Boost・Shift auto-cycle・Mouse Flickを1か所でarbitrationすること。

概念:

```cpp
if (effectiveBoostDown && !swipeBoost)
{
    if (_boostCharge < _values.BoostChargeMax * 2)
    {
        ++_boostCharge;
    }
}
else
{
    // 既存release/fire path
}
```

### 8.3 動作

Shiftを押し続けると、

```text
charge
charge
charge
...
gaugeEnough
↓
cooldown == 0
↓
1 frameだけvirtual release
↓
Boost発動
↓
_boostCharge = 0
_altAttackCooldown開始
↓
Shiftはまだ押されている
↓
次のcharge開始
↓
cooldown中もcharge継続
↓
cooldown解除後、gaugeEnoughなら再度virtual release
```

となる。

追加のtimerや独自counterを作る必要はない。

既存native stateを利用することで、melonPrimeDSと同じ「押しっぱなしで周期的にBoost」を最小stateで実現できる。

---

# 9. 通常Boostとの優先順位

通常BoostとShiftを同時に押した場合は通常Boostを優先する。

```cpp
manualBoostDown == true
```

なら、Shift側は勝手にreleaseしてはならない。

ユーザーがSpaceで手動チャージ中にShiftを押しても、そのチャージを途中発射しないこと。

Spaceを離し、Shiftがまだ押されている場合は、その時点からauto-cycleへ移行してよい。

---

# 10. Mouse Flick Boostとの競合回避

現在 `PlayerEntity::ModCheckMouseFlick()` は、

```cpp
_controls.Boost().IsDown()
```

中はFlick判定をresetしている。

新しいShift連続Boost中も同様にする。

変更:

```cpp
if (!_controls.MouseAim()
    || _controls.Boost().IsDown()
    || _controls.HoldMorphBoost().IsDown()
    || ...)
{
    Mods::Input::MouseFlick::Reset();
    return;
}
```

理由:

melonPrimeDSでもShift auto-cycle中はcustom raw swipe pulseを同時発火させない。

Shift連続BoostとFlick Boostが同じフレームに競合すると、

- charge state
- full-charge flick
- auto release

が同時に走り、二重Boostまたは意図しないcharge resetを起こす可能性がある。

したがって、

```text
Shift auto-cycle中:
    Mouse Flick Boost判定なし
    continuous mouse steeringはあり
```

とする。

---

# 11. 修正5: Morph Ball continuous mouse steering

## 11.1 適用範囲

最低限、以下の条件を満たす時だけ有効にする。

```text
IsMainPlayer
!IsBot
Hunter == Samus
IsAltForm
!IsMorphing
Controls.MouseAim == true
NoAimInput == false
WeaponMenuOpen == false
SpectatorMode == false
CameraSequence BlockInput == false
FrameAdvance == false
FrameAdvanceLastFrame == false
```

他HunterのAlt Formへ一般化しないこと。

## 11.2 入力ソース

使用するのは必ず、

```cpp
_input.MouseDeltaX()
_input.MouseDeltaY()
```

現在フレームの値。

platform-specific raw input objectへアクセスしない。

## 11.3 座標変換

既存Mouse Flickと同じ符号・basisを使う。

画面入力:

```text
mouse up    = forward
mouse down  = backward
mouse left  = left
mouse right = right
```

既存Flickと同じ定義:

```cpp
forward = -mouseDeltaY;
left = -mouseDeltaX;
```

world direction:

```cpp
worldX =
    _altRollFbX * forward
    + _altRollLrX * left;

worldZ =
    _altRollFbZ * forward
    + _altRollLrZ * left;
```

これにより、カメラを基準にMorph Ballを操作できる。

## 11.4 感度

pixel数を直接ゲームphysicsへ入れない。

既存Mouse Flickと同じく、

```cpp
delta / 4 * MouseSensitivity
```

というFruityのaim scaleを基準にnormalizationする。

continuous steering用のgainはnamed constantまたはsettingとして分離すること。

例:

```cpp
constexpr float MorphSteerFullScaleDegrees = ...;
```

ただし数値はmagic numberとして `ProcessAlt()` に直書きせず、melonPrimeDSとの実機比較で調整する。

要求:

- 小さいmouse deltaは小さいsteering
- 大きいmouse deltaは上限でclamp
- DPI依存をできるだけ小さくする
- `MouseSensitivity` を変えると自然にsteering感度も変化する
- raw mouse polling rateの違いで極端に挙動が変わらない

## 11.5 Invert設定

continuous steeringは通常aimに近い操作なので、

```cpp
InputSettings::InvertMouseX()
InputSettings::InvertMouseY()
```

を反映する。

ただし既存Mouse Flick Boost方向は現在のphysical swipe directionを維持し、invert設定によって逆転させない。

Mouse Flickの既存仕様を壊さないこと。

---

# 12. `_boostAimLock` の扱いを修正

現在Mouse Flick Boost後、

```cpp
_boostAimLock = 18;
```

となり、さらに、

```cpp
if (_boostAimLock > 0)
{
    traction = 0;
}
```

としている。

このままcontinuous mouse steeringを同じ `traction` に乗せると、Boost直後18フレームはマウス操作できない。

これは今回の目的、

```text
Shift連続Boost中もマウスでステアリングできる
```

と矛盾する。

したがってtractionを分離する。

概念:

```cpp
const float baseTraction = ...;

const float keyboardTraction =
    _boostAimLock > 0 ? 0.0F : baseTraction;

const float mouseTraction =
    baseTraction;
```

既存のRoll keyには `keyboardTraction` を使用する。

continuous mouse steeringには `mouseTraction` を使用する。

これにより、

- Flick Boost直後の意図しないWASD方向上書きは防止
- マウスによるsteeringだけは継続可能

になる。

`_boostAimLock` 自体を削除する必要はない。

---

# 13. Mouse steeringとWASDの合成

WASDを壊さない。

推奨:

1. 既存Roll keyによる `speedDelta` を維持。
2. mouse steeringを追加のcamera-relative steering contributionとして加算。
3. 最終的なsteering contributionを合理的な上限でclamp。

単純にmouse deltaを無制限加算しないこと。

特に、

```text
W + mouse up
W + mouse diagonal
A + mouse right
```

などで既存最大tractionを大幅に超えないようにする。

現在のkeyboard diagonal挙動を不用意に弱体化しないこと。

既存keyboard movementのphysics parityを優先し、その上にmouse steeringを追加する。

---

# 14. Mouse Flickとcontinuous steeringは同一sampleを使用可能

Flick判定が同じmouse deltaを読むこと自体は問題ない。

重要なのはconsumeしないこと。

```text
MouseDelta
   ├─ continuous steering
   └─ MouseFlick::Check
```

のfan-out構造にする。

Flickのために、

```cpp
_mouseDeltaX = 0;
_mouseDeltaY = 0;
```

のような処理をしてはならない。

別フレームへ遅延queueすることも禁止。

---

# 15. SRP推奨構造

`PlayerInput.cpp` をさらに巨大化させないため、Morph mouse固有ロジックは分離を推奨する。

候補:

```text
src/MphRead.Native/Mods/Input/MorphBallMouse.hpp
src/MphRead.Native/Mods/Input/MorphBallMouse.cpp

src/MphRead.Native/Mods/Input/PlayerEntityMorphBallMouse.hpp
src/MphRead.Native/Mods/Input/PlayerEntityMorphBallMouse.cpp
```

責務:

`MorphBallMouse`

- mouse delta → normalized steering intent
- clamp
- invert
- camera-space 2D入力の純粋計算

`PlayerEntityMorphBallMouse`

- PlayerEntity state gate
- `_altRollFb/_altRollLr` world basisへの投影
- `speedDelta`への適用

Shift auto-cycleは既存Boost stateとの結合が強いため、無理に別classへstateを複製せず、Boost arbitrationの小さなpure helper程度に留める。

---

# 16. C++版だけを対象とする

今回の対象は `develop3_rendering` のC++版。

C#版 `src/MphRead` のMouseFlickコメントには、

```text
nothing in ProcessAlt reads a mouse delta at all for the four hunters that roll
```

という既存設計説明がある。

今回の要求はこの挙動をC++側で意図的に拡張するものなので、C#コードを機械的に正解として戻さないこと。

ただし既存のゲームphysics、Boost damage、charge threshold、cooldown値などはFruityのnative実装を引き続き基準とする。

---

# 17. テスト追加

現状、このMorph Ball mouse/Flick周辺専用のnative test fileは見当たらない。

最低限、pure helperを切り出して自動テスト可能にすること。

## 17.1 Shift auto-cycle

テーブルテスト:

```text
manual=false fast=false charge=0 cooldown=0
→ chargeしない、fireしない

manual=true fast=false
→ 通常charge

manual=false fast=true charge不足
→ charge

manual=false fast=true charge十分 cooldown>0
→ chargeを維持

manual=false fast=true charge十分 cooldown=0
→ 1 frame release/fire

manual=true fast=true charge十分 cooldown=0
→ manual優先、勝手にauto releaseしない
```

Boost発動後、

```text
_boostCharge == 0
_altAttackCooldown > 0
```

から再びchargeへ戻ることも確認する。

## 17.2 Mouse steering math

最低限:

```text
delta = 0,0
→ steering 0

mouse up
→ forward positive

mouse down
→ backward

mouse left
→ left

mouse right
→ right

diagonal
→ 正しいcamera-relative diagonal

large delta
→ clampされる

InvertX
→ X steeringのみ反転

InvertY
→ Y steeringのみ反転
```

## 17.3 Binding

確認:

```text
Boost default = Space
HoldMorphBoost default = LeftShift
HudOverlay default != LeftShift
```

Load/Save round-tripでも同じ。

旧設定:

```text
HudOverlay=Key:LeftShift
HoldMorphBoost行なし
```

からのmigrationもテストする。

---

# 18. 実ゲーム受入テスト

Samusで以下を確認する。

### A. 通常Morph Ball

1. Morph Ballへ変形。
2. WASD操作が以前と同じ。
3. マウス上移動で前方向へsteer。
4. マウス下で後方。
5. 左右もcamera-relativeに正しい。
6. マウス停止後に勝手な入力が残らない。

### B. Mouse Flick

1. Shiftを押していない。
2. Mouse Flickで既存Boostが発動。
3. Flick方向とBoost方向が一致。
4. 既存120-degree threshold/cooldown behaviorを壊していない。

### C. Shift連続Boost

1. Morph Ball状態でShiftを押し続ける。
2. 最初のcharge後、自動的にBoostが出る。
3. Shiftを離さず次のBoostが出る。
4. 3回以上連続して発動する。
5. key repeat OS eventに依存しない。
6. Shift press edgeではなく`IsDown()` stateで動く。

### D. Shift＋mouse

1. Shiftを押し続ける。
2. 連続Boost中にmouseを左右へ動かす。
3. Boost cycleを止めずにsteerできる。
4. `_boostAimLock` によってmouse steeringが18フレーム死なない。
5. mouse steeringがBoost directionを不自然に反転させない。

### E. 通常Boost regression

1. Space押下でcharge。
2. Spaceを離す。
3. 従来どおりBoost。
4. Shift機能追加前とcharge/fire semanticsが変わっていない。

### F. 競合

1. Space + Shift同時押し。
2. Space manual chargeが勝手に途中releaseされない。
3. Shift中にMouse Flickが二重発火しない。
4. Weapon menu中にsteering/flickしない。
5. pause/camera sequence/frame advance中にmouse deltaを持ち越さない。

### G. Hunter isolation

Samus以外の全Hunterで、

```text
Shift HoldMorphBoost
Morph mouse steering
```

によるAlt Form behavior変更がないこと。

---

# 19. Performance条件

この追加による通常フレームのコストを最小化する。

最初にcheap gateする。

概念:

```cpp
if (_hunter != Hunter::Samus
    || !IsAltForm()
    || _isBot
    || !IsMainPlayer())
{
    return;
}
```

mouse deltaが0ならsqrt等を実行せずreturn。

hot pathでは、

- scalar arithmetic
- branch数最小
- allocationなし
- loggingなし

を基本とする。

DebugLogは既存同様、`DebugLog::Active()` の時だけ。

---

# 20. 変更候補ファイル

必須または高確率:

```text
src/MphRead.Native/Entities/Players/PlayerInput.hpp
src/MphRead.Native/Entities/Players/PlayerInput.cpp
src/MphRead.Native/Mods/InputSettings.hpp
src/MphRead.Native/Mods/InputSettings.cpp
src/MphRead.Native/Mods/Input/PlayerEntityMouseFlick.cpp
```

SRP分離する場合:

```text
src/MphRead.Native/Mods/Input/MorphBallMouse.hpp
src/MphRead.Native/Mods/Input/MorphBallMouse.cpp
src/MphRead.Native/Mods/Input/PlayerEntityMorphBallMouse.hpp
src/MphRead.Native/Mods/Input/PlayerEntityMorphBallMouse.cpp
```

build systemへ新 `.cpp` の追加が必要なら、現在のCMake source collection方式に従って登録する。

---

# 21. 変更してはいけないもの

今回の作業では以下を触らない。

```text
Renderer API
Vulkan backend
OpenGL backend
shader
RHI resource lifetime
NVIDIA Reflex
network protocol
server packet format
game assets
Hunter physics constantsそのもの
```

`RollAltTraction`、Boost speed、damage等の既存metadata値を変更して操作感を作るのは禁止。

操作入力の変換側で実現する。

---

# 22. Definition of Done

以下をすべて満たした時だけ完了とする。

- [ ] `HoldMorphBoost` actionが追加されている
- [ ] Shiftが既定のHoldMorphBoost
- [ ] Space通常Boostが維持されている
- [ ] HudOverlayとのLeftShift競合がない
- [ ] 旧設定migrationがある
- [ ] Shift押しっぱなしでBoostが3回以上自動継続する
- [ ] Shift中にmouse steering可能
- [ ] 通常Morph Ball中にもmouse steering可能
- [ ] Mouse Flick Boostが維持されている
- [ ] Shift中はMouse Flick Boostが競合しない
- [ ] `_boostAimLock` がmouse steeringをブロックしない
- [ ] WASD Roll操作がregressionしていない
- [ ] Samus以外のAlt Formを変更していない
- [ ] Raw InputとQt fallbackのどちらでも同じgameplay pathを通る
- [ ] input hot pathにallocationを追加していない
- [ ] native unit tests追加
- [ ] CTest green
- [ ] Windows MSVC Release build green
- [ ] Linux build green
- [ ] 既存renderer testsに影響なし

---

# 23. 最終的に目指す操作体系

```text
Biped
  Mouse         → Aim
  WASD          → Move

Samus Morph Ball
  WASD          → Roll
  Mouse         → Continuous steering
  Mouse Flick   → Directional instant Boost
  Space hold    → Manual charge
  Space release → Manual Boost
  Shift hold    → Automatic repeated charge/release Boost
  Shift + Mouse → Repeated Boost while steering
```

これを最終仕様とする。

特に重要なのは、

```text
Shift連続Boostを追加するために通常Boostを置き換えない
Mouse Flickを削除しない
Boost中にmouse steeringを止めない
platform raw inputをPlayerEntityへ漏らさない
```

の4点である。
