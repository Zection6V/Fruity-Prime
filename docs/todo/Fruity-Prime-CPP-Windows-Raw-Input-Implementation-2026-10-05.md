# Fruity-Prime C++ Windows Raw Input 調査・実装指示

- 対象リポジトリ: `Zection6V/Fruity-Prime`
- 対象ブランチ: `develop3_rendering`
- 調査対象HEAD: `a3ac455573a4bb3026db6cc2fc73a825b6dbb54d`
- 比較対象: `ag-advania/melonPrimeDS`
- melonPrimeDS比較HEAD: `c4165c87416902bb13b670e3147ecf05988017ed`
- 調査日: 2026-10-05
- 調査時判定: **IMPLEMENTATION REQUIRED**
- 実装状況（2026-10-05）: **IMPLEMENTED / ACCEPTANCE PENDING** — 実装・ローカル検証・Windows/Linux/macOS通常CI完走済み（変更後run 15/15成功）。実マウスの全polling rate評価、実DPI、pen等のhardware/UI matrix確認が残る。

1〜17節は調査対象HEAD時点の調査・実装指示。現在の実装結果、受入証拠、残る確認は18節に記録する。

## 1. 結論

`develop3_rendering` の現行C++版には、WindowsのWin32 Raw Inputを使ったマウス入力経路は実装されていない。

少なくとも実際にWindowsデスクトップのゲームウィンドウと入力を所有している現行Qt経路には、次のRaw Input API・メッセージ処理が存在しない。

- `RegisterRawInputDevices`
- `WM_INPUT`
- `GetRawInputData`
- `GetRawInputBuffer`
- `RAWINPUT`
- `RIDEV_*`

また、GLFWラッパーにも `GLFW_RAW_MOUSE_MOTION` / `glfwRawMouseMotionSupported` / raw mouse用のwindow input modeは存在しない。現行CMakeでは、デスクトップウィンドウはQtが所有し、GLFWはゲームパッドAPI用途として残っている。

したがって「GLFWが内部的にRaw Inputを使っているから実装済み」とみなすこともできない。ゲームウィンドウ自体がQtであり、Fruity-Prime側の現在の相対マウス処理はQtイベントとカーソルワープを使っている。

## 2. 現行Fruity-Primeの入力経路

### 2.1 デスクトップウィンドウはQt所有

`CMakeLists.txt` は現行構成を明示している。

> Qt owns every native menu; GLFW remains only for desktop gamepads.

参照:

- `CMakeLists.txt` L211付近
  - https://github.com/Zection6V/Fruity-Prime/blob/a3ac455573a4bb3026db6cc2fc73a825b6dbb54d/CMakeLists.txt#L211
- Qtソース収集
  - https://github.com/Zection6V/Fruity-Prime/blob/a3ac455573a4bb3026db6cc2fc73a825b6dbb54d/CMakeLists.txt#L573
- Qtオブジェクトをnative本体へ組み込み
  - https://github.com/Zection6V/Fruity-Prime/blob/a3ac455573a4bb3026db6cc2fc73a825b6dbb54d/CMakeLists.txt#L619

Raw Inputを実装する場所は、GLFWラッパーではなくQt platform/window層を第一候補とする。

### 2.2 現行WindowsマウスはQMouseEventの座標差分

`src/MphRead.Native.Qt/Platform/QtRendererPlatform.cpp` の現行処理は次の構造。

1. `QtWindow::Run()` がQtイベントをpumpする。
2. `QEvent::MouseMove` を `QtWindow::MouseMove()` へ送る。
3. grab中はイベント位置とウィンドウ中央の差を相対移動量として仮想カーソルへ加算する。
4. 各move後に `RecentreGrabbedCursor()` を呼ぶ。
5. `QCursor::setPos()` で実カーソルを中央へ戻す。
6. その差分を `MouseMoveEventArgs::DeltaX/DeltaY` としてゲーム側へ送る。

参照:

- `QtWindow::Run`
  - https://github.com/Zection6V/Fruity-Prime/blob/a3ac455573a4bb3026db6cc2fc73a825b6dbb54d/src/MphRead.Native.Qt/Platform/QtRendererPlatform.cpp#L361-L405
- `QtWindow::Cursor`
  - https://github.com/Zection6V/Fruity-Prime/blob/a3ac455573a4bb3026db6cc2fc73a825b6dbb54d/src/MphRead.Native.Qt/Platform/QtRendererPlatform.cpp#L454-L470
- `QtWindow::MouseMove`
  - https://github.com/Zection6V/Fruity-Prime/blob/a3ac455573a4bb3026db6cc2fc73a825b6dbb54d/src/MphRead.Native.Qt/Platform/QtRendererPlatform.cpp#L759-L790
- `QtWindow::RecentreGrabbedCursor`
  - https://github.com/Zection6V/Fruity-Prime/blob/a3ac455573a4bb3026db6cc2fc73a825b6dbb54d/src/MphRead.Native.Qt/Platform/QtRendererPlatform.cpp#L827-L837

これは「相対マウス風」の実装ではあるが、Win32 Raw Inputではない。

特にWindowsではOSポインタ座標を介したイベント差分とカーソルワープに依存するため、Raw Inputの `RAWMOUSE::lLastX/lLastY` を直接取得する経路とは性質が異なる。

### 2.3 プレイヤー照準はMouseStateのフレーム間座標差分

通常のプレイヤー入力はイベントの `DeltaX/DeltaY` を直接消費していない。

`PlayerEntity::PlayerInput::UpdatePointer()` は、現在と前回の `MouseState` の位置差から照準deltaを作る。

参照:

