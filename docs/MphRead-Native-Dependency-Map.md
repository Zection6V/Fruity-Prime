# MphRead C#→C++ 依存関係台帳

スナップショット: `2026-09-11` / ブランチ `develop2` / commit `2da573aa860507eab7150dfd3a69667528721063`

> この文書は、C#を唯一の仕様とする厳密移植を再開する前の依存関係インベントリです。Native側の存在だけで意味的な完全再現を認定していません。

## 目的と判定単位

個別のCSファイルを先に移植すると、未移植の型・外部ライブラリ・プラットフォーム境界をC++側の独自設計で埋めてしまう危険があります。先に全体の対応関係、直接依存、ビルド条件、既知の意味差を固定し、その後に依存が閉じた葉から1ファイル単位の監査へ戻ります。

- `src/MphRead` 配下の全CSファイルの対応先、配置差、未対応状態
- 各CSのnamespace、宣言型、直接 `using` 依存（MphRead内 / 外部）
- `src/MphRead.Native` の全hpp/cppペアとinclude依存
- C#プロジェクトのターゲット、条件付きビルド、パッケージ、共有ソース、Android別名
- 既に実施した外部監査の判定と、移植をブロックしている依存契約

`using` と `#include` はソースから機械抽出した直接依存です。型メンバー呼び出し・ジェネリック制約・reflection・実行時ロードまでを静的文字列だけで完全解決したものではありません。その範囲は未確定として残し、ChatGPT監査とビルド/差分テストで一つずつ閉じます。

## 1. 現在の対応状況

| 項目 | 件数 | 意味 |
|---|---:|---|
| C#ソース | 302 | `src/MphRead/**/*.cs` |
| Nativeヘッダ | 48 | `src/MphRead.Native/**/*.hpp` |
| Native実装 | 48 | `src/MphRead.Native/**/*.cpp` |
| Native hpp/cppペア | 48 | ヘッダと実装の両方が存在 |
| 同一相対パスで対応 | 47 | `.cs`→同じ相対位置の`.hpp/.cpp` |
| 配置を移動した対応 | 1 | `Mods/Launcher/Portable/SetupProgress.cs`→`Mods/Launcher/SetupProgress.hpp/.cpp` |
| 対応先未作成 | 254 | C#を唯一の仕様とする対象 |

同一相対パスで見ると、C# 302件のうち47件だけが対応済みで、255件は同一位置にありません。その255件のうち1件は上記の配置移動、残り254件はNative側の対応先未作成です。Native側48ペアは、配置移動を考慮すれば全てC#ファイルに対応します。

hppはinclude専用ディレクトリに集めず、cppと同じ機能ディレクトリに置く前提を維持します。

## 2. ディレクトリ別集計

| C#相対ディレクトリ | C# | paired | relocated | missing | Nativeペア |
|---|---:|---:|---:|---:|---:|
| `.` | 16 | 1 | 0 | 15 | 1 |
| `Entities` | 25 | 2 | 0 | 23 | 2 |
| `Entities/CamSeq` | 2 | 0 | 0 | 2 | 0 |
| `Entities/Enemies` | 42 | 2 | 0 | 40 | 2 |
| `Entities/Players` | 14 | 0 | 0 | 14 | 0 |
| `Export` | 3 | 0 | 0 | 3 | 0 |
| `Formats` | 18 | 3 | 0 | 15 | 3 |
| `HUD` | 1 | 0 | 0 | 1 | 0 |
| `Metadata` | 7 | 0 | 0 | 7 | 0 |
| `Mods` | 25 | 11 | 0 | 14 | 11 |
| `Mods/Chat` | 3 | 0 | 0 | 3 | 0 |
| `Mods/Input` | 11 | 4 | 0 | 7 | 4 |
| `Mods/Launcher/Gui` | 22 | 4 | 0 | 18 | 4 |
| `Mods/Launcher/Portable` | 7 | 1 | 1 | 5 | 1 |
| `Mods/MapGen` | 16 | 3 | 0 | 13 | 3 |
| `Mods/Network` | 43 | 6 | 0 | 37 | 6 |
| `Mods/Render` | 16 | 5 | 0 | 11 | 5 |
| `Mods/Sound` | 2 | 0 | 0 | 2 | 0 |
| `Mods/Update` | 8 | 2 | 0 | 6 | 2 |
| `Sound` | 2 | 0 | 0 | 2 | 0 |
| `Testing` | 8 | 0 | 0 | 8 | 0 |
| `Utility` | 11 | 3 | 0 | 8 | 3 |

