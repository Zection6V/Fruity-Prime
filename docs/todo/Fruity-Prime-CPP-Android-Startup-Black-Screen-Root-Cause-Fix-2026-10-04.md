# Fruity Prime C++ Android 起動直後 Black Screen 根本原因監査・修正指示

- 対象リポジトリ: `Zection6V/Fruity-Prime`
- 対象ブランチ: `develop3_rendering`
- 監査基準 HEAD: `a3ac455573a4bb3026db6cc2fc73a825b6dbb54d`
- 監査日: 2026-10-04
- 対象: C++ Android版のみ
- 症状: APK起動直後からランチャーが表示されず、black screen のままになる報告

## 1. 結論

起動直後 black screen の根本原因は、ゲーム本体の Vulkan / OpenGL レンダラーではない。

根本原因は、2026-10-03 の Qt Quick Android ランチャー移行で導入された **Android startup bootstrap の状態設計**にある。

現在の起動経路は次の依存関係になっている。

```text
Android Activity onCreate
  -> LauncherView(QtQuickView) を全面表示
  -> Main.qml をロード
  -> QML status == READY を待つ
  -> READY のときだけ nativeCreate()
  -> C++ Android bootstrap
  -> QuickFront()
  -> ShellHost.page = "front"
  -> 初めて Main.qml の実コンテンツをロード
  -> StartPage / SetupPage が描画される
```

一方、`Main.qml` は `ShellHost.page` が `front / pause / end` のどれでもない間、base Loader の `sourceComponent` を `null` にする。初期 `_page` は空なので、**QML自体がロードできていても native bootstrap 完了前は実質的に何も描かない**。

さらに Java 側は Qt QML が `ERROR` になった場合に Toast を出すだけで、`nativeCreate()` を実行せず、fallback UI、retry、timeout、永続的なエラー画面のいずれも持たない。このため次のどの失敗でも、ユーザーからは同じ「起動直後から真っ黒」に見える。

- QML source load が `ERROR`
- Qt Quick scene graph が初回frameをpresentできない
- Android Qt surface/container 初期化が失敗する
- `READY` 後の `nativeCreate()` が完了しない
- `QuickFront()` 内の起動処理が完了しない
- `ShellHost.page = "front"` に到達しない

つまり black screen は単なる描画色の問題ではなく、**Qt Quick の成功を native bootstrap の前提条件にし、かつ bootstrap 前の画面を空にしているため、起動失敗をすべてblack screenへ縮退させてしまう構造的欠陥**である。

### 確信度

| 判定 | 内容 | 確信度 |
|---|---|---:|
| 根本原因 | Qt Quick `READY` が `nativeCreate()` の hard gate であり、失敗時に永続的fallbackがない | **CONFIRMED** |
| blackになる直接理由 | `ShellHost.page == ""` の間 `Main.qml` の base Loader が `null` | **CONFIRMED** |
| QML ERROR 時の永久停止 | Toastのみで `nativeHandle == 0` のまま | **CONFIRMED** |
| ゲーム Vulkan/OpenGL が起動blackを起こしている | 起動時点では game `SurfaceView` 自体が未生成 | **EXCLUDED** |
| APKにQt/QML/app SOが不足 | 最新CI APKには必要物が存在 | **EXCLUDED** |
| `pageChanged` notify漏れ | `ShellBridge::SetPage()` は正しく emit している | **EXCLUDED** |
| `QtQuickView` listener登録が遅く READY を取り逃す | Qt側が未登録時statusをqueueする | **EXCLUDED** |
| `QT_ANDROID_SURFACE_CONTAINER_TYPE=1` による TextureView 経路が端末依存の発火トリガー | Android固有かつ直近追加。静的監査だけでは最終確定不可 | **HIGH-PRIORITY RUNTIME A/B** |

---

## 2. 根拠

### 2.1 `nativeCreate()` が QML `READY` に完全依存している

`android/app/src/main/java/fr/livetek/fruityprime/MainActivity.java` L42-L64:

- L42: `LauncherView` を生成
- L48: Qt QML status listenerを登録
- L50: `READY` 判定
- L54: `READY` の場合だけ `nativeCreate(...)`
- L59-L60: `ERROR` はToastだけ
- L64: Qt launcherを含むrootをActivity全面に設定