- `UpdatePointer`
  - https://github.com/Zection6V/Fruity-Prime/blob/a3ac455573a4bb3026db6cc2fc73a825b6dbb54d/src/MphRead.Native/Entities/Players/PlayerInput.cpp#L346-L357
- previous/current mouse snapshot
  - https://github.com/Zection6V/Fruity-Prime/blob/a3ac455573a4bb3026db6cc2fc73a825b6dbb54d/src/MphRead.Native/Entities/Players/PlayerInput.cpp#L2541-L2544

Qtのframe loopでは `_mouse.X/_mouse.Y` を現在のvirtual cursorから設定した後に `QCoreApplication::processEvents()` を呼んでいるため、現行の通常プレイヤー照準は「そのevent pumpで到着した最新move」を次のMouseState snapshotまで持ち越す構造になっている。

Raw Input導入時は、この点も同時に改善し、event pump後にRaw deltaをMouseStateへ反映してからsimulationへ進める。

## 3. melonPrimeDSで確認できたRaw Input実装

melonPrimeDSにはWindows Raw Inputが明示的に実装されている。

主要ファイル:

- `src/frontend/qt_sdl/MelonPrimeRawInputWinFilter.h`
- `src/frontend/qt_sdl/MelonPrimeRawInputWinFilter.cpp`
- `src/frontend/qt_sdl/MelonPrimeRawInputState.h`
- `src/frontend/qt_sdl/MelonPrimeRawInputState.cpp`
- `src/frontend/qt_sdl/MelonPrimeRawWinInternal.*`

Windows実装では次を行っている。

- `RegisterRawInputDevices`
- `WM_INPUT`
- `GetRawInputData`
- `GetRawInputBuffer`
- mouse/keyboardのRaw Input state
- relative mouse `lLastX/lLastY` の累積
- absolute mouse sampleの除外
- wheel/button state
- hidden Raw Input sink
- multi-instance ownership
- Joy2Key互換経路
- batch drain
- stuck-state recovery
- aim直前のlate-latch

参照:

- device registration
  - https://github.com/ag-advania/melonPrimeDS/blob/c4165c87416902bb13b670e3147ecf05988017ed/src/frontend/qt_sdl/MelonPrimeRawInputWinFilter.cpp#L818-L866
- native `WM_INPUT` filter
  - https://github.com/ag-advania/melonPrimeDS/blob/c4165c87416902bb13b670e3147ecf05988017ed/src/frontend/qt_sdl/MelonPrimeRawInputWinFilter.cpp#L868
- late-latch
  - https://github.com/ag-advania/melonPrimeDS/blob/c4165c87416902bb13b670e3147ecf05988017ed/src/frontend/qt_sdl/MelonPrimeRawInputWinFilter.cpp#L604-L630
- single-event Raw Input parsing
  - https://github.com/ag-advania/melonPrimeDS/blob/c4165c87416902bb13b670e3147ecf05988017ed/src/frontend/qt_sdl/MelonPrimeRawInputState.cpp#L106-L195
- buffered Raw Input
  - https://github.com/ag-advania/melonPrimeDS/blob/c4165c87416902bb13b670e3147ecf05988017ed/src/frontend/qt_sdl/MelonPrimeRawInputState.cpp#L196-L329
- delta fetch
  - https://github.com/ag-advania/melonPrimeDS/blob/c4165c87416902bb13b670e3147ecf05988017ed/src/frontend/qt_sdl/MelonPrimeRawInputState.cpp#L330

melonPrimeDSの実装品質は高いが、その全構造をFruity-Primeへそのまま移植してはいけない。

melonPrimeDSはmulti-instance、エミュレーションthread、Joy2Key、raw keyboard hotkey、hidden window、Qt fallbackなど、Fruity-Primeには不要な要件も解いている。Fruity-Primeの現行構造に対して丸ごと移植するとSRPを崩し、入力経路を必要以上に複雑化する。

## 4. 実装方針

### 4.1 第1段階のスコープ

Windowsの**Raw Mouse Motionだけ**を実装する。

Raw Inputで所有するもの:

- grab中のゲーム内camera/aim用 relative X/Y

Qtに残すもの:

- keyboard
- mouse button
- wheel
- text input
- launcher/menuのabsolute pointer
- pen
- touch
- stylus placement
- window focus/event handling

第1段階ではraw keyboardを実装しない。

理由は、現在の問題はマウス照準の相対入力品質・入力遅延であり、keyboard/buttonまでRaw Inputへ移すとKeybind、chat、Qt Quick UI、press/release edge、focus recoveryの変更範囲が急増するため。

### 4.2 `RIDEV_NOLEGACY` は使用しない

Raw Input登録で `RIDEV_NOLEGACY` を付けない。

Qtのmouse button、wheel、menu pointerは従来イベントを引き続き必要とする。legacy mouse eventを止めるとQt UIと既存binding処理に副作用が出る。

Raw motionとQt mouse moveが二重加算されないようにする処理は、`QtWindow::MouseMove()` 側で「Raw aim capture中はQt moveをaimへ流さない」ことで解決する。

### 4.3 `RIDEV_INPUTSINK` は第1段階では使用しない

第1段階はforegroundのgame HWNDへflags=0でmouseを登録する。

バックグラウンドRaw Inputは不要であり、focusを失った状態でdeltaを蓄積すると、復帰時のcamera jumpやownershipの複雑化につながる。

### 4.4 Raw deltaにDPI scaleを掛けない

`RAWMOUSE::lLastX/lLastY` はRaw relative motionとして扱う。

Qtのlogical pixel / devicePixelRatioとの変換をRaw deltaへ適用してはいけない。

既存のmouse sensitivityはゲーム側で従来どおり適用する。

## 5. 新規クラス

次の2ファイルを追加する。

