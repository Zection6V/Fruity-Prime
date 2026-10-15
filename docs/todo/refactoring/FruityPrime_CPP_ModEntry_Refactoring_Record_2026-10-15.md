# Fruity-Prime C++ — `ModEntry` 段階的リファクタリング 実施記録

> **対象指示書**：`FruityPrime_CPP_ModEntry_Incremental_Refactoring_Instructions_2026-10-15.md`  
> **実施日**：2026-10-15　**作業開始時**：`develop13`（HEAD `b6f8099c`、tree `66aff704`）。検証完了後、ユーザーの指示によりコミット・プッシュを実施（§10）。  
> **証拠一式**：`docs/todo/refactoring/ModEntry-evidence/`（ハーネス、各段階のゲートログ、実行出力）

## 0. 結論

- ME-00〜ME-07 を **22 個の小さな差分**に分け、差分ごとに同じゲート（CLI 契約 45 ケース・フラグ字句索引・CTest 全件、ME-06 以降は実 UDP ループバック）を通してから次へ進んだ。**全差分でベースラインとの相違 0 件**。
- `ModEntry.cpp` は 2,662 行 → **385 行**。2 つの公開入口（`TryHandleHeadless` / `TryHandle`）と `Program` の 2 箇所のフックは不変。各コマンドの本体は `Mods/Entry/` の 11 モジュールへ**本文そのまま**移し、`ModEntry` には「どの順で何を試すか」だけが残った。
- 新しい自動テスト 3 本（`FruityPrime.ModEntryCommandLine` / `ModEntryOptions` / `ModEntryValues`）を CTest に登録。CTest は 119 → 122 本、全件 PASS。
- 最終状態で、指示書が手動扱いとしたシナリオもこの機械で実行：ウィンドウ／GPU 系診断・`-launcher -server`（D07）・`-update -noupdate`（D14）・`-thumbnail` 2 回（D22）・`-hostlocal`（失敗経路と成功経路）・`-connect`（実 UDP）・`DesktopUpdate::ApplyFlag`（サンドボックスで適用→再起動まで）。**すべてベースラインと一致**（§6）。
- ビルド構成は MSVC（全段階）、Android x86_64（clang、リンクまで）、**GCC 16.2（MSYS2、全 15 ファイルのコンパイル）**、`MPHREAD_SERVER` 定義時の構文コンパイル。Linux／macOS 実機ビルドはこの機械に環境がなく未実施（§5）。
- ベースラインで見つかった終了時クラッシュは、ModEntry 分割とは**別差分で修正・検証済み**（§1.1-1）。通常起動の反復試験はベースライン 180 回中 7 回のアクセス違反に対し、修正版 180 回で 0 回。デバッガ下でも 60 回で 0 回。修正版の CLI 45/45・UDP 8/8 一致、CTest 122/122 PASS、マップ生成中のランチャー終了も確認した。
- 残っていた外部ダウンロード比較も完了（§6.1）。`-installserver` は両版とも配布パッケージなしの同一失敗、`-mapsync` はベースラインの 1 ファイルを再試行後、20 ファイルすべての SHA-256 が一致。
- 性能・サイズの改善は**主張しない**。FruityPrime.exe は +8,704 byte（+0.05%、TU を跨ぐ呼出しが inline されなくなった分）。コンパイルは `ModEntry.cpp` 単体では 4.13 s → 1.95 s だが、分割した全 TU の合計 CPU 時間は 16.75 s に増える（§7）。

## 1. ME-00 ベースライン