Permalink:

`https://github.com/Zection6V/Fruity-Prime/blob/a3ac455573a4bb3026db6cc2fc73a825b6dbb54d/android/app/src/main/java/fr/livetek/fruityprime/MainActivity.java#L42-L64`

これは startup bootstrap と presentation readiness を逆方向に依存させている。

本来は「bootstrap state を確立したうえで UI readiness を進める」べきだが、現在は「UIが完全READYになるまで native bootstrap を開始しない」。

### 2.2 `Main.qml` は bootstrap 前に意図的に空になる

`src/MphRead.Native.Qt/qml/Main.qml` L102-L112:

```qml
Loader {
    id: base
    anchors.fill: parent
    ...
    sourceComponent: ShellHost.page === "front" ? start
                   : ShellHost.page === "pause" ? pause
                   : ShellHost.page === "end" ? end
                   : null
}
```

Permalink:

`https://github.com/Zection6V/Fruity-Prime/blob/a3ac455573a4bb3026db6cc2fc73a825b6dbb54d/src/MphRead.Native.Qt/qml/Main.qml#L102-L112`

`ShellBridge::_page` は初期状態で空文字列。

したがって QML engine が正常にMain.qmlを生成できても、`QuickFront()` が `front` を設定するまでは、ランチャー本体を描画しない。

### 2.3 `front` 設定は QuickFront の最後

`src/MphRead.Native.Android/AndroidQuick.cpp` L48-L60:

```cpp
void QuickFront()
{
    OnQt([]
    {
        Mods::Launcher::LauncherPrefs::Load();
        Mods::InputSettings::Load();
        auto settings = GameState::LoadSettings();
        Mods::GameSettings::Apply(settings);
        bridge->SetSettings(std::move(settings));
        const bool ready = Mods::Launcher::GameFiles::Ready();
        if (ready) Mods::Launcher::GameFiles::ApplyPaths();
        bridge->SetRooms(...);
        bridge->SetPage(QStringLiteral("front"));
    });
}
```

Permalink:

`https://github.com/Zection6V/Fruity-Prime/blob/a3ac455573a4bb3026db6cc2fc73a825b6dbb54d/src/MphRead.Native.Android/AndroidQuick.cpp#L48-L60`

最初に可視状態へ遷移するのではなく、設定ロード、GameFiles判定、rooms生成等の後で初めて `front` にする。

したがって、この前段の例外、stall、将来の重いI/O追加もすべて「black screen」として現れる。

### 2.4 QML ERROR が recoverable state になっていない

`MainActivity.java` L59-L60:

```java
} else if (status == QtQmlStatus.ERROR) {
    runOnUiThread(() -> Toast.makeText(
        MainActivity.this,
        "Could not load the Qt menus",
        Toast.LENGTH_LONG).show());
}
```

この分岐では:

- `nativeCreate()` しない
- `nativeHandle` は `0`
- Qt viewを置換しない
- error detailを保存しない
- retryしない
- finishもしない
- persistent error panelも出さない

Toast消失後はActivity内にblack/transparent Qt viewだけが残り得る。

これは失敗時の state transition が存在しないことを意味する。

### 2.5 Android固有の surface container を強制している

`src/MphRead.Native.Android/AndroidQuick.cpp` L82-L87:

```cpp
int main(int argc, char** argv)
{
    // Composite the menus in the Activity's view tree above the game's
    // SurfaceView, including transparent pause/results pages.
    qputenv("QT_ANDROID_SURFACE_CONTAINER_TYPE", "1");
    QGuiApplication application(argc, argv);
```

Permalink:

`https://github.com/Zection6V/Fruity-Prime/blob/a3ac455573a4bb3026db6cc2fc73a825b6dbb54d/src/MphRead.Native.Android/AndroidQuick.cpp#L82-L87`

Qt側の `QAndroidPlatformWindow::SurfaceContainer` は:

```cpp
enum class SurfaceContainer {
    SurfaceView, // 0
    TextureView // 1
};
```

したがって `1` は TextureView 強制。