```text
src/MphRead.Native.Qt/Platform/WindowsRawMouseInput.hpp
src/MphRead.Native.Qt/Platform/WindowsRawMouseInput.cpp
```

責務は「Win32 Raw mouse eventを収集し、frame側へrelative deltaを渡す」だけに限定する。

Renderer、gameplay、menuの判断をこのクラスへ入れない。

想定インターフェース:

```cpp
class WindowsRawMouseInput final : public QAbstractNativeEventFilter
{
public:
    WindowsRawMouseInput() = default;
    ~WindowsRawMouseInput() override;

    bool Attach(HWND hwnd);
    void Detach() noexcept;

    void SetCapture(bool enabled) noexcept;
    [[nodiscard]] bool Available() const noexcept;
    [[nodiscard]] bool CaptureActive() const noexcept;

    [[nodiscard]] std::pair<std::int32_t, std::int32_t> TakeDelta() noexcept;
    void Discard() noexcept;

    bool nativeEventFilter(
        const QByteArray& eventType,
        void* message,
        qintptr* result) override;

private:
    HWND _target = nullptr;
    bool _registered = false;
    bool _capture = false;
    std::int64_t _pendingX = 0;
    std::int64_t _pendingY = 0;
};
```

実際のnamespace・include方針は周辺のQt platformコードへ合わせる。

Windows固有ヘッダは `.cpp` 内へ閉じ込める。Windows型をhppへ露出させたくない場合は `void*` native handleまたはPImplにしてよい。

## 6. 登録処理

game `QWindow` のnative handleが確立した後にmouse TLCだけを登録する。

基本形:

```cpp
RAWINPUTDEVICE device{};
device.usUsagePage = 0x01; // Generic Desktop Controls
device.usUsage = 0x02;     // Mouse
device.dwFlags = 0;
device.hwndTarget = hwnd;

if (!RegisterRawInputDevices(&device, 1, sizeof(device)))
{
    // GetLastErrorを一度記録し、Raw Input unavailableとしてQt fallbackへ。
}
```

解除:

```cpp
RAWINPUTDEVICE device{};
device.usUsagePage = 0x01;
device.usUsage = 0x02;
device.dwFlags = RIDEV_REMOVE;
device.hwndTarget = nullptr;

RegisterRawInputDevices(&device, 1, sizeof(device));
```

### 6.1 既存registrationを奪わないこと

Raw Inputは同一process内でdevice classごとに登録先windowが一つである。

登録前に `GetRegisteredRawInputDevices()` で既存のmouse registrationを確認する診断を追加する。

- 未登録: Fruity-Prime game HWNDへ登録
- 同じgame HWND: 継続可能
- 別HWNDへ既登録: warningを出して**fail closed**し、既存Qt warp fallbackへ戻す

少なくとも第1段階では、他のprocess-internal ownerを無条件に上書きしない。

## 7. Qt native event filter

Windowsでは `QAbstractNativeEventFilter` で `MSG*` を取得する。

処理対象は `WM_INPUT` のみ。

概念実装:

```cpp
bool WindowsRawMouseInput::nativeEventFilter(
    const QByteArray&,
    void* message,
    qintptr*)
{
    if (!_registered)
    {
        return false;
    }

    auto* msg = static_cast<MSG*>(message);
    if (msg == nullptr || msg->message != WM_INPUT)
    {
        return false;
    }

    RAWINPUT raw{};
    UINT size = sizeof(raw);
    const UINT read = GetRawInputData(
        reinterpret_cast<HRAWINPUT>(msg->lParam),
        RID_INPUT,
        &raw,
        &size,
        sizeof(RAWINPUTHEADER));

    if (read == UINT(-1) || read == 0)
    {
        // failure counter
        return false;
    }

    if (_capture
        && raw.header.dwType == RIM_TYPEMOUSE
        && (raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
    {
        _pendingX += raw.data.mouse.lLastX;
        _pendingY += raw.data.mouse.lLastY;
    }

    return false;
}
```

重要:

- `MOUSE_MOVE_ABSOLUTE` はaim deltaとして使用しない。
- `nativeEventFilter()` は `false` を返す。
- `WM_INPUT` をFruity側だけで飲み込んではいけない。
- foreground `WM_INPUT` の通常Win32 cleanupをQt/WindowProc側へ継続させる。
- mouse button / wheel / keyboardはここで処理しない。
- hot pathでheap allocation、logging、mutex、formattingを行わない。
- accumulatorは同じGUI/event threadでproducer/consumerが完結するならatomic化しない。thread ownershipが変わる場合だけ同期を導入する。

## 8. `QtRendererPlatform.cpp` 統合

### 8.1 `QtWindow` にRaw mouse ownerを追加

Windows限定で例えば次を持つ。

```cpp
std::unique_ptr<WindowsRawMouseInput> _rawMouse;
```

`_window->create()` 後、`winId()` が確立した時点でattachする。

OpenGL/Vulkanの双方が同じQtWindow layerを通るため、renderer別にRaw Inputを二重実装しない。

### 8.2 grab開始

`QtWindow::Cursor(CursorState::Grabbed)` で:

1. blank cursor
2. existing mouse grab
3. Raw Inputがavailableなら `SetCapture(true)`
4. stale deltaをdiscard
5. virtual cursor / MouseState baselineを同期
6. 必要なら実カーソルを**一度だけ**中央へ置く

Raw Inputが有効な間は、各mouse eventごとの `RecentreGrabbedCursor()` を停止する。

### 8.3 grab終了

normalへ戻す時:

1. `SetCapture(false)`
2. pending deltaをdiscard
3. mouse grab解除
4. cursor restore

解除後に古いRaw deltaを次のgameplay frameへ持ち越さない。