| 項目 | 値 |
|---|---|
| 監査基準との差 | 指示書は `66fdfd4f`（2,678 行）を基準とするが、作業時点の HEAD は `b6f8099c`、`ModEntry.cpp` は **2,662 行**、SHA-256 `d1f9a410…3797`。差分の一例：指示書が `TryHandle` 先頭に挙げる `-instagibdemo/-instagib` は現行コードに存在しない。以降は**現行 HEAD を契約**とし、行番号ではなく関数名・ブロックで追跡した |
| ツールチェーン | MSVC 19.44.35228（VS 2022 17.14）、CMake 3.31.6（VS 同梱）、Ninja、`tools/build/build-cpp.bat msvc Release`、Qt 6.11.2 msvc2022_64、Windows 11 22621 |
| 実際の定義 | `build.ninja` 上で **`MPHREAD_QT` と `MPHREAD_SHELL` のみ**。**`MPHREAD_SERVER` は現行 C++ のどの CMake ターゲットでも定義されない**（C# の csproj のみ）。よって `ServerUsage` と `doubleClicked=false` の分岐は現状どの C++ 製品でも死んでいる（削除はせず保持、§5 で構文コンパイルのみ検証） |
| Program の呼出し | `Program::Run`：`ConsoleSetup::Run` → (Win) `ConsoleWindow::Prepare` → `TryHandleHeadless` → `CheckSetup` → `ParseArguments` → `TryHandle` → 既存 CLI。**本作業で `Program.cpp` は無変更** |
| 既存 CTest | 119/119 PASS（`ctest-baseline.log`）。指示書が既知の赤とする Protocol30–32 Golden もこの Windows ツリーでは PASS |
| コマンド台帳 | `command-inventory.tsv`（84 ブロック：フラグ・値・段階・前後関係・出力・終了コード・ゲームファイル要否・`#if`・副作用・対応ケース） |
| フラグ字句索引 | `flag-index.py` → `flag-index-baseline.tsv`（292 参照・217 種）。各段階で再生成し**完全一致**を確認 |
| CLI 契約 | `run-cli-cases.ps1` の 45 ケース（D01–D23 の実行可能分、A12、ネット設定全種、Map/Q3/Demo 系の異常系、`-dpstest`/`-simcheck`/`-movieheadlesscheck`/`-netcombatcheck` 実行）。ベースライン同士で 2 回取得し 45/45 一致を確認してから比較基準にした |

### 1.1 ベースライン取得で判明した既存事項（#1 のみ別差分で修正）

| # | 事項 | 証拠・扱い |
|---|---|---|
| 1 | **終了時のアクセス違反（0xC0000005）**：setup 後の即終了コマンド（`-q3maps missing` など）がベースラインでもプロセス終了時にクラッシュ。初回調査では約 10–15%、空の `-mapdir` でも再現（20 回中 2–3 回）。原因は `CustomRooms::StartGenerating()` の **detach した生成スレッド**と静的オブジェクト破棄の競合。`CustomRooms::NeedsGenerating` → `NodeDirectory` → `Paths::FileSystem()` → 破棄済みの `Paths::_allPaths` を読む。原因捕捉時の証拠：`ModEntry-evidence/runs/exit-av-stack.txt`、`exit-av-symbols.txt` | 分割前から存在し、分割で起動順序は変えていない。ユーザー指定により**別差分で修正済み**：`CustomRooms.cpp/.hpp` と `src/FruityPrime/Main.cpp` のみ。実装・修正版検証は直下。過去の段階ゲートは最大 5 回再試行して回数を記録していたが、修正版ゲートの `ExitCrashRetries` は **0** |
| 2 | `-mode bogus` / `-weapon bogus`：`Enum.TryParse` 準拠の移植のため、**不正な名前は既定値でなく列挙の 0 値（`GameMode::None` / `BeamType::PowerBeam`）を書き込む**。`-mode` 未指定時の Battle とは異なる | `TestModEntryValues` V11b/V13b で現行挙動として固定 |
| 3 | `-netcombatcheck` の 2 本目の分岐は到達不能、`-gamepad` の setup 後コピーは `Program` 経由では到達不能 | 指示書どおり**削除せず**保持。ヘッダコメントに明記 |
| 4 | `-classicframes DIR` はインストールディレクトリ基準（`-mapdir` 等と異なり起動ディレクトリ基準ではない） | 既存挙動として保持（stdout を比較） |
| 5 | `-uishot` の PNG はベースライン同士でも 28 枚中 18 枚がバイト不一致 | 画素一致は判定基準にせず、終了コード・ファイル集合・stdout で比較 |
| 6 | Windows の GUI 版 exe は**出力をリダイレクトされるとコンソールに接続しない**（`ConsoleWindow::Prepare`）ため Ctrl+C を受け取れない | ループバック試験では `-console -debuglog` で非リダイレクト起動し、出力はデバッグログから回収 |
| 7 | MSVC C4834（`[[nodiscard]]` 破棄）4 件 | すべて既存行。移動に伴い発生ファイルが変わっただけ |
| 8 | 呼出し元のない関数：`AppBaseDirectory`、`IsAsciiWhitespace`/`TrimNumberWhitespace`/`TrimAscii` | HEAD でも参照 0 件を確認のうえ削除（挙動変化なし） |