この指定は commit `11809dc3b84c3ff91af93bc3628fdc0b7fd24776` で、ゲーム `SurfaceView` の上に透明Qtメニューを合成するため追加された。

これは**今回の端末依存black screenを発火させる第一候補**だが、affected deviceのlog/A-B結果なしに「TextureViewそのものが原因」と断定してはいけない。

重要なのは、仮に TextureView / EGL / Qt scene graph が端末上で初回presentに失敗しても、現bootstrap設計ではそれを検出できずblack screenに永久縮退する点である。

---

## 3. 除外できた原因

### 3.1 APKにapp native libraryがない

除外。

最新HEADのCI artifact `FruityPrime-cpp-android-428` を監査した結果、APKには少なくとも以下が存在する。

- `lib/arm64-v8a/libFruityPrime_arm64-v8a.so`
- `lib/x86_64/libFruityPrime_x86_64.so`
- Qt6 Core / Gui / Qml / Quick / QuickShapes / QuickEffects / OpenGL
- Android platform plugin `qtforandroid`
- QML plugins
- `assets/android_rcc_bundle.rcc`

app SOには `main`, `JNI_OnLoad`, `Java_fr_livetek_fruityprime_MainActivity_nativeCreate` も存在する。

したがって「SOがAPKに入っていない」は根因ではない。

### 3.2 QML module/plugin不足

除外。

全ランチャーQMLのimportを監査した結果、主要依存は:

- `QtQuick`
- `QtQuick.Shapes`
- `QtQuick.Effects`
- `FruityPrime.Launcher`

最新CI APKには該当Qt QML pluginが梱包されている。

またapp SO内qmldirにも:

```text
module FruityPrime.Ui
prefer :/qt/qml/FruityPrime/Ui/
Main 1.0 Main.qml
singleton Theme 1.0 Theme.qml
```

が存在する。

`Theme.qml` singleton登録漏れも除外。

### 3.3 QRC URIが間違っている

`LauncherView.java` L16:

```java
super(context, "qrc:/qt/qml/FruityPrime/Ui/Main.qml", "FruityPrime");
```

app SO内のQML resource prefixと一致する。

### 3.4 `pageChanged` notify漏れ

除外。

`ShellBridge.hpp`:

```cpp
Q_PROPERTY(QString page READ Page NOTIFY pageChanged)
```

`ShellBridge.cpp::SetPage()`:

```cpp
_page = std::move(page);
emit pageChanged();
```

Main.qmlのbinding更新契約は成立している。

### 3.5 listener登録前にREADYを取り逃す

除外。

QtQuickViewのAndroid実装は、status listener未登録時のstatusを保持し、listener設定後にqueue済みstatusを配送する。

したがって:

```java
launcher = new LauncherView(this);
launcher.setStatusChangeListener(...);
```

という順番だけを今回の根因とする根拠はない。

### 3.6 game `SurfaceView` / Vulkan / OpenGL game renderer

起動直後症状については除外。

`GameSurfaceView` は `StartMatch()` -> `BeginMatch()` で初めて生成される。

さらに `AndroidJniHost::CreateGameView()` は:

1. Java `GameSurfaceView` を生成
2. C++ `GameView` を生成
3. `setNativePeer(game.get())`
4. return

その後 `MainActivity::BeginMatch()` が:

```cpp
owner.AddView(_content, gameView);
```

する。

つまり少なくとも起動ランチャーblack screenに game surface のsurface callback raceを持ち込むべきではない。

---

# 4. 必須修正方針

## P0-1. bootstrap状態を `page == ""` で表現しない

現在の空文字 `page` は「Qt runtime起動中」「native bootstrap未実行」「初期化失敗」「意図的にmenu hidden」が区別できない。

明示的な startup state を導入すること。

最低限:

```text
BootingQt
QtReady
BootingNative
FrontReady
Failed
```

QML pageとは別のstateにするのが望ましい。

`ShellBridge` に `startupState` / `startupError` を持たせるか、Android固有bootstrap controllerで保持する。

**禁止:** 空文字列だけでstartup状態を兼用する。

## P0-2. QMLは native bootstrap 前でも必ず何かを描画する