全ファイルの行単位インベントリは次の索引に分割しています。

- [C#依存索引 1/2](MphRead-Native-CSharp-Dependency-Index-1.md)
- [C#依存索引 2/2](MphRead-Native-CSharp-Dependency-Index-2.md)
- [Native hpp/cpp include索引](MphRead-Native-Native-Include-Index.md)

## 3. 作業上の依存レイヤー

以下はnamespaceと既存構成から作った作業順の仮説です。コンパイラの完全なDAGではなく、個別監査の開始順を固定するためのレイヤーです。

| レイヤー | 主な領域 | 依存上の役割 |
|---:|---|---|
| 0 | C#/.NET、OpenTK、Avalonia、Silk.NET、Android、NcsfPlay | 言語・外部API・プラットフォーム境界。C++薄型アダプタの観測可能な意味を固定する |
| 1 | `Utility`、ルート型、`Enums`、`RawStructs`、`Metadata` | 共有値、ログ、コンソール、バイナリ読み取り、基本データ表現 |
| 2 | `Formats`、`Export`、`Sound` | ROM/モデル/衝突/カリング/音声データの読み取り契約 |
| 3 | `Entities`、`Entities/Enemies`、`Entities/Players`、`CamSeq` | Scene、EntityBase、プレイヤー、敵、武器の実行モデル |
| 4 | `Mods/Network`、`Mods/Chat`、`Mods/MapGen` | 通信プロトコル、サーバー、チャット、マップ生成。Entity/Format依存が多い |
| 5 | `HUD`、`Mods/Render`、`Mods/Input` | 描画、GL/OpenTK、入力、HUD。外部APIとEntity/Scene契約を同時に要求 |
| 6 | `Mods/Launcher`、`Mods/Update` | GUI/Text/Portable、設定、アップデート。Avalonia/ファイル/ネットワーク依存 |
| 7 | `Program`、`ModEntry`、各プラットフォーム起動アダプタ | プロセス入口から各サブシステムを接続する最終層 |

重要な入口は `src/MphRead/Program.cs` の `Program.Main()` です。引数なしのWindows/macOS経路は `ModEntry.TryHandleHeadless()` から `GuiLauncher.TryRun()`、失敗時に `TextLauncher.Run()` へ進みます。Androidは `MainActivity.cs` から共通CSソースを取り込みます。従って `Mods/Launcher` 内の関数をプロセス全体のMainとして扱わず、Program→ModEntry→Launcherの順で依存を閉じます。

## 4. 既知の依存ブロッカー

| ブロッカー | 関連CS | 現状 |
|---|---|---|
| 音声契約 | `FhSound.cs`、`ThumbnailMode.cs` | NativeのSound/Sfx/Music相当契約が不足し、単純なinclude追加ではC#の副作用・ライフタイム・戻り値を再現できない |
| Launcher/UI | `GuiLauncher.cs`、`TextLauncher.cs`、`Updater.cs`、`UpdateDownload.cs`、`UpdateInstall.cs` | Avalonia、OSファイル、Process/HTTP/設定の境界が未分類。GUI独自実装を先に足さない |
| Entry/dispatch | `Program.cs`、`ModEntry.cs`、`NetHostSession.cs` | 下位依存が未閉鎖。Nativeのmain/WinMainは薄いアダプタに限定し、入口から新しい仕様を追加しない |
| Input/platform | `SyntheticInput.cs`、`PointerInput.cs`、`EsBindings.cs`、`GamepadLayout.cs` | OpenTK/GLFW、DebugLog、C#のstruct/enum/boxing/例外/スレッド意味の差が残る |
| 時刻・文化・出力 | `FrameTiming.cs`、`FrameTimingCheck.cs`、`RenderOptions.cs` | `CurrentCulture`、UTF-16/UTF-8、`Console.WriteLine`、`Math.Clamp`の境界を完全一致させる必要がある |
| Gameplay/Formats | `Entities/**`、`Formats/**`、`HUD/**` | 共有するScene、Read、Entity、Model、Collision、Renderer契約の依存閉包が未作成 |

既存監査でFAILになったものは、Native側に似た名前の実装を追加する根拠にはしません。C#の依存契約を先に列挙し、対象CSの完全再現に必要な最小接続だけを作ります。

## 5. C#プロジェクトとビルド依存

このスナップショットでソースから確認できたビルドマニフェストは次の4件です。`src/MphRead.Native`専用の追跡済み `Makefile` / `CMakeLists.txt` は今回のファイル一覧では検出されませんでした。Nativeのビルド経路は別途明示的に固定する必要があります。

### `src/MphRead.Android/MphRead.Android.csproj`

```xml
       It compiles the same sources as the desktop project rather than
       It is also a compile check on the shared sources: every time they grow
    <TargetFramework>net9.0-android35.0</TargetFramework>
         the shared sources gate the Avalonia screens on it. -->
    <DefineConstants>$(DefineConstants);MPHREAD_AVALONIA</DefineConstants>
    <!-- The shared sources are globbed in by hand below, so the SDK must not
    <Compile Include="*.cs" />
    <Compile Include="..\MphRead\**\*.cs"
    <AndroidResource Include="Resources\**\*.xml" />
    <AndroidResource Include="Resources\**\*.png" />
    <TrimmerRootDescriptor Include="Properties\TrimmerRoots.xml" />
    <PackageReference Include="Avalonia" Version="11.3.11" />
    <PackageReference Include="Avalonia.Android" Version="11.3.11" />
    <PackageReference Include="Avalonia.Themes.Fluent" Version="11.3.11" />
    <PackageReference Include="Avalonia.Fonts.Inter" Version="11.3.11" />
    <PackageReference Include="OpenTK" Version="4.9.4" />
    <PackageReference Include="ReFuel.StbImage" Version="2.1.1" ExcludeAssets="native" />
    <AndroidAsset Include="..\..\maps\*.json;..\..\maps\*.bsp;..\..\maps\*.tex;..\..\maps\*.fpmap"
    <AvaloniaResource Include="..\MphRead\Assets\fruity-prime-logo.png"
    <AvaloniaResource Include="..\MphRead\Assets\fruity-prime-mark.png"
    <Using Include="MphRead.Mods.Render.GlEs" Alias="GL" />
    <Using Include="MphRead.Mods.Sound.AlEs" Alias="AL" />
    <Using Include="MphRead.Mods.Sound.AlcEs" Alias="ALC" />
    <ProjectReference Include="..\NcsfPlay\NcsfPlay.csproj" />
```

### `src/MphRead/MphRead.csproj`

```xml
    <!-- MphReadServer=true builds the dedicated-server package: no launcher of
         sources possible at all. -->
    <TargetFramework>net9.0</TargetFramework>
    <MphReadWindowsUi Condition="$(RuntimeIdentifier.StartsWith('win')) and '$(MphReadServer)' != 'true'">true</MphReadWindowsUi>
           dotnet publish -r win-x64 -p:MphReadServer=true
             produces MphReadServer.exe
    <AssemblyName Condition="'$(MphReadServer)' == 'true' and $(RuntimeIdentifier.StartsWith('win'))">FruityPrimeServer</AssemblyName>
    <DefineConstants Condition="'$(MphReadServer)' == 'true'">$(DefineConstants);MPHREAD_SERVER</DefineConstants>
    <MphReadAvalonia Condition="'$(MphReadServer)' != 'true'">true</MphReadAvalonia>
    <DefineConstants Condition="'$(MphReadAvalonia)' == 'true'">$(DefineConstants);MPHREAD_AVALONIA</DefineConstants>
    <ApplicationIcon Condition="$(RuntimeIdentifier.StartsWith('win')) and '$(MphReadServer)' != 'true'">Assets\fruity-prime.ico</ApplicationIcon>
    <ApplicationIcon Condition="$(RuntimeIdentifier.StartsWith('win')) and '$(MphReadServer)' == 'true'">Assets\fruity-prime-server.ico</ApplicationIcon>
  <ItemGroup Condition="'$(MphReadAvalonia)' != 'true'">
  <ItemGroup Condition="'$(MphReadAvalonia)' == 'true'">
    <PackageReference Include="Avalonia" Version="11.3.11" />
    <PackageReference Include="Avalonia.Desktop" Version="11.3.11" />
    <PackageReference Include="Avalonia.Themes.Fluent" Version="11.3.11" />
    <PackageReference Include="Avalonia.Fonts.Inter" Version="11.3.11" />
    <AvaloniaResource Include="Assets\fruity-prime-logo.png" />
    <AvaloniaResource Include="Assets\fruity-prime-mark.png" />
    <PackageReference Include="OpenTK" Version="4.9.4" />
    <PackageReference Include="ReFuel.StbImage" Version="2.1.1" />
  <ItemGroup Condition="'$(MphReadServer)' != 'true'">
    <PackageReference Include="Silk.NET.OpenAL.Soft.Native" Version="1.23.1"
  <ItemGroup Condition="'$(MphReadServer)' != 'true' and $(MphReadAudioRid.StartsWith('win'))">
    <None Include="$(PkgSilk_NET_OpenAL_Soft_Native)\runtimes\$(MphReadAudioRid)\native\soft_oal.dll"
  <ItemGroup Condition="'$(MphReadServer)' != 'true' and $(MphReadAudioRid.StartsWith('linux'))">
    <None Include="$(PkgSilk_NET_OpenAL_Soft_Native)\runtimes\$(MphReadAudioRid)\native\libopenal.so"
  <ItemGroup Condition="'$(MphReadServer)' != 'true' and $(MphReadAudioRid.StartsWith('osx'))">
    <None Include="$(PkgSilk_NET_OpenAL_Soft_Native)\runtimes\$(MphReadAudioRid)\native\libopenal.dylib"
    <ProjectReference Include="..\NcsfPlay\NcsfPlay.csproj" />
    <None Include="..\..\maps\**\*.fpmap;..\..\maps\*.json;..\..\maps\*.bsp;..\..\maps\*.tex" Link="maps\%(RecursiveDir)%(Filename)%(Extension)">
    <None Include="..\..\maps\**\*.json;..\..\maps\**\*.bsp;..\..\maps\**\*.tex;..\..\maps\**\*.pk3"
```

### `src/NcsfPlay/NcsfPlay.csproj`

```xml
    <TargetFramework>net9.0</TargetFramework>
    <PackageReference Include="CommunityToolkit.HighPerformance" Version="8.4.2" />
    <PackageReference Include="SoundFlow" Version="1.4.1" />
    <PackageReference Include="System.IO.Hashing" Version="10.0.9" />
```

### `tools/nettest/nettest.csproj`

```xml
    <TargetFramework>net9.0</TargetFramework>
    <Compile Include="../../src/MphRead/Mods/Network/NetProtocol.cs" />
    <Compile Include="../../src/MphRead/Mods/Network/NetTransport.cs" />
    <Compile Include="../../src/MphRead/Mods/Network/NetProbe.cs" />
    <PackageReference Include="OpenTK" Version="4.9.4" />
```

プロジェクト依存の要点:

- Desktop C# は `net9.0`。通常版は条件付きでAvalonia、OpenTK、ReFuel.StbImage、Silk.NET OpenALを持ち、サーバー条件ではLauncher/音声条件が変わる。
- Android は `net9.0-android35.0` で `src/MphRead/**/*.cs` を共有コンパイルし、`GL`、`AL`、`ALC` のusing aliasを差し替える。共通CSの移植はdesktopだけで完結しない。
- `NcsfPlay` は `CommunityToolkit.HighPerformance`、`SoundFlow`、`System.IO.Hashing`に依存し、MphReadからProjectReferenceされる。
- `tools/nettest` はNetworkの3ファイルを直接Compileし、OpenTKを参照するため、Network移植の独立ビルド依存になる。

## 6. 既存のファイル単位監査台帳

ここは意味的完全性の最終認定ではなく、これまでのChatGPT外部監査の最新結果です。`PASS/NO-OP` は追加修正不要、`FAIL/NO-OP` は依存不足または意味差があるため未変更を意味します。

| CS | 最新判定 | 変更/注記 |
|---|---|---|
| `Enums.cs` | PASS / NO-OP | 変更なし |
| `ConsoleWindow.cs` | PASS / NO-OP | 変更なし |
| `Utility/Console.cs` | PASS | `e10bfaef...` |
| `ModEntry.cs` | FAIL / NO-OP | 依存閉包不足。過去にincludeだけの修正あり |
| `Mods/Launcher/Gui/GuiLauncher.cs` | FAIL / NO-OP | Avalonia/Launcher依存 |
| `Mods/Launcher/Portable/TextLauncher.cs` | FAIL / NO-OP | Launcher依存 |
| `Mods/Input/TouchSettings.cs` | PASS | `702b4dbe...` |
| `Utility/Output.cs` | PASS | `15aeff...` |
| `Mods/Input/GamepadLayout.cs` | FAIL / NO-OP | 入力契約不足 |
| `Mods/Input/EsBindings.cs` | FAIL / NO-OP | OpenTK/GL binding差 |
| `Mods/LogShare.cs` | FAIL / NO-OP | OS/ファイル依存 |
| `Mods/MapTexturePack.cs` | PASS | `1234af9...`、`91b855b...` |
| `Utility/RawStructs.cs` | FAIL / NO-OP | 共有データ契約不足 |
| `Mods/Update/UpdateDownload.cs` | FAIL / NO-OP | HTTP/ファイル契約不足 |
| `Mods/Update/UpdateInstall.cs` | FAIL / NO-OP | Process/ファイル契約不足 |
| `Mods/Launcher/Portable/Frontend.cs` | PASS | `a6d3ef...`まで |
| `Sound/FhSound.cs` | FAIL / NO-OP | Sound契約不足 |
| `Mods/MapReport.cs` | FAIL / NO-OP | Formats/Map依存 |
| `Mods/Update/Updater.cs` | FAIL / NO-OP | Launcher/Update依存 |
| `Mods/Network/DemoClip.cs` | FAIL / NO-OP | Network/IO依存 |
| `Mods/AdventureSave.cs` | FAIL / NO-OP | Format/Save依存 |
| `Program.cs` | FAIL / NO-OP | 入口の下位依存未閉鎖 |
| `Mods/Network/NetHostSession.cs` | FAIL / NO-OP | Network/Game依存 |
| `Mods/RespawnChoice.cs` | FAIL / NO-OP | Gameplay依存 |
| `Mods/Branding.cs` | PASS |  `2da573aa860507eab7150dfd3a69667528721063`  |
| `Mods/Credits.cs` | PASS / NO-OP | 変更なし |
| `Utility/RepackAccess.cs` | FAIL / NO-OP | 実行時/アクセス契約不足 |
| `Mods/Headless.cs` | PASS / NO-OP | 変更なし |
| `Mods/Render/PreviewCamera.cs` | FAIL / NO-OP | Renderer/Camera依存 |
| `Mods/MapGen/BuiltMap.cs` | FAIL / NO-OP | MapGen/Format依存 |
| `Mods/Testing/FrameTimingCheck.cs` | FAIL / NO-OP | Culture/出力差 |
| `Mods/Launcher/Portable/ThumbnailMode.cs` | FAIL / NO-OP | Sound/Sfx/Music契約不足 |
| `Mods/Testing/SyntheticInput.cs` | FAIL / NO-OP | OpenTK/Native object契約不足 |
| `Mods/Input/PointerInput.cs` | FAIL / NO-OP | DebugLog/同期契約不足 |
| `Mods/Render/RenderOptions.cs` | FAIL / NO-OP | Culture/Clamp/型意味差 |
| `Mods/Render/FrameTiming.cs` | FAIL / NO-OP | Culture/配列/同期契約不足 |
| `Mods/Input/GamepadState.cs` | FAIL / NO-OP | enum/struct/boxing/format意味差 |

## 7. 名前空間のファンイン（直接usingの静的集計）

| MphRead namespace | 参照CS数 |
|---|---:|
| `MphRead.Entities` | 65 |
| `MphRead.Formats.Culling` | 63 |
| `MphRead.Formats` | 55 |
| `MphRead.Effects` | 22 |
| `MphRead.Formats.Collision` | 22 |
| `MphRead.Mods.Network` | 19 |
| `MphRead.Hud` | 16 |
| `MphRead.Sound` | 13 |
| `MphRead.Text` | 13 |
| `MphRead.Mods` | 7 |
| `MphRead.Editor` | 6 |
| `MphRead.Entities.Enemies` | 6 |
| `MphRead.Formats.Sound` | 6 |
| `MphRead.Utility` | 6 |
| `MphRead.Export` | 3 |
| `MphRead.Mods.Input` | 3 |
| `MphRead.Mods.Render` | 3 |
| `MphRead.Mods.Update` | 3 |
| `MphRead.Mods.Launcher` | 2 |
| `MphRead.Archive` | 1 |
| `MphRead.Memory` | 1 |
| `MphRead.Mods.Chat` | 1 |

| 外部namespace / alias | 参照CS数 |
|---|---:|
| `System` | 251 |
| `System.Collections.Generic` | 146 |
| `OpenTK.Mathematics` | 142 |
| `System.Diagnostics` | 115 |
| `System.IO` | 71 |
| `System.Linq` | 57 |
| `System.Runtime.InteropServices` | 31 |
| `System.Globalization` | 27 |
| `System.Text` | 24 |
| `System.Threading` | 21 |
| `Avalonia` | 20 |
| `Avalonia.Controls` | 19 |
| `Avalonia.Media` | 19 |
| `System.Collections.Frozen` | 17 |
| `System.Collections.Immutable` | 15 |
| `OpenTK.Windowing.GraphicsLibraryFramework` | 13 |
| `System.Threading.Tasks` | 13 |
| `Avalonia.Input` | 12 |
| `OpenTK.Graphics.OpenGL` | 10 |
| `Avalonia.Threading` | 9 |
| `System.Buffers` | 9 |
| `OpenTK.Windowing.Common` | 8 |
| `OpenTK.Windowing.Desktop` | 7 |
| `Avalonia.Layout` | 6 |
| `ReFuel.Stb` | 6 |

この集計はファイル依存の優先順位を決めるためのファンインで、namespaceをincludeへ機械変換する指示ではありません。同名型、global using、完全修飾名、reflectionは個別監査で解決します。

## 8. 以後の実行プロトコル

1. この台帳を基準に、まず `missing` の依存閉包をレイヤー順に分類する。
2. 独立した葉を2件ずつ、新しいChatGPT Chatへ英語で依頼する。各ChatにはGitHubリンクと対象CS、現行Native、依存箇所を渡し、C#のみを仕様にする。
3. ChatGPTの回答を受けた後、こちらで差分を検証する。FAILなら変更せず依存ブロッカーを台帳へ追加し、PASSだけ狭いファイル単位で修正する。
4. 修正が必要な場合だけ意図したhpp/cppと関連接続を限定してコミットし、`develop2` のローカル/remote SHAを確認する。
5. 回答待ち中も作業を止めず、もう一方のChatを進める。完了済み・不要なブラウザタブは閉じ、アクティブな2件を維持する。
6. Program/ModEntry/Launcherの入口系は、下位のUtility/Format/Scene/Network/Platform契約が台帳上閉じてから再監査する。

## 9. 限界と次に必要な解析

この版で全302CS・全48Nativeペア・4ビルドマニフェストを固定しました。ただし次はまだ静的インベントリの範囲外です。

- C#の実際のメンバー呼び出しグラフ、継承/override/virtual dispatch
- generic制約、nullable、配列/Span/structのコピー・boxing・参照同一性
- reflection、属性、delegate、イベント、lock、Thread/Task、Process/文化依存
- Native側の実コンパイル単位、リンクライブラリ、未追跡の生成/外部ビルド定義
- C#とNativeの差分テスト、実ROM、GUI、Android、ネットワーク実行時の観測結果

この文書の完了は「全依存関係を洗い出す作業の開始点を作った」ことを意味し、「C++移植完遂」や「全ファイル完全再現」を意味しません。次のChatGPT監査は、この台帳から依存が閉じている葉を選びます。