**終了クラッシュ修正（ModEntry 分割とは独立）**：生成スレッドを `GenerationState::Worker` に保持し、`StopGenerating()` が停止を要求して join する。生成中のマップは完了させ、マップ間で停止する。`main` は `Program::Main` の復帰直後、静的破棄前に呼び出す。`GenerationState` のデストラクタにも停止と join を置き、`std::exit` 経路を補完した。`Program.cpp` と ModEntry の起動順序は変更していない。

2026-10-15 の修正版検証：

| 検証 | 結果・証拠 |
|---|---|
| MSVC Release | `tools/build/build-cpp.bat msvc Release` 成功。`ModEntry-evidence/build-fix.log`。exe SHA-256 `07513f73c7226c5a6fea7502c3d8e1be41caa0f029e5d67a54c6ce2d3a8e6c94` |
| 再試行なしの反復起動 | `-q3shaders missing.pk3`・`-q3maps missing.pk3`・`-mapitems missing.pk3` を各版・各 60 回。ベースラインはアクセス違反 **3/60、3/60、1/60**。修正版は各 **0/60**、全 180 回が期待する終了コード 1。`tools/verify-exit-stress.py`、`runs/exit-stress/baseline.json`・`fix.json` |
| Win32 デバッガ下 | 原因調査と同じ `tools/catch-av.py` で修正版の `-q3shaders missing.pk3 -noupdate` を **60 回**。全回 exit 1、`no access violation caught`。`runs/exit-av-fix.txt`。この調査ツール自体は「捕捉なし」を終了コード 1 とする仕様 |
| 段階ゲート再実行 | `verify-stage.ps1 -Label fix -Server`：CLI **45/45** 一致、フラグ索引同一、実 UDP **8/8** 一致、CTest **122/122 PASS**、クラッシュ再試行 **0**。`gate-fix.log`、CTest 全出力は `runs/ctest-fix-full.log` |
| 生成中のランチャー終了 | §6.1。ウィンドウを閉じた後も生成中の 1 マップを完了し、次のマップに進まず exit 0 |

これらは Windows/MSVC の実行検証。§5 の Android/GCC 結果は ModEntry 分割完了時点のもので、今回の終了クラッシュ修正後には再実行していない。Linux/macOS の未検証扱いはユーザーの選択どおり維持する。

## 2. 最終構成