### 8.4 `MouseMove(QMouseEvent*)`

Raw capture中:

- Qt `QMouseEvent` の位置差分をgame aimへ送らない
- cursor warpもしない
- absolute UI pointer用途へも使わない。grab中はgame aim ownerがRaw側だからである

Raw unavailable時:

- 現行 `QMouseEvent + RecentreGrabbedCursor()` 実装をそのままfallbackとして残す

これによりRaw + Qtの二重加算を禁止する。

## 9. frame境界でのRaw delta反映

現行 `QtWindow::Run()` は概略として:

```text
low-latency admission/sleep
OnInputSample
MouseStateへvirtual cursorをcopy
QCoreApplication::processEvents
OnRenderFrame
```

Raw Input導入後は、Qt event pumpの**直後かつsimulation/render callbackの前**にRaw deltaを回収する。

推奨順序:

```text
low-latency admission/sleep
OnInputSample
QCoreApplication::processEvents
TakeDelta
virtual cursor / MouseStateへRaw deltaを反映
必要なら集約したOnMouseMoveを1回発火
OnRenderFrame
```

具体的にはRaw capture中だけ:

```cpp
const auto [dx, dy] = _rawMouse->TakeDelta();

if (dx != 0 || dy != 0)
{
    _cursorX += static_cast<float>(dx);
    _cursorY += static_cast<float>(dy);

    _mouse.X = _cursorX;
    _mouse.Y = _cursorY;

    MouseMoveEventArgs args{};
    args.X = _cursorX;
    args.Y = _cursorY;
    args.DeltaX = static_cast<float>(dx);
    args.DeltaY = static_cast<float>(dy);

    if (_events != nullptr)
    {
        _events->OnMouseMove(args);
    }
}
```

これにより通常プレイヤー側の既存 `MouseState.X/Y - PrevMouseState.X/Y` 計算にも、今回のevent pumpで到着したRaw deltaが同じsimulation frameで見える。

`RenderWindow::OnInputSample()` のLow Latency markerはRaw Input取得より前に維持し、InputSample以降に届いた入力を可能な限り今回のframeへ含める。

## 10. Focus / pause / UI / stylus

### Focus loss

`FocusOut` で最低限:

- Raw capture停止
- pending Raw delta discard

focusがない状態のmotionを再開後へ持ち越さない。

### Menu / launcher / chat / pause / end screen

現在 `RenderWindow` が算出するgrab条件をRaw captureのauthorityとして再利用する。

grab=falseならRaw aim contributionは必ず0。

独自にもう一つgameplay state machineをRaw inputクラス内へ作らない。

### Pen / stylus

`WindowsPenInput`、`PointerDevice`、`StylusZone` は変更しない。

stylus modeでは既存grab判定がgame mouse captureを外すため、Raw mouseとpen deltaを混ぜない。

## 11. melonPrimeDSから取り込むべきもの / 取り込まないもの

### 取り込む

- Windows Raw Input登録の基本
- `WM_INPUT` / `GetRawInputData`
- `RAWMOUSE::lLastX/lLastY`
- absolute sample除外
- cumulative deltaモデル
- stale delta discard
- failure時fallback
- telemetry思想
- 高polling rateを前提にhot pathをallocation-freeにする方針

### 第1段階では取り込まない

- raw keyboard
- raw hotkey system
- multi-instance subscription
- process-wide owner transfer
- hidden Raw Input window
- `RIDEV_INPUTSINK`
- Joy2Key routing
- stuck keyboard/button recovery
- `GetRawInputBuffer` batch drain
- `NtUser*` private/internal API fast path
- macOS/Linux Raw Input backend

これらはFruity-Primeの現行要件を超える。

## 12. 4kHz / 8kHz向け第2段階

最初からmelonPrimeDSの `GetRawInputBuffer` 実装を移植しない。

理由:

- `WM_INPUT` message dispatchとbuffer drainはlifetime/orderが難しい。
- melonPrimeDS自身も `GetRawInputBuffer` と `GetRawInputData` の共有状態、message drain、`DefRawInputProc`、stuck input recoveryに多数の修正を積み重ねている。
- Fruity-Primeはsingle-windowであり、まず標準 `GetRawInputData` 経路を計測する方が安全。

第1段階に軽量telemetryを用意する。

最低限:

```text
raw_registered
raw_capture_active
wm_input_events_per_frame
raw_nonzero_samples_per_frame
raw_get_data_failures
raw_dx_sum
raw_dy_sum
qt_fallback_frames
max_wm_input_events_per_frame
```

4kHz / 8kHz環境でmessage dispatchが実測ボトルネックになった場合だけ、第2段階としてmelonPrimeDSの次の考え方を移植する。

- fixed-size scratch
- `GetRawInputBuffer`
- batch accumulation
- `DefRawInputProc`
- late-latch
- overflow/failure handling

ただしmelonPrimeDSのmulti-instance/keyboard/hotkey部分は混ぜない。

## 13. 診断・fallbackスイッチ

少なくとも開発中は次を追加する。

```text
FRUITY_RAW_MOUSE=0
```

意味:

- Windows Raw Mouse Motionを強制無効
- 現行Qt warp pathへ戻す
- A/B比較、回帰切り分け、互換性確認に使用

任意で:

```text
FRUITY_RAW_MOUSE_DIAGNOSTICS=1
```

通常buildでframeごとのlogを出してはいけない。診断有効時でも一定間隔で集約出力する。

Raw Input registration失敗はfatalにしない。現行Qt経路へ確実にfallbackする。

## 14. テスト項目

### 静的 / 単体

Raw packet解釈をOS APIから分離できるなら、次をテストする。