`Main.qml` に軽量な `boot` component を追加する。

要件:

- `QtQuick` 基本primitiveのみ
- networkアクセスなし
- game filesアクセスなし
- thumbnail生成なし
- launcher settings依存なし
- custom imageがなくても描画可能
- 不透明なbackgroundを持つ
- `Starting Fruity Prime...` 等の状態表示が可能

例となる状態遷移:

```text
Qt main starts
  -> ShellBridge startupState = BootingQt
  -> Main.qml boot component visible
  -> QtQuickView READY
  -> Java nativeCreate
  -> startupState = BootingNative
  -> Android native setup
  -> QuickFront lightweight publish
  -> startupState = FrontReady
  -> normal front/setup UI
```

これにより正常起動でも「最初の意味のあるframe」がnative bootstrapの完了を待たなくなる。

## P0-3. `QtQmlStatus.ERROR` を terminal failure state として処理する

Toastのみは禁止。

ERROR時には最低限:

1. `nativeHandle == 0` であることを明示ログ
2. Qt/QML error stateを永続化
3. Android native `TextView` 等でpersistent error panelを表示
4. launcher Qt viewを隠す、またはerror panelを必ず最前面へ
5. `logcat` にstartup phase / ABI / SDK / device / renderer/container情報を出す
6. black screenのまま継続しない

ユーザー向け文面は短くてよいが、ログには原因判定可能な情報を残すこと。

## P0-4. `nativeCreate()` の成功を検証する

現コードは:

```java
nativeHandle = nativeCreate(...);
if (resumed) nativeOnResume(nativeHandle);
nativeOnWindowFocusChanged(nativeHandle, hasWindowFocus());
```

となっている。

`nativeCreate()` が `0` を返した場合は、その後のnative callへ進ませない。

必須:

```text
if nativeHandle == 0:
    startup -> Failed
    persistent error UI
    return
```

JNI exceptionについても同じfailure pathへ統合する。

## P0-5. `QuickFront()` を「可視化」と「重い初期化」に分割する

現在は `front` publish が処理末尾にある。

SRPを守り、少なくとも以下に分離する。

```text
PublishFrontShell()
LoadLauncherState()
LoadGameFileState()
BuildRoomList()
ApplyLoadedState()
```

初期画面表示に不要な処理が最初のframeをブロックしてはいけない。

推奨順序:

```text
1. bootstrap pageを既に表示
2. native root/pathを準備
3. minimum launcher stateをpublish
4. front/setupへ遷移
5. rooms / thumbnail / update check等を後段へ
```

ただし thread safety を破って background thread から直接QObjectを更新しないこと。QObject publish はQt threadに戻す。

## P0-6. first-frame present を監視する

`QtQmlStatus.READY` は「QML sourceがREADY」であって、「ユーザーがframeを見た」ことを保証しない。

この区別が今回重要。

C++側で `QQuickWindow` の実frame完了を観測し、例えば:

- `frameSwapped`
- scene graph lifecycle signal

を利用して一度だけ:

```text
[android-startup] first_qt_frame_presented
```

を記録する。

状態遷移を:

```text
QmlReady != FirstFramePresented
```

として分離すること。

Java側はActivity開始からfirst-frameまでのwatchdogを持ち、timeout時にはblackのまま放置せずpersistent diagnostic UIへ遷移する。

watchdogはクラッシュ回避の代用ではない。原因を状態として観測可能にするためのもの。

---

# 5. Android TextureView 経路の必須A/B

## 5.1 なぜ調べる必要があるか

`AndroidQuick.cpp` は:

```cpp
qputenv("QT_ANDROID_SURFACE_CONTAINER_TYPE", "1");
```

で Qt Android windowを TextureView に強制している。

この変更は Android 固有であり、Qtランチャー移行後に追加された。起動blackが一部Android端末のみで発生するなら、端末GPU/SurfaceFlinger/EGLとの組合せによる first-present failure の最優先候補になる。

ただし現時点のソース・APKだけでは「TextureViewが発火原因」とまでは証明できない。

## 5.2 A/B手順