```text
src/MphRead.Native/Mods/
  ModEntry.hpp            無変更（公開 API 2 つ）
  ModEntry.cpp            2 段の入口：起動順序と、どの順に何を試すか
  Entry/
    CommandLine           フラグ探索のみ（IsFlag/HasFlag/IndexOfFlag/ValueAfter/ValuesAfter/StartsWithHyphen）
    OptionValues          型付きの値読み取り（invariant/current culture、Parse* の既定値、MachineName）
    LaunchPath            起動ディレクトリ基準のパス解決（FullPathCombine）
    RenderOverrides       ApplyRenderOverrides（2 回呼ぶ位置は ModEntry のまま）
    PreSetupDiagnostics   -rhicontract … -smoketest（設定ロード前）
    NetworkOptions        回線模擬 → ラグ補償 → ヒットリグ → ヒット解決（4 関数、呼ぶ順は ModEntry）
    UiCommands            -uishot … -tapcheck（noinline と Qt/SHELL 分岐を保持）
    ServerCommands        サーバ用 usage、更新の事前処理、-masterserver、-servers、-hosts、-server/-dedicated
    GameChecks            カタログ・シミュレーション試験、ゲーム内チェック（77 skip 保持）
    MapCommands           -mapbundle、MapGen 系、MapAudit 系、サムネイル
    NetworkCommands       -installserver、-hostlocal、-hostgame、-connect、-netcheck、-demoinfo
  Testing/TestModEntryCommandLine.cpp, TestModEntryOptions.cpp, TestModEntryValues.cpp
```

設計上の判断：

- **動的 registry・`std::function` の表・全オプションの事前解析は導入していない**。各モジュールは元の `if` 列をそのまま持つ `bool TryRunXxx(args)` で、未指定なら副作用なしで `false`。既存の `homingCheck` ラムダ（`std::function`）は移動しただけで増やしていない。
- **公開は小さな名前空間関数のみ**。`-masterserver` と `-servers/-hosts` は `ServerCommands.cpp` 内部関数で、外へ出すのは `TryRunServerCommands` だけ。
- 移動した本文は**字句どおり**（`::HasFlag` → `HasFlag` の修飾差のみ）。`ModEntry.cpp` 側は局所的な using 宣言で既存の `::HasFlag(...)` 呼出しをそのまま残した。
- `WriteLine` / `SetExitCode` の 1 行ヘルパーは各ファイルにローカルで置いた。`Mods/` の他 6 ファイルと同じ慣習で、元の本文の呼出しを書き換えないため。
- `MapCommands` は setup 前（`-mapbundle`）と setup 後の両方の関数を持つ。**どちらの段階で呼ぶかは ModEntry の呼出し位置が決め**、モジュールは段階を跨いで移動していない。

## 3. 段階ごとの差分レビュー票（要約）

全段階共通：Program 呼出し契約＝同じ／初期化・設定の順序＝同じ／引数解釈と優先順位＝同じ／stdout・stderr・exit code＝同じ（45/45）／例外とプロセス終了＝同じ／フラグ字句索引＝同一／依存グラフ＝新規 `.cpp` は既存の `GLOB_RECURSE` で製品ライブラリに入り、テスト 3 本のみ CMake に追記。