- relative X/Y accumulation
- Xのみ
- Yのみ
- zero delta
- `MOUSE_MOVE_ABSOLUTE` を無視
- capture=falseで無視
- `TakeDelta()` 後に0
- `Discard()` 後に0
- grab transitionでstale deltaを持ち越さない
- 64bit accumulatorでburst中にoverflowしない

### Windows実機

最低限次を確認する。

| 項目 | 必須結果 |
|---|---|
| 125Hz mouse | 正常 |
| 1000Hz mouse | 正常 |
| 4000Hz mouse | 正常 |
| 8000Hz mouse | 正常、frame time劣化なし |
| Windowed | 正常 |
| Borderless fullscreen | 正常 |
| DPI 100% | 正常 |
| DPI 150% | sensitivity変化なし |
| DPI 200% | sensitivity変化なし |
| OpenGL | 正常 |
| Vulkan | 正常 |
| OpenGL → Vulkan switch | stale/duplicate inputなし |
| Vulkan → OpenGL switch | stale/duplicate inputなし |
| Alt+Tab | 復帰時jumpなし |
| Focus loss/regain | stale deltaなし |
| Pause | aim停止 |
| Chat | aim停止、text正常 |
| Launcher/menu | absolute pointer正常 |
| End screen | pointer/click正常 |
| Pen/stylus | 従来動作維持 |
| Mouse buttons | Qt経路で従来動作維持 |
| Wheel | Qt経路で従来動作維持 |
| Raw registration failure | Qt fallbackで操作可能 |
| `FRUITY_RAW_MOUSE=0` | Qt fallbackで操作可能 |

## 15. 受入条件

すべて満たした時だけ完了とする。

1. Windows grab中のaim/camera movement sourceが `RAWMOUSE::lLastX/lLastY` になる。
2. Raw capture中にQt mouse moveとの差分を二重加算しない。
3. Raw deltaへdevicePixelRatioを掛けない。
4. current event pumpで取得したRaw deltaを同じsimulation frameへ反映する。
5. menu/chat/pause/end screen/stylusではRaw aimを混入させない。
6. focus loss / grab解除でpending deltaを破棄する。
7. OpenGL/Vulkanの双方が同一Raw Input implementationを共有する。
8. renderer switch後も新window handleで正常に再登録される。
9. registration失敗時に現行Qt warp pathへ安全にfallbackする。
10. mouse button、wheel、keyboard、text、penの既存挙動に回帰がない。
11. 4kHz / 8kHz mouseでイベント欠落・camera jump・stuck input・顕著なframe time悪化がない。
12. `FRUITY_RAW_MOUSE=0` で旧経路とのA/Bが可能。
13. Windows以外のbuildへWin32 header/typeを漏らさない。
14. Windows/Linux/macOSの通常CI buildを壊さない。

## 16. 実装優先順位

### P0

- `WindowsRawMouseInput`
- Win32 mouse registration
- native event filter
- relative delta accumulator
- `QtWindow` grab integration
- Qt move double-count防止
- frame pump後のsame-frame delta反映
- focus/grab reset
- safe Qt fallback

### P1

- diagnostics
- registration collision detection
- failure injection / forced fallback
- high-polling test
- renderer switch validation

### P2

実測で必要な場合のみ:

- `GetRawInputBuffer`
- batch drain
- late-latch強化

## 17. 最終判断

**Fruity-Prime C++ `develop3_rendering` には、Windows Raw Inputは現時点で未実装。実装する価値がある。**

ただし、melonPrimeDSのRaw Input subsystemをそのままコピーするのではなく、Fruity-PrimeではQt platform layerに「Windows raw relative mouse motion」という単一責務で追加する。

最初の完成形は次の構造を目標とする。

```text
Win32 WM_INPUT
    ↓
WindowsRawMouseInput
    ↓
frame-local accumulated dx/dy
    ↓
QtWindow::Run event-pump boundary
    ↓
virtual MouseState + aggregated MouseMoveEventArgs
    ↓
existing RenderWindow / PlayerInput
```

この構造なら、現在のRenderer/RHIから入力実装を分離したまま、OpenGL/Vulkan共通でRaw Inputを使用できる。

また、Raw Inputが使えない環境では現在のQt `QMouseEvent + QCursor::setPos()` pathをfallbackとして残せるため、導入リスクを限定できる。


## 18. 実装結果と検証（2026-10-05）

### 実装した責務

- `Platform/WindowsRawMouseInput.*`: foreground mouse TLC登録、既存owner検出、native filter、固定stack packet reader、relative accumulation、capture/reset、failure fallback、集約telemetry。
- `Platform/RawMouseMotion.hpp`: OS/Qt非依存のGUI thread用64bit累積。absolute sampleと非capture sampleを除外し、take/discard/capture transitionで消費・破棄。極端な累積でもsigned overflowを起こさない。
- `QtRendererPlatform.cpp`: GL/NoApiで同じownerを使用。native surface破棄前にdetachし、再作成時にattach。raw中のQt motion/毎event warpを抑止。input marker後にQt pumpを実施し、rendererのcapture条件を再確認してからrawをsimulation callback前にMouseStateと集約eventへ反映。
- `Renderer.cpp/.hpp`: 既存grab条件を`UpdateCursorCapture`へ集約。focus/chat条件を追加し、pump後およびkey event後にも適用。同じpumpでchatを開いて閉じても、その間のpending raw motionを持ち越さない。raw ownerへgameplay state machineを追加していない。raw readerは非capture/menu/failure fallback中、dispatch counterだけを記録して不要なpacket decodeを省略する。
- raw解除時には実カーソル位置からabsolute pointerを戻し、最初のmenu clickがunbounded aim座標を参照しない。
- `MouseState::PositionEpoch/MotionValid`と`DeltaFrom`で、virtual/absolute座標の切替・focus loss/regain・rendererによるwindow再生成をまたいだsnapshotから照準deltaを生成しない。`PlayerInput`のpen経路は従来どおり`PointerDevice::TakeDelta()`を使用する。free cameraも非focus時のabsolute motionを取り込まない。
- desktop専用`RenderWindow::OnInputEventsProcessed` override宣言はAndroidから除外する。初回CIでAndroidのundefined-symbolを検出し、共通headerの宣言範囲を修正した。
- button/wheel/keyboard/text/penはQt/既存経路のまま。`WindowsPenInput`、`PointerDevice`、`StylusZone`の実装は変更していない。