**同じ affected device / 同じAPK source SHA / 同じQt / 同じassets** で1変数だけ変える。

### A: current

```cpp
qputenv("QT_ANDROID_SURFACE_CONTAINER_TYPE", "1");
```

### B: diagnostic

環境変数を設定しない、または明示的にSurfaceView相当へ切り替える。

他のコードは変更しない。

両方で必ず取得:

```text
QtQmlStatus
QSG_INFO output
Qt graphics API
Qt scene graph initialization
surface container type
first_qt_frame_presented marker
EGL / QRhi warning/error
Android SurfaceFlinger関連logcat
```

### 判定

| A | B | 判定 |
|---|---|---|
| black | visible | TextureView強制経路が発火トリガーとして CONFIRMED |
| black | black | TextureView単独原因ではない。QML/native bootstrap側へ戻る |
| visible | visible | affected deviceとの差分をGPU/SDK/driverへ絞る |

**A/B前にTextureView指定を恒久削除しないこと。** 現指定は試合中の透明 pause/results UI を game SurfaceView 上へ合成する目的があるため、起動だけ直してoverlayを壊す修正は禁止。

---

# 6. Qt Quick graphics backend

Qt 6.11 の Qt Quick Scene Graph はRHIを使い、プラットフォームごとにbackendを選択する。公式仕様上、WindowsはD3D11、macOSはMetal、それ以外はOpenGLが現在のdefaultであり、Androidは通常OpenGL系になる。

参考:

- `https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph-renderer.html`

今回のAndroid frontendで backend の偶発差を排除したい場合は、最初の `QQuickWindow` が作られる前に Qt Quick frontend用graphics APIを明示することを検討する。

ただしこれは**ゲーム本体の Vulkan/OpenGL renderer選択とは別物**として扱うこと。

禁止:

- Qt Quick frontend backendとgame RHI backendを同じ設定値で無条件に結合する
- game rendererのVulkan設定を理由にQt QuickもVulkanへ強制する
- startup black修正のためにgame rendererを変更する

Android Qt launcherは「UI presentation owner」、game rendererは「game scene owner」として責務分離を維持する。

---

# 7. 診断ログを追加する

最低限次のphase markerを出す。

```text
[android-startup] activity_on_create
[android-startup] launcher_view_constructed
[android-startup] qml_status_loading
[android-startup] qml_status_ready
[android-startup] qml_status_error
[android-startup] native_create_begin
[android-startup] native_create_ok handle=...
[android-startup] native_create_failed
[android-startup] quick_front_begin
[android-startup] front_published
[android-startup] first_qt_frame_presented
```

追加情報:

```text
Build.VERSION.SDK_INT
Build.MANUFACTURER
Build.MODEL
supported ABI
Qt version
QSG renderer/backend
surface container
screen size
orientation
```

リリースbuildでもphase markerは残す。大量のper-frame logは不要。

Qt message handlerもAndroid logcatへ流す。

特に以下を捕捉すること。

```text
QQml / qml import errors
QRhi init failures
EGL errors
OpenGL ES context errors
scene graph errors
TextureView / surface creation warnings
```

---

# 8. CIの欠落

最新HEAD `a3ac4555` の GitHub Actions run `37209166495` は以下がgreen:

- Android native build contract
- arm64-v8a NDK build
- x86_64 NDK build
- Qt package preparation
- signed APK build
- APK artifact upload

しかし workflow はAPKを**起動していない**。

現在保証できているのは:

```text
compile -> link -> package -> sign
```

まで。

保証できていない:

```text
install -> Activity launch -> QML READY -> native bootstrap -> first frame -> front UI visible
```

このため今回のblack screenはCIを完全に通過できる。

## 必須追加CI gate

x86_64 Android emulatorを使ったstartup smokeを追加する。

最低要件:

1. APK install
2. `am start -W` で `.MainActivity` 起動
3. processが一定時間生存
4. `qml_status_ready` を確認
5. `native_create_ok` を確認
6. `front_published` を確認
7. `first_qt_frame_presented` を確認
8. screenshot取得
9. screenshotがall-black / zero-varianceでないことを確認
10. logcatにfatal QML/QRhi/EGL errorがないことを確認