| 段階 | 移した責務 | 追加の検証 | exe SHA-256（先頭） | CTest |
|---|---|---|---|---|
| ME-01 | フラグ探索 → `CommandLine` | A01–A13 | （手動ゲート） | — |
| ME-02 | `-rhicontract`〜`-smoketest` → `PreSetupDiagnostics`、`FullPathCombine` → `LaunchPath` | `-rhicontract` 出力ハッシュ一致（D01） | 5D9658F604DC | 120/120 |
| ME-03 | UI ブロック → `UiCommands` | `-uishot` 実行（28 枚・stdout 一致） | 1B153136C5E3 | 120/120 |
| ME-04a | `ApplyRenderOverrides` → `RenderOverrides` | D11a–f（状態） | 747C96C845C7 | 121/121 |
| ME-04b | 回線模擬（NetLag） | D13a–d | 46F6F8C98646 | 121/121 |
| ME-04c | ヒットリグ | NET-rig a/b | 0F5B16143B34 | 121/121 |
| ME-04d | ラグ補償／ヒット解決 | D10a–c | E25577127BE3 | 121/121 |
| ME-05a | 型付き値の読み取り → `OptionValues` | V01–V18 | 06BF66912747 | 122/122 |
| ME-05b | `-q3maps/-q3shaders/-mapcheck` | | 2B9489EC437F | 122/122 |
| ME-05c | `-bottour`〜`-mapmaterials` | | 8BC125BC5E47 | 122/122 |
| ME-05d | `-mechanics`〜`-dpstest` → `GameChecks` | | E9A9073AF431 | 122/122 |
| ME-05e | `-aimcheck`〜`-simcheck` | CTest の homing/movie/netcombat 実行系 7 本 | DC9B18CDC1B2 | 122/122 |
| ME-05f | `-altprobe/-portalviews/-shotat` | | 8E8294BDFFC6 | 122/122 |
| ME-05g | `-mapgen/-mphtexdump/-mapsync` 合流 | | 2E61384CD6D7 | 122/122 |
| ME-05h | `-gpulifetime`〜`-maptest` | GPU：`-maptest`×2・`-shotat`（PNG バイト一致）・`-gpulifetime` | E11998E79500 | 122/122 |
| ME-05i | `-thumbnail(s)` | | 662B3809DD4B | 122/122 |
| ME-06a | `-servers/-hosts` → `ServerCommands` | 実 UDP ループバック 8/8 | 38ABC0C3A869 | 122/122 |
| ME-06b | `-masterserver` | 同上 | E8122CB94383 | 122/122 |
| ME-06c | usage・更新事前処理・`-server` | 同上、`MPHREAD_SERVER` 構文コンパイル＋負対照 | DB739A157D54 | 122/122 |
| ME-06d | `-installserver`〜`-demoinfo` → `NetworkCommands` | 同上 | 95DF60E5A0B4 | 122/122 |
| ME-07 | include／using の整理、未使用関数削除 | 全ゲート | D862A58C69E1 | 122/122 |
| ME-07b | `-mapbundle` → `MapCommands` | 全ゲート | DAA7842D7048 | 122/122 |

各段階の詳細は `ModEntry-evidence/gate-<段階>.log`（ME-01 は手動で同じ 3 点を確認：A ケース全 PASS、CLI 45/45、索引一致）。

最終状態（ME-07b）での再確認：GPU 4 ケース（`-maptest`×2・`-shotat` PNG バイト一致・`-gpulifetime`）4/4 一致、`-uishot` 28 枚・同一ファイル集合、`MPHREAD_SERVER` 構文コンパイル PASS、Android x86_64 差分再ビルド PASS。

再現手順：ベースライン exe は `tools/build/out/msvc-Release/FruityPrime-me00-baseline.exe`、最終は `FruityPrime-me07b.exe` として残してある。`pwsh -File ModEntry-evidence/verify-stage.ps1 -Label <名前> -Server` が現在のビルドを同じゲートにかける。`MPHREAD_SERVER` 検査と計測は `ModEntry-evidence/tools/`。

### 3.1 ME-06 の実 UDP ループバック（`server-loopback.py`）

127.0.0.1 のみで、外部サービスには一切接続しない。ベースライン同士で 8/8 一致を確認済み。

1. `-masterserver -port 27993 -hostports bad` → 警告を出して起動し続ける（タイムアウトで停止）
2. 専用ディレクトリ `-masterserver -port 27991 -hostports none`
3. 専用サーバ `-server -port 27992 -players 4 -servername "ME06 Loopback" -rotation <sandbox> -bots 1 -botlevel 2 -simulate -master 127.0.0.1 -masterport 27991`
4. `-servers`（名前・エンドポイント・マップ・人数の一覧）、`-hosts`
5. `-netcheck` クライアントが参加し 8 秒間プレイ（サーバ側の参加・識別・チャット・退出ログを比較）
6. サーバとディレクトリに**実際の Ctrl+C** を送り、両方が終了コード 0 で `shutting down` することを確認

比較対象：終了コード一式、両サーバの `[server]`/`[master]` 行（rotation・bot・登録・参加/退出・停止）、`-servers`/`-hosts`/拒否メッセージの全文、クライアントの参加マイルストーン。時刻・ping・クライアントのエフェメラルポートのみマスク。