`WM_INPUT` filterは常にfalseを返し、Qt/WindowProcの通常cleanupを継続させる。hot pathでheap/log/mutexを使用しない。登録フラグは0、解除は`RIDEV_REMOVE`のみ。
Win32のforeground registrationは同じprocess内の別HWNDがforegroundでもtargetにpacketを届ける場合があるため、capture状態に加えて`GetForegroundWindow() == game HWND`を確認する。
API仕様確認: [WM_INPUT cleanup](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-input)、[Raw Input overview](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-raw-input)。

### 開発用スイッチ

| 環境変数 | 動作 |
|---|---|
| `FRUITY_RAW_MOUSE=0` | raw登録を行わず従来Qt warpへ戻す |
| `FRUITY_RAW_MOUSE_DIAGNOSTICS=1` | 2秒間隔で集約counterを出力。通常frame logなし |
| `FRUITY_RAW_MOUSE_FAIL_REGISTER=1` | 登録失敗を注入し、既存registrationを変更せずQt fallbackへ戻す |
| `FRUITY_MOUSECHECK=1` | raw capture中のQt motion二重加算を検出。fallback中は従来の水平/垂直drift検査 |

### 実施した検証

| 検証 | 結果と証拠 |
|---|---|
| Windows MSVC Release | `tools/build/build-cpp.bat msvc Release` 成功。`tools/build/out/raw-mouse-build-final.log` |
| CTest | **25/25 PASS**。`RawMouseMotion`/`WindowsRawMouseInput`を含む。`tools/build/out/raw-mouse-ctest.log` |
| portable motion contract | X/Y、zero、absolute除外、inactive除外、take/discard、grab/focus transition、64bit burst、saturationを検証。804000 synthetic samplesも一致。実機8kHz試験ではない |
| Win32/Qt stress | `--stress`: 134 packet burstを600回、GL/NoApi各80400件、送信数と消費数が完全一致。same-frame/aggregation/focus/button/wheel/textもPASS。集約telemetryのread failure=0、max events/frame=134。`tools/build/out/raw-mouse-live-stress.log`。SendInput生成・検証用二重read・イベント処理を含む時間のため実マウスのframe-time評価ではない |
| Win32 registration | 強制無効、登録失敗、別owner拒否、同じHWNDのborrow、所有registration解除、後から別ownerになった場合の保護、新HWNDへの再登録、focus停止、data-read失敗を検証 |
| Qt live window | 実`SendInput → WM_INPUT → GetRawInputData → QtWindow::Run`経路。GLとNoApi/Vulkan windowで各24 packetを3 frameへ集約。current-pump delta、MouseState先行反映、二重加算防止、Qt button/wheel/text維持を確認 |
| focus移動/復帰 | 別native HWNDへ本当にfocusを移し、background motionを与えてから復帰。background aim混入と復帰後stale deltaなし。live window試験内 |
| snapshotの照準delta | live window試験でraw解除・background・replacement windowの`DeltaFrom`が0、通常pumpの`DeltaFrom`が実Raw packet sumに一致することを検証。capture/focus metadataを含む実`MouseState`を使用 |
| windowed/borderless fullscreen | live window試験のGLはwindowed、NoApiはborderless fullscreen |
| Qt scale 100/150/200% | 全caseでlive window試験PASS。`raw-mouse-live-window.log`、`raw-mouse-live-dpi150.log`、`raw-mouse-live-dpi200.log`。`QT_SCALE_FACTOR`による試験であり、Windows設定UIの実DPI変更を代替したとの主張ではない |
| 実ゲームrenderer switch | `MP1 SANCTORUS`、spawn済みplayerとbots、一時停止・復帰・試合内3回switch・launcher復帰でPASS。全5 windowでraw再登録。`tools/build/out/raw-mouse-switch.log` |
| forced fallbackで実ゲームswitch | `FRUITY_RAW_MOUSE=0`でも同じswitch harness PASS。水平256 moveのdriftなし、垂直入力維持。`tools/build/out/raw-mouse-fallback.log` |
| registration failureで実ゲーム | failure注入 + Vulkanで起動、試合、pause、launcher復帰とfallback mousecheck PASS。`tools/build/out/raw-mouse-failed-registration.log` |
| 非Windows header/type guard | Android NDK Clang x86_64 API28 + Qt Android headersで`WindowsRawMouseInput.cpp`のsyntax check PASS。Linux/macOS full buildの代替ではない |
| Android x86_64実リンク | desktop override宣言修正後、NDKの`fruity_mphread_native_android` target build/link PASS。`tools/build/out/raw-mouse-android-x86_64-build.log` |
| Android arm64実リンク | 同じ修正後、NDKの`fruity_mphread_native_android` target build/link PASS。`tools/build/out/raw-mouse-android-arm64-build.log` |
| 修正後macOS CI | SHA `f6adc2e0`の[macOS / Clang job](https://github.com/Zection6V/Fruity-Prime/actions/runs/37224652685/job/111501738497)成功。実job logで`FruityPrime.RawMouseMotion` PASS・Reflexを合わせたCTest 2/2、通常build/package/uploadを確認。`tools/build/out/raw-mouse-ci-macos-final.log` |
| 修正後Linux CI | 同SHAの[Linux / GCC job](https://github.com/Zection6V/Fruity-Prime/actions/runs/37224652685/job/111501738541)成功。実job logで`FruityPrime.RawMouseMotion` PASS・Reflexを合わせたCTest 2/2、通常build/package/uploadを確認。`tools/build/out/raw-mouse-ci-linux-final.log` |
| 修正後Windows CI | 同SHAの[Windows / MSVC job](https://github.com/Zection6V/Fruity-Prime/actions/runs/37224652685/job/111501738607)成功。実job logで`RawMouseMotion`と`WindowsRawMouseInput` PASS、Reflexを合わせたCTest **3/3**、通常build/package、GL/Vulkan/Qt backend contract、artifact uploadを確認。`tools/build/out/raw-mouse-ci-windows-final.log` |
| 修正後Android x86_64 CI | 同SHAの[NDK x86_64 job](https://github.com/Zection6V/Fruity-Prime/actions/runs/37224652685/job/111501766018)成功。初回のundefined-symbol failureは修正後jobで再発しない |
| 修正後Android arm64 CI | 同SHAの[NDK arm64-v8a job](https://github.com/Zection6V/Fruity-Prime/actions/runs/37224652685/job/111501766045)成功。初回のundefined-symbol failureは修正後jobで再発しない |
| 修正後Android APK/起動CI | 同SHAでAPK jobおよびx86_64 emulator startup smokeの[API 28](https://github.com/Zection6V/Fruity-Prime/actions/runs/37224652685/job/111504502980)、[API 30](https://github.com/Zection6V/Fruity-Prime/actions/runs/37224652685/job/111504502822)、[API 35](https://github.com/Zection6V/Fruity-Prime/actions/runs/37224652685/job/111504502878) jobがすべて成功。Windows実マウスの証拠とは別のAndroid回帰検査 |
| CIに組み込んだ検査 | Windowsはportable motion + Win32 registration、Linux/macOSはportable motionを既存Reflex regressionと合わせてbuild/CTestする。初回[run 37222546091](https://github.com/Zection6V/Fruity-Prime/actions/runs/37222546091)、SHA `2cf9f587979a278ec81d50a6d1b90d7dd98254fe`でAndroidのoverride undefined-symbolを検出。修正およびsnapshot保護を追加した[run 37224652685](https://github.com/Zection6V/Fruity-Prime/actions/runs/37224652685)、SHA `f6adc2e0ce7b406dfc057dd89b358f81bd2cef11`は**15/15 job成功、run全体success** |

上記logとcaptureは`tools/build/out/`のignoredローカル証拠。調査時HEADとは別に、`develop3_rendering`の作業ツリーへ実装した変更をCI検証用`codex/windows-raw-mouse-ci`へ分離してnormal pushする。初回は12ファイル、snapshot保護を含む最終scopeは14ファイル。元の作業ツリーの未関連変更は保持する。

### 再実行

```powershell
# repository root
$env:BUILD_JOBS = '6'
& tools/build/build-cpp.bat msvc Release
ctest --test-dir tools/build/out/msvc-Release --output-on-failure

# display/focusが必要。CIの通常CTestには登録しない。
& tools/build/out/msvc-Release/fruity_qt_raw_mouse_window_tests.exe
& tools/build/out/msvc-Release/fruity_qt_raw_mouse_window_tests.exe --stress
$env:QT_SCALE_FACTOR = '1.5'
& tools/build/out/msvc-Release/fruity_qt_raw_mouse_window_tests.exe
$env:QT_SCALE_FACTOR = '2'
& tools/build/out/msvc-Release/fruity_qt_raw_mouse_window_tests.exe
Remove-Item Env:QT_SCALE_FACTOR

$env:FRUITY_RAW_MOUSE_DIAGNOSTICS = '1'
$env:FRUITY_MOUSECHECK = '1'
$env:FRUITY_SWITCHCHECK = '1'
$env:FRUITY_SHOT_ROOM = 'MP1 SANCTORUS'
Push-Location tools/build/out/msvc-Release
try {
    & ./FruityPrime.exe -shellshot ../raw-mouse-switch-shots -rhi opengl -vkvalidation -noupdate
} finally {
    Pop-Location
}
```

### 残る受入確認

- 125/1000/4000/8000Hzの**実マウス**で、入力欠落・jump・stuck input・frame timeへの影響を測定する。synthetic burstとSendInput試験では完了扱いにしない。
- 実DPI 100/150/200%、実pen/stylus、chat/end screen等の手動操作を含むhardware/UI matrix。

受入条件15節の「すべて満たした時だけ完了」を維持する。以上が未確認のため、この文書をdoneへ移さず、goalの全受入完了とはしない。P2のbuffer drainは実測で必要と分かるまで追加しない。


### 高頻度message試験の追加証拠

`fruity_qt_raw_mouse_window_tests --stress`は、134件/burstを600 burst、GLとNoApiの両windowへ実Win32 `SendInput`で送る。送信数・読取数の一致、全packetの同一pump反映、1 frameにつき最大1回の集約motion callback、Qt input channel維持、別HWNDへのfocus移動/復帰を検証する。OSがburstを複数pumpへ分割しても、全件の消費を確認してから次burstへ進む。

2026-10-05の実行は各80400件、合計160800件で送信数・消費数が完全一致。集約diagnosticsは`raw_get_data_failures=0`、`max_wm_input_events_per_frame=134`を記録した。試験時間counterはSendInput生成、検証用の追加GetRawInputData、Qtイベント処理等を含み、試合のframe timeや真の入力遅延ではない。134件は60fps/8000Hzの1 frameに近い**burst量**であり、物理USB reportが8000Hzで届いた証拠ではない。

### 修正後のCI対象と実機確認手順

修正後の対象SHAは`f6adc2e0ce7b406dfc057dd89b358f81bd2cef11`、[run 37224652685](https://github.com/Zection6V/Fruity-Prime/actions/runs/37224652685)。CI検証branchのfetch後local/remote SHA一致を確認した。初回runは新runによりcancelされており、初回のLinux/macOS成功を修正後SHAの証拠には流用しない。

実機試験ではメーカーの設定画面でreport rateを確認・変更し、Windowsのディスプレイ設定で実DPIを変更する。保存profileのrate値、`QT_SCALE_FACTOR`、SendInput生成件数から実機のrate/DPIを確認済みとは扱わない。

```powershell
# built executable directory。各rate/backend/DPIの組合せで別名のCSV/logを使う。
$env:FRUITY_RAW_MOUSE_DIAGNOSTICS = '1'
& ./FruityPrime.exe -launcher -rhi opengl -noupdate -fpscap display `
    -fpsmeasure ../raw-physical-1000hz-opengl-dpi100.csv `
    *> ../raw-physical-1000hz-opengl-dpi100.log

# 同じ条件・同じroom・同じplayer/botsで従来pathを比較する。
$env:FRUITY_RAW_MOUSE = '0'
& ./FruityPrime.exe -launcher -rhi opengl -noupdate -fpscap display `
    -fpsmeasure ../qt-physical-1000hz-opengl-dpi100.csv `
    *> ../qt-physical-1000hz-opengl-dpi100.log
Remove-Item Env:FRUITY_RAW_MOUSE
```

各試行でspawn済みの試合を60秒以上動かし、連続的な水平/垂直movementと急な反転、button/wheel、pause/chat、Alt+Tab、renderer切替・復帰を操作する。CSVの`focused=1,paused=0,main_active=1`のsegmentでmean/p95/p99とevent処理時間を比較し、raw metricsのread failure・件数・dx/dy sumを照合する。CPU/GPU設定、room、解像度、fps cap、画面内の状況をそろえる。frame time悪化・欠落・jump・stuck inputの有無は数値と実操作の両方を記録する。

frame timeのpercentileは指定したCSV、event処理時間は終了時に出力される`<指定CSV>.phases.csv`の`events`行を使う。phaseの平均時間は`inclusive_ns / timed_calls`で求め、samplingされた`timed_calls`数も記録する。少数のphase sampleだけからtail latencyや8kHzのframe-time回帰がないとは判定しない。

| 実機受入項目 | 現在の記録 |
|---|---|
| 125 / 1000 / 4000 / 8000Hz | 未実施 |
| Windows DPI 100 / 150 / 200%で同じ物理移動のsensitivity | 未実施 |
| GL / Vulkan、windowed / borderlessで実マウス操作 | 未実施（synthetic window試験とgame switch harnessはPASS） |
| 実pause/chat/end screen/menu操作・pointer/click | 未実施（pause/menu composite harnessはPASS） |
| 実Alt+Tab・focus復帰・renderer切替時の照準 | 未実施（native focus・snapshot・game switch harnessはPASS） |
| 実pen/stylus | 未実施。2026-10-05のpresent HID PnP compatible/hardware ID一覧ではUsage Page 000D等のdigitizer/pen IDを検出できず。対応機器による実操作が必要 |

### 15節の受入条件との照合

| 番号 | 現時点の根拠 | 判定 |
|---|---|---|
| 1 | `GetRawInputData`のrelative mouse X/Yをcapture中に累積し、live packet sumとaim eventが一致 | 自動検証PASS |
| 2 | raw ownership中のQt move抑止、live windowと実ゲームmousecheck | 自動検証PASS |
| 3 | Raw deltaにScaleを適用しない実装、Qt scale 1/1.5/2でpacket sum一致 | 自動検証PASS。実DPI matrixは未実施 |
| 4 | input marker/pump/latch/renderの順序、live current-pump MouseState/event一致 | 自動検証PASS |
| 5 | grab predicateにmenu/pause/chat/end/stylus条件。pause/menu harnessはPASS | chat/end/stylusの実操作は未確認 |
| 6 | capture transition discard、別native HWNDへのfocus移動・background packet・snapshot差分保護 | 自動検証PASS。実Alt+Tab操作は未実施 |
| 7 | 共通QtWindow owner、GL/NoApi live window packet試験 | 自動検証PASS |
| 8 | replacement windowのpacket試験・epoch保護、実ゲーム5 HWNDで登録、試合内3回switch | 自動検証PASS。実操作時の照準は未確認 |
| 9 | forced registration failure + Vulkan実ゲームでQt fallback/mousecheck | 自動検証PASS |
| 10 | Qt button/wheel/text test、pen pathを変更しないsource確認 | 実penと各UIの実操作は未確認 |
| 11 | synthetic 804000 sample、Win32 SendInput 160800 packet一致 | **実4/8kHz・実frame timeの証拠不足** |
| 12 | `FRUITY_RAW_MOUSE=0`で実ゲームswitchと従来drift check | 自動検証PASS |
| 13 | Win32 typesはcpp内guard、Android両ABI実リンク成功、Linux/macOS最新SHA CI成功 | build検証PASS |
| 14 | SHA `f6adc2e0`でWindows/Linux/macOSの通常CI、backend contract/package成功、run全体15/15 success | CI検証PASS |

この表の自動検証PASSは14節の実機matrix全体を代替しない。CIは全job完走したが、手動試験が未実施の項目があるため、全受入完了は未証明。