さらに fault injection gate を追加する。

### QML error injection

テストbuildで意図的にinvalid QML sourceを指定し、期待値を:

```text
black screenではない
persistent error UIが見える
native processがANRしない
qml_status_errorが記録される
```

とする。

これにより「失敗してもblack screen」という今回の設計退行を再発防止できる。

---

# 9. 実装優先順位

## Must close before Android black-screen issueをclose

- [ ] startup stateを明示化し、空 `page` とbootstrap状態を分離
- [ ] native bootstrap前の可視 `boot` UI
- [ ] `QtQmlStatus.ERROR` persistent fallback
- [ ] `nativeCreate() == 0` failure path
- [ ] `QuickFront()` のfirst-visible transitionを重い処理から分離
- [ ] first Qt frame present marker
- [ ] startup watchdog
- [ ] affected deviceで TextureView A/B
- [ ] Android emulator startup CI
- [ ] QML error fault-injection CI

## Must not do

- [ ] black background色だけを変えてcloseしない
- [ ] Toast追加だけでcloseしない
- [ ] arbitrary delay / sleepでREADYを待たない
- [ ] game Vulkan/OpenGL rendererを原因扱いして変更しない
- [ ] `QT_ANDROID_SURFACE_CONTAINER_TYPE=1` を根拠なしに削除しない
- [ ] CI APK build successをruntime successとして扱わない

---

# 10. 受入基準

以下を全て満たしたときのみclose可能。

### Cold start

- [ ] game filesなしで起動し、boot -> setup/front が見える
- [ ] game filesありで起動し、boot -> front が見える
- [ ] 初期表示中にall-black状態へ永久停止しない

### Lifecycle

- [ ] background -> foreground
- [ ] focus lost -> regained
- [ ] supported configuration change
- [ ] Activity再生成対象を現manifest契約の範囲で確認

### Game transition

- [ ] front -> match
- [ ] match renderer first frame
- [ ] pause Qt overlay
- [ ] resume game
- [ ] result/end panel
- [ ] return to launcher

### Failure behavior

- [ ] forced QML errorでblack screenにならない
- [ ] forced nativeCreate failureでblack screenにならない
- [ ] first-frame timeoutでblack screenを放置しない

### Device/backend matrix

最低限:

- [ ] Android API 28
- [ ] Android API 35
- [ ] x86_64 emulator
- [ ] arm64 physical device
- [ ] affected black-screen report device

可能ならGPU vendor差:

- [ ] Qualcomm Adreno
- [ ] ARM Mali
- [ ] Google/Pixel系GPU

---

# 11. Regression boundary

Android起動bootstrapの大きな変更点は以下。

### `ec6e98b50953b37d6b69d996042874ce152e3a27`

`Replace native Avalonia and Skia UI with Qt Quick across desktop and Android`

ここでAndroid launcherがQtQuickView化され、`nativeCreate()` がQML `READY` 後へ移動した。

### `11809dc3b84c3ff91af93bc3628fdc0b7fd24776`

Android Qt menuをgame surface上へ合成するため `QT_ANDROID_SURFACE_CONTAINER_TYPE=1` が追加された。

したがって affected device の最小bisect範囲はまずこの2変更を境界として扱う。

---

# 12. 最終判断

今回の起動black screenを「Android Vulkanの不具合」「ゲームのOpenGL描画不具合」として追うのは誤り。

**確定している根本欠陥は startup/bootstrap のfail-openではなくfail-blackな設計である。**

現在は:

```text
Qt QML must succeed
    -> native bootstrap may start
        -> page becomes visible
```

となっている。

修正後は:

```text
Activity always has visible startup state
    -> Qt runtime state is observable
    -> native bootstrap state is observable
    -> first frame state is observable
    -> success => front
    -> failure => persistent diagnostic/fallback UI
```

とする。

その上で affected device に対して `QT_ANDROID_SURFACE_CONTAINER_TYPE=1` のA/Bを行い、TextureViewが**端末依存の発火トリガー**かを1変数実験で確定する。

この順序なら、原因を隠す場当たり修正を避けつつ、black screenそのものを構造的に再発不能にできる。