## 4. 追加したテスト

| テスト | ケース | 内容 |
|---|---|---|
| `FruityPrime.ModEntryCommandLine` | A01–A13 | 指示書 §6.1 の 12 ケース＋先勝ち／`ValuesAfter` の読み方の補足 |
| `FruityPrime.ModEntryOptions` | D10a–c, D11a–f, D13a–d, NET-rig | 適用順そのもの（`-fpscap display` 後の `-vsync`、入力順でなくブロック順、`-relayedpuppets` が補間も切る、回線模擬の拒否で終了） |
| `FruityPrime.ModEntryValues` | V01–V18 | §6.1「必須追加」：不在・invalid・負数・区切り・非 ASCII・float 丸め・`allowThousands` 有無・current culture は全文化共通の範囲のみ・Parse* の既定値 |

## 5. プラットフォーム／構成

| 構成 | 結果 |
|---|---|
| Windows + MSVC + Qt（`MPHREAD_QT`+`MPHREAD_SHELL`） | **PASS**：全段階でビルド・CTest・CLI・GPU・ループバック |
| `MPHREAD_SERVER` 定義時 | **構文 PASS**：`ServerCommands.cpp`/`ModEntry.cpp` を ninja と同じコマンドに `/Zs /DMPHREAD_SERVER` を足してコンパイル。末尾に `#error` を仕込んだ複製で定義が届くこと（負対照）も確認。リンクは該当ターゲットが存在しないため対象外 |
| Android（`MPHREAD_SHELL` なし、POSIX 分岐） | **PASS（コンパイル・リンク）**：`tools/build/build-android-cpp.bat x86_64 libs` で `libFruityPrime_x86_64.so` までビルド（ME-07 時点で全 863 ステップ、ME-07b 後に差分再ビルド）。新規・移動ファイル内の診断は既存と同じ `[[nodiscard]]` 破棄 4 件のみ。端末上での実行はしていない（既知どおり端末未検証の head） |
| GCC 16.2（MSYS2 mingw64、`MPHREAD_QT`+`MPHREAD_SHELL`） | **PASS（コンパイル）**：`build-cpp.bat msys2 Release configure` で構成した ninja ツリーと同一の g++ コマンドで `ModEntry.cpp`・`Entry/*.cpp` 11 本・テスト 3 本の計 15 ファイル。診断は既存の `[[nodiscard]]` 4 件のみ（`runs/gcc-compile-units.txt`、`tools/gcc-compile-units.py`）。ツリー全体のリンクは、ライブラリの前提チェック対象が GCC で別件の失敗を起こすため対象外 |
| Linux + Qt / macOS + Qt | **ローカル未実施（環境なし）**：WSL にディストリビューションが入っておらず、macOS 機もない。検証完了時点ではコミット・プッシュの許可がなく CI も未使用。その後ユーザーの指示でプッシュするが、本記録のローカル検証と将来の CI 結果は区別する。代替として、POSIX 分岐（`/proc/self/cmdline`、`gethostname`）は Android（Linux カーネル）ビルドで、GCC 固有の診断は MSYS2 GCC で確認した。macOS 専用分岐は `GetCommandLineArguments` の `_NSGetArgc/_NSGetArgv` 3 行のみで、本文は字句どおり移動 |

## 6. 手動シナリオ（指示書が自動 CI から外すもの）

この機械（RTX 5070 Ti、ゲームファイルあり）で実行し、ベースライン exe と最終 exe を同じ手順で比較した。以下の手動セットは外部サービスに接続せず、追加のダウンロード比較はユーザーの許可を受けて §6.1 で実行した。

