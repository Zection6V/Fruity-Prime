# Fruity-Prime C++ — `ModEntry` 限定・段階的リファクタリング実装指示書

> **ステータス**：実装担当者向け指示／C++未変更／実装前の契約固定が必要  
> **監査基準**：`Zection6V/Fruity-Prime` の `main`、commit `66fdfd4f23d0e84837c1b16b5a91b8b48113708b`（2026-10-15）、ユーザー提供の同一Git Tree ZIP  
> **対象**：`src/MphRead.Native/Mods/ModEntry.cpp`（2,677改行終端行相当、約2,678行、105,712 bytes）、`ModEntry.hpp`（21行）、入口の参照用として`Program.cpp`。  
> **性質**：変更指示・作業分解・契約テスト仕様。リファクタリングを実施した結果でも、全テストを実行済みという報告でもない。

## 0. この指示書の結論

`ModEntry`の**全体を一度に書き換えてはいけない**。最初にCLIの解釈・実行順・副作用・終了コードを固定し、その後は**1つの責務・1つの小変更・1回の回帰検証**で進める。新しい巨大な`Dispatcher`や動的コマンド登録表への全面置換は禁止する。

**完了の定義**：`Program`からの呼び出し契約、コマンドの優先順位、Headless経路、各オプションのデフォルト値・文化依存の解析、ログ・戻り値・プロセス終了コード、プラットフォーム条件を変更せず、`ModEntry.cpp`を「起動・実行コマンド振分けの薄い入口」へ整理すること。行数だけの削減、ファイル数の増加、未計測の高速化を成果としない。

**作業制約**：

- C++20。**`namespace MphRead`を変更しない**。`Entities/`、`Formats/`、`Metadata/`には手を付けない。`Program.cpp`は原則変更せず、既存の2つの呼び出し点を保持。
- 新しい共通化ロジックは`src/MphRead.Native/Mods/`配下。`.hpp`と`.cpp`を同階層に置く。`include/`ディレクトリは新設しない。
- ネットワークプロトコル、サーバー権威、レンダラ設定の意味、ゲームロジック、既存テスト仕様の変更は対象外。
- **コミット・プッシュはしない**。担当者はローカル差分と検証記録を提出し、承認前に別領域へ展開しない。
- 旧C#の`src/MphRead/Mods/ModEntry.cs`は移植の参照元だが、現行C++に追加された機能・終了コード・条件分岐を削除してC#へ機械的に戻してはならない。**今回の「挙動を変えない」契約は、監査基準の現行C++**。

## 1. 実コードで確定した呼び出し経路

```text
Program::Main(args)
  └─ Program::Run(args)                                      Program.cpp:615
       ├─ ConsoleSetup::Run()                               :617
       ├─ Windows: ConsoleWindow::Prepare(args)             :619–620
       ├─ ModEntry::TryHandleHeadless(args)                  :622  ← ゲームファイル確認前
       │    ├─ true  → Program::Run終了（通常起動へ進まない）
       │    └─ false → 次へ
       ├─ CheckSetup(args)                                  :626  ← paths.txt等の判定
       │    ├─ true  → Program::Run終了
       │    └─ false → 次へ
       ├─ Program::ParseArguments(args)                     :631
       ├─ ModEntry::TryHandle(args)                          :632  ← ゲームファイル確認後
       │    ├─ true  → Program::Run終了
       │    └─ false → Programの通常の既存CLIへフォールバック
       └─ 既存のメニュー／抽出／export／room等の処理

Program::Mainのcatch(...) → CrashReport::Report(..., "startup")、終了コード1（Program.cpp:596–608）
```

**最重要な責務境界**：名前が`Headless`でも、`TryHandleHeadless`にはQt UI診断・グラフィックス設定・ローカルサーバー操作が含まれる。正確には**`CheckSetup()`より前に実行される入口**であり、「描画機能を呼ばない」という意味ではない。

| 実コード位置 | 分類 | 役割・保持する契約 |
|---|---|---|
| `ModEntry.hpp:9–17` | 公開API | `static bool TryHandleHeadless(const vector<string>&)` / `TryHandle(...)`の2つのみ。戻り値`true=処理済み`を保持 |
| `ModEntry.cpp:191–287` | CLI検索 | `-`を任意個除去・大文字小文字無視・**先に見つかった値**を使用。後述の例外的挙動を保持 |
| `:293–353` | 数値・enum解析 | invariant/current culture等の異なる規則を**統合しない** |
| `:362–502` | OS/出力境界 | OS引数再取得、パス、マシン名、stdout、終了コード設定 |
| `:507–633` | 文字列整形／既定値 | UTF-16コード単位の表示幅、サイズ、サーバーポート・名前・Hunterなど |
| `:636–809` | 設定適用 | 描画設定を副作用付きで逐次適用。順序依存あり |
| `:812–1065` | 用途別コマンド補助 | サーバーヘルプ・サーバー一覧、Qt/シェルUI診断、サムネイル |
| `:1069–1868` | 前段ディスパッチ | 初期化、特殊診断、更新、サーバー起動・停止 |
| `:1871–2676` | 後段ディスパッチ | ゲームファイルを使用する診断、マップ/ネット/接続/サムネイル |