| シナリオ | 内容と結果 |
|---|---|
| ウィンドウ／GPU 診断 13 種 | `-vulkancheck` `-vulkanresourcecheck` `-vulkanpresentcheck` `-vulkanpresentfallbackcheck` `-reflexcheck` `-presentconformance` `-rhiconformance` `-glfwpathcheck` `-thumbnailwindowcheck` `-windowcheck` `-gamepadcheck` `-backdropparity`（117 ファイル）`-goldencapture hud`、`-shellshot`（ランチャー→試合→ポーズ→復帰） |
| D07 `-launcher -server` | ランチャーが起動し、`[server]` 行は 1 行も出ない（サーバは起動しない）。ランチャーは終了しないためタイムアウトで停止 |
| D14 `-update -noupdate` | ローカルビルドはネットワークに出る前に止まる。`[update] this is a local build, so it is left alone` が出ること自体が、`-update` が `-noupdate` を `Updater::Disabled(false)` で上書きする証拠（上書きされなければ理由は空） |
| D22 `-thumbnail A -thumbnail B` | 指定順に 2 室を撮影（captured 2/2）。キャッシュはビルド出力内（`msvc-Release/thumbnails`）にあり、実行前に退避して実行後に復元、ハッシュで原状回復を確認 |
| `-hostlocal` | サーバ実体がない場合の失敗経路（exit 1）と、ビルドの複製を `FruityPrimeServer.exe` として一時配置した成功経路（子サーバ起動 → 127.0.0.1:27888 に問い合わせ → 停止、残存プロセス 0） |
| `-connect` | ループバック試験に追加：実ゲームクライアントが専用サーバに参加し、サーバ側で `"CN" playing Kanden` と識別される |
| `DesktopUpdate::ApplyFlag` | サンドボックスに exe と DLL を置き、`-applyupdate <ダミーのインストール先> -1 --relaunch -debuglog -credits` を実行。両ビルドとも `[update] applying … done`、38 ファイルのコピー、既存ファイルの保持、インストール先からの再起動（`-debuglog -credits` で起動したログ）が一致 |

比較結果：手動セット 18/18 一致、ループバック（`-connect` 追加後）はベースライン 5 回・最終 3 回すべて 8/8 一致、`-hostlocal` 成功経路と `-applyupdate` は出力一致。

マスクした値はすべて、ベースライン同士でも変わることを確認したものに限る：Vulkan パイプラインキャッシュのサイズと生成時間、Qt の一時ディレクトリ名、GPU 使用メモリ、転送ナノ秒、描画画素数、`-reflexcheck` のドライバ待ち回数、サーバ側の bot の発射数と取得パワーアップ数（ベースラインで 18／5、1／2 と揺れる）。

付随して判明した既存挙動：`--relaunch` 以降の引数も**すべてのフラグ検索から見える**ため、たとえば再起動引数に `-rhicontract` を入れると適用する側のプロセスがそれを先に処理し、`-applyupdate` に到達しない（両ビルド同一）。

### 6.1 追加検証と未実施事項

| 項目 | 結果 |
|---|---|
| `-installserver` | ベースライン／ME-07b とも **exit 1**。`v0.14.0` に `win-x64` サーバパッケージがないという同一メッセージ。起動ごとのデバッグログパスを除き stdout 一致、stderr は空、`server/` 残存なし。**インストール成功経路は未検証**（配布物なし） |
| `-mapsync` 初回 | 両版とも **exit 0**。ベースラインは `mario.pk3` のダウンロードに失敗し **19/20**、ME-07b は **20/20**。この初回差分は保存し、一致したとは扱っていない |
| `-mapsync` ベースライン再試行 | 元の取得済みファイルを保持して再実行し、未取得の `mario.pk3` **1 件のみ取得、exit 0**。最終的な 20 ファイルの名前・サイズ・SHA-256 は **全件一致**。外部取得失敗を常に終了コードへ反映するかの仕様変更はしていない |
| マップ生成中のランチャー終了 | 修正版とゲームファイルの書き込み先を `tools/build/out/msvc-Release/modentry-exit-launcher-fix/` に隔離。既取得の `simpsons.pk3` の BSP を検証用の `EXIT_FIX_A/B` という 2 つの名前で配置。20:37:03 JST に A の生成を開始、20:37:36 にランチャーへ Alt+F4、**20:38:21 に A の生成完了とともに exit 0**。A の `translated.txt`・Model/Collision/Ent/Node が生成され、B のキャッシュはなく、`building EXIT_FIX_B` もない |
| Linux／macOS の実機ビルドと実行 | **未実施**。§5 の環境制約について、ユーザーが「Accept as unverified」を選択済み |

ダウンロードの証拠は `ModEntry-evidence/runs/downloads/` の初回 stdout/stderr、`mapsync-me00-baseline-retry.*`、20 件ずつの SHA-256 一覧を含む `comparison.json`。`tools/compare-downloads.py` で再比較可能。初回の両終了コードは引き継ぎ JSONL のダウンロード実行結果から `initial-results.txt` に抽出して保存した。

ランチャー終了の証拠は `runs/exit-launcher-fix.result.txt`・`exit-launcher-fix.stdout.txt`・`exit-launcher-fix.stderr.txt` と `runs/exit-launcher-fix-summary.json`。`tools/prepare-exit-launcher.ps1` が隔離先を作成する。Computer Use で対象ウィンドウを一意に特定し、Alt+F4 を送った。画面キャプチャは `IGraphicsCaptureItemInterop.CreateForMonitor` の `0x80070424` で取得できず、ウィンドウのアクセシビリティ情報、生成ファイル、プロセスの終了を証拠とした。終了待ち中に受動 watchdog の `[freeze]` 行が出るが、生成完了後に正常終了している。テストの終了を確認後、ユーザーの画面作業に合わせて以後の画面・キー操作は行っていない。

## 7. 計測（改善の主張はしない）

以下は **ME-07b（終了クラッシュ修正前）** の比較。別差分の修正版 exe は 18,935,296 B で、ME-07b 比 +2,560 B。この増分を ModEntry 分割の測定値に混ぜていない。

| 項目 | ベースライン | 最終 | 差 |
|---|---|---|---|
| FruityPrime.exe（MSVC Release） | 18,924,032 B | 18,932,736 B | +8,704 B（+0.05%） |
| `ModEntry.cpp` のみ再コンパイル（MSVC `/c`、ninja と同一コマンド、逐次・2 回の最小値） | 4.13 s | 1.95 s | `ModEntry.cpp` だけを触る変更は速くなる |
| 同じ責務の全 TU の合計（ModEntry + Entry 11 本） | 4.13 s | 16.75 s | **全体の CPU 時間は増える**（TU ごとにヘッダを読み直すため）。最長の 1 本は 2.17 s で、並列ビルドの壁時計時間は増えない |

## 8. 推奨フォローアップ（本作業の範囲外）

1. `-netcombatcheck` 2 本目・`-gamepad` setup 後コピーの削除可否を、指示書どおり独立チケットで判断。
2. `MPHREAD_SERVER` を定義する C++ ターゲットが今後も作られないなら、`ServerUsage` と関連分岐の扱いを決める。

## 9. 変更していないこと

`namespace MphRead`、`Program.cpp`、`ModEntry.hpp`、`Entities/`・`Formats/`・`Metadata/`、ネットワークプロトコル、サーバ権威、レンダラ設定の意味、ゲームロジック、既存テスト。

## 10. Git 提出

検証完了後、ユーザーの「コミットプッシュしておいて」により提出を許可された。ModEntry 分割は `c00028af`（`Refactor ModEntry into focused command modules`）、終了クラッシュ修正は独立した `d32112b6`（`Join background map generation before process shutdown`）に分離。指示書・本記録・検証ハーネスとテキスト証拠は追加の記録コミットにまとめた。

取得済みマップ、スクリーンショット、描画画像と一時 sandbox はローカルに保持し、Git には含めない。テキスト証拠のパス・ログ・ハッシュは保存し、出力の空白も改変しない。別作業の `pending/` は今回のコミット対象外。