参照：[Program.cpp](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Program.cpp#L615-L638)、[ModEntry.cpp](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/ModEntry.cpp)。

## 2. 先に保全する「変更してはいけない挙動」

### 2.1 引数探索の実際の規則

`IsFlag`(`:223`)は各引数の**先頭のハイフンを0個以上除去**して名前を大文字小文字無視で比較する。`HasFlag`(`:228`)は全引数を順に見る。`ValueAfter`(`:252`)は**最初に一致した引数の直後のトークンをそのまま返す**。次のトークンが別の`-flag`であっても、この関数自身は除外しない。`ValuesAfter`(`:269`)は重複した指定をすべて順序通りに返す。`IndexOfFlag`(`:240`)も最初の位置を返す。

**変更禁止**：`unordered_map`化／一般的なCLI parserへの置換で以下を変えてはいけない。

- `-FLAG`、`--flag`、`flag`等が現在の`IsFlag`では一致すること。
- 重複指定で先頭の値が選ばれること。`-thumbnail`だけは`ValuesAfter`で複数値を拾うこと。
- `-name -server`のような「次トークンがフラグ」の扱いが、**呼び出し側の個別判定によって違う**こと（全体で勝手に拒否しない）。
- 引数の順ではなく、**ソースコード中の`if`の並びがコマンド優先順位を決める**こと。複数主コマンドを与えた場合も例外ではない。
- `Program::ParseArguments`の結果ではなく、生の`vector<string>`を引き続き用いること。
- `GetCommandLineArguments()`(`:362`)は元の`args`とは別にOSから引数を読み直し、サーバー自動更新に渡している。勝手に置き換えないこと。

### 2.2 数値・文字列・既定値を統一しない

| 関数／経路 | 現行の解析契約 | 変更すると危険な点 |
|---|---|---|
| `TryParseDoubleCurrent(:331)` | `DoubleTryParseCurrentCulture` | `-gamepad -seconds`等で文化依存。invariantへ統一不可 |
| `TryParseDoubleInvariant(:309)` / `TryParseFloatInvariant(:320)` | 数値スタイルに`AllowThousands`可否あり、引数がないとout値を0にする | `-dpstest`、`-at`、`-mapitems`の解析とfallbackが変わる |
| `ParseSize(:561)` | `-size`の`WxH`、不正なら警告＋サムネイル既定値 | **`TryHandle()`の先頭でコマンドと無関係に実行** |
| `-shotat`(`:2119–2150`) | `-size`は**カンマ区切り**で初期値`960,540` | `ParseSize`の`WxH`へ一本化すると互換性破壊 |
| `-maptest`(`:2384–2454`) | `-size`は`WxH`、不正なら`MapAudit::WindowSize`未変更 | `-shotat`と合わせない |
| `ParsePort(:590)` / `ParseMasterPort(:601)` | 個別の既定値、current cultureのint解析 | 同じ`port`でも`-masterserver`は`-port`優先`-masterport`代替 |
| `ParseName(:612)` | `-name`不在時に`MachineName()` | `-connect`/`-hostgame`の名前に影響 |
| `DotNetUtf16Length(:507)` + `LeftAlign(:549)` | `.NET String.Length`に合わせUTF-16単位で幅計算 | UnicodeをUTF-8バイト長やコードポイント長に置き換えない |

### 2.3 呼び出し順序そのものが仕様

`TryHandleHeadless`の「入口から最初の`return true`まで」を、次の順番から変えない。

1. `ApplyRenderOverrides(args)`、起動ウィンドウモード（`:1071–1079`）。
2. `-rhicontract`、Vulkan/RHI/present診断、`MPHREAD_SHELL`診断、`-smoketest`（`:1080–1160`）。**InputSettingsやDebugLogのロード前に終了可能**。
3. `InputSettings::Load()`と一時`-nativeaim`変更（`:1161–1172`）。不正値なら`ProgramException`。
4. `LauncherPrefs::Load()`、`-debuglog`の強制・`DebugLog::Attach()`、macOS診断（`:1173–1182`）。
5. `Updater::Disabled(-noupdate)`、**2回目の**`ApplyRenderOverrides(args)`（`:1183–1185`）。呼び出し重複は確認済み。初期化順を調べず「DRYだから1回」にしない。
6. Pointer / aim assist / gamepad / frame-timing（`:1187–1220`）。
7. Desktop update apply（`:1222–1247`）。**初期化後、他の通常コマンドより前**で実行。
8. `DesktopUpdate::Clean()`（特定checkならスキップ）、`UpdateInstall::UseDesktopIfPossible()`（`:1249–1254`）。
9. NetLag・HitRig・NetUnlagged・NetHitPrediction・NetSmoothingなどの**設定副作用を上から順番に適用**（`:1256–1425`）。不正な`-netlag`等はメッセージを出して`true`だが、ここでは必ずしも`SetExitCode(1)`しない。
10. Credits / mapdir / golden capture / mapbundle / update / UI captures（`:1427–1550`）。
11. **再度**WindowMode選択、launcher/text launcher判定、server usage（`:1552–1593`）。前段のウィンドウ変更はUI診断前、後段は通常起動前であり意味が異なり得る。
12. サーバー更新の事前処理（`:1595–1609`）→ `-masterserver` → `-servers` → `-hosts` → `-server|-dedicated`。順序変更禁止（`:1611–1868`）。

`TryHandle`でも**コマンドを探す前**に`ParseSize`(`:1873`)、条件付き`CustomRooms::StartGenerating()`(`:1879`)、`-instagibdemo/-instagib/-nohelmet/-netdebug`の変更(`:1887–1907`)が発生する。診断コマンドを新しい関数に移す場合もこの順番を守る。

### 2.4 `true`・`false`・終了コード・例外は別の契約

| 結果 | 意味と注意点 |
|---|---|
| `return true` | コマンドを処理済みとして`Program::Run`を終了。**成功を保証しない**し、必ずしも終了コードを設定していない |
| `return false` | 現行`TryHandleHeadless`から次のsetup判定へ、`TryHandle`からProgramの既存処理へ進む |
| `SetExitCode(n)` | `NativeRuntime::SetEnvironmentExitCode`への代入。**その場でprocess exitしない** |
| `SetExitCode(77)` | game-file依存のmovie/homingチェックがデータ不足時に使うskip値。CTestの`SKIP_RETURN_CODE 77`とセット |
| 例外 | `Program::Main`で捕捉しcrash report＋終了コード1。例外を`false`に変換しない |
| `std::exit(1)` | 専用サーバー起動の`ProgramException` catch(`:1856–1864`)に実在。通常の`SetExitCode(1)`＋`true`へ勝手に変更しない |

**既存不整合と区別**：`main`のProtocol30～32 Golden SHA検証はLF/CRLF差で既知の赤。`ModEntry`の修正と混ぜない。基準テストが赤なら「事前から赤」と証拠を添え、`ModEntry`起因の赤を相殺しない。

## 3. コマンドの担当領域／優先順位（実装順に整理）

以下の「順序」は`ModEntry.cpp`における**ブロックの出現順**。表内でまとめたフラグ同士の順序もソース上の上から順のままとする。区分変更はできても、この順番を同時に変えない。

### 3.1 setup前：`TryHandleHeadless`（コマンド主経路）

| 行 | コマンド／検査契機 | 現在の動作・分割先候補 | 特に守る条件 |
|---:|---|---|---|
| 1080 | `-rhicontract <file>` | `SceneBackendContract()`をファイルに保存 | 起動ディレクトリからのパス解決、例外時exit=1、最優先 |
| 1097–1136 | `-vulkancheck`, `-backdropparity`, `-presentconformance`, `-rhiconformance`, `-reflexcheck`, `-vulkanpresentcheck`, `-vulkanresourcecheck`, `-vulkanpresentfallbackcheck` | RHI診断 | 各`Run()`の戻り値をexitに転送。コマンドの優先順位を維持 |
| 1138–1153 | `-glfwpathcheck`, `-thumbnailwindowcheck`, `-windowcheck` | Shell診断 | `#if MPHREAD_SHELL`、`legacyglcheck`従属オプション |
| 1155 | `-smoketest` | 互換診断 | ロード前に直接終了 |
| 1187–1220 | `-pointercheck`, `-gamepadcheck`, `-gamepad`, `-frametimingcheck` | Input/FrameTiming診断 | 設定ロード**後**。`-gamepad`は後段にも現れる |
| 1222–1247 | DesktopUpdate::ApplyFlag | 起動中アップデートの適用 | 引数位置＋セパレータ、待機対象PID、再起動引数の扱いを固定。一般テストで不用意に実行しない |
| 1256–1425 | `-netlag`, `-netspike`, `-netloss`, `-netjitter`, `-netseed`, `-netreorder`, `-netduplicate`, `-hitrig`, `-rig*`, `-maxrewind`, `-nointerp`等 | 通信・描画・命中検証の設定副作用 | 下表のオプション順序、invalidの継続/中断、stdoutを保持 |
| 1427 | `-credits` | Credits出力 | それ以前の設定副作用の後に処理 |
| 1433–1493 | `-mapdir`, `-goldencapture`, `-mapbundle` | Map/GoldenCapture | `mapdir`は`mapbundle`より前。`goldencapture`はSHELL条件 |
| 1495–1508 | `-update` | リリース確認とURLの表示 | `Updater::Disabled(false)`で`-noupdate`状態を上書きすることを保持 |
| 1510–1550 | `-uishot`, `-uibench`, `-uidesign`, `-classicframes`, `-shellshot`, `-tapcheck` | UIコマンド | Qt有無によるエラー文面、`-uibench*`の拒否、`-classicframes`第2値の位置 |
| 1552–1593 | `-launcher`, `-menu`, `-text`, serverビルドの引数なしusage | 起動メニュー | Win/macの無引数とLinux無引数の扱いを維持。`-launcher`は`-server`より先 |
| 1595–1609 | `-server`, `-dedicated`, `-masterserver`への事前更新 | Server updater | OS引数再取得、`ServerUpdate::AtStartup()`がtrueなら以降停止 |
| 1611–1675 | `-masterserver` | ディレクトリ起動 | port選択・hostports parsing・公開ホスト・stop_source |
| 1676–1683 | `-servers` | ディレクトリへの一覧照会 | NetMasterClient/NetStatus問い合わせの順 |
| 1684–1741 | `-hosts` | host discoveryと20秒待機 | shared stateとmutex/condition_variableの寿命を保つ |
| 1743–1868 | `-server`, `-dedicated` | 専用Authority server起動 | MapRotation・bots・master登録・AutoUpdate・shutdown・`std::exit(1)`、前段の更新とセット |
| 1743 | 非server系の残余 | `return false` | `CheckSetup()`を省略しない |

**ネットワーク設定副作用の現行順**（`:1256–1425`）：`netlag → netspike → netloss → netjitter/netseed/netreorder/netduplicate → bad-line表示 → homingtrace → nounlagged → snapshotpuppets → clientpin → pressage → hitrig → rigmorph → rigspeed → rigrange → rigstrafe → maxrewind → nohitprediction → nohitmarker → hitlog → servershots → noobservedimpacts → noconfirmedimpacts → noretarget → nodeathprediction → noclaims → nointerp → relayedpuppets`。`relayedpuppets`は位置コピー設定のほか、**NetSmoothingもfalseにする**。

### 3.2 setup後：`TryHandle`（コマンド主経路）

| 行 | コマンド／検査契機 | 責務グループ | 注意点 |
|---:|---|---|---|
| 1873–1907 | `ParseSize`、background MapGen、instagib/nohelmet/netdebug | 事前副作用 | **どの分岐より先**に実行。`thumbnail`値指定時はbackground生成しない |
| 1909–1986 | `-mechanics`, `-gamepad`, `-resourceaudit`, `-healthsimtest`, `-powerupsimtest`, `-netlobbytest`, `-rooms`, `-dpstest` | カタログ／テスト | `-gamepad`はsetup前段でも扱う。DPSのseconds/distanceはinvariant |
| 1987–2056 | `-mapgen`, `-q3maps`, `-q3shaders`, `-mapcheck` | MapGen解析・生成 | `-mapgen`は全件生成＋失敗集計。部屋ごとの例外継続 |
| 2057–2158 | `-altprobe`, `-portalviews`, `-shotat`, `-mphtexdump` | マップ検査／描画 | `-shotat`だけ座標3値と`-size W,H`を扱う |
| 2160–2217 | `-mapsync`, `-bottour`, `-moverprobe`, `-padprobe`, `-mapitems`, `-mapmaterials` | マップ同期／診断 | 実ファイル・ネットワーク副作用あり |
| 2219–2296 | `-aimcheck`, `-weavelaltcheck`, `-movieheadlesscheck`, `-homing*check`, `-netcombatcheck`, `-hostauthoritycheck`, `-lockjawenemycheck` | ゲーム内テスト | 無ゲームデータ時に`77`返すチェックと返さないチェックが混在 |
| 2298–2345 | `-dialanchepeercheck`, `-dialanchecheck`, `-spireposecheck`, `-simcheck` | Combat/Simulation | port、players、mode、secondsのdefaultを維持 |
| 2347–2457 | `-gpulifetime`, `-mapteststorymoviecycle`, `-mapteststorymovietransition`, `-maptest` | GPU/MapAudit | `maptest`には多くの独立フラグとムービー／非同期cleanup検査 |
| 2459–2490 | `-installserver` | server package | インストールという副作用があるため自動CLI試験から分離 |
| 2491–2541 | `-hostlocal` | ローカル専用server管理 | 子プロセス起動・sleep・Stopまで。失敗時と正常時の終了コード差 |
| 2543–2590 | `-hostgame` | Directoryにhost要求して接続 | MapRotation、timeout、要求失敗時exit=1、成功時NetConnect |
| 2592–2598 | `-connect` | 既存serverへ接続 | `ParsePort/ParseName/ParseHunter/ParseRecolor`を使用 |
| 2600–2651 | `-netcheck` | 実UDP/AI/録画検証 | spectate/rejoin、HUD、AI、map vote、秒数、colorの既定値を保持 |
| 2653–2674 | `-demoinfo`, `-thumbnail`（複数）, `-thumbnails` | リプレイ／出力 | `ValuesAfter(…, "thumbnail")`で複数処理。コマンドの順はdemoinfoが先 |
| 2676 | 未処理 | `return false` | Program本来のCLIへ処理を返す |

### 3.3 分岐重複の監査結果（**勝手に削除しない**）

- **`-gamepad`**：Headless側`:1203`で必ず`true`になる経路があるため、`Program::Run`の通常フローでは後段`:1915`へ届かない。ただし`TryHandle()`が公開APIとして直接呼ばれる可能性を排除するまでは削除しない。分類は「通常フローで重複、削除可否保留」。
- **`-netcombatcheck`**：`:2264–2283`の`homingCheck("netcombatcheck", ...)`は`HasFlag`がtrueなら`77`または実行結果で`true`を返す。後段`:2292–2296`の`ValueAfter("netcombatcheck")`が値を返す入力では前段も必ずtrueとなるため、**同一の`TryHandle()`呼び出し内で後段は論理上到達不能**。候補を「重複確定／削除判断は単独変更・検証後」として記録する。
- **`ApplyRenderOverrides`**：`:1071`と`:1185`の2回実行。重複を見つけても、設定ロードとの順序・Qt生成前の適用をテストで固定するまでは統合禁止。
- **WindowMode起動設定**：`:1074–1079`と`:1552–1564`の2箇所。呼出し時点が異なるため、`DRY`だけを理由に消去禁止。
- **`-netcombatcheck` のgame-file前提**と**`-movieheadlesscheck`/homing checkの77 skip**は既存コードの挙動であり、すべての診断を一律「常にGameFiles必要」にしない。

## 4. 推奨する新しい責務境界（最小構成）

**最終構成は必須のファイル数ではない**。まず少数の確定した責務を切り出し、改善が証明されなければ追加クラスを作らない。CMakeの`GLOB_RECURSE`で新規`.cpp`は収集対象となるため、テスト専用ファイルの配置にも注意。

```text
src/MphRead.Native/Mods/
  ModEntry.hpp                     [既存] 公開APIは変更しない
  ModEntry.cpp                     [既存] 2段入口と現在順序の振分け
  Entry/                           [必要になったものだけ段階追加]
    CommandLine.hpp/.cpp           初回の安全な分離：引数探索・純粋parse
    Diagnostics.hpp/.cpp           RHI／Inputなどの検査ハンドラー（準備が済んだ群のみ）
    UiCommands.hpp/.cpp            UiCapture/ShellCapture/Frame系（UI専用）
    NetworkOptions.hpp/.cpp        NetLag・HitRig等の設定適用（副作用順を保持）
    MapCommands.hpp/.cpp           MapGen／MapAuditのコマンド委譲（機能本体は移植しない）
    ServerCommands.hpp/.cpp        directory/server起動。最後の高リスク段階のみ
```

- 公開するのは小さな名前空間関数と必要最小の値型だけ。通常起動時に全ハンドラーを生成する**動的registry、`std::function`群、`unordered_map`、ヒープ管理のCommandオブジェクトは禁止**（測定等で明確な必要性がある場合は別提案）。
- コマンド側が現在の`vector<string>`から直接値を取る設計を初期段階では維持。巨大な`AllOptions`構造体を作って全オプションを毎回解析しない（Pay for What You Use）。
- 他サブシステムに既存`Run(...)`や`Configure(...)`がある場合、**実機能本体はそこに残し**、新モジュールに重複実装を作らない。
- `Entry/CommandLine`は純粋な探索だけを担当。`ParseSize`（警告出力、Thumbnail既定値）、`ParseName`（OSマシン名）、`ServerUsage`（出力）、`ApplyRenderOverrides`（描画設定）を「純粋parser」と誤分類しない。
- 依存に`Scene`、`Qt`、`OpenGL`等の大きなヘッダーが必要な機能は**それぞれの`.cpp`内**でincludeする。`ModEntry.hpp`は現状の軽量な状態を維持。
- モジュール境界に`Handled(bool)`しか戻さない過剰なラッパーは作らない。既存`SetExitCode`の有無を保持するため、戻り値はまず`bool handled`でよい。将来`ExitCode`型に移すのは別の**挙動変更**として扱う。

### 4.1 安全なディスパッチ抽出の形（概念例）

以下は**変更順を示す擬似コード**であり、現在の実装全文を置き換える完成コードではない。

```cpp
bool ModEntry::TryHandleHeadless(const std::vector<std::string>& args)
{
    ApplyRenderOverrides(args);    // 現行の最初の適用位置を残す
    /* WindowMode::Startupを現行の順番で適用 */
    if (Entry::TryRunPreSetupDiagnostics(args))
        return true;
    InputSettings::Load();        // この前後へ検査を移さない
    /* -nativeaim → prefs → DebugLog → Updater::Disabled */
    ApplyRenderOverrides(args);    // 現行の2回目も残す
    /* 以降の分岐を従来の順番で一つずつ委譲 */
    return false;                 // 未処理なら必ずProgramへ返す
}
```

**レビューの焦点**：`if (Entry::TryRunXxx(args)) return true;`は副作用前後の挿入位置と、`true`時の早期終了まで含めて「元の`if`列と等価」でなければならない。`TryRunXxx`内部に移す際、コマンドが未指定のときに設定初期化・`std::function`構築・全コマンドの引数解析を始める実装は避ける。

## 5. 担当者が実施する作業順序（各ゲートを通るまで次へ進まない）

### ME-00 — 無変更のベースライン採取【必須・最優先】

**変更**：なし。記録のみ。

1. `main`のcommit/treeと`ModEntry.cpp/.hpp`のSHA、コンパイラ・CMake版、ターゲットOS、`MPHREAD_QT`/`MPHREAD_SHELL`/`MPHREAD_SERVER`の**実際のコンパイル定義**を採取。ソース上の`#if`の存在を「そのビルドで有効」とは見なさない。
2. `Program.cpp:615–638`の呼出し経路を記録し、`Program`の2回のhookと`CheckSetup`の順番が不変であることを固定。
3. 本書の§3で列挙した全主コマンドについて、`flag名／値の有無／前段or後段／優先順位／初期化／出力／exit code／ゲームファイル要否／コンパイル条件／副作用`のCSV/TSVを作成。欠落がないか実コードとの機械的検索で検証。
4. 自動テスト可能な**無害な**コマンドのみ、旧バイナリのstdout/stderr/終了コード/出力ファイルhashを保存。ゲームデータやGPUが必要なコマンドは環境依存ラベルを付ける。
5. ネットワーク接続、server起動、installer、`-update`、DesktopUpdate::ApplyFlagは勝手にCIで実行しない。隔離した専用fixtureが用意できるまで手動検証へ分類。

**完了ゲート**：ベースラインに未収集コマンド・不明な終了コード・実行環境不明の`#if`が残る場合、リファクタリングを開始しない。許容するスキップには理由を添える。

### ME-01 — 引数探索だけを抽出【最初のC++変更候補／低リスク】

**対象**：`ModEntry.cpp:191–287`の`IsAsciiWhitespace`／`TrimNumberWhitespace`／`TrimAscii`／`TrimStartHyphen`／`IsFlag`／`HasFlag`／`IndexOfFlag`／`ValueAfter`／`ValuesAfter`／`StartsWithHyphen`。

1. `Mods/Entry/CommandLine.hpp/.cpp`に必要な関数を移動。元の匿名namespaceからの参照は明示的な`Entry::...`または局所的`using`で修正。**関数本文は原則そのまま移動**。
2. 公開インターフェイスを`std::string_view`と`const std::vector<std::string>&`等に限定し、呼出し先をこの段階で最適化しない。
3. parser単体fixture（§6.1）を追加。`--FLAG`、重複、終端、空値、`-flag -other`、大文字小文字などの実結果と一致させる。
4. エントリーヘッダーを別巨大ヘッダーにしない。`clang++/MSVC`の構文・リンク検証を行う。

**変更禁止**：`ParseSize/ParsePort`の移動、数字の解析規則、分岐順序、`main`の引数受け渡しを同時変更しない。

**完了ゲート**：parserケース全一致＋両dispatcherのCLIスモーク結果に差分0。崩れたらこの差分だけを戻す。

### ME-02 — 前段の「即時診断」だけを切り出す【中低リスク】

**対象**：`TryHandleHeadless:1080–1160`のRHI/Vulkan/Platform/SMOKE。**`ApplyRenderOverrides`とWindowModeの位置は固定**。

1. 元の`if`列を**上から同順**に、新モジュール内の小さな連続`if`列として移動。関数は`bool TryRunPreSetupDiagnostics(args)`相当で、処理済みなら`true`、それ以外は`false`。
2. `-rhicontract`のパス解決、書込失敗時`SetExitCode(1)`、`MPHREAD_SHELL` guardとQt未利用経路を保持。
3. 未処理なら次の`InputSettings::Load()`へ**何の副作用もなく**通過できることを確かめる。

**完了ゲート**：無ゲームファイル環境でも実行できるケースのstdout/終了コード一致。`-rhicontract`の出力バイト一致。CheckSetupの開始条件不変。

### ME-03 — 独立UIコマンド補助を抽出【中低リスク】

**対象**：`RunUiCapture(:934)`、`RunUiBench(:950)`、`RunUiDesigns(:984)`、`RunShellCapture(:996)`、`RunTapCheck(:1024)`、`GenerateThumbnails(:1034)`のうち、**1機能ずつ**。

- `noinline`コンパイラ属性、プラットフォームの`#if`、Qtなし時のメッセージと戻り値、`-uibench`の旧CPU系オプション拒否を維持する。
- `-classicframes`は`IndexOfFlag`の**直後だけでなく2トークン後**を任意スクリプトにしている。移動時に共通parserへ変換しない。
- `GenerateThumbnails`は`force`/`jobs`の既定値、ファイル存在、並列作業数をそのまま渡す。ここでBatch側へ最適化は加えない。

**完了ゲート**：Qt有/無での分岐と、データがある場合の描画・出力が一致。GUI依存試験は対応OSで実行。

### ME-04 — 描画・入力・ネットワーク設定の適用を整理【中～高リスク】

**対象**：`ApplyRenderOverrides(:636–809)`とHeadless`:1161–1425`の設定系。**挙動の修正はせず、責務のみ移動**。

1. `ApplyRenderOverrides`は**関数単位で移動**可能だが、`TryHandleHeadless`からの2つの呼び出し位置を変えない。
2. Render: `fpscap=display`は`ForceVSync(true)`してCapをUnlimitedへ変換し、**後続の**`-vsync`が再設定する。`-fpsmeasure`はcapを読む。順序を入れ替えない。
3. Input: `InputSettings::Load()`→`-nativeaim`の一時上書き→Prefs→DebugLog。`-nativeaim`の`touch|dual|nonexact-touch|nonexact-dual`以外で`ProgramException`を維持。
4. Network: まず既存設定適用コードの呼出し先と副作用を一覧化。**1カテゴリを1差分**にし、`NetLag`、`HitRig`、`NetSmoothing`等の処理をひとまとめの大変更にしない。
5. Update: `Updater::Disabled`、DesktopUpdate::Apply、DesktopUpdate::Clean/UseDesktopIfPossibleの位置はこの段階では**移動しない**。

**完了ゲート**：設定値の前後一致、ログの重複回数も一致、FPS/VSync/RHIの指定順と`-nointerp/-relayedpuppets`の優先順位一致。ネットワーク機能の試験が必要な箇所は、実UDPシナリオの前にマージ可扱いしない。

### ME-05 — 後段のMapGen／診断コマンドを小単位で移す【中リスク】

**対象**：`TryHandle:1909–2457`。**初回は`-q3maps`等、他と状態を共有しにくいコマンドから始める**。

1. 第一候補：`-q3maps`, `-q3shaders`, `-mapcheck`, `-mphtexdump`, `-mapmaterials`。**1–2枝ごと**に`Entry/MapCommands`等へ委譲する。
2. 次の候補：`-rooms`, `-dpstest`, `-altprobe`, `-moverprobe`, `-padprobe`, `-mapitems`。オプションの解析値と既定値を比較。
3. Map生成や描画を伴う`-mapgen`, `-portalviews`, `-shotat`, `-mapsync`, `-maptest`は**後回し**。特にMapAuditのムービー・音声・cleanup・ウィンドウ設定を変更しない。
4. `-movieheadlesscheck`やhoming系の`77` skipを維持。既存の`-netcombatcheck`二重分岐の片方を取り除く場合、**独立の“重複到達性”チケットにして、fixtureで実証してから**。
5. 現在の`ParseSize`→`StartGenerating`→instagib等の副作用を移動しない。

**完了ゲート**：各コマンドの入出力、生成物hash、失敗時のログ、`return true/false`一致。MapGenの成果物生成後もユーザーデータを汚染しない隔離試験で検証。

### ME-06 — Server系CLIの責務を切り出す【高リスク・最後】

**対象**：`TryHandleHeadless:1595–1868`、`ListServers(:853)`、`ServerUsage(:812)`。

1. 最初に`-servers`と`-hosts`の表示・問合せだけを整理し、`-masterserver`と専用server起動は別の後続差分にする。
2. `-masterserver`は`-port`優先、`-masterport`次点、`-hostports`の解析・ログ・`ShutdownSignals`・`stop_source`を変更しない。
3. `-server/-dedicated`は`Update::ServerUpdate::AtStartup(typed)`の**後**、`MapRotation`ロード、`ServerLaunchOptions::Apply`、bots、master登録、shutdown→`server.Run`を同順で保持。
4. `-simulate/-authority`は現在「受理して無視」。**ホストAuthority復活や役割変更を禁止**。
5. `GetCommandLineArguments()`はOS差がある。`-server`の手入力コマンドを`args`から復元する変更は別の互換性調査まで禁止。
6. `-hostlocal`, `-hostgame`, `-connect`, `-netcheck`は`TryHandle`側の別責務であり、`-server`起動と一体化しない。最後にグループを統合してもよいが、インターフェイスの共有だけに留める。

**完了ゲート**：専用serverとclientは別プロセス・server Authority不変。起動停止、SIGINT/終了シグナル、ルーム作成、Bot、server名、rotation、master登録と一覧表示の実UDPテストPASS。検証環境がない場合、この段階は**保留**とする。

### ME-07 — `ModEntry.cpp`を薄くする最終レビュー【全ゲート通過後】

- `TryHandleHeadless`と`TryHandle`の公開APIと順序を維持したまま、**既存の制御フローが読める程度に**各責務を委譲していることをレビュー。
- `#include`は実際の利用先へ移し、コンパイル時間と対象バイナリの差を測定。リンク結果が未確認なら「通常ゲームのバイナリから診断機能を完全除去できた」と書かない。
- 行数の削減を目標値にしない。小さな処理（parse数行等）は入口に残してもよい。機械的な分割よりKISS優先。
- 不要になった`#include`を削除しても、`#if`・`noinline`・privateな到達性・テストビルドを崩さない。

## 6. 回帰テスト仕様（担当者が実装前に固定するもの）

### 6.1 引数探索の純粋テスト（自動化必須）

| ID | 入力例 | 期待する**現行の規則** |
|---|---|---|
| A01 | `{"-FPS"}` / `HasFlag("fps")` | true（大文字小文字無視） |
| A02 | `{"--fps"}` / `HasFlag("fps")` | true（先頭ハイフンを全除去） |
| A03 | `{"fps"}` / `HasFlag("fps")` | true（ハイフンなしも一致） |
| A04 | `{"-name","first","-name","second"}` / `ValueAfter("name")` | `first`（先勝ち） |
| A05 | 上記 / `ValuesAfter("name")` | `first`,`second`（指定順） |
| A06 | `{"-name"}` / `ValueAfter("name")` | 値なし (`nullopt`) |
| A07 | `{"-name","-server"}` / `ValueAfter("name")` | 文字列`-server`（共通関数自体は拒否しない） |
| A08 | `{"--NaMe","ABC"}` / `IndexOfFlag("name")` | `0` |
| A09 | `{"-name",""}` / `ValueAfter("name")` | 空文字の値は存在する |
| A10 | `{"-thumbnail","A","-thumbnail","B"}` / `ValuesAfter("thumbnail")` | `A`,`B` |
| A11 | `{"-fog","-fps"}` / `StartsWithHyphen(ValueAfter("fog"))` | true（呼出し側が適用を止める） |
| A12 | `{"--", "-fps"}` / `HasFlag("fps")` | true（`--`はend-of-optionsではない） |

**必須追加**：`TryParse*`を抽出する段階では「不在」「invalid」「負数」「数値の区切り」「非ASCII」「locale」「floatの丸め」「旧C#由来の`allowThousands`」のfixtureを別ファイルに追加。`-size`3系統を共通parserにまとめず個別に保全。

### 6.2 Dispatcher優先順位・初期化の契約（実行環境別）

| ID | テスト条件／起動例 | 検証点 |
|---|---|---|
| D01 | `-rhicontract <sandbox-file>`、`paths.txt`なし | `CheckSetup()`より前にファイル出力して終了。stdout/exit/hash一致 |
| D02 | `-smoketest`＋別の後段フラグ | 先のsmoketestが選択され、Setup後処理は走らない |
| D03 | `-pointercheck`＋`-frametimingcheck` | pointerが先行（設定ロード後、他の診断前） |
| D04 | `-frametimingcheck`と無関係な`-size broken` | 前段で終了するため`TryHandle`先頭の`ParseSize`警告へ進まない |
| D05 | `-credits`＋`-update` | 先のcreditsが選択される。ただしそれ以前の初期化は行われる |
| D06 | `-mapbundle`＋`-update`（ファイル用意済み、sandbox） | mapbundleが先行。`-mapdir`の設定後に走る |
| D07 | `-launcher`＋`-server`（隔離環境のみ） | launcher判定がserver起動より先。server起動しないこと |
| D08 | `-masterserver`＋`-servers`（ネットワーク隔離） | masterserverが先行（長時間動作するため自動CIではタイムアウト管理） |
| D09 | `-servers`＋`-hosts`（閉域の偽directory） | serversが先行 |
| D10 | `-nointerp`＋`-relayedpuppets`（状態fixture） | 両方処理し、最終的にSmoothing off。ログ文言/順序保持 |
| D11 | `-fpscap display -vsync off`（GPU不要な設定fixture） | 先にdisplay cap処理、その後`vsync off`で上書き |
| D12 | `-nativeaim bogus` | ProgramException経由の既存終了・ログ契約。正常扱いにしない |
| D13 | `-netlag INVALID`（隔離） | diagnosticを出して`return true`。**既存コードでここはexit=1を明示設定しない** |
| D14 | `-update -noupdate` | `-update`ブロックがUpdater.Disabled(false)を行う。**外部アクセスを伴うので純粋fixtureで検証** |
| D15 | `-classicframes DIR SCRIPT`（データあり） | `IndexOfFlag + 2`で第2引数を渡す。位置の解釈不変 |
| D16 | `-gamepad`、Programの通常呼び出し | 前段で処理するため後段へ到達しない。外部入力装置はstub可能ならstub |
| D17 | `-netcombatcheck ROOM`、`CheckSetup()`通過後に`GameFiles::Ready()`がtrue／falseになる各fixture | 先にhomingCheckを経由する。Ready=falseなら77、Ready=trueならRunのexit |
| D18 | `-movieheadlesscheck`、`CheckSetup()`通過後に`GameFiles::Ready()`がfalseになるfixture | skipのexit=77／メッセージが既存と一致 |
| D19 | `-mapgen`、`-thumbnail ROOM`（個別） | 後段開始時の`CustomRooms::StartGenerating`の有無が異なる |
| D20 | `-shotat ROOM -size 960,540` と `-maptest ROOM -size 960x540` | サイズ文法の違いを保つ。`shotat`でも前段`ParseSize`は`960,540`を警告し得るため、ログ込みで比較。実画像の検査は別途 |
| D21 | `-netcheck`＋`-demoinfo`（fixture） | netcheckが先に処理される。AI/HUD指定が保全される |
| D22 | `-thumbnail A -thumbnail B`（sandbox） | 2室を同じ指定順でCaptureRoomsへ渡す |
| D23 | 該当ModEntryコマンドなし | `TryHandleHeadless=false`、CheckSetup、`TryHandle=false`、Program通常処理へ戻る |
| D24 | `-server/-dedicated`（専用の閉域統合試験） | Update事前経路と起動停止、サーバーAuthorityの一貫性 |

**テスト実装上の注意**：`TryHandle()`は`Program::CheckSetup()`後に呼ばれる。**`paths.txt`が全くない状態では通常`CheckSetup()`が先に処理してしまい、D17/D18の77分岐には到達しない**。この2ケースはsetupを通過できるが`GameFiles::Ready()`がfalseとなるfixtureを用意するか、目的を明記した直接dispatch単体試験として構築し、通常起動の再現であるかのように扱わない。`D01`等のファイル出力は作業用一時ディレクトリに限定。ネットワークや更新を伴うD06～D24の一部は**実サービスに接続せず、fake endpointまたは隔離環境**で行う。ゲームデータ依存テストはゲームファイルの合法的なローカル所有と`paths.txt`の準備を前提とし、CIで無理に実行させない。

### 6.3 プラットフォーム／構成マトリクス

| 構成 | 必須確認 |
|---|---|
| Windows + MSVC + Qt | `GetCommandLineW`→`CommandLineToArgvW`、`ConsoleWindow`、Qt Capture、デスクトップupdate、`-launcher`とプロセス終了コード |
| Linux + Qt | `/proc/self/cmdline`、`ConsoleSetup`の作業ディレクトリ、OpenGL、`-launcher`/無引数 |
| macOS + Qt | `_NSGetArgc/_NSGetArgv`、PlatformDiagnostics開始、path、Qt画面 |
| Android | `MPHREAD_SHELL`なしの経路、Qt capture非対応返却値、activity起動。現実に適用される`#if`の確認 |
| `MPHREAD_SERVER`定義ありのビルド | 無引数`ServerUsage`、`-server/-masterserver`、`ShutdownSignals`。**定義が実際に有効なtargetを特定してから実施** |
| Headless／ゲームデータなし | `TryHandleHeadless`内で終わる診断、ディレクトリ、serverコマンドが**`CheckSetup`前**に処理されること |
| ゲームデータあり | MapGen、MapAudit、NetCombat、Launcher、基本ゲーム開始 |

**ビルド境界**：`CMakeLists.txt:51–63`で所有者の`.cpp`を`GLOB_RECURSE CONFIGURE_DEPENDS`している。`CMakeLists.txt:195–202`では`Testing/Test*.cpp`を製品静的ライブラリから除外する一方、その他の`Mods/Network/Testing`に位置する**実行可能な診断ハーネス**は製品CLIから呼ばれる。単にパスに`Testing`を含むという理由で削除・ifdef化しない。

**現状テスト**：`FruityPrime.MenuLocalization`、`FruityPrime.NetworkSmoothingTimeline`、`FruityPrime.SessionAuthorityState`などは既存CTestにあるが、それらだけでは`ModEntry`の引数の優先順位を検証できない。上記A/Dテストを**新しい独立した`TestModEntry...`として追加**し、必要ならCTestへ登録する。

## 7. 変更前後の比較・完了条件

### 毎段階の差分レビュー票

```text
チケット: ME-__
対象コミット/ZIP SHA: 66fdfd4f...
対象関数と旧行番号: ...
移した責務: ...
変更したファイル: ...
Program呼出し契約: 同じ / 不一致（要停止）
初期化・設定の順序: 同じ / 不一致（要停止）
引数解釈とコマンド優先順位: 同じ / 不一致（要停止）
stdout/stderr/exit code: 同じ / 不一致（要停止）
例外とプロセス終了: 同じ / 不一致（要停止）
Windows / Linux / macOS / Android: PASS / SKIP(理由) / FAIL
検証したテストID: ...
実行できないテストと理由: ...
依存グラフ、CMakeターゲット: 変化なし / 説明あり
ベースラインとの相違: 0件 / ...
計測（任意）: コンパイル時間・オブジェクト/リンクサイズ・起動負荷
判断: 次段階へ進む / 差分を戻す / 環境待ち
```

### 中止・差し戻しの条件

- `CheckSetup`以前に動いていたコマンドがsetup後へ移る、または逆になる。
- `-server`の役割／Match Authority、Update手順、ShutdownSignals寿命が変わる。
- `ValueAfter/ValuesAfter`の挙動や重複フラグ優先順が変わる。
- 現在値返しの`bool`が、単純化の過程で「`false`でも終了コードを設定済み」といった新状態を生む。
- `-movieheadlesscheck`やhomingのskip 77を通常の失敗/成功に変える。
- `-maptest`の実行・ムービー・音声cleanup、`-netcheck`通信、`-hostlocal`の子server停止が変わる。
- DebugLog・Qt/RHI・Audio・InputSettingsの初期化順が変わる。
- テストが赤でも理由が証明されていないのに次のモジュールを変更する。

### 最終合格ライン

1. **ME-00～07を一括変更せず**、各段階の差分とベースライン比較が残っている。
2. 公開API・Programのhook 2箇所・オプション検索規則・全主コマンドの優先順位が一致。
3. 実行環境が整う範囲の自動テストと手動シナリオを完走し、未実行環境と残リスクを明示。
4. OS/Buildの`#if`がコンパイル・リンク可能。Windows/MSVCを含む対象バイナリで検証。
5. CPU時間・メモリ・リンクサイズの改善を主張する場合は**同条件のBefore/After計測**を添える。未計測なら改善量を主張しない。
6. ModEntryはdispatcherとして読め、既存のビジネスロジックを再実装していない。不要な分割は追加しない。
7. **C++変更以外の領域へ作業を広げない。コミット・プッシュをしない。**

## 8. 監査で判明した特有のリスクと未検証事項

| 優先度 | リスク | 補足・担当者への確認指示 |
|---|---|---|
| **P0** | `TryHandleHeadless`/`TryHandle`の境界移動 | `Program::Run`でSetup前後が違う。分割先ごとに呼出し段階を固定 |
| **P0** | 設定/更新の順序変更 | `ApplyRenderOverrides`2回、WindowMode2回、`DesktopUpdate::AtStartup`など、見た目の重複に意味がある可能性を固定 |
| **P0** | サーバー起動・終了の仕様逸脱 | `ServerUpdate`、`ShutdownSignals`、`std::exit(1)`、`StartServerAuthority`の契約を同時変更しない |
| **P1** | `-netcombatcheck`の実質重複 | 上側が必ず先に処理するので後側の到達性は低い。削除は別チケットと専用fixture |
| **P1** | `-gamepad`の重複 | Program通常経路で後側へ進まない。公開API直接呼出しの可能性は未排除 |
| **P1** | 文化依存の数値と`-size`3仕様 | 誤った共通化でCLIの値が変わる。個別テストを用意 |
| **P1** | `GetCommandLineArguments`/desktop updater | 実際のOSコマンドラインを再取得。自動updateを不用意に実行しない |
| **P1** | Headless診断の終了コード0/1/77 | 処理済み=成功ではない。`true`とexit codeを別々に検証 |
| **P1** | ビルドの`MPHREAD_SERVER`条件 | `ModEntry.cpp`にあるだけではその構成が存在するとは限らない。compile_commands等で調査 |
| **P2** | ファイル分割後の起動性能 | 入口のコードは概ねコールドパス。分割だけでFPSが上がると期待しない。測定後に評価 |

**今回の作成時に実施していないこと**：`ModEntry.cpp`の改変、CMakeターゲット追加、専用のCLI fixture実装、MSVC/Android実ビルド、ゲームファイル込み統合テスト、GitHubへのコミット/プッシュ。これらは担当者が上記ステップで実施・証拠化する対象である。

## 9. 固定ソース／関連資料

- [`ModEntry.cpp`（本監査の固定コミット）](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/ModEntry.cpp)
- [`ModEntry.hpp`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/ModEntry.hpp)
- [`Program.cpp:615–638`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Program.cpp#L615-L638)
- [`CMakeLists.txt:51–63,195–202`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/CMakeLists.txt)
- [`Software Design Philosophy and Best Practices`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/.claude/design/Software_Design_Principles_and_Best_Practices.md)
- [`C# ModEntry.cs`（仕様の比較参照）](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead/Mods/ModEntry.cs)
- 母体監査：`FruityPrime_CPP_Mods_Complete_Refactoring_Plan_2026-10-15.md`。本書は**ModEntry単独で読めるよう必要事項を取り込み**、ほかのModsリファクタリングを着手条件にしない。

**ソース完全性の確認**：提供ZIPはリポジトリのGit Tree SHA `2ee29a13cba4a76c8d32e235a21cdd76158ebd07`と一致。`ModEntry.cpp`のSHA-256：`985e014be846548a382499674042f4a81c47bba39638048c81bc2bfc0f238809`。この基準に対する行番号であり、改修後は行番号が移動するため**関数名・責務・テストIDを優先して追跡**する。

## 10. CLIフラグのソース位置索引（担当者用・字句抽出台帳）

次の索引は、ZIPの`ModEntry.cpp`で`HasFlag`／`ValueAfter`／`ValuesAfter`／`IndexOfFlag`等に直接渡された**文字列リテラル**を抽出したもの。ヘルパーへの動的な引数、定数(`DesktopUpdate::ApplyFlag`)、オプション配列(`netjitter`等)、`ApplyRenderOverrides`内のフラグ等は抽出条件が異なるため、**これだけで全フラグを保証しない**。機能名と二次オプションが混在する。変更時には必ず周辺本文と`#if`を確認する。

**索引で保証するのは出現位置のみ**。有効なコマンド、コマンド処理済み、reachable、テスト済みを意味しない。実装者は移動後にこの文字列集合・相対順序を再比較する。

### 10.1 `TryHandleHeadless`（setup前）

| フラグ文字列 | 出現行 | 参照方式 |
|---|---|---|
| `exclusivefullscreen` | 1074, 1552 | `HasFlag` |
| `fullscreen` | 1076, 1556 | `HasFlag` |
| `borderless` | 1076, 1556 | `HasFlag` |
| `windowed` | 1078, 1560 | `HasFlag` |
| `rhicontract` | 1080 | `ValueAfter` |
| `vulkancheck` | 1097 | `HasFlag` |
| `backdropparity` | 1102 | `ValueAfter` |
| `backdropobserve` | 1105 | `HasFlag` |
| `presentconformance` | 1108 | `HasFlag` |
| `rhiconformance` | 1113 | `HasFlag` |
| `reflexcheck` | 1118 | `HasFlag` |
| `vulkanpresentcheck` | 1123 | `HasFlag` |
| `vulkanresourcecheck` | 1128 | `HasFlag` |
| `vulkanpresentfallbackcheck` | 1133 | `HasFlag` |
| `glfwpathcheck` | 1139 | `HasFlag` |
| `thumbnailwindowcheck` | 1144 | `HasFlag` |
| `legacyglcheck` | 1146 | `HasFlag` |
| `windowcheck` | 1149 | `HasFlag` |
| `smoketest` | 1155 | `HasFlag` |
| `nativeaim` | 1162 | `ValueAfter` |
| `debuglog` | 1174 | `HasFlag` |
| `noupdate` | 1184 | `HasFlag` |
| `pointercheck` | 1187 | `HasFlag` |
| `gamepadassistdebug` | 1193 | `HasFlag` |
| `gamepadassistbaseline` | 1194 | `HasFlag` |
| `gamepadassisttelemetry` | 1195 | `ValueAfter` |
| `gamepadcheck` | 1197 | `HasFlag` |
| `shots` | 1199 | `ValueAfter` |
| `gamepad` | 1203 | `HasFlag` |
| `seconds` | 1206 | `ValueAfter` |
| `verbose` | 1212 | `HasFlag` |
| `frametimingcheck` | 1216 | `HasFlag` |
| `spireposecheck` | 1249 | `HasFlag` |
| `dialanchecheck` | 1249 | `HasFlag` |
| `dialanchepeercheck` | 1250 | `HasFlag` |
| `formcheck` | 1250 | `HasFlag` |
| `netlag` | 1256 | `ValueAfter` |
| `netspike` | 1263 | `ValueAfter` |
| `netloss` | 1269 | `ValueAfter` |
| `homingtrace` | 1296 | `HasFlag` |
| `nounlagged` | 1301 | `HasFlag` |
| `snapshotpuppets` | 1306 | `HasFlag` |
| `clientpin` | 1311 | `HasFlag` |
| `pressage` | 1316 | `HasFlag` |
| `hitrig` | 1321 | `ValueAfter` |
| `rigmorph` | 1333 | `HasFlag` |
| `rigspeed` | 1338 | `ValueAfter` |
| `rigrange` | 1344 | `ValueAfter` |
| `rigstrafe` | 1350 | `ValueAfter` |
| `maxrewind` | 1356 | `ValueAfter` |
| `nohitprediction` | 1372 | `HasFlag` |
| `nohitmarker` | 1377 | `HasFlag` |
| `hitlog` | 1382 | `ValueAfter` |
| `servershots` | 1386 | `HasFlag` |
| `noobservedimpacts` | 1391 | `HasFlag` |
| `noconfirmedimpacts` | 1396 | `HasFlag` |
| `noretarget` | 1401 | `HasFlag` |
| `nodeathprediction` | 1406 | `HasFlag` |
| `noclaims` | 1411 | `HasFlag` |
| `nointerp` | 1416 | `HasFlag` |
| `relayedpuppets` | 1421 | `HasFlag` |
| `credits` | 1427 | `HasFlag` |
| `mapdir` | 1433 | `ValueAfter` |
| `goldencapture` | 1441 | `ValueAfter` |
| `goldendir` | 1444 | `ValueAfter` |
| `mapbundle` | 1452, 1454 | `HasFlag, ValueAfter` |
| `out` | 1455 | `ValueAfter` |
| `update` | 1495 | `HasFlag` |
| `uishot` | 1510 | `ValueAfter` |
| `uibench` | 1517 | `HasFlag` |
| `uidesign` | 1523 | `ValueAfter` |
| `classicframes` | 1530, 1533 | `IndexOfFlag, ValueAfter` |
| `shellshot` | 1539 | `ValueAfter` |
| `tapcheck` | 1546 | `HasFlag` |
| `launcher` | 1572 | `HasFlag` |
| `menu` | 1572 | `HasFlag` |
| `text` | 1575 | `HasFlag` |
| `masterserver` | 1595, 1611 | `HasFlag` |
| `server` | 1595, 1743 | `HasFlag` |
| `dedicated` | 1596, 1743 | `HasFlag` |
| `noautoupdate` | 1598 | `HasFlag` |
| `port` | 1614, 1749 | `ValueAfter` |
| `masterport` | 1617, 1680, 1840 | `ValueAfter` |
| `hostports` | 1627, 1811 | `ValueAfter` |
| `public` | 1656 | `ValueAfter` |
| `publicaddress` | 1659 | `ValueAfter` |
| `servers` | 1676 | `HasFlag` |
| `master` | 1678, 1686, 1837 | `ValueAfter` |
| `hosts` | 1684 | `HasFlag` |
| `players` | 1756 | `ValueAfter` |
| `rotation` | 1763 | `ValueAfter` |
| `servername` | 1775 | `ValueAfter` |
| `name` | 1779 | `ValueAfter` |
| `friendlyfire` | 1788 | `HasFlag` |
| `noshadowfreeze` | 1789 | `HasFlag` |
| `affinityweapons` | 1790 | `HasFlag` |
| `instagib` | 1791 | `HasFlag` |
| `bots` | 1793 | `ValueAfter` |
| `botlevel` | 1797 | `ValueAfter` |
| `novote` | 1806 | `HasFlag` |
| `nomapupload` | 1807 | `HasFlag` |
| `simulate` | 1829 | `HasFlag` |
| `authority` | 1829 | `HasFlag` |
| `nomaster` | 1835 | `HasFlag` |
| `unlisted` | 1835 | `HasFlag` |

### 10.2 `TryHandle`（setup後）

| フラグ文字列 | 出現行 | 参照方式 |
|---|---|---|
| `mapgen` | 1879, 1987, 1989 | `HasFlag, ValueAfter` |
| `instagibdemo` | 1887 | `HasFlag` |
| `instagib` | 1892 | `HasFlag` |
| `nohelmet` | 1897 | `HasFlag` |
| `netdebug` | 1903 | `HasFlag` |
| `mechanics` | 1909 | `HasFlag` |
| `gamepad` | 1915 | `HasFlag` |
| `seconds` | 1918, 1972, 2180, 2337, 2396, 2514, 2606 | `ValueAfter` |
| `verbose` | 1924 | `HasFlag` |
| `resourceaudit` | 1928 | `HasFlag` |
| `healthsimtest` | 1934 | `ValueAfter` |
| `powerupsimtest` | 1941 | `ValueAfter` |
| `netlobbytest` | 1948 | `HasFlag` |
| `rooms` | 1954 | `HasFlag` |
| `dpstest` | 1963 | `ValueAfter` |
| `hunter` | 1967, 2404 | `ValueAfter` |
| `weapon` | 1969 | `ValueAfter` |
| `distance` | 1978 | `ValueAfter` |
| `bombs` | 1983 | `HasFlag` |
| `force` | 1990 | `HasFlag` |
| `q3maps` | 2024 | `ValueAfter` |
| `q3shaders` | 2043 | `ValueAfter` |
| `map` | 2046, 2208 | `ValueAfter` |
| `mapcheck` | 2050 | `ValueAfter` |
| `altprobe` | 2057 | `ValueAfter` |
| `at` | 2061 | `ValueAfter` |
| `delay` | 2074 | `ValueAfter` |
| `portalviews` | 2088, 2090 | `HasFlag, ValueAfter` |
| `shotat` | 2119 | `ValueAfter` |
| `out` | 2130 | `ValueAfter` |
| `size` | 2140, 2419 | `ValueAfter` |
| `mphtexdump` | 2153 | `ValueAfter` |
| `mapsync` | 2160 | `HasFlag` |
| `bottour` | 2172 | `ValueAfter` |
| `players` | 2175, 2329, 2388 | `ValueAfter` |
| `moverprobe` | 2185 | `ValueAfter` |
| `padprobe` | 2191 | `ValueAfter` |
| `alt` | 2193 | `HasFlag` |
| `mapitems` | 2197 | `ValueAfter` |
| `scale` | 2202 | `ValueAfter` |
| `mapmaterials` | 2212 | `ValueAfter` |
| `lockjawenemycheck` | 2219 | `ValueAfter` |
| `aimcheck` | 2220 | `ValueAfter` |
| `weavelaltcheck` | 2225 | `ValueAfter` |
| `movieheadlesscheck` | 2231 | `HasFlag` |
| `homingspawncheck` | 2277 | `ValueAfter` |
| `netcombatcheck` | 2292 | `ValueAfter` |
| `dialanchecheck` | 2298 | `ValueAfter` |
| `dialanchepeercheck` | 2299 | `ValueAfter` |
| `port` | 2303 | `ValueAfter` |
| `dialanchepeera` | 2311 | `ValueAfter` |
| `dialanchepeerb` | 2311 | `ValueAfter` |
| `spireposecheck` | 2318 | `ValueAfter` |
| `simcheck` | 2325 | `ValueAfter` |
| `mode` | 2342, 2401, 2495, 2556 | `ValueAfter` |
| `formcheck` | 2343 | `HasFlag` |
| `gpulifetime` | 2347, 2350 | `HasFlag, ValueAfter` |
| `cycles` | 2358 | `ValueAfter` |
| `frames` | 2363 | `ValueAfter` |
| `mapteststorymoviecycle` | 2372 | `HasFlag` |
| `mapteststorymovietransition` | 2378 | `HasFlag` |
| `maptest` | 2384 | `ValueAfter` |
| `hudshots` | 2402, 2626 | `HasFlag` |
| `teamprobe` | 2403 | `HasFlag` |
| `drawrate` | 2412 | `ValueAfter` |
| `maptestmoviechain` | 2442 | `HasFlag` |
| `maptestmovieskip` | 2443 | `HasFlag` |
| `maptestmovieclose` | 2444 | `HasFlag` |
| `maptestmoviealcall` | 2445 | `HasFlag` |
| `maptestcleanupexception` | 2446 | `HasFlag` |
| `maptestpartialinit` | 2447 | `HasFlag` |
| `maptestmovies` | 2448 | `HasFlag` |
| `bots` | 2451 | `HasFlag` |
| `shots` | 2451, 2603 | `ValueAfter` |
| `renderprobe` | 2452 | `HasFlag` |
| `allnodes` | 2452 | `HasFlag` |
| `itemshots` | 2453 | `HasFlag` |
| `itempickupcheck` | 2453 | `HasFlag` |
| `installserver` | 2459 | `HasFlag` |
| `hostlocal` | 2491 | `ValueAfter` |
| `servername` | 2518 | `ValueAfter` |
| `master` | 2519, 2546 | `ValueAfter` |
| `nomaster` | 2523 | `HasFlag` |
| `hostgame` | 2543 | `ValueAfter` |
| `masterport` | 2549 | `ValueAfter` |
| `maprotation` | 2559 | `ValueAfter` |
| `connect` | 2592 | `ValueAfter` |
| `netcheck` | 2600 | `ValueAfter` |
| `spectate` | 2612, 2616 | `HasFlag, ValueAfter` |
| `rejoin` | 2622 | `ValueAfter` |
| `ai` | 2628, 2631 | `HasFlag, ValueAfter` |
| `headless` | 2637 | `HasFlag` |
| `mapvote` | 2638 | `ValueAfter` |
| `recolor` | 2644 | `ValueAfter` |
| `recorddemo` | 2649 | `HasFlag` |
| `demoinfo` | 2653 | `ValueAfter` |
| `replay` | 2656 | `HasFlag` |
| `thumbnails` | 2669 | `HasFlag` |

**直接抽出の対象外として別途確認するもの**：`ApplyRenderOverrides(:636–809)`の`rhi`,`vkvalidation`,`cel`,`performance`,`fog`,`fov`,`fps`,`fpscap`,`fpsmeasure`,`gpuprofile`,`vsync`,`celbands`,`celedge`,`prohud`,`weaponstyle`,`crosshair`,`crosshairsize`,`radar`,`radarbackground`,`radaroutlines`。さらに`DesktopUpdate::ApplyFlag`、`netjitter/netseed/netreorder/netduplicate`（配列）、`homingcontinuouscheck/homingdisplaycheck/homingparitycheck/hostauthoritycheck`（ラムダ引数）、`homingspawncheck`等は本文で追跡する。

**索引整合性**：先行する全量Mods監査の`mods_cli_flag_index.tsv`を元に生成。setup前120出現・105種、setup後121出現・98種の直接抽出をこの表に反映した（同名フラグが両側にあるため、合計203種の*非重複*フラグという意味ではない）。
