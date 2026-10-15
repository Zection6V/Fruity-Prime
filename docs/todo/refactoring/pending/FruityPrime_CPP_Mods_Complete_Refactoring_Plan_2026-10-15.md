# Fruity Prime C++ `Mods/` リファクタリング計画・ソース全量照合／品質監査版

> 調査基準日：2026-10-15（`main` 固定SHAで整合性と実装順序を追加監査）  
> 対象リポジトリ：[`Zection6V/Fruity-Prime`](https://github.com/Zection6V/Fruity-Prime)  
> 基準ブランチ：`main` / 基準コミット：[`66fdfd4f23d0e84837c1b16b5a91b8b48113708b`](https://github.com/Zection6V/Fruity-Prime/tree/66fdfd4f23d0e84837c1b16b5a91b8b48113708b)  
> 改修主対象：`src/MphRead.Native/Mods/` **（C++20）**。依存元の`src/MphRead.Native.Qt/Shell/ClassicHost.*`や`Renderer.cpp`、CMake、テストは**読取監査の範囲**に含む。将来の修正は必要に応じてネイティブ薄い連携部分のみ個別審査する。  
> 作業内容：**調査と計画書作成のみ。ソースコード変更、コミット、プッシュは行わない。**  
> 注記：過去に指定されていた `develop3_rendering` は調査時点で参照できなかった。現行 `main` は `develop13` より4コミット進んでいたため、現在のC++コードの設計計画は `main` を基準にする。実施開始時には必ず対象コミットを再固定する。

> **ZIP全量照合済み**：受領した`Fruity-Prime-main(1).zip`からGitのblob/treeを再計算し、GitHub `main`のTree SHA `2ee29a13cba4a76c8d32e235a21cdd76158ebd07`と完全一致した。対象947ファイルの内容・パスに関する全量ローカル解析と独立テストを実施した。**ASTによる全意味解析、完全ビルド、UDP/実機、性能実測は依然未完了**。C++ソースやGitHubは変更していない。以下の各章では旧調査の時点を明示し、今回の実行証拠を最優先する。
> **文書形式**：主題別統合版。過去の第1～9パスに記録された情報を、時系列の追補ではなく対象・作業単位で再配置した。各調査での未測定・未実行の事項は未完了のまま保持する。
> **編集原則**：旧版1,450行の内容をテーマ別に移した。既存の詳細な表、実装手順、行番号、検証条件、保留事項は削除しない。重複した説明は「全体方針」と「個別チケットの具体的根拠」で役割を分ける。

## 0. ZIP全量監査の実行結果と優先対応（担当者はここから開始）

### 0.1 入力の真正性・適用範囲

- **GitHub `main`のコミット**：`66fdfd4f23d0e84837c1b16b5a91b8b48113708b`、Tree SHA：`2ee29a13cba4a76c8d32e235a21cdd76158ebd07`。
- **受領ZIP SHA-256**：`d4ae10724449a74435db1afe6b74318d499e82ee87f7d19d7c71dc4fcd2a28b8`。Gitオブジェクト形式（ファイルの実バイト列・実行権限・サブツリー）から復元した**ルートTree SHAが完全一致**。`Mods`サブツリーSHA：`43a8f77b40c3c9434ab28d110ee9021814bb9b08`。よって今回の調査はGitHubの正しい`main`と同一内容を対象とする。
- `Mods/`の **947ファイル**（439 `.cpp`、502 `.hpp`、その他6）。全ファイルのパス・サイズ・行数・SHA-256を`mods_file_inventory.tsv`へ収録。ファイル本文をローカルで全量取得済み。
- ソースのクォート付き`#include`を走査し、**4,219件の実在パスへ解決した直接依存**を`mods_dependency_edges.tsv`へ収録。未解決4件はAOT/CMake生成ヘッダー（ローカライズ、K8x12L、RHI検査、GoldenCapture provenance）。これはコンパイル失敗4件の意味ではない。Mods内の直接includeグラフで多ノードSCCは**0件**（条件付きプリプロセッサ・Mods外を介す循環までは判定できない）。
- **字句解析の関数候補4,699件（修飾名3,801件）**を`mods_function_index.tsv`へ収録。**AST・コンパイラ確定の全関数一覧ではなく、偽陽性・未検出がある調査用索引**。責務分類は`category_hint`でありSRP違反の確定ではない。
- **可変static候補1,097件**（ヘッダー宣言とCPP定義を重複計上する場合を含む）、**重複候補26行**、**低参照候補75件**を抽出。いずれも字句解析のスクリーニング結果であり、**宣言数＝独立owner数／重複＝共通化可能／低参照＝削除可能、ではない**。レビュー済みの重要ownerと重複群は別表を参照。削除許可件数は**0**。

### 0.2 実行した検査と、実行していない検査

| 領域 | 結果 | 確実に言えること／残る制約 |
|---|---|---|
| Git source provenance | **PASS** | ZIPとGitHub `main`の完全なGit Treeハッシュ一致 |
| 静的構造 | **実施** | 947ファイル、4,219直接include、関数・static・重複の字句索引。AST意味解析と全リンク到達性は**未完** |
| AOTローカライズ | **PASS** | en/ja/frを生成、全7,170グリフをビルドホスト生成し、独立C++ `TestMenuLocalization`実行PASS |
| Claimダメージ／基礎ネット規則 | **PASS 8件** | `ClaimDamagePolicy`と7件の独立コンパイル試験。フルCMakeのCTest結果ではない |
| レンダリング静的ゲート | **PASS 2件** | Phase 4旧GL API（1,571ソース）、Phase 5シェーダーAPI（20 shader sources） |
| Protocol 30/31/32 Golden preserved | **FAIL 3件** | 追跡中のLFファイルと検査器のCRLF前提の期待SHA-256の不一致。後述P0-NEW-01で原因確定 |
| Linux全体ビルド/CTest | **環境ブロック** | CMake構成がOpenGLヘッダー／ライブラリ不足で停止。C++ビルド失敗やCTest PASSとは判定しない |
| Native audit補助スキャナ | **一部実行** | `helper_copies`走査中にタイムアウト。全スキャナPASS扱いはしない |
| UDP/実機/性能/MSVC/macOS/Android | **未実施** | 完全監査の残りの必須ゲート。FPSやCPU/heap削減量を断定しない |

**証拠成果物**：別添`FruityPrime_CPP_Mods_Audit_Evidence_2026-10-15.zip`に`audit_provenance.json`、7つの正本`mods_*`、補助索引、実試験結果、ログ、再実行可能な解析スクリプトを収録。`mods_hotpath_profile.tsv`は**未測定項目を空欄にした計測候補表**であり、ベンチマーク完了の証拠ではない。

### 0.3 確定した重要修正指示（優先順）

#### P0-NEW-01：Protocol 30/31/32 Goldenハッシュの改行コード不整合（既存の赤テスト）

**事実**：`CMakeLists.txt:399–407`は`FruityPrime.NetworkProtocol30GoldenPreserved`／31／32をCTestに登録し、`tools/testing/check-network-protocol-golden.py`が生バイト列をSHA-256比較する。現在の`tools/testing/golden/network-protocol-v30.hex`／31／32はLFだが、スクリプトに埋め込まれた期待ハッシュは**3つとも同じデータをCRLFへ変換した結果と完全一致**する。3件とも実行時exit 1。パケットフィールド変更を示す証拠はない。

| Protocol | Git上のLFファイル SHA-256（先頭16桁） | 現行チェッカーが期待するCRLF SHA-256（先頭16桁） | LF→CRLF変換後 |
|---|---|---|---|
| 30 | `1851831c568ee8f5` | `3846367e22fce66b` | 期待値と完全一致 |
| 31 | `0a111ebe03d137a0` | `d63eaa24cb5b3509` | 期待値と完全一致 |
| 32 | `4abd4d135e1dfa7c` | `2bc7acc78a45edca` | 期待値と完全一致 |

**修正方針**：本件はリファクタリングとは**独立の既存テスト修正**。第一候補は、Gitのblobに保存されたLFを正本として期待ハッシュを再固定し、必要なら`.gitattributes`で対象GoldenのLFを明示すること。別案は、LFとCRLFを同一視する**限定的な改行正規化**の後にSHAを検証すること。いずれも**改行以外のデータ変更を無条件に承認しない**。期待ハッシュを更新するときは正本SHAと差分・レビュー記録を添付し、必ずProtocol 30/31/32の全3件を再実行する。Goldenの値と同時にwire serializerを変更することは禁止。

**CI追加ゲート**：`.github/workflows/build_cpp.yml:261,445,773`で確認した`ctest -R`の選択セットは上記3件を含まない。少なくとも一つのネイティブCIジョブで3件を明示的に実行し、失敗を検出できることを確認する。`NetworkProtocol33Golden`と現行ネットコード試験も維持する。**すでにCI全体が失敗しているとは断定しない**（確認した選択セットでは対象外）。

#### P1-NEW-02：ClassicMenuの一覧変換コストとJoin先の一致性を別チケットにする

- `ClassicMenu/ClassicMenu.cpp:28`の`TickSeconds = 2*560190/33513982`、`:370–375`の`Tick()`→`Composer::Refresh()`で、更新は**DS約29.91Hzのメニューティック**。PCの高FPS描画が`Servers()`を直接その頻度で走査するという説明は誤り。
- `Compose.cpp:69–70`：`ServerRows=9`、`SettingsRows=7`。サーバー一覧の5つの文字列コールバック＋Pingカラー1つが各行で`Servers()`を取得し、要約行も取得する。**画面の1回のRefreshで少なくとも9×6+1=55個の取得経路**がある（実際の時間・割当数は未計測）。`ClassicHost::Servers()`は`PlayModel`の全Qt行を`ServerRow`へ変換する。画面表示1ティックで1回の不変一覧取得を検討する。
- **機能正しさは別**：`Compose.cpp:502`は選択時の行indexを保持し、Hunter画面を経た`:1102`で`Host::Join`を呼ぶ。`ClassicHost.cpp:188–199`は**Join時点の最新**`PlayModel::Servers()`から同じindexでendpointを選ぶ。途中に並び替え／削除がある場合は別サーバーへ接続し得る設計。endpointに紐付けた選択token／世代／クリック時のスナップショットを使う案を別の**M-01B**とし、M-01のコピー削減と混ぜない。表示・接続の回帰試験は並び替え、削除、Hunter選択中の更新を含める。
- `ClassicHost::Settings()`は毎回`SectionRows`と動的`row.Live()`値を組み立てる。値変更直後の反映契約を固定してから、1更新内のみキャッシュ。**プラットフォームQt層を含む改修は別途範囲審査**。

#### P1-NEW-03：継続すべき責務分割と、静的状態のレビュー

- `NetSession::_context`はstaticでも、既に`SessionLifecycle::Ports`や`SessionRuntimePump`へ責務分割済み。**無条件の非static化は禁止**。現行Dedicated Server／ローカルクライアントの権限境界、Stop→Startの状態分離が先。
- `NetSmoothing`は25個前後のstatic定義を持つ固定長リングと補間時計。`AckPoint`は`NetUnlagged::LaunchFrameFor`・Intent・HitClaimの時刻に関連。**描画専用クラスとして扱って時計を移動するのは禁止**。ルーム変更・トラック物・Morphを含めシミュレーション順序をGolden化する。
- `NetHitClaims`はOutbox/Inbox/ReplayLedger/RateLimiter/DeathStateなどを**すでに別型へ分割済み**。最大申告ガードと確定ダメージPolicyを混同しない。静的owner一覧は実装済みの部品寿命を検証するための証拠で、全面再設計の指示ではない。
- `GamepadManager::ClearAll()`は再帰mutex内部から`ClearDevice()`を呼ぶので非再帰化は個別変更。`DebugLog::GetState()`の意図的な生存期間設計は保持。

#### P1-NEW-04：MapGen/ModEntryの大型関数は分割候補であり速度問題の証拠ではない

| コード | 字句索引で確認した単一関数長（概数） | 担当者の先行作業 |
|---|---:|---|
| `MapGen/Q3Import.cpp:224 Q3Import::Build` | 851行 | パース／変換／RHI資産構築の段階と失敗後rollbackを列挙 |
| `ModEntry.cpp:1069 ModEntry::TryHandleHeadless` | 800行 | CLIフラグ・終了コード・Headless優先順位をfixture化 |
| `ModEntry.cpp:1871 ModEntry::TryHandle` | 805行 | GUI／診断／ネットコマンドの到達性を分離（現行優先順位維持） |
| `MapGen/MapTextureBake.cpp:1373 MapTextureBake::Bake` | 676行 | texture-bakeの出力Golden、メモリピーク測定を先行 |
| `Network/Testing/Harness/NetFeatureCheck.cpp:571 NetFeatureCheck::Observe` | 611行 | テスト観測と本番ロジックの依存を別々に分類 |

`mods_duplicate_review.tsv`ではWin32エラーメッセージ処理（`ThumbnailBatch`対`ServerUpdate`）やMapGen probeの初期化定型処理を共通化候補としてレビューした。ただし`DesktopUpdate`と`UpdateDownload`の`AssignLastError`は同文でも**異なる状態owner**へ書いており、DRYを理由に状態を一体化しない。`mods_dead_code_evidence.tsv`の75候補は**全件未判定、削除不可**。

### 0.4 監査の正確な到達判定

**完了**：ZIPの全Git Treeバイト一致検証、全947ファイルのインベントリ、全ファイル本文の字句走査、クォート付き直接includeの存在解決、主な関数/静的状態候補抽出、主要ownerの実装本文レビュー、独立単体テスト9件と静的ゲート2件のPASS確認、Golden赤テスト3件の改行根本原因特定。

**未完了**：Clang AST／compile_commandsからの厳密な全シンボル呼び出し・推移依存、全staticの実owner/スレッド/reader-writer台帳、全重複の意味同値性、製品リンクマップと到達性、実際のCPU/alloc/lock・fps測定、フルLinux CMake/CTest、MSVC/macOS/Android、実UDP障害マトリクス、GPU GoldenCapture。これらが揃うまでは**「完全監査完遂」「完全に安全」「パフォーマンス改善を保証」と表現しない**。

---

## 最新 `main` との整合性監査（旧監査からの変更履歴）

**監査基準**：`main` HEAD = `66fdfd4f23d0e84837c1b16b5a91b8b48113708b`（再照合済み）。旧版の基準コミットと同一であり、**ソース変更に起因する記載ズレはない**。一方、旧計画の未確認事項の一部はソースから確定でき、重要な設計上の記述誤りがあったため以下を修正した。

| 優先 | 監査結果と旧記述の問題 | 新しい確定事項・担当者の処置 | 根拠 |
|---|---|---|---|
| **P0** | 「NetSmoothingは完全に描画専用」「補間時計はHitClaimに影響しない」という読まれ方は誤り | `AckPoint()` は`IntentPacket.AckFrame/AckSubFrame`および`HitClaimOutbox::Entry.AckFrame`の算出に直接使われる。**表示時計の変更はwire上の時刻を変更し得る**ため、P6はClaim/AckFrameのGolden試験と同時に行う | [`NetSmoothing.cpp:580-596`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Replication/NetSmoothing.cpp#L580-L596)、[`NetPlayerBridge.cpp:154-168`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Player/NetPlayerBridge.cpp#L154-L168)、[`NetHitClaims.cpp:273-294`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Claims/Hit/NetHitClaims.cpp#L273-L294) |
| **P0** | `NetSession::StartHost()` を有効なホスト権限経路として扱う余地 | `SessionLifecycle::StartHost()` は `logic_error` を送出して旧式のclient-host simulationを拒否。`StartServerAuthority()` はサーバーロールを設定。**公開APIが残ることと到達可能性・有効性は別** | [`SessionLifecycle.cpp:41-79`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Session/SessionLifecycle.cpp#L41-L79)、[`NetSession.hpp:122-125`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Session/NetSession.hpp#L122-L125) |
| **P0** | `NetHitClaims::MaxDamageFor()`を正確な実ダメージ上限/確定規則として扱う危険 | この関数は最大武器値を取り、`raw * 5 + 1`を返す**ゆるい申告上限ガード**。最終的なClaim調停・倍率/制限は`DamageProfile()`／`SettleDamage()`と既存Policy群で処理。SSOT共通化で混同禁止 | [`NetHitClaims.cpp:636-674`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Claims/Hit/NetHitClaims.cpp#L636-L674)、[`NetHitClaims.cpp:860-874`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Claims/Hit/NetHitClaims.cpp#L860-L874) |
| **P1** | `Servers()`/`Settings()`の戻り値型を未確認としていた | `Host.hpp`で**双方とも`std::vector`値返し**と確定。`Compose::Refresh()`が表示ラムダを走査する。構造上の反復取得は確認済み、実測の割当数/時間だけが未確定 | [`Host.hpp:98-100,132`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/ClassicMenu/Host.hpp#L98-L100)、[`Compose.cpp:1420-1434`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/ClassicMenu/Compose.cpp#L1420-L1434) |
| **P1** | サーバー一覧の「安定IDで再照合」をそのまま実行可能な前提 | 現在の`ServerRow`には安定IDがなく、`Host::Join(int row, ...)`はindex指定。単純なキャッシュ導入で「表示とJoin先の不一致」を作らない。**クリック/Joinの実際の一覧世代の保護方法を先に決定** | [`Host.hpp:17-25,96-104`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/ClassicMenu/Host.hpp#L17-L25)、[`Compose.cpp:497-505`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/ClassicMenu/Compose.cpp#L497-L505) |
| **P1** | GLOBについて「Testingが製品ターゲットに含まれるか不明」としていた | `MPHREAD_NATIVE_SOURCES`は`MphRead.Native`全体の再帰収集を受け、`/Testing/Test*.cpp`を除外後、`fruity_mphread_native`静的ライブラリに渡される。**`Mods/Network/Testing`のCPPはコンパイル候補に入る**。最終exeへのリンク・死コード除去は引き続き未確定 | [`CMakeLists.txt:51-77,195-202,238-286`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/CMakeLists.txt#L195-L202) |
| **P1** | P3のセッション所有権変更をP4より先に完了させる一方、M-08ではM-04～M-07への依存を要求していた | **P3は読取監査・所有者表の固定を先行**。侵襲的なowner移行は別変更セットとし、P4/P6の保全仕様固定後に限る。M-04～M-07を待たずに実行できるP3は調査・非動作変更のみ | 本文のフェーズ表・M-08依存の内部整合性照合 |
| **P2** | `NetSmoothing`の関数一覧に追跡オブジェクトの経路が欠落 | `RecordTrackedPositions(163)`と`SampleTrackedPosition(410)`を追加し、プレイヤー・フォーム・追跡物が同じ表示時計を使う契約を試験する | [`NetSmoothing.hpp:54-64`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Replication/NetSmoothing.hpp#L54-L64)、[`TestNetSmoothing.cpp`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Testing/TestNetSmoothing.cpp) |
| **P2** | 生成済みローカライズヘッダーがGitの追跡ファイルに見える可能性 | `ClassicMenuLanguages.generated.hpp`と`MenuK8x12L.generated.hpp`は**CMakeビルドディレクトリの生成物**。入力は`Languages/en.json,ja.json,fr.json`と`Fonts/k8x12L.bdf`、生成器は`tools/generate-classicmenu-*.py`。独立CTest `FruityPrime.MenuLocalization`あり | [`CMakeLists.txt:1003-1037,1345-1357`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/CMakeLists.txt#L1003-L1037) |

**旧整合性監査の境界**：当時は重点ファイルのみ本文確認。今回のZIPでは全947ファイルを内容取得・字句解析済み。ただし全呼出し元の網羅的AST検査、実機実行、CPU/メモリ/通信の性能計測、全947ファイルの意味上の精査は未完了。実際に未実施の計測結果、PASS、速度改善を記入しない。

**品質判定（追加監査）**：構造・重点経路の調査と実装指示は強化できたが、**無条件に実装可能／調査完遂／完璧ではない**。P0の厳密な所有権/逆参照台帳、フルCTest・実UDP・プラットフォーム実行、ホットパス実測が未取得（ZIPのソース全量・字句インベントリは取得済み）のため、特にM-04～M-08の動作変更はゲート保留。M-01・M-02も現状のindexベースJoin契約を保った表示取得の削減のみを先行可能とする。

### 追加監査で確定した実行経路・是正事項（P0/P1）

| ID | ソース上の確定事実 | 計画に反映する指示 | 主要根拠 |
|---|---|---|---|
| **QA-01 P0** | `Renderer.cpp` の `Scene::OnSimulationFrame()` は固定 `1/60` 秒のシミュレーションステップで `NetHooks::AfterSimulation()` を呼び、そこで **保留Intent送信 → `NetSmoothing::Tick()` → authorityならsnapshot送信／clientならremote状態適用** の順番になる | `Tick()`を描画コールバックへ移さない。時間軸分離を行うならまずこのステップ内順序とAckの採取時点をGolden trace化する | [`Renderer.cpp:1805-1902`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Renderer.cpp#L1805-L1902)、[`NetHooks.cpp:383-426`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Integration/Hooks/NetHooks.cpp#L383-L426) |
| **QA-02 P0** | `NetUnlagged::LaunchFrameFor()` も `NetSmoothing::AckPoint()` を参照し、優先順はFiringEventのAck → remote Intent Ack → smoothing Ack → AppliedSnapshot/NetFrame fallback | M-06のテスト範囲に**射撃launch frame、rewind/lag compensation、イベント再生**を追加。IntentとHitClaimだけのGoldenでは不足 | [`NetUnlagged.cpp:348-376`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Combat/NetUnlagged.cpp#L348-L376) |
| **QA-03 P0** | `Compose.cpp:502`で選択indexを保持し、Hunter選択を経て `:1102` で `Host::Join(index,...)` を呼ぶ。`Host::Join`は現在indexベースで、`ServerRow`に安定IDはない | **表示snapshot最適化と選択先同一性の機能修正を別チケットに分割**。描画キャッシュだけでは別画面を経由したJoin先は固定されない | [`Compose.cpp:497-505,1095-1107`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/ClassicMenu/Compose.cpp#L1095-L1107)、[`Host.hpp:17-25,100-103`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/ClassicMenu/Host.hpp#L17-L25) |
| **QA-04 P1** | `GamepadManager::ClearAll()` は `Gate`保持中に `ClearDevice()` を呼び、後者は同じ再帰mutexを再取得する。`UpdateDevice()` では `Activate()` 内の `Publish()` と後段 `Publish()` が同一呼び出し中に実行され得る | 再帰mutexを単純な `std::mutex` に置換しない。ロック内helper抽出とPublish重複の評価は独立し、順序と通知意味をテスト固定してから | [`GamepadManager.cpp:110-220,287-351`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Input/GamepadManager.cpp#L287-L351) |
| **QA-05 P1** | `DebugLog::GetState()` はソースコメントで **atexit/terminate hookからアクセスできるよう意図的にcarrierをリークする** と明示 | `static State`/自動破棄への置換は禁止。crash/terminateの保証範囲はOSと実機テストに限定し、async-signal-safe性を別途確認 | [`DebugLog.cpp:349-356`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/DebugLog.cpp#L349-L356) |
| **QA-06 P1** | `CMakeLists.txt` にProtocol 30/31/32 Golden preserved、33 Golden、NetSmoothing、SessionAuthorityState、HitClaimOutbox、ClaimDamagePolicy、MenuLocalizationテストがある。メニュー言語CTestは非クロスコンパイル時だけ構築 | 既存試験を再利用し、**protocol過去バージョンのGoldenも回帰ゲート**とする。Android上でMenuLocalization CTestを実行済みとは扱わない | [`CMakeLists.txt:390-405,425-441,548-555,1345-1357`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/CMakeLists.txt#L390-L405) |
| **QA-07 P1** | 同じ `AUD-01～09` が「優先調査チケット」と「未取得証拠」で異なる内容を表す。また7つの主成果物と詳細CSVの命名が混在していた | `AUD-*`を調査担当チケット、`EVD-*`を証拠チェックに分離。7つのTSV/MDを**正本**とし、詳細CSVはそこから生成する補助ビューと定義 | 文書内の管理ID・成果物台帳の照合 |
| **QA-08 P1** | Qt実装の`ClassicHost::Servers()`は`PlayModel::Servers()`の`QVariantList`を全件走査し、`QVariantMap`から名前/マップ/モード/人数/Pingを毎回構築。`Settings()`も`SectionRows()`を再計算して表示行を生成する。`ClassicHost::Join`はOK押下時の最新`PlayModel::Servers()`からindexでendpointを取り出す | M-01/M-02のコピー削減対象を**Qt→C++表示行変換**と明示。`Settings()`の動的`row.Live()`値は同一更新内でのみキャッシュ。Join先は選択時のindexとは別世代になり得るため別試験 | [`ClassicHost.cpp:161-200,405-451`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native.Qt/Shell/ClassicHost.cpp#L161-L200)、[`PlayModel.hpp:62`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native.Qt/Shell/PlayModel.hpp#L62) |

**実施上の境界**：QA-01～08は追加で読んだソース／文書の**確定事実**。最適化のCPU改善、Joinの競合発生頻度、mutex競合、実機パリティは**未測定**。仕様上の不具合修正（Join先同一性等）と純粋リファクタリングを一括にしない。


## 担当者向けガイド（最初に読む）

この文書は**計画・調査に基づく実装指示書**であり、リファクタリングが実施・性能検証済みという報告ではない。着手する作業チケットを選んだら、同章内にまとめた「関数分類 → 実装で確認した事実 → 切り分け手順 → テストと停止条件」を順に確認する。

| 読みたい内容 | 参照先 | 読む目的 |
|---|---|---|
| 対象・基準・調査精度 | [1. 現状調査](#1-現状調査と調査根拠) | ファイル数、確定済み・未確定の区別 |
| 設計規約・守るべき動作 | [2. 設計契約](#2-設計原則非交渉条件受入ゲート) | 改変禁止、回帰ゲート、性能測定 |
| Phase 0～12 | [3. 全体ロードマップ](#3-全体ロードマップphase-012) | 依存、並列可否、着手順 |
| 各関数の具体的な指示 | [4. 主要領域別作業指示](#4-主要領域別作業指示) | 作業チケット・現状コード・移行案 |
| ベースライン・未完了監査 | [5. 調査と検証の実施手順](#5-調査と検証の実施手順) | 必須証拠・採取方法・保留事項 |
| テンプレート・完了報告 | [6. 担当者共通の運用](#6-担当者共通の運用と成果物) | 着手/レビュー/差し戻し手順 |

**優先順位の考え方**：Phase 0と回帰テスト（Phase 1）が先。比較的独立したメニュー、起動処理、入力については保護テストを整えたうえで小さく着手できる。ネットワークの権威状態・戦闘裁定・補間時刻は動作意味が強く結びつくため、基準トレースを固定するまで移動や共通化を行わない。

---

## 1. 現状調査と調査根拠
**初期調査の基準**：当初はGit Treeによる947ファイル全量のメタデータと、一部主要ファイル本文のみを確認。その後ZIPを受領し、固定コミットの全ソースをローカル解析（0章）。全AST・実測・全ターゲット実行は未完了。

### 対象の規模、重点監査領域、保護すべき既存資産
#### 規模の全体像

GitHubの `Mods/` 再帰ツリーを列挙した結果。**バイト数にはソース以外の `.bdf` / `.json` / `.inc` を含む**。ファイル数・容量は「無駄の量」ではなく、調査・移行の規模指標である。

- 合計 **947ファイル / 7,384,974 bytes（約7.04 MiB）**：`.cpp` 439、`.hpp` 502、`.bdf` 1、`.json` 3、`.inc` 2。
- `Network/`：468ファイル / 2,513,352 bytes。`Gameplay/` 147、`Server/` 95、`Session/` 90、`Testing/` 43。
- `MapGen/`：61ファイル / 1,273,975 bytes。
- `ClassicMenu/`：20ファイル / 1,172,059 bytes。ここには **845,811 bytes の `Fonts/k8x12L.bdf`** が含まれる。
- 直下：62ファイル / 638,578 bytes。`ModEntry.cpp` が最大級。
- `Input/`：118ファイル / 436,745 bytes。`Diagnostics/`：60ファイル / 435,932 bytes。
- `Render/`：66ファイル / 340,669 bytes。`Update/`：16ファイル / 217,036 bytes。

#### 優先度の高いファイル・境界

| 監査ID | 確認した構造 | 具体的な対象 | 判定 |
|---|---|---|---|
| A-01 | 起動オプション、診断コマンド、サーバー、レンダリング設定などが入口へ集中 | [`ModEntry.cpp`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/ModEntry.cpp)（約2,678行 / 105,712 B） | **構造上の複数責務を確認** |
| A-02 | `NetSession` は内部の担当クラスを分離済みだが、公開APIと `SessionContext` が静的所有 | [`NetSession.hpp`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Session/NetSession.hpp), [`NetSession.cpp`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Session/NetSession.cpp) | **状態所有権の再検討対象** |
| A-03 | ヒットClaimの宣言・再送・検証・裁定・結果反映などの大きな統合実装 | [`NetHitClaims.cpp`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Claims/Hit/NetHitClaims.cpp)（約1,810行 / 89,606 B） | **SRP/依存方向の詳細監査優先** |
| A-04 | 入力/状態同期・ゲームオブジェクト接続の大型ブリッジ | [`NetPlayerBridge.cpp`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Player/NetPlayerBridge.cpp)（約1,041行 / 46,549 B） | **責務境界候補** |
| A-05 | 64フレーム履歴・固定スロット・補間状態をもつ描画同期 | [`NetSmoothing.hpp`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Replication/NetSmoothing.hpp), [`NetSmoothing.cpp`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Replication/NetSmoothing.cpp) | **性能/表示意味の監査対象** |
| A-06 | インポート、構築、マテリアル・形状・光源変換が大規模に集中 | [`MapGen/Q3Import.cpp`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/MapGen/Q3Import.cpp)（約3,651行 / 156,627 B） | **オフライン処理の段階分離候補** |
| A-07 | マップ定義/パック処理も大きい | `MapDefinition.cpp`（約2,100行）、`MapPacker.cpp`（約1,921行） | **入出力・変換規則の分離候補** |
| A-08 | メニュー構築・ROM由来データ処理が大型実装 | `ClassicMenu/Compose.cpp`（約1,580行）、`MenuData.cpp`（約1,629行） | **ページ別責務と更新頻度の調査** |
| A-09 | ログはOS処理、例外スタック、ファイル、標準出力tee等を統括 | [`DebugLog.cpp`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/DebugLog.cpp)（約1,966行 / 69,592 B） | **コールド/ホット、クラッシュ時安全性の分離** |
| A-10 | `Network/Testing` を含む `.cpp` が通常の再帰ソース収集対象 | [`CMakeLists.txt`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/CMakeLists.txt#L51-L76) | **製品と検査機能のビルド境界を要確認** |
| A-11 | 言語テーブルはビルド生成・二分探索を既に利用 | `ClassicMenu/MenuLocalization.cpp`、**ビルド生成**の`generated/classicmenu/ClassicMenuLanguages.generated.hpp` | **既存AOT方式を維持・改善** |
| A-12 | k8x12Lフォントはビルド時生成、字形アトラスは初回使用時に作成 | `ClassicMenu/MenuUnicodeFont.cpp` | **再実装対象ではなく計測・ライフサイクル監査** |

**確度の区別**：上記のファイルサイズ、静的API、CMake収集、既存AOTはソースで確認した事実。一方「性能ボトルネック」「到達不能コード」「重複している規則」「不要なメモリ」は **まだ未確定**。以降は検証仮説として扱い、計測・呼出しグラフ・パリティテストなしで削除しない。

#### 保護すべき既存資産

すでに `Network/Session` / `Network/Server` / `Network/Protocol` / `Network/Gameplay` / `Network/Domain` へ責務を分割している点、`NetHitClaims` に独立台帳・Outbox・Policy群が存在する点、`ClassicMenu` のAOT言語テーブルとフォント生成、`Diagnostics/FramePerformance.cpp` / `CpuSampling.cpp` が存在する点は活用する。**既存の責務分離を全面破棄しない。**

`CLAUDE.md` に明記される「ゲームロジックは `Mods/` に、`Entities/` / `Formats/` / `Metadata/` は薄いフックのみ」という境界と、[設計原則文書](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/.claude/design/Software_Design_Principles_and_Best_Practices.md)を優先する。C#側は既存ゲーム規則・旧UI・既存挙動を照合する**閲覧専用の互換仕様資料**とする。一方、現行C++で追加された専用サーバー権威・UDP Protocol 33・補間時計などは、**現行C++のパケット定義・Goldenテスト・サーバー実装を現行仕様の正本**として扱う。C#を参照することを理由に新C++仕様を旧式へ戻さない。C++側の最適化に際して仕様の複製を増やさない。

### Git Treeの全量取得と識別子
- 対象：`main` / `66fdfd4f23d0e84837c1b16b5a91b8b48113708b`。
- Git tree SHA：`2ee29a13cba4a76c8d32e235a21cdd76158ebd07`。
- GitHub Git Trees API `recursive=1` を取得。`truncated=false`、リポジトリ全体2,972エントリ。これは**ファイルパスとサイズについての全量走査**であり、全ファイルの本文・シンボル・実行経路の解析完了を意味しない。
- `src/MphRead.Native/Mods/` の blob は **947件**。`.cpp` **439件**、`.hpp` **502件**、その他 **6件**。
- `.hpp` に同一パス基底名の `.cpp` が存在しないもの **83件**。ヘッダーオンリー設計、インターフェース、別名実装を含むため、83件を未使用・不要と扱ってはならない。

### Mods/直下のファイル分布
| Mods直下 | 件数 |
|---|---:|
| `(root)` | 62 |
| `Chat` | 8 |
| `ClassicMenu` | 20 |
| `Combat` | 22 |
| `Diagnostics` | 60 |
| `Gameplay` | 1 |
| `Input` | 118 |
| `Launcher` | 22 |
| `Localization` | 2 |
| `MapGen` | 61 |
| `Multiplayer` | 13 |
| `Network` | 468 |
| `Platform` | 4 |
| `Render` | 66 |
| `Sound` | 4 |
| `Update` | 16 |
| **合計** | **947** |

### Network/内訳
| Network直下 | 件数 |
|---|---:|
| `Bots` | 4 |
| `Diagnostics` | 16 |
| `Discovery` | 4 |
| `Domain` | 9 |
| `Gameplay` | 147 |
| `Integration` | 8 |
| `Protocol` | 28 |
| `Replay` | 14 |
| `Server` | 95 |
| `Session` | 90 |
| `Testing` | 43 |
| `Transport` | 8 |
| `UI` | 2 |
| **合計** | **468** |

### 大型ファイルと資産サイズ
Git treeで確認したファイルサイズ（byte）。**サイズはコストや重複の証拠ではなく、レビュー分割の優先度指標**である。

| ファイル | bytes | 次の検証 |
|---|---:|---|
| `MapGen/Q3Import.cpp` | 156,627 | パース、変換、IO、リソース構築の責務境界 |
| `ModEntry.cpp` | 105,712 | モード起動、初期化、所有権、CLI境界 |
| `MapGen/MapPacker.cpp` | 98,293 | オフラインパック処理とランタイムの分離 |
| `MapGen/MapDefinition.cpp` | 89,991 | JSON読み書き、モデル、所有権の分離 |
| `Network/Gameplay/Claims/Hit/NetHitClaims.cpp` | 89,606 | Claimの認証・検証・確定・通知の分割 |
| `Network/Testing/Harness/NetFeatureCheck.cpp` | 83,460 | テスト専用リンク境界の確認 |
| `ClassicMenu/Compose.cpp` | 81,330 | UI構築・モデル読み出し・動的文字列の責務分離 |
| `MapGen/MapTextureBake.cpp` | 79,316 | テクスチャ変換のAOT化とバッファ寿命 |
| `Network/Testing/Maps/MapAudit.cpp` | 76,567 | テスト実行経路・通常ビルド混入確認 |
| `DebugLog.cpp` | 69,592 | 通常ログとクラッシュログの同期・寿命分離 |

**大きな非CPP資産**：`ClassicMenu/Fonts/k8x12L.bdf` 845,811 bytes、`MapGen/Q3TextureMatches.inc` 188,029 bytes。ソースツリー上の容量と実行時常駐メモリは別物。AOT生成物やフォント素材を単純に削除しない。

### 全ディレクトリの監査対象・担当フェーズ対応表
現存する `Mods/` のすべての直下ディレクトリと直下ファイル群をフェーズへ割り当てる。「大きくないため監査しない」という例外は設けない。ただし、**問題がないものを無理に変更しない**。

| パス | ファイル数 | 主な責務と検証対象 | 主フェーズ |
|---|---:|---|---|
| `Mods/` 直下 | 62 | entry point、ログ、設定、スクリーン、サムネイル、scene lifecycle、Window、branding | P0, P2, P3, P8, P10, P11 |
| `Chat/` | 8 | UI text、fonts、network chat、メッセージ寿命 | P5, P8 |
| `ClassicMenu/` | 20 | ROM互換UI、widget構築、AOT言語、font atlas、page更新 | P8 |
| `Combat/` | 22 | Weapon/Projectile/Homing/trace/特殊extension | P4 |
| `Diagnostics/` | 60 | Perf/GPU/状態診断/再現可能な検査 | P1, P10 |
| `Gameplay/` | 1 | Gameplay clockの時間契約 | P3, P4, P6 |
| `Input/` | 118 | keyboard/mouse/pen/touch/pad、Aim、Action、hotplug | P7 |
| `Launcher/` | 22 | GUI/Portable、file picker、game path、preferences | P2, P10 |
| `Localization/` | 2 | HUD text、言語/レイアウト境界 | P8 |
| `MapGen/` | 61 | map import/format/bake/build/pack/catalog | P9 |
| `Multiplayer/` | 13 | team/layout/match world/resource、規則の共通化 | P4, P5 |
| `Network/` | 468 | session、server、wire、transport、claims、replication、bots、testing | P1, P3, P4, P5, P6 |
| `Platform/` | 4 | OS固有path/browser/外部リンク | P10 |
| `Render/` | 66 | HUD、frame timing、shader、launcher、readback | P6, P8 |
| `Sound/` | 4 | sound mixer/AL、非同期・shutdown | P3, P10 |
| `Update/` | 16 | version/check/download/install/desktop/server | P10 |

`Network/` の内部も、次の対応で全域を確認する。

| `Network/` 下位領域 | 件数 | フェーズ | 特別な境界 |
|---|---:|---|---|
| `Bots/` | 4 | P1/P3/P4 | bot intentはローカルプレイヤー状態と混ぜない |
| `Diagnostics/` | 16 | P1/P10 | 本番off時のoverhead抑制、必要時の再現性 |
| `Discovery/` | 4 | P5/P10 | master/room discoveryとlive gameplayの境界 |
| `Domain/` | 9 | P4/P5 | immutable規則、match definition、shot identityの正本 |
| `Gameplay/` | 147 | P3/P4/P6 | authority、予測、entity同期、補間の分離 |
| `Integration/` | 8 | P2/P3 | ゲーム本体への薄いhook、net launch |
| `Protocol/` | 28 | P4/P5 | version、packet struct、codec/wire bytes |
| `Replay/` | 14 | P1/P5 | 再生と実UDPの同一の意味・タイミング |
| `Server/` | 95 | P3/P4/P5 | 独立server owner、server authoritative |
| `Session/` | 90 | P3/P5 | client session lifetime、ownership、lobby |
| `Testing/` | 43 | P1/P2 | 検査CLIと本番リンクの明確な区分 |
| `Transport/` | 8 | P5 | socket/UDP低レイヤーのみ |
| `UI/` | 2 | P8 | voteの描画/入力とauthority判定の分離 |

### 主要ソース再照合の確認範囲
対象リビジョンは `main` の `66fdfd4f23d0e84837c1b16b5a91b8b48113708b`。以下の4ファイルをGitHubのファイル取得で再確認した。これは静的コード調査であり、ローカルでのビルド・実行・プロファイルではない。

| ファイル | 確認した行数 | 追加で確かめるべき具体的論点 | 確証レベル |
|---|---:|---|---|
| `Network/Session/NetSession.hpp` | 219 | 静的APIが多数公開される。呼び出し元の依存を調べ、`SessionContext` の明示的注入へ段階移行可能か検証する。静的APIが多いことだけでは性能問題と判定しない | 静的APIの存在：確認済／移行効果：未計測 |
| `Network/Gameplay/Replication/NetSmoothing.cpp` | 656 | `NetSmoothing`の状態更新・表示時参照・履歴管理に加え、**`AckPoint`のIntent/Claim時刻への依存**を明示し、フレームレート依存の実測を取得する | ファイル規模：確認済／遅延原因：未確定 |
| `ClassicMenu/Compose.cpp` | 1,580 | `Host::Servers()` が`std::vector<ServerRow>`値返しであることは確認済み。表示コールバックからの反復取得・Refresh更新時の回数とコストを計測する | 値返し・反復取得：確認済／時間・割当数：未計測 |
| `MapGen/MapDefinition.cpp` | 2,100 | JSON変換処理の分割と `shared_ptr` 所有権の縮退可能性を、スキーマ互換・寿命解析で判定する | コード形状：確認済／所有権変更の安全性：未確定 |

### 取得方法と確証レベル
**初期の個別ソース照合の記録**：当時はGitHub接続から固定SHAのソース5ファイルを取得し、`git clone`はDNS解決に失敗。**現在はZIPの内容がGitHub Treeと完全一致することを証明し、全ファイルの読取・字句分析と一部単体テストを実施済み**。本体のAST解析・完全ビルド・実測は未実施。したがって、以下は**ソース上の確定事項と性能仮説を分離した監査**であり、全947ファイルの完了宣言ではない。

### Git Treeと実コードの照合要点
1. `ClassicMenu/Compose.cpp:479`：`row` ラムダは `_host->Servers()` を呼ぶ。486、488、490、492、494行の各表示用ラムダは個別に `row()` を呼ぶ。`Host::Servers()` は `std::vector<ServerRow>` の値返し（Host.hpp:100）と確定。したがって各呼出しは値の生成/移動・コピー等の処理を伴い得るが、ホスト側構築方法と実際の時間は未計測。`Composer::Refresh()`で1更新につき1回スナップショットを取得し参照可能か、使用期限・リスト更新・行選択の一貫性まで検証する。
2. `Network/Session/NetSession.hpp:211`：`static SessionContext _context;`。静的ファサードと内部状態の存在は確定。ゲーム起動モードの複数インスタンス共存性・再初期化・テスト分離は未実証。
3. `Network/Gameplay/Replication/NetSmoothing.cpp:48`：`std::array<std::uint32_t, NetSmoothing::HistoryFrames> NetSmoothing::_stamp{}`。118、172、451、514、533行などで `frame % HistoryFrames` を使用。固定長リングは既にコスト管理された方式であり、置換を目的化しない。
4. `DebugLog.cpp:337`：`std::recursive_mutex Lock;`、354行：`static State* state = new State();`。プロセス終了・例外経路での意図的な寿命設計の可能性があるため、単純な自動変数化・非再帰mutex化は禁止。
5. `CMakeLists.txt:52-53`：`GLOB_RECURSE`。ただし実際にどのターゲットへ入るかは関数呼出し・リンクマップの確認が必要。テストコードを一括削除しない。

---

## 2. 設計原則・非交渉条件・受入ゲート
変更してよい構造と変更してはいけない挙動を先に区別する。性能上の採否は同じ条件の実測結果で決める。

### 基本方針と設計原則
`Mods/` を一度に大量移動・全面書換えするのは避ける。最初に **動作・性能・プロトコル・描画結果の基準値** を固定し、その後、(1) 起動・テスト機能の境界、(2) 状態所有権、(3) ネットワーク/戦闘の正本、(4) 毎フレームのホットパス、(5) UI/入力/描画、(6) マップ生成や更新のコールドパス、(7) 根拠付き削除、の順に進める。

- **主目的**：コード量を減らすこと自体ではなく、余計な実行・メモリ確保・共有状態・変更の波及・ビルド負荷を減らすこと。
- **最重要制約**：ゲームの規則、通信ワイヤ形式、 authority、60Hz/30Hzの意味、描画結果、保存設定、OS間互換性を勝手に変更しない。
- **原則**：KISS / SRP / SoC / DRY / SSOT / Data-Oriented Design / Zero-Cost Abstraction / Pay for What You Use / AOT Precomputation / Shift-Left Validation / ベストプラクティス。
- **実施単位**：１フェーズにつき狭い責務の変更セット。機能修正と純粋なリファクタリングを原則別にする。
- **禁止**：測定なしで「高速化した」と判断する、参照検索だけで未使用と判断して削除する、巨大な汎用フレームワークを新設する、`include/` ディレクトリを増やす、C#に手を入れる。

#### 設計原則を実装判断に変換する

| 原則 | この計画での判断基準 | 避ける設計 |
|---|---|---|
| SRP / SoC | 変更理由が一つの型・翻訳単位に局所化される | 起動・ネット判定・UI・ログを同じ所有者が処理 |
| DRY | 同じ**知識・規則**を一つにし、呼び出し側は再利用 | 似ているだけの別仕様まで共通化する |
| SSOT | authority、shot identity、設定、規則、データ形式の正本を明示 | クライアント/サーバー/表示それぞれが独自に真偽を確定 |
| Zero-Cost Abstraction | 静的束縛、コンパイル時規則、値型、最適化可能な薄い関数 | 毎フレームの仮想呼出し・ヒープ確保・文字列変換のための抽象化 |
| Pay for What You Use | 起動モードで不要な機能は初期化・更新・計測しない | サーバーでUI/GPU依存、通常プレイで診断機能の常時処理 |
| AOT Precomputation | 不変データ・索引・フォント・規則は必要に応じてビルド時確定 | 毎回JSON解析、同じ検索表再構築、フレームごとの準備 |
| DOD | 実アクセスに合わせた連続配列・固定長容量・局所性 | 少数固定要素に過度な動的連結/ハッシュ探索 |
| Shift-Left Validation | 依存・ABI・ワイヤ・テスト・パフォーマンスを早期にゲート化 | 最終統合時にだけ結果を確認 |
| KISS | 限定的で可逆的な変更を優先 | 再設計自体が目的の過剰な層・ラッパー |

### 絶対に守る互換・実行・配置の契約
1. **C++版のみ**：作業可能領域は `src/MphRead.Native/Mods` と、必要なネイティブテスト・CMake・生成ツール・文書。C#ソースは変更しない。`Entities/Formats/Metadata` への新規変更は原則禁止。どうしても必要なら最小のフックに限定し、別レビュー・別変更とする。
2. **ファイル配置**：`.hpp` と対応 `.cpp` は同階層に置く。新しい汎用 `include/` ツリーを作らない。既存の `MphRead` 名前空間や互換経路を根拠なく改名しない。
3. **Authority**：サーバーが状態の正本。ローカルプレイヤーは入力・予測・描画を担い、ホストがプレイヤーであっても authority を譲渡しない。サーバーへの到達可能性、部屋のライフサイクル、ホスト離脱時の意味を保つ。
4. **互換性**：既存のUDPプロトコル、Packetレイアウト、サイズ、エンディアン、予約バイト、version、retransmit、AckFrame、shot identity、Replayを維持。意味変更が不可避なら別機能変更・明示的プロトコル更新・旧版互換判定が必須。
5. **ゲームプレイ**：武器射撃、チャージ、追尾、継続ダメージ、ステータス効果、モーフ/ブースト、Respawn、観戦、AI、Pickup、ロビー、マップ切替のイベント順序と演算精度を維持。
6. **時間軸**：simulation tick（ゲーム上の状態確定）と render frame（見た目）とネットワーク受信を分け、render FPSが上がってもゲーム規則/UDP送信頻度を勝手に上げない。
7. **浮動小数点**：演算順序、丸め、FMAの扱いを保つ。CMakeでの `-ffp-contract=off` 等を破らない。`-ffast-math` 等を性能目的で導入しない。
8. **プラットフォーム**：Windows/MSVC、Linux、macOS、Androidで既存機能を維持。Vulkan/GL/GL ES、Qt、ヘッドレスサーバー、音声非同期タスクの各ライフサイクルを区別する。
9. **観測可能性**：障害時のログ、診断コマンド、Headlessテスト、デモ再生、GoldenCaptureの出力/挙動を保つ。性能のため診断を削る場合も **計測時のみ有効** へ移すなど再現可能性を確保する。
10. **変更の可逆性**：各フェーズは独立してビルド可能な状態で終え、プロファイルごとの前後比較を残す。未完成な移行を次フェーズへ持ち越さない。

### 正しさ・性能・設計ゲート
#### 正しさゲート

- 本番C++の全対象構成がビルドできること。MSVC Release、Linux Release、macOS、Android（可能なCI対象）で検証。必要なゲームデータがないテストの `SKIP` は合格に数えず別記録。
- `ctest --test-dir <build-dir> -C Release --output-on-failure` がベースラインと同数以上の必要テストを実行し、既存正常テストはすべてPASSすること。
- 影響機能のC++単体テスト、統合シナリオ、Headless、ワイヤのGolden Vector、ネットワーク実UDPマトリクス（遅延・ジッタ・損失・重複・順序逆転・8プレイヤー）を追加/実施すること。
- ネットワーク変更時には server/client 間の一致、受信遅延・再送・disconnect、ホスト退室、spectator、bot、map rotation を明示して検証する。
- 描画変更時には GL / Vulkan で画像比較し、UI、透明表現、テクスチャ、アトラス、レイヤー、GoldenCaptureおよびデバイスリセットを検査する。
- バイナリ/保存データ変更時には以前のファイルからの読み込み、およびラウンドトリップの一致を証明する。
- Sanitizerを適用できるビルドで ASan / UBSan / TSan（TSanは対応環境で別ジョブ）等を実施。スレッド破棄・遅延コールバック・ハンドル再利用を重点化する。

#### 性能ゲート

「変更後に遅くなっていない」ことを測定するため、同一ハードウェア、ドライバ、バックエンド、マップ、解像度、VSync/Reflex、シミュレーション条件、計測秒数、ビルドフラグ、warmupで3～5回測定する。ウォームアップ、GPUコンパイル/キャッシュ、描画上限とserver待ちを混同しない。

| 計測領域 | 必須計測項目 | 判定の方向 |
|---|---|---|
| CPUメインスレッド | frame p50/p95/p99、処理別CPU時間、instruction/cycle、branch miss | p95/p99の悪化なし・局所的な改善 |
| simulation / combat | 1 tickあたり時間、shot/claim当たり時間、allocation/frame、ピークキュー長 | 追加割当0を原則、時間短縮 |
| UDP | packet bytes、packets/s、encode/decode µs、queue depth、loss時再送数 | 規則・wireが同じなら増加なし |
| NetSmoothing | receive-to-visible、sample age、snap/hold/interpolate比率、frame jitter | 入力反応を犠牲にせず安定表示 |
| UI / input | 1 frameあたり処理、キャッシュミス、文字atlas更新回数、初回描画時間 | 通常フレーム0不要作業 |
| MapGen | parse/bake/pack各工程のwall time、peak RSS、出力byte/hash | 出力を維持して高速化 |
| 起動/ビルド | process起動時間、RSS、リンク成果物サイズ、フル/増分コンパイル時間 | 未使用機能のコスト低減 |

受入基準：正しさに1件でも回帰があれば不合格。性能は変動幅・サンプル不足を考慮し、**基準値に対する有意な劣化は不合格**。たとえば p95の2%以上の悪化は一次調査条件とし、計測ノイズ内なら追試する（2%は**判定を始める暫定目安**であり、改善済みの実測値ではない）。壁時計のみのFPS比較で「完了」としない。

#### 設計ゲート

- 新しいグローバル可変状態を作らない。状態には作成者、所有者、スコープ、破棄時点を明示する。
- 循環依存・相互include・広域公開ヘッダの増加なし。ヘッダ内の不要なheavy includeを減らす。
- `std::function` / 仮想呼出し / `shared_ptr` / `unordered_map` / Mutex等は**使用頻度と所有権に合致する場合のみ**。禁止ではなくpay-for-what-you-useで判断する。
- 抽象化の過剰増殖を回避。「移動しただけ」「名前が変わっただけ」では完了にならない。責務と依存の改善、検証能力または実行コスト削減の証拠を残す。
- 機能別 `README` / ADR に public API、入力・出力、副作用、tick、エラー処理、スレッド、SSOTを記載する。

### 設計判断の詳細基準
#### クラス分割の判定

- 1つのファイルが1,000行を超えることは**調査トリガー**であり、機械的な分割条件ではない。
- 「同じ変更理由」や「同じ寿命」を共有するなら一定の大きさでも維持する。異なるowner/更新タイミング/スレッド/依存方向を持つなら分割する。
- `*Policy` / `*Rules` は純粋計算である限りstatic functionでもよい。状態とI/Oが混ざるものはinstance ownerへ寄せる。
- 不要な1関数1クラス、Pimpl乱用、virtual interfaceの乱立、テンプレート肥大、薄いWrapperだけの層を禁止。

#### SSOTの候補と正本の選び方

| 対象 | 正本の定義 | 派生物に許されること |
|---|---|---|
| サーバー試合結果 | server-owned match/runtime state | clientでprediction・可視化。確定の上書き禁止 |
| 弾の識別 | `ShotKey` と明示的なevent/frame | 表示用一時IDは可、別のshot identityを作らない |
| 武器/ダメージ | canonical immutable weapon+combat policy | UI/予測は同じpolicyを参照 |
| Protocol | packet definition + codec + Golden Vector | 高速読み取り表をAOT生成してもwire layoutと同期 |
| Match/Slot | canonical MatchDefinition/SlotCapacity/lifecycle | 各配列やUIは容量をderiveする |
| Config | persisted key/default/validationを持つ1つのowner | Classic/通常Settingsから同じ操作を提供 |
| Locale | AOT generated catalog + stable locale IDs | runtime lookup/cacheは可、別辞書を持たない |
| Frame time | simulation tick / snapshot tick / presentation timeの各正本 | 正しい変換を介して表示で利用 |

#### Zero-Cost / DODの採否基準

- 8人など上限が固定されたスロット処理は、小規模で連続した `std::array` + index に有利な可能性が高い。ただし sparseなデータや寿命の短い対象には適切な別手段を選ぶ。
- コンパイル時に確定可能な lookup table は `constexpr` / `consteval` / code generationを比較。巨大なソース生成がコンパイル時間を増やす場合、アセット埋込みバイナリ/圧縮データの方が妥当かも測定する。
- `std::span`、`string_view`、参照はコピーを減らすが、**所有者より長く保持しない**。frame-local scratch memoryはframe末に破棄できることを保証する。
- `std::function` を hot path で使用する場合は型消去/alloc・inline消失を計測し、必要ならtemplateや関数ポインタへ変える。ただし読みにくさに見合わない最適化は採用しない。
- `unique_ptr` で所有者が一意なら `shared_ptr` を避ける。参照先の寿命を追えないraw pointer化は危険。
- logging offではformattingなし。ログの意味、クラッシュ時の使用可能性、時系列を確保する。
- コンパイル時分岐と実行時切替を混同しない。Vulkan/OpenGLやClassic/Modern等、ユーザーが起動中に切替える必要のあるものをビルド時限定にしない。

#### 比較すべき「改修前後」の5点

各変更セットで最低限、(1) 責務と依存の差分、(2) 公開APIと呼出し箇所の差分、(3) 動作/Golden Vectorの差分、(4) 性能/メモリの差分、(5) 削除できた重複または回避できた実行時コスト、をレビューする。

### 重大リスクと停止・差し戻し条件
| リスク | なぜ危険か | 必須対策 | 停止条件 |
|---|---|---|---|
| Static→instance の大規模移行 | initialization順序、グローバルな既存呼出し、server/client state混在 | 橋渡しAPI、参照グラフ、再起動fixture | shutdown/restart/room changeのいずれかでstate混線 |
| Authority規則共通化 | 予測をサーバー裁定に混ぜる危険 | canonical pure rules、同一入力でのparity、real UDP | 複数clientで勝敗/HP/shotが不一致 |
| NetSmoothing短縮 | 補間結果はremote replicaの`PlayerEntity`位置適用に用いられ、さらに`AckPoint`経由でIntent/ClaimのAckFrameを決める。見た目だけの純粋な変更ではない | remote replica適用とauthoritative stateを区別し、**AckFrame/AckSubFrame・hit claimの時刻Golden試験**、切断/再生境界テスト | authority position/判定が変化、または意図しないAckFrame差分 |
| 生存時間の短縮 | lambda/workerが破棄済みsceneを参照 | RAII、cancel/join、headless movie check、ASan | UAF、クラッシュ、非同期hang |
| テストコードのbuild分離 | CLIが引き続き参照しているテストまで除外 | entrypointリンクマップ、CTest、診断CLI | リンクエラー/実行コマンド消滅 |
| MapGen最適化 | 演算順序や出力バイト差で衝突/材質が変化 | hash/pixel/collision golden | 出力内容が予告なく変化 |
| AOT locale・glyph | Unicode mappingのずれ、日本語字形の倍率回帰 | 文字ID/atlas/レイアウトfixture | 表示・配置・設定切替の回帰 |
| ログの高速化 | 障害調査に必要なログ消失 | off時formatなし、on時保証、flush/crash path検査 | UAFまたはクラッシュログ欠落 |
| build最適化 | ソース除外がAndroidや専用serverだけを壊す | platform matrix、link map、compile definition整合 | いずれかの製品variantが未ビルド |

**差し戻し方針**：フェーズ単位の変更を取り消せる状態で管理し、失敗時は機能修正を重ねてごまかさず、前の正常な責務境界へ戻す。外部公開形式の違いは明示的なschema/version migrationで処理し、暗黙変換に依存しない。ここでいう差し戻し方針は将来の作業用であり、本書の作成ではGit操作・commit・pushは行っていない。

---

## 3. 全体ロードマップ（Phase 0～12）
この章はフェーズ間の順序を決める。**担当者がコードに触れる際の関数レベル指示は次章の対象別チケットを正本として使う。**

### Phase 0～12：各フェーズの目標・作業・完了条件
**フェーズの依存関係**：P0（証拠）→P1（回帰ゲート）→P2（ビルド/入口整理）→P3-A（所有権の読取監査）→P4～P10（サブシステムごとに順次、または独立部分のみ並行）→P11（削除）→P12（最終監査）。特に **ネットワークの状態所有権とauthority変更は並列で行わない**。各フェーズの「完了条件」が満たされない限り次へ進めない。

#### Phase 0 — 在庫・依存・実測の凍結（変更なし）

**目的**：何が無駄なのかを推測でなく証拠で決め、削除・最適化の優先順位を確定する。

**対象**：`Mods/` 全体、`CMakeLists.txt`、ネイティブCI、既存計測、テスト実行入口。

**作業**

1. 947ファイルすべての構造情報、compile unit、外部参照、ソースサイズ、実体サイズを一覧化。`Network/Testing`・`Diagnostics`・`MapGen`・`Thumbnail*` 等について、「通常起動に必要か」「コマンド呼出し時のみ必要か」「専用ツール専用か」を明記。
2. `.cpp` と `.hpp` の依存関係図、循環図、`ModEntry` への集中度、静的状態一覧を自動生成。高fan-out（変更が多くの再コンパイルを招く）ヘッダを抽出。
3. 候補ごとに **無駄の種類**を付与：`D1`到達不能、`D2`未使用の機能、`D3`同一規則の重複、`D4`不要な常時実行、`D5`不要な動的メモリ、`D6`依存拡散、`D7`コールドパスの非効率、`D8`冗長な互換層、`D9`過剰な抽象化、`D10`余分な変換/コピー。
4. プロファイラによってTop 20のCPU時間消費箇所、Top 20のalloc箇所、最も遅いbuild translation unitsを特定。`-measure` 系既存計測オプションの実際のパラメータを確かめて固定手順化。
5. ゲームデータ依存のテストと不要なテストスキップを整理し、現行実行時間・成功件数・実測ハッシュを保存。ベースラインはブランチ名だけでなくcommit SHAに紐付ける。
6. 各候補に「性能期待」「変更影響」「仕様確認の必要性」「優先度」「検証方法」「戻し方」を登録し、P0/P1/P2の順でランク付け。

**完了条件**：全ファイルに分類があり、重大候補はコード位置・呼出し側・可観測影響・除去条件を持つ。計測がない候補に速度改善の見込み値を付けない。**この時点では削除しない。**

#### Phase 1 — ベースライン回帰テストと静的ゲートの構築

**目的**：リファクタリングを小さく安全に進められる「変更不可の契約」を作る。

**対象**：`Mods/Diagnostics/`、`Mods/Network/Testing/`、`src/MphRead.Native/Testing/`、`tools/testing/`、CMake/CI。新しいゲームロジックは追加しない。

**作業**

1. 既存CTestsを機能別に分類：CLI、combat、network、smoothing、input、UI/localization、map、render、lifetime、platform。ラベル・skip条件・データ要件をドキュメント化。
2. Golden Testsを追加：UDP packetをbyte-for-byte検証、設定ファイルの読み書き、ローカライズキー・glyph mapping、フレーム単位の状態遷移、シード固定MapGen出力、描画ピクセル。
3. 「同じイベント列なら最終状態も同じ」という differential trace を作る。サーバー/ローカル/リモート描画のいずれを検証しているか毎ケース明記。
4. 長期ライフサイクルのループ：サーバー起動停止、Client再接続、`NetSession`リセット、room change、map rotation、alt-tab、render backend switch、movie終了/scene destroy、非同期オーディオ破棄。
5. 受信遅延・損失・重複・reorder・spawn直後・死亡/respawn・Morph+Fire同時押し・Prime譲渡/交代・継続武器/追尾のdeterministic fixtureを確立。
6. 静的CIルール：新規循環include、`Entities/Formats/Metadata` の新たな大規模書換え、ヘッダ依存の過度な増加、テスト専用コードの無条件初期化、未承認のwire変更を検出。

**完了条件**：主要な動作を自動化でき、失敗を明確なテスト名で検出可能。テスト自体による production path 変更がない。プロファイル測定時にはテスト計測オーバーヘッドを無効化できる。

#### Phase 2 — `ModEntry` の責務分割と本番・検査ビルド境界

**目的**：巨大入口を単純なdispatcherへ縮小し、不要なテスト/ツール処理が通常アプリに負担を与えないようにする。

**重点対象**：`ModEntry.cpp/.hpp`、`ConsoleWindow.cpp`、`Thumbnail*`、`Diagnostics/*`、`Network/Testing/*`、CMake source collection。

**作業**

1. `ModEntry` の引数処理を実機能群へ分割：`Launch/CommandLine`、`Launch/RenderOptions`、`Launch/ServerCommand`、`Launch/MapTools`、`Launch/TestCommands`、`Launch/UpdateCommands` 等。新しい巨大な共通クラスを作らず、**純粋な引数解析→検証→機能呼出し** の薄いレイヤーにする。配置は `Mods/` 内で各 `.hpp/.cpp` を同階層に。
2. `HasFlag` / `ValueAfter` / parse helpers の同義重複を調査し、空文字・未知flag・大小文字・負値・順序・既定値・エラー出力の振る舞いを固定。単一のparse policyをSSOTにする。
3. 読み込みの重い機能を**そのコマンドを使うときだけ**初期化する。通常ゲーム起動、専用サーバー起動、テスト実行、地図ツール/サムネイル生成の経路を明確に分ける。
4. [`CMakeLists.txt`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/CMakeLists.txt#L51-L76) の `GLOB_RECURSE` を踏まえ、実行機能のエントリー・ソース所有権を設定。単に `Mods/Network/Testing` を除外すると `ModEntry` の診断CLIが壊れる可能性があるため、**保持する診断コマンドと実行専用テストバイナリを先に仕分け**する。
5. 診断を維持する場合の選択肢： (a) tests-only static libraryへ分離、(b) 明示的な検査機能ビルドオプション、(c) CLI専用実行バイナリへ移行。選択は実際のリンク結果と運用要件で決め、Releaseの「必要なdiagnostics」は残す。
6. `#include` の前方宣言化・private実装への移動、汎用 `std::function` registry の常時ヒープ確保回避、コンパイル時テーブル化可能なコマンドの選定を行う。ただしオプション順序・ヘルプ文言を変えない。
7. Windows GUI / Android / Headless server の起動パスで不要なQt、OpenGL、Audioの初期化がないか計測し、無関係な初期化を遮断する。

**完了条件**：CLI互換テスト全PASS。`ModEntry.cpp` が主に起動振分けだけを担い、各機能への深い依存を減らす。本番ビルドのリンクマップで不要部分が削減されたか検証。単純な物理行数削減だけを成功条件にしない。

#### Phase 3 — State / Lifetime / Ownership 基盤の整理

**目的**：リセット/再入場/scene破棄/複数プロセス/非同期実行で不整合を生む状態を所有者へ帰属させる。

**重点対象**：`Network/Session/NetSession.*`、`SessionContext.hpp`、`NetLifecycleTracker`、`NetRoomChange`、`NetPlayerLifecycle`、`MoviePlaybackSession.*`、`ShutdownSignals.*`、`Headless.*`、`Sound/*`。

**作業**

1. 状態一覧を `process / application / window / network session / match / room / player life / render frame / task` のスコープに分類し、ownerとreset場所を一つにする。
2. まず現行 `NetSession::_context` の読み書き・静的メソッド呼出し・サーバー側 `DedicatedServer` とのつながりを作図。`SessionContext` は既にサブ状態の集合であるため、**新たな巨大State型を重複作成しない**。
3. **P3-Aの必須作業は現状の所有権監査・生存期間/リセット規約の明文化と、既存 `SessionLifecycle::Ports` / `SessionContext` 境界の再利用に限定する**。P3-A内の手順(A1)：静的 `NetSession::_context` とすでに参照受渡ししている `LifecyclePorts()` / `SessionRuntimePump::Ports` の所有権と再入を図示。手順(A2)：既存Portsを用いて副作用順序を維持できる小さな非動作変更だけ実施。**新規クライアントsessionのインスタンス化、全 `NetSession::` 呼出し置換、静的入口削除はP3-Aの必須条件としない**。必要性が計測・テストで立証された場合のみ、Phase 4の戦闘契約とPhase 6のACK時刻試験を固定した後の独立チケットで段階移行する。
4. サーバーは専用サーバープロセスで動作し、`SessionLifecycle::StartServerAuthority()` によって `NetRole::Server` / authorityを取得する。旧 `StartHost()` は例外を投げ、クライアント兼Authority起動を拒否している。将来 `NetSession` をインスタンス化する場合も「サーバーの全状態をクライアントsessionに押し込む」ことにならないようにする。**サーバーとローカルプレイヤーを別のownerにする**。UDPとimmutable protocol rulesだけを共有する。
5. サーバー側判定とローカルの予測/表示の状態を明確に分け、`NetHitClaims`・`NetSmoothing`・`NetPlayerBridge` のグローバル変数を用途別に分類する。共有したいのは**規則**であり、**可変ランタイム状態ではない**。
6. Async audio、Cutscene、scene破棄、window閉鎖、Device reset、UDP workerの停止順序を可視化。終了・キャンセルシグナル、owner付きtoken、`std::jthread` / `stop_token` / RAII等を適切に使い、破棄済みsceneを遅延コルーチンが参照しないことを保証。
7. `std::shared_ptr` は複数ownerが本当に必要なオブジェクトだけに用い、循環参照/永続キャプチャを監査。借用ポインタ/`span`/`string_view` が非同期・長寿命ownerからぶら下がらないようにする。
8. static初期化順序、static destruction、再初期化、接続ID/ConnectionSecret、slotやgeneration/epochを追跡し、テストが順序依存にならないことを確認する。

**完了条件**：同一プロセス内での複数連続session・map rotation・start/stop・destroy/recreateで状態混線がない。`NetSession` の静的ファサードを維持したまま状態ownerとreset契約が明確化され、authority の所在、ローカルプレイヤーのUI、接続ハンドシェイク、server bots、UDPが不変。ASan/TSan/lifetimeテスト合格。**インスタンス化は必須ではなく独立変更**。プロトコル変更は含めない。

**Phase 3のゲートを厳密に二分する。** `P3-A` = ownership/read-write/reset/shutdown/threadの証拠採取、責務と寿命の文書化、既存`SessionContext`/Portsの利用（コード意味を変更しない）。`P3-B` = 新しいownerインスタンス導入、static state除去、初期化/破棄順の実変更。**P4/P6が依存するのはP3-Aのみ**であり、P3-Bを先に強制しない。P3-Bの実施可否は保護済みネットワークテストと定量的な必要性から判断する。これにより「P4はP3完了が必要、P3侵襲改修はP4後」という循環依存を排除する。


#### Phase 4 — Combat / Authority / Gameplay 規則をSSOT化

**目的**：発射時点の規則とサーバー裁定を一箇所に集め、予測と検証で異なる条件を持たせない。

**重点対象**：`Network/Gameplay/Claims/Hit/NetHitClaims.*`、`ClaimDamagePolicy`、`ClaimImpactRules`、`NetDamage`、`NetHitPrediction`、`NetShotEvents`、`NetUnlagged`、`Combat/WeaponVariant`、`WeaponCharge`、`ProjectileHoming`、`Gameplay/StatusEffects`、`Network/Domain/ShotKey` 等。

**作業**

1. 「ショットが生成された時点の規則」を優先する。`weapon / charge / affinity / Prime role / Double Damage / launchFrame / AckFrame / owner / target` が各コンポーネントでいつ、何を正本として評価するかを表にする。
2. `NetHitClaims` を意味単位に切り出す：client claim declaration、outbox/retry、authority validation、impact/reconciliation、verdict dispatch、metrics/diagnostics。既存 `HitClaimOutbox` / `HitVerdictOutbox` / `AuthorityHitLedger` 等を再利用し、同名の二重機構を作らない。
3. 判定規則は副作用なしの関数（入力：immutable ShotContext + GameRuleSnapshot、出力：Decision/DamageResult）に寄せる。shotの確定・台帳書き込み・通知・描画をそれぞれ違う層へ分ける。
4. owner宣言 / observer表示 / authority検証の役割を明文化。authorityが過去フレームを遡る必要がある場合のhistoryの意味を固定し、**クライアントの自己申告を最終判定に使わない**。
5. 連続武器（Shock Coil等）、弾道、追尾、AoE、変身時Fire同時到着、turret、Missile/Power Beam、Freeze/Burn/Disruptの例外を列挙し、共通判定で吸収可能な範囲だけを抽出。
6. enum/bitflag/time base/slot capacityが複数宣言されている場合は正本を一つにし、サイズに敏感なpacketは `static_assert` とGolden Vectorで保護。`NetProtocol.hpp` の大きなinclude集約による伝播も監査。
7. 微小計算の高速化は、ゲーム結果のパリティを先に証明。不要な一時ベクトル、変換、文字列組立て、再検索を排除し、使い回せる固定小容量データを選ぶ。最適化のためにスレッド競合を導入しない。

**完了条件**：各武器×charge×affinity×Prime×Double Damage×headshot×resistance×lag×respawnのマトリクス成功。Claimのdup/drop/reorderを含む実UDP試験成功。サーバーauthority確定結果と予測のsettlementが一致し、wire互換、shot idempotency維持。予測/authorityで**同一規則を呼ぶことをテストで確認**。

#### Phase 5 — Network / Codec / Transport の低オーバーヘッド化

**目的**：配信レート・待ち時間・通信量を悪化させず、不要な変換・alloc・mutable共有状態を減らす。

**重点対象**：`Network/Protocol/Packets/*`、`Codec/*`、`Network/Transport/*`、`Session/SessionPacketDispatcher`、`SessionIntentPublisher`、`SessionSnapshotPublisher/Receiver`、`Server/Core/*`、`Server/Lobby/*`、`Network/Replay/*`。

**作業**

1. protocol fieldとサイズ・endianness・有効bit・optional区間・version・max MTUを一元管理。送信・受信・デモ再生のcodecを**共通の決定論的な規則**で検証する。
2. 一時 `std::vector`、追加コピー、文字列化、シリアライズ前の重複構築、待機中queueなどを計測。固定最大長packetには `std::array`/`span` と境界確認を優先。可変サイズpacketへ盲目的に固定巨大バッファを使わない。
3. 差分state、one-shot event、retry対象、予測の破棄、各種ACKを分類し、**上書き可能な状態**と**欠落不可のイベント**に異なるqueue policyを用いる既存設計を保持・監査。
4. `NetTransport` と session logic の境界：socket/receive/sendだけtransport、再送/ピア状態はsession、試合規則はdomain、authorityはserverのownerへ。transportにgameplay詳細を増やさない。
5. hosted room/dedicated server/ローカルクライアントの共通処理は immutable rule/codecs に限定。セッション変数やglobal player stateをホスト間共有しない。
6. `ServerPeerConnectionChannel`、admission、liveness、chat rate-limit、secret、rebind、room-change、snapshot受信タイミングを含むintegrity testを追加する。
7. ReplayとLiveの両方で同一packetが同じ状態遷移を生むことを確認。録画仕様と診断ログフォーマットは必要な限り維持。
8. per-tick/packetの性能計測を実行。packet sizeを小さくすることがCPU負荷増・再送率増・遅延増になるなら採用しない。

**完了条件**：byte互換、長さ互換、全versionの契約検証、UDP matrix合格。alloc/packet、µs/encode・decode、queue peak、bytes/sが非劣化。Server authority/hosted matchを通した正常動作確認。

#### Phase 6 — 高FPS描画と60Hzシミュレーション、NetSmoothingの分離

**目的**：サーバーから受けた最新状態とリモート表示の補間時計を区別し、可能な限り低遅延で滑らかに表示する。ただし現状の表示時計は`AckPoint()`経由でIntent/HitClaimの時刻と結合しているため、**単なる「描画だけの変更」と扱わず、送信ACK/巻戻し履歴との整合を維持する**。

**重点対象**：`Network/Gameplay/Replication/NetSmoothing.*`、`FormReconciliation`、`NetPlayerBridge`、`NetPlayerLifecycle`、`PlayerEntityNetAim`、`RemoteShotConfirmation`、`Render/PlayerEntity*`、`Render/FrameTiming.*`。

**作業**

1. まず `NetSmoothing` の現在の履歴（`HistoryFrames = 64`）、`MinDelayFrames = 2`、`MaxDelayFrames = 8`、`_readFrame`、hold/snap処理を実際の遅延データと結び付ける。**「30Hzに見える」ことと実際のネットワーク更新レートを混同しない。**
2. authoritative simulation、latest received snapshot、prediction、remote replica の表示用サンプルを役割で分類。**現行の `NetPlayerBridge` は補間位置を `PlayerEntity` に `Move(..., true)` で適用している**（同ファイル:501～524）ため、「描画専用の独立位置が既に実装済み」とは断定しない。新しい表示座標への分離は別変更とし、姿勢変換/当たり判定/追尾入力の利用箇所と `AckPoint()` の接続を追跡してから着手する。
3. 高FPS描画での可変 `dt` サンプリングは**現行実装ではなく将来の仕様改善候補**。現行 `NetSmoothing::Tick()` は `_readFrame += 1.0` のステップ時計であり、描画回数に合わせて呼ぶとACK時刻が変わる危険がある。まず現在のSimulation Tick / Receive / Present頻度を測定し、フレームごとの補間はACKの生成時計を分離する設計とGoldenテストを先に確定してから評価する。意図的遅延の短縮も別チケットとして計測する。
4. 旧historyと新historyが重複保持しているなら、snapshot history / transform history / predicted input history / server rewind historyを用途で峻別。不要なコピー/再格納を減らすが、authoritative rewindを壊さない。
5. high FPS時の毎描画更新の`std::function`/文字列/alloc、モデル可視性、補間フラグ、変身・死亡・テレポート/部屋移動時のsnapを計測する。
6. authoritative eventの時刻とvisual stateのtick参照を保持。Morph/Halfturret/weapon pickup/damage/homing/spectatorにおけるdiscontinuityに補間をまたがせない。
7. 現在の補間のままで無駄な処理だけ除く「挙動不変版」を先に完了。その後に限り、遅延制御や補間式の変更を**別仕様改善**として検討する。

**完了条件**：Remote render p95 frametime、受信→可視化までの遅延、snap/hold比率、CPU/allocが非劣化。高FPS（例：120/240/540Hz級）で描画が自然であり、**`CaptureIntent` の AckFrame/AckSubFrameと `Declare` のAckFrameの時刻規約・既存Golden値が意図せず変わらず**、ヒット裁定・サーバー上の座標が変わらないこと。壁越し・上下階層・spectator切替・disconnect・変身・respawnの視覚回帰なし。

#### Phase 7 — Input / Aiming / Settingsの境界と共通規則

**目的**：入力デバイス間で重複している意味変換だけを共通化し、毎フレームの軽さと既存操作の忠実性を両立する。

**対象**：`Input/`（118ファイル）、`InputSettings.cpp/.hpp`、`GameSettings.*`、`Input/AimAssist/*`、`SpectatorMode.*`、`Input/SpectatorInput.*`。

**作業**

1. 物理デバイス入力（RawInput、mouse、pen、touch、gamepad）→正規化→意味のあるAction→ゲームsimulation、というパイプラインを図示。異なるデバイスの特殊値を無理に統合しない。
2. `Modern` 60Hz aimと `Classic DS` 30Hz扱い、ネイティブtouch、MouseMotionTouchSource、MorphBallのルールをテストで固定。キーとスタイラスの時刻規則の違いを維持。
3. `InputSettings` と `GameSettings`、Qt settings、Classic menuで default / enum / serialization /表示ラベルの正本を特定。操作プロファイル・ホイール履歴・dead zone・hotplug/reconnectの保持範囲を一元管理。
4. 入力ソース追跡、gamepad/device mapping、HID platform adapterの重複調査。アダプタがプラットフォーム依存、Action policyがプラットフォーム非依存となるようSoCを適用。
5. 全デバイスを毎frameスキャンしている場合は使用中/接続中のみ有効化できる箇所を測定。毎frameの文字列変換、設定読み込み、動的コピーを除去。
6. カメラ入力とSpectator切替/Alt+Tab/キー解放イベントを別責務にし、入力が失われたときも動作がロックされないことを検証する。

**完了条件**：RawInput/Pointer/Gamepad/Touch/Aim系テスト、実機hotplug、Classic/Modern、Morph連続Boost、spectator、focus lossで挙動一致。毎frameコストとalloc削減を確認。

#### Phase 8 — UI / Localization / Renderの更新責務とキャッシュ

**目的**：画面ごとに異なるpresentation責務を整理し、描画フレーム単位の無駄をなくす。

**対象**：`ClassicMenu/*`、`Localization/*`、`Chat/*`、`Render/*`、`RenderOptions.*`、`EndScreen.*`、`PauseMenu.*`、`MapPick.*`、`WindowMode.*`、`WindowGeometry.*`、`ScreenCapture.*`。

**作業**

1. `ClassicMenu/Compose.cpp` の `BuildServers`、`BuildCreate`、`BuildSettings`、`BuildLobby`、`BuildRoom`、`BuildPause` 等を**ページ定義・イベント処理・描画データ更新**に分離。各画面のownerを狭くし、共通部品は重複した知識だけをまとめる。
2. `MenuData.cpp` にあるROM読込/モデル/レイアウト/Font参照の責務を区分。ROMの座標や字形に必要な互換性を維持し、ROMを再パースする必要がない更新で再解析しない。
3. `MenuLocalization.cpp` では **既存のビルド時生成（AOT）を維持**。英語キー→IDの索引、各言語レコード、fallback、Unicode atlasについて再検索・遅延初期化/同じキーの重複変換だけを計測。JSON方式へ戻したり翻訳文をC++に散在させない。
4. `MenuUnicodeFont.cpp` の `k8x12L` ビルド生成 + lazy atlasを維持。通常フレームのglyph生成回数0を目標に、追加glyph時の再割当/texture更新を測定。未知glyphのQt fallback・日本語字形・ピクセル寸法を保つ。
5. Classic と通常SETTINGS双方から「Modsメニュー言語」のSSOTを使用し、ゲーム本体の言語切替とは別の設定値として扱う。UIがClassic限定にならないことを確認。
6. `Render/*` のHUD、crosshair、radar、team UI、shot visualizationの表示判定をrender-only policyへ。simulationから直接GPU資源・描画キャッシュを操作する境界違反を調査。
7. GPUのリソース再生成、window fullscreen switch、Android context recreation、async readback/screenshotの所有権を整理。Vulkan/OpenGL/GL ES間の条件分岐はbackend境界へ移し、generic rendering logicへ散らさない。
8. dirty bit / revision / event による更新条件を導入できる箇所を特定。**実際に変わったデータだけ**再レイアウト・uniform更新・atlas転送する。新しい巨大なUIキャッシュ階層を作らない。

**完了条件**：多言語切替・日本語句読点/全角文字・k8x12L倍率・UIヒットボックス・word-wrap・HUDピクセル比較・RHI conformance合格。Stable-state UIのalloc/frameが実測で改善し、キャッシュ破棄漏れなし。

#### Phase 9 — MapGen / Import / Packing をステージ分割（コールドパス）

**目的**：地図読み込み・インポート・変換・再パックの巨大処理を分割し、RAMと総変換時間を減らす。**通常描画フレームの最適化より後に実施**する。

**対象**：`MapGen/*` の全61ファイル。特に `Q3Import.cpp`、`MapDefinition.cpp`、`MapPacker.cpp`、`MapNodePacker.cpp`、`MapTextureBake.cpp`、`Q3Bsp.*`、`MapBuilder.*`、`Q3TextureMatches.inc`。

**作業**

1. 処理パイプラインを `read→validate→parse→normalize→geometry→material/texture→lighting→collision→entities→pack→write` に分け、各中間表現の生存期間と所有権を定義する。
2. `Q3Import.cpp` を用途に沿って分割し、既存の `Q3Bsp`/`Q3Md3`/`Q3Movers`/`ShaderAnim` 等の処理を二重実装しない。
3. 入力のread/parse/validation、変換ロジック、書込、ログ出力をSoCで分離し、テスト時にはファイルI/Oなしで変換規則を検証可能にする。
4. 大容量中間vectorの不要コピー・素材の再変換・重複検索・allocationピークを調べ、reserve、所有権move、contiguous storage、index参照、明示的なbuffer reuseを比較する。出力が変わる並列化や浮動小数点再配列は別実験とする。
5. `Q3TextureMatches.inc` 等の大規模静的データは、**既存がビルド時定数ならその強みを維持**。AOT表引き・整列配列・コンパイル時間と実行時間のトレードオフを実測する。
6. `.pk3`読み込み、フォールバックテクスチャ、オフライン/オンライン互換、衝突、アニメ材質、パッド、透過、ポータル、マップ境界のGolden fixtureを作る。
7. build/parse/packの各処理の入出力のbyte hash/semantic hashを記録。非決定性がある箇所はseedまたはソート規則を固定し、意図しない出力変更を拒否。

**完了条件**：代表マップ全件の入出力互換、衝突/テクスチャ/ノード一致、同一条件下でparse時間・peak RSSの改善。MapGenのためだけにゲーム常時メモリを増やさない。

#### Phase 10 — Diagnostics / Launcher / Update / Sound / Platform / Root残余を整理

**目的**：機能開始時だけ利用する処理を分離し、全体を通して異常系の信頼性を維持する。

**対象**：`DebugLog.*`、`Diagnostics/*`、`Launcher/*`、`Update/*`、`Sound/*`、`Platform/*`、`Thumbnail*`、`MoviePlaybackSession.*`、`Branding.*`、`Credits.*`、`CrashReport.*`、`LogShare.*`、`Window*`、`WorldEvents.*`、`Gameplay/NativeGameplayClock.hpp` 等。

**作業**

1. `DebugLog.cpp`：exception/backtrace/crash handler、ログformat、sink、tee、ログrotation、OS system info、flush/closeを分類。crash signal/SEH時に**安全でないlock/alloc/reentrant codeを呼ばない設計**を別経路として監査する。
2. `Diagnostics/*`：off状態では高コストログformatとデータ収集を避け、on状態では必要な記録が消えない構造へ。`FramePerformance` や `CpuSampling` を必要時だけ起動し、`NetLog` / `NetShotDiagnostics` の各tick生成物をサンプリングで検証。
3. `Launcher/Portable` と `Launcher/GuiLauncher`：ゲームファイル探索、設定、UI、local server起動を分離。起動用のコールドデータがgameplay hot pathに残らないようにする。
4. `Update/*`：version、update check、download、install、desktop/serverの分岐を整理。network transportと更新用HTTPを不用意に共通化しない。ファイル書込はatomicity・ロールバック・証明書検証・途中終了耐性を保つ。
5. `Thumbnail*`：batch/controller、capture、generator、cache、log、window/renderer lifetimeを区分。不要なGPU/CPUリソースを通常起動では作成しない。キャッシュヒット/ミス両方の品質と速度を計測。
6. `Sound/*`・`MoviePlaybackSession`：デコード/音声deviceとscene lifetimeの明確な終端を作る。Headlessでムービーを再生しないときでも、非同期callbackがdangling sceneを参照しないことをテストする。
7. `Platform/*` と `Window*`：OS固有処理を薄く閉じ込め、共通ロジックをOS分岐で汚染しない。GPU/sound/menuの更新順序とフォーカス切替も維持。
8. 直下の小さなクラス（`Branding`, `Credits`, `WorldEvents` 等）はサイズだけで移動せず、各ownerとの境界および構築時コストが必要な場合のみ扱う。

**完了条件**：Headless起動/停止、更新、rollback、サムネイル生成、ログrotation、crash再現、Android起動、movie scene破棄、Qt window、openGL/Vulkanで全PASS。問題の原因を追えるログを維持し、通常時のログ処理負荷は減少。

#### Phase 11 — 証拠に基づく廃止コード・重複・互換レイヤーの削除

**目的**：ここで初めて「大量の無駄」を**削除可能と確定したものだけ**消す。前フェーズで行った責務分割後の残骸も対象。

**削除フロー**

1. 候補登録：ファイル/シンボル、元の目的、呼出し元、CMakeターゲット、C++/C#参照関係、platform条件、CLIからの到達、動的登録経路を提示。
2. 稼働中reachable traceとcompiler/linker mapを採り、`unused` を `dead` と短絡せず間接呼出しを確認。
3. **4分類**：`REMOVE`（未到達かつ不要で裏付けあり）、`CONSOLIDATE`（意味が同一の重複）、`RETAIN`（互換/診断/特殊環境で必要）、`DEFER`（検証未了）。
4. `REMOVE`は１種類ずつ削除してCTest・実機・binary symbol・performance compare。削除箇所のテストも必要なら検査用ライブラリへ移管。
5. `CONSOLIDATE`はデータの正本だけ残し、薄いファサードを移行後に消す。shot rules、packet field、settings default、key mapping、map formatの重複を優先。
6. `DEFER`は放置ではなく、証明に必要なテスト・対象platform・担当phaseを記載。**削除率の数値ノルマを課さない**。
7. rename/moveによる余分なヘッダ、古いalias、未利用feature flag、互換shim、空クラス、常にfalseの条件分岐は振る舞い一致テスト後に削除。

**削除承認チェック**：`(a)` 呼出しがない、`(b)`動的な外部入力からも到達しない、`(c)`過去プロトコル/保存/デモ/CLI互換の役割を持たない、`(d)`全platform条件で不要、`(e)`回帰テスト通過、`(f)`削除後のリンク/起動/サイズ差分を説明できる、の全部を満たすこと。

**完了条件**：削除・統合候補のすべてに処置と証拠がある。コンパイル・リンク・起動・描画・マップ・UDPのパリティを保ち、ファイル数/実装行数/バイナリサイズ/compile time等の**実差分**が文書化される。

#### Phase 12 — 全体統合、性能チューニング、残余負債の明示

**目的**：局所最適が別領域に負担を押し付けていないことを保証し、維持可能な構造で終了する。

**作業**

1. P0時点の同一コミットベースラインと P12成果を、同一プロファイル・複数試行で比較。改善項目と悪化項目を個別に列挙する。
2. include依存・循環、fat header、runtime static、alloc/frame、cache invalidation、worker lifetime、CMake build boundaryの再監査。
3. 所有権表・コンポーネントDAG・protocol SSOT・settings SSOT・render/simulation boundary・テストマトリクスを最終版へ更新。
4. Windows/Linux/macOS/Android、server/game、GPU backend、high FPS、ローカル・実UDP・実ネットワーク条件で検査。
5. 性能目標未達箇所を特定し、**計測に裏付けられた小修正だけ**を行う。新設の過剰抽象化や互換shimが残っていないか確認。
6. ドキュメントに残るC#向け古いパス表記などをC++実体に合わせる。ただしこの計画はC++のみのため、C#実装は変更しない。
7. `Closeout`：各項目をPASS/FAIL/DEFERで確定し、未解決を曖昧に「完了」としない。

**最終完了条件**：全テスト・互換・性能ゲートを満たし、全サブツリーの処置が明文化されていること。成果は「大きなファイルがない」ことではなく、**動作を保ちつつ、責務・正本・コスト・所有権・検証性が明確になったこと**で評価する。

### フェーズ間依存と並列作業の可否
| フェーズ | 優先 | 先行依存 | 並列可否・理由 | リスク |
|---|---|---|---|---|
| P0 現状把握 | P0 | なし | 単独で基準を固定 | 低 |
| P1 回帰ゲート | P0 | P0 | 影響機能別のtest fixture追加は一部並列可 | 低～中 |
| P2 入口/製品境界 | P0 | P1 | Network状態所有権改修とは並行しない | 中 |
| P3 所有権/終了 | P0 | P2 | **P3-A：読取監査と非動作変更のみ先行**。状態ownerの侵襲改修（P3-B）はP4/P6の契約固定後、必要性が立証された場合にだけ別変更セットで実施 | **最重要** |
| P4 Combat/authority | P0 | P3-A | **単独**。protocol, gameplay、claimに跨る | **最重要** |
| P5 Codec/transport | P1 | P4 | サーバーのpacket面は直列。その他の純粋codec fixtureは並列可 | 高 |
| P6 Replication/render | P1 | P3-A/P5 | RHI backendのみの改善とは分割可 | 高 |
| P7 Input/settings | P1 | P2/P3-A | `NetPlayerBridge` を触る変更と同時実施しない | 中～高 |
| P8 UI/localization | P1 | P2/P7のsettings境界 | C++ menuとfont計測は別々に可 | 中 |
| P9 MapGen | P2 | P1/P2 | ネットワークとは独立したfixtureで進められる | 中 |
| P10 Lifecycle/tooling | P1 | P2/P3-A | Logging/Updateは可。Movie lifetimeはP3に合わせる | 中～高 |
| P11 不要コード削除 | P0 | P2～P10 | 到達解析の前に一括削除しない | 中 |
| P12 全体監査 | P0 | P11 | 最終統合は直列 | 高 |

**最初の着手候補**：`P0 → P1 → P2`。見た目に大きい `MapGen/Q3Import.cpp` の全面分割から着手しない。理由は、CPUのホットパスに対する効果が明らかではなく、ゲーム起動・authority・ビルド境界の問題を先に解く方が事故防止と性能測定精度に寄与するため。

### 実コード確認によるロードマップの補正
| 対象 | 第2パスによる補正 | 実施順 |
|---|---|---|
| Phase 2 | `ModEntry` の分割より前にコマンド→機能→リンク依存の台帳を作る。テスト機能を無条件削除しない | 先行 |
| Phase 3 | **P3-Aは** `SessionContext`/`SessionLifecycle::Ports`のowner図と読取監査。**P3-B（必要時のみ）**はP4/P6の保護契約を固定してから侵襲的owner移行を個別審査。全面非static化を前提としない | P3-Aのみ先行、P3-Bは独立判定 |
| Phase 5 | パケットの固定長/可変長とワイヤ互換の監査を別ゲート化 | Phase 3・4の後 |
| Phase 6 | FPSだけでなく表示遅延、補間誤差、CPU時間を同時測定 | Phase 1の計測後 |
| Phase 8 | `Compose` の `Servers()` 値返却と行単位コールバックを優先調査。AOT翻訳とアトラスは保護 | 独立着手可 |
| Phase 9 | MapDefinitionのパーサー/スキーマ変換を移動と最適化に分ける | 独立着手可 |
| Phase 10 | ログの再帰ロックとクラッシュ経路は安全性を先に検証 | Phase 1後 |
| Phase 11 | 削除候補の証拠は参照数だけでなくリンク到達性とユーザー向けCLI互換を必須とする | 最終盤 |

### 小さな変更セットの推奨着手順
| 順 | チケット | 対象 | 先に揃える証拠 | 最初の変更の範囲 |
|---|---|---|---|---|
| 1 | P0-01～05 | Mods全体 | inventory・依存・CPU/alloc | 計測と記録のみ |
| 2 | P1-01～06 | Tests/CI | Golden/UDP/lifecycle | 回帰テストのみ |
| 3 | P8-01 | ClassicMenu/Compose | `Servers()`値返し確認済み・呼出し回数の測定 | 同一更新世代のsnapshot化のみ |
| 4 | P2-01～02 | ModEntry | init/shutdown図 | 一つの入口責務を抽出 |
| 5 | P3-01～02 | NetSession | static state/owner一覧 | ownerの明示のみ |
| 6 | P4-01～02 | Combat/Claims | shot authority matrix | 純粋規則の整理のみ |
| 7 | P6-01～03 | NetSmoothing | RTT/jitter/render trace | 計測・実験のみ |
| 8 | P9-01 | MapDefinition | JSON Golden hash | parser/model責務の分離 |
| 9 | P10-01 | DebugLog | shutdown/lock contention | 観測と安全性テストのみ |
| 10 | P11 / P12 | Mods全体 | 到達性・最終matrix | 根拠のある削除・統合監査 |

**補足**：この順序はリスク低減のための推奨。実測でCPU/allocの最大ボトルネックが別に判明した場合は、依存フェーズを壊さない範囲で優先順位を変更する。

---

## 4. 主要領域別作業指示
個別領域ごとに、関数分類・ソース根拠・作業手順・回帰試験を集約した。ここにある「確認済み」は**読んだコード上の事実**を指し、測定・実装の完了ではない。

### 関数の責務分類と分類根拠の範囲
> 対象コミット `66fdfd4f23d0e84837c1b16b5a91b8b48113708b`。GitHubの実ファイル本文を読み、**関数名・定義位置から判別可能な責務**を分類した。これは推測による「全関数分類済み」ではない。以下は監査済みの主要翻訳単位の関数レベル台帳であり、未読ファイルの全関数を分類したものではない。行番号はこの固定コミットに対応する。

#### F.1 分類のルール

| 記号 | 責務 | 変更・配置の判断 |
|---|---|---|
| BOOT | 起動、引数解析、モード選択、終了 | 起動境界で一度だけ実行し、実行時ループに持ち込まない |
| STATE | セッション状態、所有権、ライフサイクル | 所有者と寿命を明示。静的ファサードと静的実体を区別 |
| AUTH | サーバー裁定、ダメージ確定、整合性検証 | Authorityを唯一の正本にする。検証と表示を分離 |
| CODEC | データ解析、エンコード、永続化 | フォーマット互換を維持し、純粋関数へ切り分け |
| PRESENT | UI構築、文字列表示、レンダリング補間 | 描画状態はAuthorityの代わりにしない |
| INPUT | 入力デバイス、操作変換、設定 | 取得・正規化・操作決定・出力を分離 |
| OBS | ログ、計測、診断、クラッシュ処理 | 本番ホットパスと例外終了経路のコスト・安全性を分ける |
| RULE | 純粋なゲーム規則・数値変換 | 決定的テスト、AOT/inline候補。無理に共通化しない |

### M-01～M-12：担当割り当て用チケット早見表
- 対象コミット `66fdfd4f23d0e84837c1b16b5a91b8b48113708b`（`main`）。下記行番号はこのコミットのもの。作業開始時に再取得して差分を確認する。
- **確認済み**：GitHub上の関数定義・ソース構造。**未検証**：呼び出し頻度、プロファイル結果、全プラットフォームでの動作。
- 各チケットは「1つの責務」「独立したテスト」「独立した差し戻し」が成立する粒度とする。大規模な一括移動はしない。
- C++版のみ。C#の既存ゲーム規則は互換基準とするが、**現行C++独自の通信・権威・補間仕様は現行C++コードとGoldenテストが基準**。既存のオンライン通信・権威判定・入力・描画の意味を変更しない。

#### G.2 着手順序と作業チケット

| ID | 対象と実関数 | 担当者の具体的な作業 | 完了判定 | 依存 |
|---|---|---|---|---|
| M-01 | `ClassicMenu/Compose.cpp:432` `Composer::BuildServers`; 479～494行の`row`コールバック | `Servers()`の**値返しに加え、`ClassicHost.cpp:161`のQt→C++全件変換も確認済み**。更新タイミングと内部生成コストを追跡。1回のUI更新内で同一スナップショットを再利用できる境界を特定し、必要なら表示モデルを一度だけ構築する。コールバックに一時オブジェクトの参照を保持させない | 同じサーバー一覧・Ping・並び順・選択動作。取得回数とコピー量のBefore/Afterを計測 | 単独 |
| **M-01B（独立）** | `Compose.cpp:502,1102`→`ClassicHost.cpp:188` `Join(int index)` | Hunter選択中の一覧更新を再現し、選択したendpointを変えない識別token/世代を設計。**M-01のコピー削減とは別の振る舞い修正チケット** | 行入替・削除・再接続でも別サーバーへ誤接続しない | M-01の挙動fixture作成後 |
| M-02 | `ClassicMenu/Compose.cpp:585` `BuildSettings`; 618～633行の`rowAt` | `Settings(_section)`の**`std::vector<SettingRow>`値返しと`ClassicHost.cpp:424`の`SectionRows`再走査・`row.Live()`評価を確認済み**。UI更新契機・内部生成コストを確認。設定行をラベル・値別に取得するたびに全一覧を再生成していないか検証。ページ単位のキャッシュを採用する場合は設定変更時に明示的に無効化 | 言語・設定切替直後の表示一致、古い設定値の残留なし | M-01の計測方式 |
| M-03 | `ModEntry.cpp:1069` `TryHandleHeadless`; `:1871` `TryHandle` | 引数解釈、起動モード選択、実行、終了コード処理を**現在の分岐順序のまま**一覧化。モードごとの起動ハンドラーへ委譲する小さな変更に分割する | 全既存フラグの優先順位・エラー出力・終了コード一致 | 起動モード一覧 |
| M-04 | `NetHitClaims.cpp:520` `Receive`; `:636` `Judge`; `:784` `Park`; `:877` `Tick` | 入力デコード→検証→保留→再試行→裁定の状態遷移図を作成。状態を変更する関数と純粋判定を分離する。`Judge`の返却コードと冪等性は変更しない | 再送・重複・順序逆転・高RTT・同時死亡の既存検証に合格 | 既存UDP回帰テスト |
| M-05 | `NetHitClaims.cpp:1352` `DamageProfile`; `:1390` `SettleDamage`; `:1478` `ApplyOne`; `:1684` `Answer` | ダメージ算出の正本と副作用の発生位置を記録。ダメージ算出を純粋規則へ抽出できるか判定し、適用と結果通知は別の責務として維持 | 武器・チャージ・耐性・Prime・Double Damage・死亡結果が一致 | M-04 |
| M-06 | `NetSmoothing.cpp:107` `Record`; `:163` `RecordTrackedPositions`; `:218` `Tick`; `:268` `FindSnapshotBracket`; `:303` `Sample`; `:368` `SampleForm`; `:410` `SampleTrackedPosition`; `:580` `AckPoint` | **`NetHooks::AfterSimulation()`の送信→Tick→適用順**を固定し、受信履歴→remote replica適用→**Intent/Claim AckFrameとNetUnlagged launch frame**の相関を追跡。wrap・欠損・順序逆転を検査 | リモート補間・追跡物・Intent/Claim/LaunchFrameのGolden一致 | ネットワーク記録 + Claim/Unlagged契約テスト |
| M-07 | `NetSmoothing.cpp:506` `Lookup`; `:526` `LookupForm`; `:599` `Reset`; `:624` `NoteRoomChanged` | Lookupの履歴参照を共通化できる範囲を調査。ルーム変更・再接続・観戦対象変更で古い履歴が漏れないことをテスト | 再接続直後のワープ・残像・異常姿勢なし | M-06 |
| M-08 | `Network/Session/NetSession.hpp:211` 静的`SessionContext`と公開static API | **P3先行**はAPI分類、`SessionContext`寿命表、既存`SessionLifecycle::Ports`の依存図だけ。static facade/ownerの侵襲的変更は、必要性が実証されP4/P6の時刻/権限契約を固定した後の別チケットへ | Dedicated server / client / playbackの役割が不変。`StartHost`拒否も保持。再開始で状態混入なし | P0/P1→P3読取監査。侵襲改修のみM-04～M-07後に再判断 |
| M-09 | `Input/GamepadManager.cpp:46` `GamepadDevice::Snapshot`; `:64` `GamepadManager::Snapshot`; `:87` `Devices`; `:121` `Publish`; `:344` `ClearAll` | Snapshotのコピー量、`Activate`と`UpdateDevice`の`Publish`重複、**`ClearAll`→`ClearDevice`のGate再帰ロック**を確認。mutex置換は再帰性解消の別変更まで禁止 | device選択・通知・切断・再接続・入力遅延の挙動一致 | 単独（ロック改修は保留） |
| M-10 | `DebugLog.cpp:337` 再帰ミューテックス; `:349` `GetState`; `:358` `TeeBuffer::overflow`; `:380` `xsputn`; `:397` `sync` | ロック獲得順と再入経路を表にする。`std::mutex`への置換は再入がないことを証明できた場合のみ。クラッシュ時と終了時のロック順序を確認 | 並列ログ・例外・終了時にデッドロックなし。ログ欠落・順序崩れなし | 単独 |
| M-11 | `MapGen/MapDefinition.cpp:141` `JsonParser`; `:783` `Deserialize`; `:799` `Serialize`; `:844` `ReadMapDefinition` | JSON字句解析、構造解釈、モデル生成、出力を機能群に分類。先に既存JSONのround-tripテストとエラー入力のテストを固定。分割時は文字列・数値の互換性を維持 | 既存マップの読み書きと失敗時エラーが一致 | MapGenのgolden fixture |
| M-12 | `ClassicMenu/MenuLocalization.cpp:35` `FindMenuTextIndex`; `:52` `TranslateMenuIndex`; `:72` `TranslateMenuText` | AOT生成済みテーブルと検索経路の現状を維持。変更するなら索引の一意性・全言語の欠落・日本語グリフ表示の回帰試験を先に追加 | 言語切替と描画一致。ランタイムJSONへの逆戻りなし | 単独 |

### 4.1 ClassicMenu・設定・ローカライズ（M-01 / M-01B / M-02 / M-12）
**ZIP監査で更新した優先実装指示**：`ClassicMenu.cpp:28,370–375`で更新周期は約29.91Hzと確定。`Compose.cpp`の1回のサーバー画面Refreshで文字列5個×9行とPingカラー9個、要約1個の**合計55個の`Servers()`取得経路**がある。`ClassicHost.cpp:161–181`のQt→vector全行変換を毎回呼ぶため、計測前の構造的な重複は確定。ただしCPU時間・割当量は未計測。先に同一ティックで不変のsnapshotを参照する性能チケットM-01を検証し、Hunter画面を跨ぐJoin先固定はM-01Bで別に扱う。`M-02`も7行分のSettings値生成と`row.Live()`を同一更新のみに束ねる条件・即時無効化を固定する。
**担当者の最初の判断**：`Host::Servers()`の`std::vector<ServerRow>`値返しを前提として更新条件を調べ、行ごとに異なる時点の一覧を読んでいないか検証する。**選択時(:502)と実際のJoin(:1102)の間にHunter選択画面が挟まるため、描画snapshotを改善しても選択先の同一性は保証されない**。表示性能の変更と選択先同一性の修正は別チケットとする。既存のAOT翻訳とlazy glyph atlasは保護対象。

#### 関数一覧：Composerの画面構築・イベント・更新
| 実関数（定義行） | 分類 | 具体的な指示 |
|---|---|---|
| `FindModel` (121), `FindText` (139), `NewPage` (157), `Clone` (169), `CloneModel` (187) | PRESENT/ページ構築 | モデル検索・複製を構築時に寄せる |
| `Hot` (262), `Register` (281), `ShowWhen` (287) | PRESENT/イベント登録 | `std::function` の寿命と再登録回数を監査 |
| `SetStretch` (293), `Move` (298), `Remove` (307), `Retext` (319) | PRESENT/レイアウト変更 | 再計測・描画無効化を一箇所に集約 |
| `Build` (403), `BuildServers` (432), `BuildCreate` (531), `BuildSettings` (585), `BuildLobby` (683), `BuildRoom` (764), `BuildManage` (855), `BuildPause` (881), `BuildEnd` (926), `BuildCredits` (988), `BuildMultiplayer` (1198) | PRESENT/画面構築 | ページごとのビルダーとして分離。不要な再生成を測定 |
| `OfflineRows` (1222), `RoomRows` (1248) | PRESENT/表示モデル | 取得時のコピーと更新世代を監査 |
| `GoTo` (1258), `Back` (1265), `PageEntered` (1382) | PRESENT/画面遷移 | 遷移・画面データ更新・副作用を分離 |
| `Type` (1288), `Call` (1303), `OpenKeyboard` (1279) | INPUT/PRESENT | 入力イベントから画面状態変更までを明確化 |
| `BuildServers` 内の `row` ラムダ (479)、表示用 `AddLive` ラムダ (486～494) | PRESENT/動的表示 | **実コードで同一の `_host->Servers()` 取得が複数表示コールバックから発生**。値返しの既存契約を維持しつつ、更新世代・コールバック寿命・一貫性を確定した後に1回取得へ変更 |

#### 関数一覧：MenuLocalization（AOT・言語切替）
| 実関数（定義行） | 分類 | 具体的な指示 |
|---|---|---|
| `ParseMenuLanguage` (28), `ColumnFor` (21) | RULE/言語設定 | 言語IDと列索引の正本を維持 |
| `FindMenuTextIndex` (35), `TranslateMenuIndex` (52), `TranslateMenuKey` (58), `TranslateMenuText` (72) | PRESENT/AOT参照 | **既存AOTカタログを維持**。実行時JSON解析へ戻さない |
| `MenuLocalizationCatalogValid` (79), `MenuLocalizationEntryCount` (86) | OBS/検証 | 起動時・テスト時に検証。毎フレームの再検査は避ける |

#### 実装の根拠：AddLiveの反復取得経路
**確認箇所：** `ClassicMenu/Compose.cpp:249-287, 384-403, 432-490`、`src/MphRead.Native.Qt/Shell/ClassicHost.cpp:161-200,405-451`、`PlayModel.hpp:62`。

- `Composer::AddLive()` は `std::function<std::string()>`、`Hot()` / `Register()` は `std::function<void()>` を受ける。
- サーバー一覧の行で `const auto row = [this, r] { return _host ? _host->Servers() : std::vector<ServerRow>{}; };` というラムダを作っている（479行）。`Host.hpp:100`では`std::vector<ServerRow>`値返し。**実装元の`ClassicHost::Servers()`（Qt側:161～181行）も確認済み**：`PlayModel::Servers()`から全`QVariant`を走査し、`QVariantMap`の`name/map/mode/players/ping`を取り出して`ServerRow`を都度構築する。これは**呼び出しのたびに一覧全体の変換がある**という確定事項。Qt側の`QVariantList`自体は暗黙共有による軽いコピーの場合があるため、Qtコンテナコピーと全行変換コストは混同しない。実際の割当量・時間は未測定。
- `std::function` 自体は必ずヒープ確保するわけではない。問題は型消去という語ではなく、**呼び出し頻度、戻り値の所有権、変更通知の有無**。

**修正計画：** `Servers()` の定義は**`ClassicHost.cpp`まで確認済み**。呼び出し頻度、行ごとの表示更新回数、`QVariant`→`std::string`変換・vector構築回数を追跡。サーバー一覧のスナップショットを一回取得して同じ世代の行で共有する案を検証。スナップショットの寿命を画面更新単位に限定し、非同期更新・行削除時のダングリング参照を防ぐ。`std::function` はイベント境界に残し、毎フレーム呼ばれる部分だけ計測後に変更する。

**ゲート：** サーバー一覧の更新、言語変更、ウィンドウ再描画、ソート・選択、切断直後の一覧更新、コールバック寿命。コピー削減の数値を報告する。

#### 安全性の根拠：スナップショットの寿命
`_host->Servers()` の戻り値を共有する設計へ変える場合、更新中のスナップショットが非同期UIコールバックに捕捉されると、参照切れや古い表示を招く。まず呼び出し回数・コピーサイズ・更新世代を計測し、更新単位で一貫した値を読む仕様を確立する。安易に `const&` を返す変更は不可。

#### 保護すべき既存設計：AOTと遅延アトラス
**確認箇所：** `ClassicMenu/MenuLocalization.cpp:28-86`、`MenuUnicodeFont.cpp:17-107`。

- ローカライズ索引は生成済みデータを二分探索しており、実行時JSON解析への置換は不要。
- Unicodeグリフは初回利用時にアトラスへ追加し、K8x12L内のグリフは生成済みビットマップ、未収録グリフのみQtフォールバックで描画する。
- 256×256ページの追加は必要時のみで、既存UVを保持する設計。これは `Pay for What You Use` の良い実装例。

**修正計画：** フォントや翻訳の共通化は表示幅・折返し・アトラス無効化の正しさを優先する。全グリフの起動時ロードは実測で優位と確認できない限り行わない。既存最適化をリファクタリング対象という理由だけで撤去しない。

#### コード行と確認状況：Servers()の呼び出し
参照：[`Compose.cpp` 479～519行](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/ClassicMenu/Compose.cpp#L479-L519)

- 479行の `row` ラムダは `_host->Servers()` を呼び、空の場合は `std::vector<ServerRow>{}` を返す。
- 486、488、490、492、494行の各 `AddLive` コールバックは、それぞれ `const auto all = row();` を実行する。1行当たり少なくとも5箇所の独立した一覧取得経路が存在する。
- 501、514、519行にも `_host->Servers()` の呼出しがある。戻り値型は`Host.hpp`で`std::vector<ServerRow>`値返しと確認済み。呼出しに伴う返却値の構築は発生するが、move/NRVO・Host実装依存のため**内部コピー数・割当量・時間は未測定**。実行頻度は`Composer::Refresh()`の呼び出し周期を計測する。
- **推奨設計**：一覧更新単位で不変の `ServerListSnapshot` を作り、同一表示更新内ではそのスナップショットを参照する。非同期更新中の寿命を保証し、行別の表示はスナップショット内インデックスで行う。`const&` 返却への単純変更は禁止。
- **検証**：`Servers()` 呼出し回数／一覧コピー回数／割当バイト／1回のUI更新コストをbefore/afterで測定。更新中にサーバーが増減しても不正参照・表示の混在がないことをテスト。

#### 実行手順：サーバー一覧・設定・言語の変更単位
##### P8-01 `Compose.cpp`のサーバー一覧取得

**確認済みコード**：479行付近の`row`ラムダが`_host->Servers()`を取得し、486～494行付近の複数`AddLive`コールバックがそれぞれ`row()`を呼ぶ。501/514/519行付近にも一覧取得がある。

**実装前に確認すること**：`Host.hpp:100`の値返しと`ClassicHost.cpp:161～181`の全件変換、さらに`ClassicMenu.cpp:28,370～375`の`Tick()`→`Composer::Refresh()`（約29.91Hz）は**ソースで確認済み**。残りはQtモデル更新通知・スレッド境界、**実際の割当・処理時間**。`BuildServers()`には9行×6+要約1の55個の反復`Servers()`取得経路があるが、計測での実行回数・CPU時間は未取得。

**変更の順序**：

1. 現行1画面更新当たりの`Servers()`呼出し回数、copy bytes、alloc countを計測。
2. **同一UI更新世代**に限定したimmutable snapshotを1回取得し、各行のテキスト・Ping・人数をそこから生成。別更新世代で古いsnapshotを再利用しない。
3. snapshotの所有者を画面モデルへ置き、`AddLive`ラムダには寿命保証された値または安全な参照のみを渡す。非同期更新中のdata raceを作らない。
4. server一覧が更新された場合はrevision/dirtyで必要な行だけ無効化。過度なキャッシュ層を追加しない。**indexの別画面間持越し問題はこのキャッシュ最適化の範囲外**。
5. server join/refresh/sort/filter/切断/ページ遷移/画面破棄の回帰試験を実行。特に「一覧で選択→Hunter選択中に一覧が変更→Join実行」を**現行仕様の観察テスト**として作る。表示最適化ではこの挙動を意図せず変更せず、Join先の安全な固定には安定したendpoint／サーバー識別子をHost側まで伝える別仕様チケットが必要か判定する。

**受入**：表示内容と更新鮮度は不変、`Servers()`の同一世代重複取得を解消、allocとCPUを前後比較、dangling captureなし。**indexだけでJoin先の同一性が保証されるとは主張しない**。現行の2画面遷移中に一覧が変わった場合の動作を記録し、必要な修正はUI-JOIN-02へ分離。

##### UI-JOIN-02：行選択からJoinまでの同一性（別の仕様修正・先行実装禁止）

**確認済み**：`Composer::BuildServers()`内の行クリックは `index` を `_vars["join"]` に格納する（`Compose.cpp:497～504`）。その後 `HunterJoinPage` のOKで `_host->Join(_vars["join"], _vars["hunter"], _vars["suit"])` を呼ぶ（`:1095～1107`）。`Host` は `Join(int row,...)` APIで、`ServerRow`に安定したendpoint IDはない。**時間差を跨いだindexの同一性保証は、現在のAPIからは証明できない**。これは存在する実装からの設計リスクであり、実害再現をしたという主張ではない。

1. **ソースで確認済み**：`ClassicHost::Join()`（`ClassicHost.cpp:188～199`）はOK時点の`PlayModel::Servers()`から`servers[row].toMap()["endpoint"]`を選択する。`PlayModel::Servers()`は`QVariantList`値返し（`PlayModel.hpp:62`）。**選択時とJoin確定時に別の一覧を読む**ことが実装上明白であり、indexの同一性は保証されない。次に非同期一覧更新が2画面間に入り得る実行経路とタイミングをテストする。
2. 基準テスト：Aを選択後、Hunter選択画面表示中に一覧の先頭へBを追加/並び替え/削除し、OKでどちらへ接続を要求するか記録。
3. **M-01は「1 Refresh単位の読取回数削減」だけ**を行う。2画面を跨いだ接続先固定は、既存バグであれば別チケットで設計・回帰テスト・機能修正として扱う。
4. 将来的にstable ID/endpointを導入する場合は`Host`契約と非同期一覧の世代、失効時のUI文言、接続拒否時の動作まで含めて設計する。現在のindexを単にキャッシュするだけの解決策は採用しない。


##### P8-02 Settingsの同種パターン

`Compose.cpp`の`_host->Settings(_section)`と`rowAt`の呼出し関係も同様に計測。**`ClassicHost::Settings()`（`ClassicHost.cpp:424～449`）は毎回`SectionRows(section)`を通して表示対象を抽出し、各行のID・ラベル・値を変換する**。さらに`key/pad`表示では`row.Live()`を評価するため、長寿命キャッシュでは動的入力表示を止める恐れがある。サーバー一覧と違い、編集直後の値反映が重要なので、**同一`Composer::Refresh()`更新内にだけsnapshotを共有**し、次の更新で必ず最新値を取得する方式を先に比較する。

##### P8-03 既存ローカライズ資産の保護

`MenuLocalization.cpp`のAOT catalogと`MenuUnicodeFont.cpp`のlazy atlasを保持。生成ヘッダーはGit管理対象でなくCMakeの`${CMAKE_CURRENT_BINARY_DIR}/generated/classicmenu/`へ出力。既存CTest `FruityPrime.MenuLocalization`を使う（`BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING`で生成されるためAndroidクロスビルドでは同テストが登録されない）。Classicと通常SETTINGSは**同じMods menu language設定**を読み、ゲーム本体の言語とは独立。文字幅計測、word-wrap、全角句読点、glyph code、k8x12LスケールをGolden化。JSONの毎フレームparseや手動glyph列挙へ後退させない。

#### 着手チケット UI-SERVERS-01：具体的な試験・受入条件
**場所:** `Mods/ClassicMenu/Compose.cpp` `Composer::BuildServers()` 432～529行、特に479～520行。

**確認した実装:** `row = [this,r]{ return _host ? _host->Servers() : std::vector<ServerRow>{}; }`。各行について名前・Arena・Mode・Players・Ping文字列・Ping色の6つの独立した`AddLive`コールバックが`row()`を呼ぶ。9行表示なら**全コールバックが評価された場合、最大54回の一覧取得**が起こり得る（更新頻度と実際の呼び出し回数は未計測）。行選択、ページ送り、件数ラベルも別途`Servers()`を呼ぶ。`Servers()`が`std::vector<ServerRow>`値返しなのは`Host.hpp:100`で確認済み。ただし、**54回という値は6コールバック×表示9行がすべて実行された条件付き上限例**であり、Hostの内部コピー／allocの実測回数ではない。

**担当者の手順:**
1. `ClassicMenu/Host.hpp`では値返し確定。Host実装を調査して同期、変更通知、値を構築する場所、メニュー側の`Refresh()`周期を確認する。
2. `AddLive`評価を呼び出す描画更新経路を特定し、同一表示更新内で`Servers()`が何度呼ばれるかカウンタで計測する。
3. Hostの公開契約を保ちつつ、**1回の表示更新につき1つの整合した一覧スナップショット**を作る。表示用の各コールバックはそのスナップショットを参照する。ページ番号は更新時に確定させる。古い参照・ポインタを非同期更新越しに保持しない。
4. 行選択時は古い行番号が別サーバーを指す可能性を検証する。**現行`ServerRow`には安定IDがなく、`Host::Join(int row, ...)`はindex方式**。安定IDによる再照合を指示するだけでは実装できないため、表示世代を維持してJoinまで同じ一覧を参照する方法、または非互換を避けた再検証方法を先に決める。**サーバー一覧の所有権や公開APIを先に変更しない。**
5. 空一覧、9/10/18件境界、ページ切替、更新中の行削除、Ping変動、クリック時の再配列をテストする。

**合格条件:** 同一更新内の一覧取得回数が原則1回（クリック等の別イベントを除く）。表示整合性とJoin対象の一致。計測前後でアロケーションと更新時間を比較する。`std::span`や参照をスレッド境界越しに延命しない。

### 4.2 起動・診断CLI・CMakeの製品境界（M-03）
**ZIP監査追加**：`ModEntry.cpp:1069`のHeadless分岐と`:1871`の通常分岐はそれぞれ約800行。`mods_cli_flag_index.tsv`で定数文字列引数の使用位置を抽出したが、マクロ展開・間接ヘルパーの全フラグを保証するものではない。分岐順序・exit code・ゲームファイル未取得時の`77` skip・Headlessのコンソール出力と例外経路を、修正前fixtureで固定する。`CMakeLists.txt:195–202`の`Testing/Test*.cpp`除外は維持し、`Mods/Network/Testing`の本番ライブラリ収集はCLI到達性を確認してから選別する。**Protocol30～32の赤Golden修正(P0-NEW-01)が基準試験を作るうえで先行条件**。
**最初に行うこと**：CLIコマンドと起動優先順位の一覧・診断コマンド到達性を作る。`Network/Testing` の一括除外は行わない。

#### 関数一覧：ModEntryのBOOT・RULE・OS依存
| 実関数（定義行） | 分類 | 具体的な指示 |
|---|---|---|
| `IsAsciiWhitespace` (191), `TrimNumberWhitespace` (196), `TrimAscii` (209), `TrimStartHyphen` (214) | RULE/BOOT | 引数解析用の純粋な文字列処理として集約。既存呼出しとの互換を維持 |
| `IsFlag` (223), `HasFlag` (228), `IndexOfFlag` (240) | BOOT | オプション探索の責務。多重走査の実測がなければマップ化しない |
| `TryParseFloatingClassic` (293), `TryParseDoubleCurrent` (331) | RULE/BOOT | 旧形式・現行形式の違いを保存。安易な統一は禁止 |
| `TryParseGameMode` (335), `TryParseHunter` (344), `TryParseBeam` (353) | BOOT | 列挙型解析。無関係な初期化から分離 |
| `GetCommandLineArguments` (362), `ParseSize` (561), `ParsePort` (590), `ParseMasterPort` (601), `ParseName` (612), `ParseHunter` (618), `ParseRecolor` (625) | BOOT | 引数取得・構文解析・設定反映を3段階に分離 |
| `ApplyRenderOverrides` (636) | BOOT/設定 | 設定値の解釈と描画APIへの副作用を別段階に分離 |
| `AppBaseDirectory` (438), `MachineName` (455) | BOOT/Platform | OS依存情報の取得を境界に閉じ込める |
| `DotNetUtf16Length` (507), `LeftAlign` (549) | RULE/互換 | C#互換の文字数・表示幅仕様を回帰テストで固定 |
| `ServerUsage` (812), `RunUiCapture` (934), `RunUiBench` (950), `RunUiDesigns` (984), `RunShellCapture` (996), `RunTapCheck` (1024), `GenerateThumbnails` (1034) | BOOT/診断 | 起動モードごとに入口を分離。通常ゲーム時は診断を初期化しない |

#### 実装の根拠：CMake再帰収集とTesting入口
**確認箇所：** `CMakeLists.txt:51-72, 112-125, 195-198, 238-293`。

- `fruity_collect_owner()` が `*.cpp` / `*.hpp` を再帰収集する。
- `Testing/Test*.cpp` の除外処理は存在するが、`Mods/Network/Testing` の通常の `.cpp` は同じ収集規則の範囲内にある。
- `-netcheck`、`-netcombatcheck`、`-powerupsimtest` 等の実行コマンドが `ModEntry.cpp` から検査コードを呼び出している。したがって「Testingという名前だから全部本番から削除」は不可。

**修正計画：** 実際のリンクマップ・シンボル・実行コマンド参照を収集し、①製品必須、②開発時だけ、③独立テスト実行ファイル、④未到達候補に分類する。CMakeターゲット分割前に配布ビルドのオプション互換とテスト入口を決める。`--gc-sections` / `/OPT:REF` 等によるリンク時除去を前提としたサイズ測定も必要。

#### コード行：ビルド対象とTestingの区分
参照：[`CMakeLists.txt` 52～53、490、1348行](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/CMakeLists.txt)

- 52行で `file(GLOB_RECURSE _sources CONFIGURE_DEPENDS "${owner_root}/*.cpp")` を利用する。
- 490行に `Mods/Network/Testing/Harness/FreezeTimeline.cpp` の明示的参照がある。
- **注意**：`GLOB_RECURSE` の存在だけでは、Testingの全コードが製品の最終リンクへ入るとは証明できない。`owner_root` の呼出し側、ターゲットのソース一覧、リンクマップ、セクションGCを追跡して判定する。
- **検証**：各ターゲットの実コンパイル単位、生成バイナリへの到達性、CI実行対象とコマンドライン検査機能を列挙し、検査機能を破壊しない範囲で分離する。

#### 実行手順：ModEntryとビルドターゲットの分離
- **P2-01 呼出し一覧**：`ModEntry.cpp`内の全関数を `boot/args/config/window/network/renderer/headless/testing/shutdown` に分類し、入口ごとに初期化順・cleanup順を表にする。
- **P2-02 入口は薄く**：`ModEntry`は実行モード決定、依存生成、主ループへの接続、shutdown順序のみを担当。新設する実装は`Mods`内の既存ownerに寄せる。機能本体を巨大な新しい`AppManager`へ移動しない。
- **P2-03 テスト境界**：`CMakeLists.txt`の再帰収集は維持可能だが、`Network/Testing`や`Diagnostics`の登録とproduction起動経路を分離。テスト実行入口がproductionバイナリから必要なら、**link targetと実行時初期化**を分けて設計する。単に`Testing`を除外して既存CLI検証を壊さない。
- **P2-04 完了**：CLIフラグ・既定起動・headless・dedicated server・GUIのすべてが従来通り。通常起動の不要初期化が増えない。リンクシンボルと起動時allocを比較。

#### 着手チケット BOOT-ENTRY-01：引数解析とディスパッチ
**場所:** `Mods/ModEntry.cpp` `IsFlag/HasFlag/IndexOfFlag` 223～240行、`TryParseFloatingClassic` 293行、`ParseSize/ParsePort/ParseMasterPort/ParseName` 561～612行、`ApplyRenderOverrides` 636行、`TryHandleHeadless` 1069行、`TryHandle` 1871行。

**担当者の手順:** 引数文字列→解析済み設定→モード選択→副作用実行の4層に分類。`TryParseFloatingClassic`など互換パーサーは「一般的な標準関数」へ置換せず、文化圏・桁区切り・エラー時既定値をGolden化。まず副作用なしのパーサーを抽出し、次に各モードの起動処理を独立CPPへ移動。`-server`、`-connect`、`-headless`、診断、UI capture、thumbnail、Androidを回帰。CLI優先順位・終了コード・ログを変えない。

### 4.3 Gamepad・Input・操作設定（M-09）
**変更時に守ること**：`UpdateDevice`は`recursive_mutex`の内側で状態更新し、イベント通知はロック外。通知の位置とrevision更新回数を変えない。

#### 関数一覧：GamepadManagerの所有・選択・公開状態
| 実関数（定義行） | 分類 | 具体的な指示 |
|---|---|---|
| `GamepadDevice::Snapshot` (46), `GamepadManager::Snapshot` (64), `ActiveState` (70), `ActiveDevice` (75), `Devices` (87) | INPUT/スナップショット | 値コピー量と取得頻度を実測 |
| `GamepadManager::Find` (98), `Activate` (110), `Publish` (121) | INPUT/選択・配信 | デバイス所有権と通知の責務を分ける |
| `ReplaceRuntime` (222), `RefreshProfiles` (237) | INPUT/設定寿命 | 再構築を入力フレームごとに行わない |
| `StickActivity` (248) | RULE/入力判定 | 純粋規則の単体テスト候補 |
| `SelectDevice` (254), `ClearDevice` (287), `RemoveDevice` (308), `ClearAll` (344) | INPUT/デバイス寿命 | ホットプラグ・切断・再接続の順序を固定 |

#### 実行手順：入力経路と設定の責務分離
- 入力取得（OS/RawInput/Qt/Android）→正規化→device arbitration→aim style→ゲーム用intent→送信を追跡する。
- Classic DSの30Hz規則とModern 60Hz規則を共通化しすぎない。**共有するのは値変換・検証可能な数学規則のみ**。
- wheel履歴リセット、Alt+Tab key-up、Morph Shift boost、touch swipe、gamepad zero-input facing、設定保存/単回overrideを固定テスト化。
- input hot pathでは文字列生成・vector再確保・ログを排除する前にalloc/frameを実測。入力イベントを遅延batch化して体感遅延を増やさない。

#### 着手チケット INPUT-GAMEPAD-01：ロックと通知
**場所:** `Mods/Input/GamepadManager.cpp` `Activate()` 110行、`Publish()` 121行、`UpdateDevice()` 130行付近、`ReplaceRuntime()` 222行、`RefreshProfiles()` 237行。

**実装から確認:** `UpdateDevice`は`std::recursive_mutex Gate`内で端末探索/追加、軸の有限値正規化、スティック・トリガー補正、ボタン判定、Active切替、`Publish`、revision更新を実施。`ActiveChanged.Invoke()`と`DeviceAdded.Invoke()`は**ロック解放後**に呼ばれている。これは再入・UI通知の安全性に重要。`Publish`は現在のRuntimeConfig/Profilesとスナップショットを更新する。

**担当者の手順:** まず`Publish()`の呼び出し元を全列挙し、`Activate()`内部と`UpdateDevice()`末尾で二重呼び出しが起こる条件をトレースする。変更するなら、外部通知がロック外にある保証を保ったまま、1更新に1回のPublishを目指す。revisionの増加回数、端末切替、ProfileName、Fallback、InputSourceTrackerの挙動を前後比較する。`recursive_mutex`を`mutex`に変えるのは再入グラフ確認後だけ。

**試験:** ホットプラグ、接続断、同一端末の更新、複数端末、手動選択、未マップ端末→マップ端末、入力ソース切替、trigger閾値、校正、デバイス追加通知での再入。

**GamepadManagerの追加確認（変更指示）**：`GamepadManager::ClearAll()`（344行）が `Gate` を取得したまま `ClearDevice()`（287行）を呼び、後者が同じ `Gate` を取得することを確認。`recursive_mutex`には**実際の入れ子取得という理由**がある。まず`ClearDeviceLocked()`等の非公開ロック済み操作へ分割する場合の通知・revision順序をテストし、**別の変更で**通常mutex化を検討する。`UpdateDevice()`（121～220行）内では選択変化時に `Activate()`→`Publish()`、その後に無条件 `Publish()` が呼ばれるため、**Publish二重実行の静的経路が存在**。ただし`NoteActive()`の副作用とデバイス状態同期を調べる前に、単純に一方を消してはいけない。`LastInputDevice()`は無ロックで`const optional<string>&`を返すため、実際の別スレッドからの読み出し有無と文字列寿命もP0-03で確認する。


### 4.4 NetSession・Server Authority・所有権（M-08）

#### 関数一覧：NetSession APIと状態の寿命
| 実関数（宣言行） | 分類 | 所有状態・方針 |
|---|---|---|
| `Role` (43), `Active` (44), `IsHost` (45), `IsClient` (46), `IsServer` (47), `IsAuthority` (122) | STATE/権限照会 | `Role` と `IsAuthority` の意味を区別。ホストをAuthorityと誤認しない |
| `LocalSlot` (49), `NetFrame` (50) | STATE/時間・識別 | フレームの正本と所有者を文書化 |
| `LastSnapshotFrame` (51), `SnapshotArrived` (52), `AppliedSnapshotFrame` (73), `SnapshotsOutOfOrder` (139) | STATE/複製 | 受信・適用・順序を別概念として保持 |
| `SnapshotsReceived` (64), `SnapshotsSent` (65), `StatesApplied` (66), `IntentsReceived` (67), `IntentsSent` (70), `IntentBytesSent` (71) | OBS/通信計測 | 通信メトリクスをまとめる。通常更新経路に余分な割当を増やさない |
| `StartServerAuthority` (75), `StartHost` (76), `StartClient` (77), `StartPlayback` (80), `RewindPlayback` (81), `InjectPlaybackPacket` (82) | STATE/起動・再生 | 入口を保ちつつ所有オブジェクトの寿命を明示 |
| `LocalHunter` (88), `SetLocalHunter` (89), `LocalColor` (90), `SetLocalColor` (91), `PlayerName` (129), `SetPlayerName` (130) | STATE/ローカル設定 | プレイヤーID設定とネット同期を混同しない |
| `ConnectionLost` (102), `ReAnnouncements` (104), `LongestServerSilence` (105), `Refused` (108), `RefusedReason` (109) | STATE/接続健全性 | 接続監視とゲーム規則の境界を明示 |
| `AuthorityStandDowns` (106), `AuthorityFrames` (107) | OBS/権限状態 | 権限の事実と診断値を分ける |
| `ServerMatch` (121), `ServerSession` (146), `LobbyMessage` (162), `LobbyCommandPending` (163) | STATE/ロビー | セッション、試合、ロビーを独立した所有単位として整理 |
| `_context` (211) | STATE/静的実体 | **関数ではなく静的データメンバー**。全ファサードの依存点。初期化・破棄・並行アクセスを監査 |

#### 実装の根拠：静的ファサードとSessionContext実体
**確認箇所：** `Network/Session/NetSession.hpp:33-83, 145-162, 194-211`、`NetSession.cpp:50-83`、`SessionContext.hpp:22-39`、`SessionRuntimeState.hpp:13-39`。

- `NetSession` は現行設計でインスタンス生成を許さない静的APIで、`static SessionContext _context` を保持する。`Role()`、`Active()`、`IsServer()`、`LocalSlot()`、`NetFrame()`などは静的アクセスである。
- 一方で `SessionContext` 内は接続、ランタイム状態、通信量、コールバック、プレイヤー識別、権威状態、レプリカ、ロビー状態に**すでに分割済み**。つまり「何も分割されていない巨大な神クラス」と判断するのは誤り。
- `NetSession::ClientId` / `Secret` は静的初期化される。これらのプロセス寿命と、試合・接続ごとにリセットする値の寿命を分けて監査する必要がある。
- `SessionLifecycle::Ports` は既存の依存束を組み立てている。新たな汎用DIコンテナは導入せず、これを起点に依存の方向を整理できる。

**修正計画：** まず `SessionContext` のフィールドごとに「プロセス / 接続 / ルーム / 試合 / プレイヤー / フレーム」の寿命表を作る。次に `NetSession` の呼び出し元を列挙し、純粋なルール層からの静的参照を優先して除去する。複数セッションを同一プロセスで動かす要件を明示するまでは、全APIの一括インスタンス化はしない。静的ファサードの置換は現段階では提案のみとし、正しさ・性能・複数セッション要件に実測根拠がある箇所に限り、戦闘とACK時刻の回帰ゲートを固定してから別チケットで移行する。

**ゲート：** サーバーとクライアントの役割が混在しない、接続の切断・再接続・試合遷移・リプレイ巻戻しで状態が漏れない、通信形式が変化しない。`NetRole::Host` が残ることとホストAuthorityが有効であることは同義ではない。**確認済み**：`SessionLifecycle::StartHost()`は`logic_error("client-host simulation is no longer supported")`を送出し、`StartServerAuthority()`は`NetRole::Server`を設定してauthorityを獲得する。残存Host enum/公開APIを現行の有効な起動モードと誤認しない。実GUIの hosted 選択が専用サーバーを起動する経路は別に確認する。

#### 判断条件：staticの全廃を目的化しない
APIが静的であることと、グローバルに共有される可変状態が問題であることは別。先に各公開APIを **(a) 読み取り専用状態参照、(b) コマンド、(c) ライフサイクル、(d) 通信I/O、(e) 診断** に分類し、依存の向きと再入・同時複数セッション要件を記録する。テスト可能性・プロセス分離・所有権の実害が確認できる部分から置換する。インラインの読み取りAPIを仮想関数化するような変更は、ゼロコスト抽象化に反する可能性があるため避ける。

#### コード行：static SessionContextとAuthority
参照：[`NetSession.hpp` 43～82、122、211行](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Session/NetSession.hpp)

- 211行で `static SessionContext _context;` を保持し、43行以降の公開静的APIが内部のランタイム、レプリカ、統計へアクセスする。
- 75行の `StartServerAuthority`、122行の `IsAuthority` はセッションの責務と権限を明示する重要なAPIであり、単なる「静的＝悪」と判断しない。
- **推奨設計**：セッション状態の作成・終了・再起動時のライフサイクル表を先に作る。インスタンス化は実際に必要な依存境界から進める。サーバー／クライアントのauthority判定を曖昧にするような移行は禁止。
- **検証**：サーバー起動→終了→再起動、接続→切断→再接続、ホスト離脱、複数部屋のプロセス隔離、ネットワークテストの再実行。

#### 実行手順：セッションownerの段階的な導入
1. **P3-01 読取専用整理**：`NetSession.hpp`の静的APIと`SessionContext`の所有メンバーを1対1で対応付け。`IsAuthority()`等の判定の呼出し先とserver/clientでの意味を記録。
2. **P3-02 明示的owner（条件付き・別変更セット）**：現在の`SessionContext`と`SessionLifecycle::Ports`の既存注入を優先利用し、具体的なテスト困難・状態漏れ・複数セッション要件が示された場合に限り、`ClientSession`相当の所有者がクライアント通信状態を保持。Dedicated serverのmatch/room/authority stateはserver ownerのまま。**shared singletonとして統合しない**。
3. **P3-03 移行用アダプタ**：既存`NetSession::`呼出しを一気に書換えず、生成時に明示されたsession参照を受け取る層から段階的に移行。静的ファサードは移行期間中の互換入口に限定。
4. **P3-04 shutdown順**：受信停止→worker/coroutine停止要求→join/完了待機→observer解除→scene依存解放→session破棄、の依存関係を現行実装に照らして検証。callbackはowner破棄後に呼ばない。
5. **P3-05 反復試験**：同一プロセスの接続/切断100回（環境に応じて時間制限を明記。**`StartHost`起動は含めず、専用サーバー+クライアントの現行経路を使用**）、room change、再起動、spectate、AI client、異常終了。stale slot/epoch/handlerを監視。
6. **完了**：静的状態の残存理由がすべて説明可能で、server authorityとclient predictionは混線しない。複数sessionが実際に必要かは実使用要件で判断し、抽象化自体を目的にしない。

#### NET-SESSION-01：Authorityと寿命の保全仕様
**場所:** `Mods/Network/Session/NetSession.hpp` `Role/Active/IsHost/IsClient/IsServer` 43～47行、`IsAuthority()` 122行、`Pump()` 174行付近、`SessionContext _context` 211行。

**実装から確認:** `IsAuthority()`は`IsServer() && _context._authorityState.IsAuthority()`。`SessionLifecycle::StartHost()`は現在必ず例外を送出し旧client-host simulationを拒否する。`StartServerAuthority()`のみが確認した明示的Server起動経路。したがって、**現行コードのこの関数については「ホスト＝Authority」と解釈してはいけない**。`StartHost()`のシグネチャーは現存するが現行`SessionLifecycle`では無効（例外送出）である。Static facadeの内側に`SessionContext`があり、状態自体は多数の下位オブジェクトに分割されている。

**担当者の手順:** (1) `StartServerAuthority/StartHost/StartClient/StartPlayback/Reset`の呼び出し順と各Contextの生成・破棄を図示。(2) 同一プロセス内に複数Sessionが本当に必要か、サーバー別プロセス構成と突合。(3) テスト容易性や終了時寿命に具体的な問題がある場合のみ、`SessionContext&`を下位に明示注入。(4) `IsAuthority`、通信プロトコル、プレイヤーの非Authorityを変更しない。Static APIの一括置換は禁止。

**試験:** dedicated server、hosted room、remote client、rejoin、room rotation、authority epoch、server終了、replay、複数プロセスUDP。

### 4.5 HitClaim・戦闘・ダメージAuthority（M-04 / M-05）
**ZIP監査追加**：ClaimDamagePolicy、HitClaimRateLimiter、HitClaimMetrics、HitClaimDeathState、RescuedHitLedgerの5個別C++試験でPASSを確認。`NetHitClaims.cpp:182–195`にはOutbox/Inbox/ReplayLedger/AuthorityLedger/MuzzleHistory/DeathState/RateLimiter等の**14個の静的メンバー定義**があり、分離済みのオブジェクトを再び単一巨大クラスへ統合しない。`Judge`で使う`MaxDamageFor`は`raw*5+1`の**ゆるい申告ガード**、`SettleDamage`はAuthority側の確定であることを区別する。実UDPを含む本体フル試験は未完了。

#### 関数一覧：NetHitClaimsの裁定・再送・結果適用
| 実関数（定義行） | 分類 | 具体的な指示 |
|---|---|---|
| `Compose` (310), `Receive` (520), `ApplyVerdicts` (335) | CODEC/通信境界 | wire互換と重複受信耐性を保持。パースと裁定を分離 |
| `TickOutbox` (319), `ResendInterval` (213), `Park` (784) | STATE/再送・保留 | 冪等性と再送期限を同じ規則で管理 |
| `GraceFor` (196), `GraceFrames` (203), `MaxClaimAge` (208), `NearestLedgerOffset` (423) | RULE/時間検証 | RTT・AckFrame・期限の正本を明示 |
| `Claiming` (218), `Arbitrating` (456), `Judge` (636) | AUTH/権限判定 | ローカル申告とサーバー裁定を厳密に分ける |
| `MuzzleObstructed` (489), `ImpactPlausible` (990), `CauseVouched` (1426) | AUTH/妥当性検証 | 物理・射線・原因の検証を独立した規則に分離 |
| `Seen` (626), `Remember` (631), `ClearLedger` (446) | STATE/重複排除 | 発射ID・ライフID・世代を含む鍵を維持 |
| `RecordIntent` (1021), `CurrentClaimLaunch` (451), `FireFrameOf` (509), `FiredFrame` (1322), `RoleFrameOf` (1327) | STATE/発射履歴 | 発射フレームとロールフレームを混同しない |
| `MaxDamageFor` (860) | AUTH/申告上限ガード | **`raw * 5 + 1`の緩いガードであり最終確定ダメージではない**。`DamageProfile`/`SettleDamage`と混ぜない |
| `DamageProfile` (1352), `SettleDamage` (1390), `DoublingSettled` (1339), `DoublingHold` (1345) | AUTH/ダメージ確定 | 武器・チャージ・Double Damageの既存Policyを正本として再利用 |
| `TrackDeaths` (1247), `NoteRescued` (1264) | AUTH/ライフサイクル | 死亡・救済とclaim結果の順序を固定 |
| `DescribeAgreement` (757) | OBS | 合意状態の診断を裁定の副作用にしない |

#### 実行手順：戦闘規則と副作用の正本化
- **P4-01 正本マトリクス**：`weapon/charge/affinity/Prime/DoubleDamage/fireFrame/ackFrame/target/impact`のproducer、transport、authority resolver、observerを記録。
- **P4-02 規則と副作用を分割**：既存`WeaponVariant`、`WeaponCharge`、`ClaimDamagePolicy`等の規則を優先再利用。純粋判定を抽出し、ledger更新・packet送信・VFXを別責務にする。
- **P4-03 NetHitClaims段階分割**：claim生成→送信/retry→authority検証→verdict→prediction retirementを順に切り分ける。各段階で全UDP fixtureを実行。
- **P4-04 特例固定**：Shock Coil継続tick、追尾弾、Morph+Fire同時、Prime handover、respawn直後、観戦、turretのイベント順をテストする。
- **禁止**：client申告のdamageを最終値として採用すること、射撃者優先の仕様を無断で変えること、protocol bumpをリファクタリングに混ぜること。

#### 既存の規則と検査ターゲット（確認済み）
- `CMakeLists.txt`の`FruityPrime.ClaimDamagePolicy`（`Testing/TestClaimDamagePolicy.cpp`）で確定ダメージ規則を検証する。
- `Testing/TestHitClaimOutbox.cpp`はclaim ID、payload、再送間隔、期限、キュー満杯時の挙動、ForgetSlotを検証している。分割時はこれらの観測値・順序を固定する。
- `FruityPrime.NetworkProtocol33Golden`（`tools/testing/TestNetworkProtocolGolden.cpp`）はプロトコル33のGolden基準として利用できる。これは今のC++ワイヤ正本に属するため、C#旧版形式へ巻き戻さない。
- `NetHitClaims::MaxDamageFor`の判定は“申告値があり得ないほど大きい”ことを排除する第一段階で、`SettleDamage`の最終確定値とは別。閾値を厳格化して従来通るclaimを拒否するのはリファクタリングではない。

#### NET-CLAIM-01：受信から裁定までの状態遷移と分割順序
**場所:** `Mods/Network/Gameplay/Claims/Hit/NetHitClaims.cpp` `Receive()` 520行、`Seen()` 626行、`Remember()` 631行、`Judge()` 636行、`Park()` 784行、`Tick()` 877行、`ApplyOne()` 1478行、`Answer()` 1684行。

**実装から確認:** `Receive`は、Authority限定チェック→パケット件数・境界検証→ストリーム/攻撃者のLife検証→被弾者Life検証→ClaimIdの既処理判定→ショット未到着への保留→即時裁定→保留枠不足時の`ResultBusy`確定、の順。既処理かつ`ResultPending`なら応答を保留し、確定済みなら過去の結果を再送する。`Judge`は時刻、LaunchFrame、Syluxのマズルガード、最大ダメージ、Unlagged位置、幾何距離、発射間隔、ショット整合性、Impact、死亡状態を検証する。**この順序はプロトコル上の観測可能な振る舞いであり、安易に入れ替えない。**

**具体的な分割境界:**
- `DecodeClaims`: wire上の件数/サイズ境界と読み出しだけ。破損時の扱いを変更しない。
- `ClaimIdentityGate`: MatchId、AuthorityEpoch、shooter/victim generation/life検証。Life不一致時の応答の有無まで現状を保持。
- `ClaimReplayGate`: Seen/Remember/ResultPending/確定応答の一貫性。再送冪等性を最優先。
- `ClaimEligibility`: ショット到着順・保留可否・Busy結果の確定。
- `ClaimJudge`: 現在の検証順序と返却コードを維持し、`MaxDamageFor`の**申告上限検査**と`NetUnlagged`の巻戻し幾何検証を維持し、最終ダメージ決定の正本と混同しない。
- `ClaimSettlement`: `ApplyOne`、ダメージ確定、Death/Score、`Answer`。サーバーAuthority以外で確定しない。

**手順:** まず`Receive`の分岐別のGolden traceを作る。次に副作用を持たないパケット読み出しを切り出す。続いてLife/Replayのゲートを**順序を変えず**小関数化する。`Judge`と`ApplyOne`は最後に移動し、各段階で既存UDP・戦闘テストを走らせる。**`MaxDamageFor()`は`raw * 5 + 1`で返す緩い申告上限ガードであり、`SettleDamage()`の確定ダメージ計算とは異なる。統合・共通化でこの意味を失わせない。** `MaxDamageFor`の二重呼び出し（判定と拒否ログ内）については、結果が同じで副作用がないことを確認後、**同一結果をローカルに保持**して再計算を避ける候補とする。

**必須試験:** 重複Claim、Pending重複、Out-of-order Intent、旧Match/Epoch、旧Life、死後射撃、Busy枠枯渇、発射武器/Charge/Prime/Double Damage、Sylux、タレット、Impact不正、Headshot、遅延/パケットロス。Verdictのバイト列、確定ダメージ、適用回数、スコアを前後比較。仕様差分が出たら分割を戻す。

### 4.6 NetSmoothing・高FPS描画・複製（M-06 / M-07）
**ZIP監査追加**：`NetHooks.cpp:383–426`の`AfterSimulation()`でIntent送信後に`NetSmoothing::Tick()`が呼ばれ、authorityはsnapshot送信、clientはリモート状態適用を行う。固定60Hzシミュレーション上のこの順序と、`NetUnlagged::LaunchFrameFor`、`NetPlayerBridge::CaptureIntent`、`NetHitClaims::Declare`のAck参照をセットでテスト保護する。`FindSnapshotBracket`は64要素の限定走査であり、FPSボトルネックとは実測前に断定しない。ルーム再基準化・追跡オブジェクト・Morph/Unmorphの共有時計は`Testing/TestNetSmoothing.cpp`に既存fixtureあり、フルビルド可能環境で再実行必須。

#### 関数一覧：履歴・クロック・位置サンプリング
| 実関数（定義行） | 分類 | 具体的な指示 |
|---|---|---|
| `ClearGrid` (68), `ResetSlot` (77), `Reset` (599), `NoteRoomChanged` (624) | STATE/履歴寿命 | 部屋変更・再接続・スロット再利用時の履歴破棄を統一 |
| `Active` (96), `PresentationOwnsRemoteReplicas` (101) | PRESENT/適用判定 | 表示用複製の所有条件を明確化。Authority側に補間を入れない |
| `Record` (107), `RecordTrackedPositions` (163), `Restart` (203), `Tick` (218) | STATE/履歴記録・時計 | 受信フレーム・追跡オブジェクト記録と表示クロックを別責務として扱う |
| `FindSnapshotBracket` (268), `Sample` (303), `SampleForm` (368), `SampleTrackedPosition` (410) | PRESENT/補間 | 位置・フォーム・追跡オブジェクトの共有表示時計を維持。60Hz/高FPSの観測を分離 |
| `Lookup` (506), `LookupForm` (526) | PRESENT/履歴検索 | 固定長履歴の索引計算を維持。欠損時の振舞いをテスト |
| `NoteStep` (547), `AckPoint` (580) | STATE/描画・ネットワーク確認点 | **AckPointはIntent/ClaimのAckFrameに使用**。予測・確認・表示の時刻基準を混同しない |
| `Describe` (638) | OBS | 診断文字列生成を常時ホットパスから切り離す |

#### 根拠：64フレーム履歴と表示適用条件
**確認箇所：** `Network/Gameplay/Replication/NetSmoothing.hpp:18-136`、`NetSmoothing.cpp:77-124, 181-220, 390-407`。

- `Slots = 8`、`HistoryFrames = 64`、`TrackedObjectsPerSlot = 6`、`MinDelayFrames = 2`、`MaxDelayFrames = 8`。
- 静的な履歴配列、読み出しフレーム、遅延、飢餓・補間・保持・スナップなどのカウンタを保持する。
- `Active()` と `IsAuthority()` で補間適用対象を制御する。受信側の表示補間と権威側の判定を混ぜないことが必須。
- 履歴検索とトラッキング位置の重複排除が存在する。ただし、**実際のCPUボトルネックとは未確認**。

**修正計画：** 60Hzシミュレーション、パケット到着、描画フレームの各時計を独立計測する。8スロット×64履歴の固定容量を維持した比較を先に実施し、必要なら更新されたスロットだけを走査する方式をA/B比較する。到着した最新状態をシミュレーションで使うことと、描画に数フレームの履歴を使うことを別の要件として検証する。低遅延化を名目に描画スナップや巻戻しの意味を変えない。

**ゲート：** 30/60/120/240/540FPS、RTT 0/50/150ms、ジッタ、欠落、逆順、テレポート、死亡・リスポーン、Morph、階層移動、観戦切替。フレーム時間のP50/P95/P99と位置誤差、表示遅延を別々に記録する。

#### 原則：鮮度と表示補間を別々に測定
通信の最新状態を早く取り込むことと、描画時に過去サンプルから滑らかに表示することは両立できる。ただし描画補間に必要な遅延をゼロにできるとは限らない。測定は **パケット受信→状態確定→描画採用→実際のpresent** を別々にタイムスタンプ化し、FPS別の補間遅延、ジッター、補外誤差を比較する。通信受信頻度を描画FPSへ無理に合わせない。

#### コード行：リング履歴と表示遅延
参照：[`NetSmoothing.cpp` 48、84、103、118、172、215、220、451、514、533行](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Replication/NetSmoothing.cpp)

- `HistoryFrames` 長の `_stamp` 配列を持ち、フレーム番号を `frame % HistoryFrames` でリング添字に変換している。
- 103、220行付近で `NetSession` の状態を参照する。215行にはplayout clock再基準化のログがある。
- **推奨設計**：現在のリング配列を無条件に置換しない。受信／権限状態更新／描画補間／presentの時刻を別々に採取し、補間遅延と処理時間を区別する。
- **検証**：60/120/240/540FPS、パケットロス・ジッター・RTT、ワープ・リスポーン・モーフ・階層移動時の画面上の一貫性。補間を短縮しても権限状態を改変しない。**同時に、`AckPoint`が決めるIntent/ClaimのAckFrameとserverの巻戻し参照先を意図せず変更しない。**

#### 手順：受信・シミュレーション・描画を分ける
- **P6-01 現状の図解**：`NetSmoothing.cpp`の`HistoryFrames`リング、stamp、clock rebase、remote state apply、描画サンプリングまでを呼出しグラフにする。
- **P6-02 時間軸**：`network arrival` / `server simulation frame` / `client simulation` / `render frame`を区別し、到着した最新のauthoritative stateがいつ描画に使われるかを測る。
- **P6-03 比較**：現行補間、遅延を縮めた補間、短時間外挿を、RTT 0/50/150ms、jitter/loss/reorderの同一fixtureで比較。**高FPSだから通信の更新間隔自体が高くなるわけではない**。
- **P6-04 表示時計・巻戻し判定の契約**：clientのremote replica表示補間はserver authorityを改変しない。ただし、**`AckPoint`がclient Intent/Claimの参照フレームを決め、さらに`NetUnlagged::LaunchFrameFor()`のfallbackにも用いられるため、表示時計はshot launch／巻戻し判定の妥当性検証とも関係する**。また`NetPlayerBridge`は表示サンプルをremote `PlayerEntity`へ`Move(..., true)`で適用する。変更するなら、client replicaの状態更新とserver判定の境界を直接テストする。補間不足時のteleport/snap/respawnは例外規則を維持。
- **P6-05 評価**：表示遅延、位置誤差、p95/p99のフレーム時間、jerk、補間buffer欠損を記録。単なるFPS改善だけでは採用しない。

#### 現存する検査ターゲット・依存経路（確認済み）
- [`src/MphRead.Native/Testing/TestNetSmoothing.cpp`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Testing/TestNetSmoothing.cpp) はclient role、フォーム遷移、追跡bomb、疎なsnapshotの共有表示時計を既に検証している。CTest名は`FruityPrime.NetworkSmoothingTimeline`（`CMakeLists.txt:425-428`）。新設せず**既存テストを拡張**する。
- [`NetPlayerBridge.cpp:154-168`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Player/NetPlayerBridge.cpp#L154-L168) は`CaptureIntent()`内で`AckPoint`を`IntentPacket.AckFrame/AckSubFrame`に反映する。
- [`NetHitClaims.cpp:273-294`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Claims/Hit/NetHitClaims.cpp#L273-L294) は`Declare()`内で`AckPoint()`を読んで`HitClaimOutbox::Entry.AckFrame`を選ぶ。失敗時は`AppliedSnapshotFrame()`へフォールバックする。
- [`NetUnlagged.cpp:348-376`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Combat/NetUnlagged.cpp#L348-L376) は射撃launch frameの選択で`AckPoint()`を参照する。優先順位はFiringEvent Ack→RemoteIntent Ack→表示時計Ack→AppliedSnapshot/NetFrame fallback。
- [`NetHooks.cpp:383-426`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Integration/Hooks/NetHooks.cpp#L383-L426) はIntent送信→`NetSmoothing::Tick()`→remote状態反映/snapshot送信の順。
- [`NetPlayerBridge.cpp:495-524`](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Player/NetPlayerBridge.cpp#L495-L524) はsampled positionをremote `PlayerEntity`へ`Move(..., true)`で適用する。**ただのGPU描画行列ではない**ので、client側のcollision/targeting/claim生成とserver authorityの責務を分けて確認する。
- 既存`FruityPrime.SessionAuthorityState`（`TestSessionAuthorityState.cpp`）はclientが権威を取得できないことを検証している。時間軸やrole変更の修正では併走させる。

#### NET-SMOOTH-01：表示時計と補間条件の保全仕様
**場所:** `Mods/Network/Gameplay/Replication/NetSmoothing.cpp` `Record()` 107行、`Restart()` 203行、`Tick()` 218行、`FindSnapshotBracket()` 268行、`Sample()` 303行、`Lookup()` 506行、`NoteStep()` 547行。

**実装から確認:** `Record`はframeを`HistoryFrames`で剰余して履歴リングに記録し、LifeId/SlotGeneration/Active/Position/AltForm/FormTransitionを保持。`RecordTrackedPositions()`は追跡オブジェクト（bomb等）を同じ表示時計に載せる。古いframeを拒否。`Tick`は`_readFrame += 1.0`した後、`_newest - _delay`との誤差を補正し、Starve時は外挿せず最新に固定。遅延をMin/Max内で適応的に調整。`FindSnapshotBracket`は履歴スタンプを走査。`Sample`は補間前にLife/フォーム/テレポート相当の境界を保護し、外挿しない。

**重要な禁止事項:** 高FPS描画を理由に`Tick()`を描画フレームごとに呼んではならない。**現在の呼出し元は`NetHooks::AfterSimulation()`（`NetHooks.cpp:402`）であることを確認済み**。**`AckPoint`が`NetPlayerBridge::CaptureIntent()`のAckFrame/AckSubFrame、`NetHitClaims::Declare()`のAckFrame、`NetUnlagged::LaunchFrameFor()`の参照frameに影響する。表示時計の変更はwireと巻戻し判定の意味の変更となり得る。**`_readFrame += 1.0`はシミュレーションtick前提の可能性が高く、レンダーフレームで回すと表示時計が加速する。Authorityの状態を描画補間結果で上書きしない。

**担当者の手順:**
1. **確認済みの入口**：`Scene::OnSimulationFrame()` → `NetHooks::AfterSimulation()` → `NetSmoothing::Tick()`。`AfterSimulation()`は保留Intentを送信してから`Tick()`を進め、その後にsnapshot送信またはremote状態適用を行う。まずこの順序と `NetPlayerBridge::CaptureIntent()` / `NetHitClaims::Declare()` / `NetUnlagged::LaunchFrameFor()` の取得時点を固定する。他の全呼び出し元も列挙して実行スレッド・頻度を記録する。
2. `Tick`が固定simulation step経由で起動することは確認済み。ただしpause・FreezeGameplay・`GameplayReady()`で呼ばれない経路があるため、**実効Tick頻度**と`Sample`呼び出し頻度を測る。描画FPSと同一だとは仮定しない。
3. 変更前のFrame→表示時刻→補間位置のGolden traceを記録する。
4. 必要なら`TickSimulation`（固定ステップ）と`SamplePresentation`（描画側読み取り）に責務を分ける。可変描画補間の導入は**既存の時間軸と境界を保つ独立PR**にする。
5. `FindSnapshotBracket`の線形走査は履歴が固定長であることを踏まえ、実測前に別コンテナへ置換しない。

**試験:** 30/60/144/240/540FPS、packet loss/reorder、長時間停止、復帰、slot再利用、LifeId変更、AltForm切替、teleport、frame wrap、bomb等の追跡物、`CaptureIntent`の`AckFrame/AckSubFrame`、`Declare`の`AckFrame` Golden。全試験で**意図しない**server state、Intent/Claim AckFrame、`LaunchFrameFor()`、HitClaim判定の変化がないこと。既存のバグを発見した場合は別の仕様修正チケットへ分離し、純粋リファクタリングの差分へ混ぜないこと。

### 4.7 Codec・Transport・Replay・UDP性能

#### 手順：wire互換を保持したcodec最適化
1. encode/decodeの各段階を `bytes/message, alloc/message, cycles/message` で測る。optional sectionの有無を組み合わせたサイズ分布を記録。
2. 再利用buffer、`reserve`、固定小容量コンテナ、moveを候補として比較。**受信パケットの寿命を超える`span`/`string_view`を保持しない**。
3. packet境界、endianness、alignment、length validation、unknown field、version mismatch、malformed inputを固定fixtureで確認。
4. retransmit/ack/claim outboxの順序を変更しない。受信キューの優先順位とドロップ規則は負荷試験で確認。
5. 同じ送信量・同じwire byte・同じgameplay結果でのみ「純粋な性能最適化」と判定。変化する場合は仕様変更として独立審査。

### 4.8 MapGen・JSON・マップ生成（M-11）
**ZIP監査追加**：大型関数の責務分割候補は`Q3Import::Build`(851行)、`MapTextureBake::Bake`(676行)。MapGen/診断Probeの`WindowSettings`・`OnLoad`・`OnRenderFrame`定型処理に同文の候補があるが、オフライン/テスト用のクラスであり、動作差や派生型契約があるなら抽象化を追加する方が悪化する。`mods_duplicate_review.tsv`で**共通化必須ではない低優先**として扱う。MapGenパッカー/レンダリングのAOT性、素材の取り込み、エラーパスのメモリ寿命をGoldenを先に固定して評価する。

#### 関数一覧：JSON parser・serializer・model mapper
| 実関数（定義行） | 分類 | 具体的な指示 |
|---|---|---|
| `JsonParser::Parse` (145), `SkipTrivia` (163), `ParseValue` (214), `ParseString` (304), `ParseNumber` (381), `ParseArray` (445), `ParseObject` (488) | CODEC/構文解析 | JSON構文のみ担当。モデルのデフォルト値・ゲーム規則を持たせない |
| `Hex` (273), `ParseHex4` (290), `JsonString` (553), `JsonBool` (562), `SignedInteger` (577), `UnsignedInteger` (595), `JsonFloat` (612) | CODEC/値変換 | 範囲・符号・Unicodeの互換をテスト |
| `WriteJsonString` (657), `AppendHex4` (647), `AppendNewLine` (698), `Indent` (707), `Property` (731), `WriteArray` (738), `WriteInteger` (759), `WriteUnsigned` (763), `WriteFloat` (767) | CODEC/シリアライズ | 順序・空白・浮動小数点の出力差分をGoldenで検証 |
| `Deserialize` (783), `Serialize` (799) | CODEC/公開境界 | 呼出しAPIは保ち、内部のパーサー/マッパー/ライターを分割 |
| `ReadObject` (808), `ReadObjectList` (824), `ReadMapDefinition` (844), `ReadMapCollision` (963), `ReadMapPreview` (981), `ReadMapImport` (998), `ReadMapMaterial` (1097) | CODEC/モデル写像 | JSON AST→型付きモデル変換。大量の`shared_ptr`は所有権を証明してから変更 |

#### 根拠：MapDefinitionのJSONと所有権
**確認箇所：** `MapGen/MapDefinition.cpp:105-106, 627-640, 783-844, 963-1097`。

- `MapDefinition.cpp` 内にJSON値表現、パーサー/シリアライザー、`ReadMapDefinition`・`ReadMapCollision`・`ReadMapImport`・`ReadMapMaterial` など複数のデータ型の変換処理が同居する。
- `JsonArray` 系で `shared_ptr<vector<T>>` を生成する経路がある。これが保存データ互換や既存C#移植の所有権に必要かは未判定。
- 共通JSONライブラリへ機械的に置換すると、数値・Unicode・不正入力・順序・null・浮動小数点の扱いが変わる可能性がある。

**修正計画：** パーサー、シリアライザー、型ごとのスキーマ変換、ファイルI/Oを分ける。ただし最初は**移動のみ**、バイト一致/意味一致のテストを作成した後に割当回数・処理時間を比較する。共有所有権を単一所有権に置換するのは呼び出し元まで寿命解析してから。

**ゲート：** 既存全マップ定義の読み込み・書き戻し、境界数値、未知フィールド、文字列エスケープ、ファイル破損、Windows/Linux/macOS/Androidでの結果整合。

#### 原則：shared_ptrの寿命検証を先に実施
共有所有権が実際に必要な箇所と、C#移植時の表現をそのまま保持しているだけの箇所を分ける。`shared_ptr` を `unique_ptr` や値に置き換えるには、参照の保存先、循環参照、複製時の意味、デシリアライズ後の寿命を呼び出し元まで確認する。パーサーの分割は先行可能だが、所有権変更は別コミット相当の独立検証単位とする。

#### 手順：Import / Parse / Bake / Packの段階分離
1. `MapDefinition.cpp`：JSON tokenize/parse、schema validation、model構築、serializeの境界を分ける。既存の`shared_ptr`は各所有者を確認し、単純な`unique_ptr`置換で公開APIや共有寿命を壊さない。
2. `Q3Import.cpp`：read→parse→normalize→geometry→materials→collision→entities→outputの入出力と一時メモリを計測。既存`Q3Bsp`等を再利用。
3. `MapPacker.cpp`：書込順・alignment・hash・互換性を固定。`reserve`やmoveを使っても生成物のbytesを変えない。
4. `MapTextureBake.cpp`と巨大静的テーブル：AOTデータの起動時負担とコンパイル時間を分けて計測。頻度の低いimport最適化をrender hot pathより優先しない。
5. MapGenはコールドパス。目標はpeak RSS、処理時間、エラーの局所化、検証容易性。単に`.cpp`を細切れにして完了扱いしない。

### 4.9 DebugLog・クラッシュログ・Platform寿命（M-10）

#### 関数一覧：FileSink・UTF8・TeeBuffer・クラッシュ時
| 実関数（定義行） | 分類 | 具体的な指示 |
|---|---|---|
| `FileSink::Write` (164), `Flush` (209), `Dispose` (216), `CloseNoThrow` (234) | OBS/ファイルI/O | 失敗時・破棄時の再入と例外保証を固定 |
| `Utf8Writer::Write` (275), `WriteLine` (287), `Flush` (300), `Dispose` (305) | OBS/エンコード | UTF-8 BOMと改行の仕様を保持 |
| `TeeBuffer::overflow` (358), `xsputn` (380), `sync` (397) | OBS/出力分配 | ロック順序と再帰書込みの可能性を監査 |
| `GetState` (349) と `State::Lock` (337) | OBS/静的寿命 | `GetState` は関数、`Lock` は `std::recursive_mutex` フィールド。終了時アクセス安全性を検証 |
| `PrunePattern` (459), `Prune` (507) | OBS/ログ保持 | ディレクトリ走査は初期化・保守時のみ |
| `CommandLineArgs` (667), `JoinCommandLineArgs` (740), `EnvironmentValue` (755) | OBS/環境収集 | 必要な診断時だけ取得 |
| `CaptureAndroidStack` (824), `NativeStackTrace` (883) | OBS/クラッシュ診断 | 異常系専用。通常フレームに負荷を載せない |
| `FlushWriterNoThrow` (968) | OBS/終了処理 | 静的破棄・非同期タスク終了との順序を検証 |

#### 根拠：再帰ミューテックスとプロセス寿命
**確認箇所：** `DebugLog.cpp:315-404, 968-1024`。

- ログ状態に `std::recursive_mutex`、コンソール状態に `std::mutex` がある。
- `TeeBuffer::overflow` / `xsputn` / `sync` で再帰ロックを取得する。`State` は `static State* state = new State()` で意図的に静的破棄を回避する。**ソースコメントにも atexit/terminate から利用するための意図的リークと明記されている**。
- 終了・クラッシュ処理からもログ経路が呼ばれるため、単純に `recursive_mutex` を `mutex` に変えたり静的オブジェクトのデストラクタに任せたりするのは危険。

**修正計画：** ロック獲得グラフ、再入呼び出し経路、異常終了時の呼び出し制約、フォーマット生成のタイミングを調査。ログ無効時の文字列生成やホットパス同期のコストを測る。先にログの無効化・レベル判定を呼び出し元へ適用できるか検証し、必要な場合だけシンクの所有権を変更する。

**ゲート：** 対象とした再現可能なcrash/terminate試験でログ出力・flushを確認し、シャットダウン/非同期オーディオ/複数スレッド/ログローテーション時のデッドロックがないこと、通常実行時のロック時間を測定。OS強制終了・電源断など全ケースでのログ保持は保証できないため除外条件を明記する。

#### コード行：GetStateとロック取得
参照：[`DebugLog.cpp` 337、354、372、389、404、973、1734、1763、1876行](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/DebugLog.cpp)

- 337行に `std::recursive_mutex Lock`、354行に `static State* state = new State();` がある。複数箇所で `lock_guard<recursive_mutex>` を使用する。
- ソース上、atexit/terminateフックが静的オブジェクト破棄中にも利用できるよう意図的に確保している。安易な自動破棄化は終了処理・terminateハンドラとの衝突を招く。
- **推奨設計**：再帰呼出しの実経路とロック順序を先に調査し、必要な再帰性を保ったまま、通常ログとクラッシュ時の最小経路を分離できるか検討する。
- **検証**：通常終了、例外、二重例外、クラッシュ時、複数スレッド同時ログ、ログ無効時。計測なしでロックレス化しない。

#### 手順：ログ・非同期破棄・外部更新処理
- **P10-01 DebugLog**：`DebugLog.cpp`337行付近の`std::recursive_mutex`、354行付近の`static State* state = new State()`の意図を再入・終了時callback・signal/crash handlerの観点で調査。ロック競合を測定。signal-safeでない処理を「高速化」の名目でsignal handlerへ移さない。
- **P10-02 非同期破棄**：audio/cutscene/scene lifecycleに対して、停止要求・完了待機・observer解除・所有オブジェクト破棄の順を固定。Headlessのmovie抑止は画面だけでなくasync task生成有無を確認。
- **P10-03 起動モード**：dedicated serverでGUI/GPU/フォント/サウンドを不必要に初期化しないことを計測。通常クライアントで診断専用の大きなデータを毎フレーム構築しない。
- **P10-04 更新・外部通信**：Update/Launcher/MapGenのネットワークI/O、キャンセル、タイムアウト、エラー処理をゲームのframe pathから分離。異常時の復旧を保つ。

#### OBS-LOG-01：再入・終了処理の詳細指示
**場所:** `Mods/DebugLog.cpp` `FileSink::Write()` 164行、`TeeBuffer::overflow()` 358行、`xsputn()` 380行、`sync()` 397行、`GetState()` 349行、`DebugLog::Attach()` 1691行、`Detach()` 1731行、`Line()` 1756行、`CaptureStack()` 1771行。

**実装から確認:** `GetState`は`static State* state = new State()`で静的破棄を避けている。状態に`std::recursive_mutex`があり、TeeBuffer経路でロックされる。キャリアの意図的リークはソースコメントで確認済み。再帰mutexが必須な具体的経路は別途追跡が必要であり、単純な`static State`化やmutex置換は危険。

**担当者の手順:** ログシンク/コンソール複製/例外スタック/プラットフォーム記号化/終了処理の呼び出しグラフを描く。ホットパスでの文字列生成とFlush頻度を計測。最初はファイル分割・内部API整理のみ行い、ログ順序・UTF-8 BOM・改行・ファイルローテーション・クラッシュ時出力を変えない。ロック階層はテスト後にのみ変更。

### 4.10 残余ファイルの関数分類と適用条件

#### 未分類ファイルに適用する証拠要件
- 本章は**主要翻訳単位・ヘッダーの一部関数について、実定義・宣言を個別確認した関数分類**。一部の長い関数やラムダ、テンプレート、マクロ生成関数はこの台帳の網羅対象外。
- `Mods/` の全 **439 CPP / 502 HPP** の全関数を列挙・分類するには、固定コミットのソースをローカル取得し、Clang ASTから `USR・qualified_name・file:line・definition/declaration・callers・callees・ownership・hot/cold` を機械抽出してCSV化する。**現時点では実施できておらず、完了扱いにしない。**
- 分類の根拠は「ファイル名だけ」ではなく実際の関数定義。1関数が複数責務を持つ場合は二重分類し、分割候補とする。未参照判定にはリンク時到達性・OS条件・起動モード・テストエントリを含める。
- 第7パスで分類済みの関数については、フェーズごとの曖昧な「関数を調査する」という作業より、本付録の関数表を作業対象の起点とする。

---

## 5. 調査と検証の実施手順
**ZIP再監査の進捗と制約**：Phase 0の全ファイルインベントリ・字句関数・直接include・静的候補の実データを生成した。Phase 1は独立単体9件＋静的ゲート2件をPASS、Protocol旧Golden3件の既存赤テストを特定した。`audit_test_results.tsv`と`mods_parity_matrix.md`を実行証拠とする。CMake構成はOpenGL開発依存の欠落で止まり、フルCTest／UDP／GPU／性能は未完了。古い「GitHubしか読めない」「7成果物は未生成」という記述は**初期段階の履歴**と読み替える。

必要な証拠をどう採るか、どこまで確定できているかをまとめた。見積や改善効果の欄に未測定値を記入しない。

### 共通：静的解析と実行プロファイルの取り方
#### 静的解析と依存グラフ

1. `Mods/` のすべての `.cpp/.hpp` を列挙し、ファイルのLOC/bytes、public API、namespace、可変静的変数、動的alloc、コールバック、include、CMake所有ターゲットを収集。
2. **翻訳単位単位**の include DAG と **機能単位**の依存DAGを作成し、上流を向く矢印と循環を検出。
3. `rg`、IDE参照、リンクマップ、シンボル表、GC-sectionsの比較を組み合わせ、`src/MphRead.Native/Mods` 内のコードを「本番到達」「診断経由」「試験専用」「プラットフォーム限定」「条件付き」「未到達候補」に分類する。
4. 仮想/関数ポインタ/イベント/コマンド文字列/テンプレート/反射相当の間接参照、CMakeの再帰glob、外部実行コマンドを考慮する。**`rg`で0件だけを理由に削除しない**。
5. 同一知識の複製を検出：weapon rules、shot key、座標変換、時間単位、locale key、settings default、slot capacity、CRC/hash、retry/ack、shutdown順序。

#### 実行プロファイル

- 既存の `Mods/Diagnostics/FramePerformance.cpp`（CPU/GPU測定）と `CpuSampling.cpp`（Windows側サンプリング）を第一候補として使う。計測の仕込み自体の負荷をA/B測定し、常時サンプルを避ける。
- シナリオを固定：オフライン8種の代表状態、1/2/8人オンライン、150ms±20ms程度の遅延・packet loss環境、表示FPS 60/高FPS、VSync on/off、Classic/Modern aim、各RHI、画面遷移、Headless server、長時間ローテーション。
- 初回起動/安定状態/マップ切替/終了を別々に採る。CPU stack、GPU timestamp、allocation、peak RSS、ビルド時間、オブジェクトサイズを紐付ける。
- ワイヤ送受信は内容（長さ・SHAまたはGolden Vector）と意味（時系列・最終状態）を比較。スナップショット一致だけで時間依存バグを見落とさない。

成果物：`docs/refactoring/cpp-mods/{inventory,dependency-graph,dead-code-candidates,baseline,regression-matrix,change-log}.md`（将来実施時に生成する想定）。**初期版の作成時点では未生成。**ZIP再監査後には7つの正本のうち測定候補・未検証の項目を明示したTSV/MDを実際に生成済み**（0章と証拠ZIPを参照）。ベンチマークは引き続き未実行。**

### 担当者共通の実行契約
> **この章の位置付け**：Phase 0～12を、実際に着手・レビュー・差し戻し可能な粒度に落とした実行仕様。これまでの章と矛盾する場合は「動作不変・計測先行・段階的移行」を優先する。以下のファイル名は基準SHAの構成に基づく。**本章は作業指示であり、実装完了や計測完了を意味しない。**

#### P.1 全作業に共通する厳格な実行契約

1. 着手時に `git rev-parse HEAD`、`git status --short`、`git branch --show-current` を記録し、対象コミットを明記する。未追跡ファイルや利用者の変更を破棄しない。作業ブランチを勝手に切り替えない。
2. **一度の変更で「責務移動」「振る舞い修正」「性能最適化」「プロトコル変更」を混ぜない**。変更セットには一つの目的、ひとつの検証、差し戻し可能な境界を持たせる。
3. `Mods` 配下を基本実装先とし、`Entities` / `Formats` / `Metadata` / C# は改造しない。外部フックが不可避なら例外として事前に差分・理由・代替案を記録する。`include/` は新設せず `.hpp` と `.cpp` を同階層に置く。
4. 現行の **サーバーauthority、ローカルプレイヤー、UDP通信** の役割を固定する。ホストへのauthority委譲や、サーバー状態をクライアントの静的コンテキストへ混ぜる変更は禁止。
5. 60Hz simulation、DS互換の30Hz入力、描画FPS、ネットワークのtick/ack/frameは別の時間軸として型・名前・テストで区別する。
6. 既存のゲームプレイ結果、入力感覚、UIの言語とglyph、MapGen出力、設定の読み書き、既存wire protocolを原則byte-for-byte/semantic parityで保護する。仕様変更は独立した変更セットでのみ扱う。
7. 計測がない段階では「無駄候補」「コピー候補」「依存候補」と記載し、リーク・性能劣化・デッドコードと断定しない。
8. **この計画書作成の段階ではソース編集、コミット、プッシュを行わない**。実装フェーズを別途開始するときも利用者の指示に従う。

### Phase 0：全ファイル・依存・状態・到達性・性能計測
#### P0-01 ソースインベントリ

- **入力**：`git ls-files src/MphRead.Native/Mods` と CMakeの実際のcompile commands。Git Treeで確認した947ファイルを基準に差分を説明する。
- **生成**：`mods_inventory.csv`。列：`path,extension,bytes,lines,subsystem,production_or_test,compile_target,entrypoints,platform_guards,owner,lifetime,hot_or_cold,investigation_status`。
- **判定**：`.hpp`に同名`.cpp`がない83件は即座に不要扱いしない。header-only規則・宣言・template・inline・generated interfaceを分類する。
- **終了条件**：947件すべてに用途とビルド対象が付く。未知は空欄でなく `UNVERIFIED` とし、完遂を宣言しない。

#### P0-02 Includeとビルド依存

- `CMAKE_EXPORT_COMPILE_COMMANDS=ON` でcompile databaseを作り、各TUの直接includeと推移includeを取得。Windows/MSVCとLinux/Clang/GCCでプリプロセッサ条件が異なるため、少なくとも代表2環境で比較。
- `mods_includes.csv`：`from,to,direct_or_transitive,conditional,target`。`mods_cycles.md`：循環includeとforward declaration候補。`mods_rebuild_impact.csv`：ヘッダ変更時の再ビルド対象TU数。
- 「依存数が多い」だけで削除せず、値型の完全定義が必要なケースとポインタ参照のみのケースを分離。**ヘッダに巨大な抽象インターフェースを増やさない**。

#### P0-03 静的状態・所有権・非同期寿命

- `static`、namespace/global、singleton、`thread_local`、`shared_ptr`、`weak_ptr`、`std::function` capture、コルーチン/workerを全探索し、定義・書込・reset・破棄・スレッド・利用モードを調査。
- `mods_state_ownership.csv`：`symbol,decl_path,owner,scope,writer,reader,reset,shutdown,thread,guard,risk,test`。
- `NetSession::_context` は既存の`SessionContext`を正本として調査し、置換前に**プロセス／サーバー／クライアント／マッチ／ルーム／プレイヤー／描画**の各寿命を明記。
- `DebugLog.cpp` の関数ローカル静的`State*`と`recursive_mutex`は、終了順序と再入の必要性をテストで立証するまで機械的にRAIIや通常mutexへ置換しない。

#### P0-04 重複と未使用の検証

- `rg`による名前検索だけでは不十分。シンボル参照、登録型コールバック、コマンドライン起動、プラットフォーム条件、テストのみの入口、プラグイン的な動的参照を照合する。
- 重複は `rule_id,source_a,source_b,semantic_equivalence,exception,proposed_ssot,tests` として記録。似たコードでも別仕様なら統合しない。
- デッドコード候補は `candidate,link_reachability,runtime_reachability,platform_matrix,tests,removal_risk` で判定。`UNVERIFIED` を削除根拠にしない。

#### P0-05 ホットパスの計測

- **モード別**：offline通常プレイ、client online、dedicated server、headless test、ClassicMenu idle、MapGen、Android GLES（可能な環境）。
- **条件固定**：同一commit/ビルド最適化/マップ/人数/シード/FPS cap/VSync/Reflex/ネットワークRTT・loss/計測時間。ウォームアップとサンプル数を記録。
- **出力**：`mods_cpu_top20.csv`、`mods_alloc_top20.csv`、`mods_frame_timing.csv`、`mods_build_cost.csv`。少なくともmedian/p95/p99、alloc count/bytes、peak RSS、frame pacing、CPU cycles（取得できる場合）を記録。
- **禁止**：fps単一値、異なるレンダラ、キャッシュ状態の異なるCIビルド、デバッグ計測付きと無しの比較で最適化の効果を判定すること。

### Phase 1：回帰テストと互換フィクスチャー
1. **P1-01 ビルド基準**：MSVC Release / Linux / macOS / Androidの既存CI targetを列挙し、通らないものは原因を分類。テストskipは理由と再現コマンドを保存。
2. **P1-02 ネットワーク契約**：既存`FruityPrime.NetworkProtocol30GoldenPreserved`／`31GoldenPreserved`／`32GoldenPreserved`／`NetworkProtocol33Golden`、`FruityPrime.SessionAuthorityState`、`FruityPrime.HitClaimOutbox`、`FruityPrime.ClaimDamagePolicy`、`FruityPrime.NetworkSmoothingTimeline`をまず一覧化・再利用し、packet encode/decode Golden Vector、optional payload、dup/reorder/drop、shot idempotency、Prime role/continuous tick、authority claimを個別fixtureへ。wire field・length・versionの変化を拒否。
3. **P1-03 状態寿命**：server start→stop→start、client join→leave→rejoin、room change、map rotation、scene破棄中の非同期audio、spectator target leaveを反復する。各回で残存handler/worker/参照数を点検。
4. **P1-04 表示契約**：Classic/通常SETTINGS双方の言語設定、glyph code、font scale、word-wrap、画面再構築、render backend切替をGolden化。
5. **P1-05 入力契約**：RawInput、Classic/Modern、マウス/タッチ/ペン/ゲームパッド、wheel、Morph+Fire、Shift Boost、Alt+Tab releaseを固定イベント列で比較。
6. **P1-06 MapGen契約**：代表的な`.pk3`、素材fallback、透過、collision、pack bytes/semantic hash、失敗入力時のdiagnosticを固定。
7. **ゲート**：失敗したケースは既存不具合かリファクタリング回帰かを分離。基準の赤テストを隠すために削除・skipしない。

### 初期調査時点で未完了だった監査
以下は**調査済みと偽らない**。第3パスまたはPhase 0の実行タスクとして扱う。

1. **947ファイルの完全な呼び出しグラフ**：全翻訳単位のincludeグラフ、逆参照、シンボル到達性、循環依存SCC、ビルド時間上位20ファイル。今回の個別ファイル確認だけでは未完成。
2. **重複の機械検出**：同名関数、類似AST、定数の複数正本、コピーされた分岐、重複状態キャッシュの候補一覧と「仕様上の意図的差異」の照合。
3. **NetSession以外の静的状態全量**：NetHitClaims、NetSmoothing、入力、レンダリング、音声、メニュー、ロガーの状態寿命とスレッド安全性を横断監査。
4. **実際の権威判定経路**：`Host`（旧式`StartHost`は例外で拒否）、`Server`、ローカルプレイヤー、専用サーバープロセスの生成・通信・試合開始・停止をコード経路と実UDPで突き合わせる。GUI hosted→専用serverプロセス経路の動作検証は未実施。
5. **性能プロファイル**：CPUサンプリング、ヒープ割当、GPU待機、ロック競合、FPS/P99、メモリピーク、バイナリサイズ。改善効果は未測定。
6. **全プラットフォームの実行検証**：MSVC、Linux GCC/Clang、macOS、Androidと各RHIのビルド・実機テスト。
7. **削除候補の確定**：現段階では「不要なファイル」や「削除してよい関数」の確定リストは作れない。到達性とCLI互換を証明したものだけを削除対象にする。

### 優先調査チケット AUD-01～09
| ID | 優先度 | 対象 | 必須成果物 |
|---|---|---|---|
| AUD-01 | P0 | `Mods` 全体 | include/呼出/逆参照グラフ、SCC、責務スコア、依存数ランキング |
| AUD-02 | P0 | `Network/Session`、`Network/Server` | 権威・所有権・寿命の実コード経路図、静的状態の全件表 |
| AUD-03 | P0 | `Network/Gameplay/Claims`、`Weapons`、`Combat` | 同一規則の複数実装一覧、プロトコル互換テスト一覧 |
| AUD-04 | P1 | `ClassicMenu` | `Servers()` 呼び出しコスト、文字列生成・レイアウト・再描画頻度の実測 |
| AUD-05 | P1 | `DebugLog`、`Diagnostics` | ロックグラフ、異常終了安全性、ログ無効時の費用 |
| AUD-06 | P1 | `MapGen` | JSON往復差分、共有所有権マップ、パイプラインのメモリピーク |
| AUD-07 | P1 | `Mods/Network/Testing`、`ModEntry` | CLI/リンク到達性台帳、本番・検査ターゲットの分離可否 |
| AUD-08 | P1 | `NetSmoothing`、Input、Render | シミュレーション/描画/受信のクロック図とP99性能 |
| AUD-09 | P2 | 残りのMods全部 | 変更理由、所有者、利用頻度、削除可否のファイル単位監査台帳 |

### EVD-01～09：全量監査の未取得証拠と完了条件
**旧監査で未取得だった意味上の証拠**を示す。ZIP再監査でファイル本文・直接include・字句索引は取得したが、AST/到達性・実性能・全プラットフォームに関する証拠は依然未取得。取得していないのに「完遂」「問題なし」「高速化できる」と判定しない。

| 監査ID | 未取得の証拠 | 完了判定 |
|---|---|---|
| EVD-01 | 947ファイル全体のinclude・シンボル依存グラフ、SCC | ファイル単位とリンク単位の依存台帳を生成し、循環を根拠付きで列挙 |
| EVD-02 | ASTベースの重複・重複定数・同一ロジック候補 | 誤検知を人手レビューした候補一覧と正本の移行先を確定 |
| EVD-03 | 全可変静的状態・スレッド間共有・所有権の一覧 | 各状態の生成、破棄、ロック、読書き主体を記録 |
| EVD-04 | Authority経路の全起動モードでのトレース | 専用サーバー／ローカルプレイヤー／接続者の権限をUDP実測で検証 |
| EVD-05 | CPU・割当・ロック・描画・通信の基準計測 | 平均、P95/P99、メモリ、バイナリサイズ、測定条件を保存 |
| EVD-06 | MSVC、Linux、macOS、Androidと各RHIの実行結果 | CIと実機の結果、未対応項目を区別して記録 |
| EVD-07 | 実行コマンドとシンボルの到達性・リンクマップ | 削除候補ごとに到達不能の根拠と復元手段を付与 |
| EVD-08 | **定義は確認済み**：`Servers()`は`std::vector<ServerRow>`値返し。未取得はHost内の再生成/コピー回数、画面更新頻度と割当 | ベンチマークと寿命安全性の両方で評価 |
| EVD-09 | JSON全マップ互換、共有所有権の実利用 | 保存形式の互換性と寿命の両方を検証 |

### 取得環境の制約と判定基準
今回のGitHub読み取りだけでは、C++の全翻訳単位を構文解析した結果、リンク到達性、実機性能を取得できない。したがって、**追加のコード形状の確認は実施済みだが、AUD-01～09の担当調査およびEVD-01～09の証拠要件を満たす完全監査は未完了**とする。次の実作業ではリポジトリをチェックアウトした環境で `compile_commands.json`、Clang AST/clang-tidy、リンクマップ、CTest、プロファイラを取得し、各監査IDに証拠を紐付ける。計測なしに削除・最適化の優先度を確定しない。

これまでの各段階の監査および今回の整合性監査は計画書の記述確認・修正のみであり、C++ソース変更、コミット、プッシュは行っていない。

### 補足ソース監査の固定コミットと条件
> 第2パス基準：2026-10-15、`main` / `66fdfd4f23d0e84837c1b16b5a91b8b48113708b`（第1パスと同一）。実装・コミット・プッシュなし。以下は追加でソース本文を直接確認した結果であり、測定していない性能改善を既成事実としない。

### 実装前に重視すべきリスクと判断
旧パスの調査で把握した設計リスクは、**既に分割された部品を再び抽象化しすぎること**、**テスト・診断機能を未使用と誤認して消すこと**、**静的状態の寿命を考えず一括で置き換えること**である。性能上の具体的な調査候補は `Compose` の一覧取得、ログの再帰ロック、`NetSmoothing` の履歴走査、MapGenの割当であるが、いずれも計測前の段階でボトルネックと断定しない。

既存のPhase 0～12の区分を維持しつつ、最新のソース整合性監査に基づき、対象・順序・受入条件を実コードの証拠で強化する。**現時点での完成度は「実施可能なフェーズ計画＋主要領域の実装本文との再照合」であり、「947ファイルすべての本文監査完了」ではない。**

### 必要な7つの正本証拠成果物と補助ビューの対応
| 成果物 | 必須列・内容 | 完了の条件 |
|---|---|---|
| `mods_file_inventory.tsv` | path、行数、bytes、owner、責務、起動モード、実行頻度、削除可否、証拠 | 947ファイルの漏れなし |
| `mods_dependency_edges.tsv` | from、to、include/symbol、依存理由、循環SCC | 全翻訳単位を対象に抽出 |
| `mods_mutable_state.tsv` | symbol、所有者、生成/破棄、読書きスレッド、同期、再入可否 | 全可変静的状態を分類 |
| `mods_duplicate_candidates.tsv` | 候補ペア、類似根拠、同一仕様か、SSOT移行先、誤検知 | AST/構文類似度とレビューの併用 |
| `mods_hotpath_profile.tsv` | callsite、頻度、CPU、alloc bytes、lock wait、P95/P99、測定条件 | 主要モード・主要RHIの基準値あり |
| `mods_dead_code_evidence.tsv` | symbol/file、到達性、動的登録、CLI、リンク状況、削除判定 | 根拠なしの削除ゼロ |
| `mods_parity_matrix.md` | 通信プロトコル、authority、戦闘、入力、HUD、保存、Android等 | 修正前後の実行結果を記録 |

**成果物名のSSOT**：上記7つの`mods_*.tsv/.md`を監査提出物の**正本**とする。ZIP再監査で実ファイルを作成済みだが、**各列の意味的完了は別**。inventory=ファイルの所在/サイズ/ハッシュ全量、dependencies=クォート付き直接include、mutable/duplicates/dead=字句解析候補、hotpath=未測定、parity=部分実行という到達状態を保持する。P0/P12に列挙した詳細な`mods_*.csv`・`mods_cycles.md`等は正本から生成する**補助ビュー**であって、同じ目的の別正本を増やさない。

| 正本 | 補助ビュー／レポート（任意だが最終ゲートで必要なものは提出） |
|---|---|
| `mods_file_inventory.tsv` | `mods_inventory.csv`、ファイル用途／ビルドowner別集計 |
| `mods_dependency_edges.tsv` | `mods_includes.csv`、`mods_cycles.md`、`mods_rebuild_impact.csv` |
| `mods_mutable_state.tsv` | `mods_state_ownership.csv`、owner/lifetime DAG |
| `mods_duplicate_candidates.tsv` | `mods_duplicate_rules.csv`、レビュー済みSSOT候補 |
| `mods_hotpath_profile.tsv` | `mods_cpu_top20.csv`、`mods_alloc_top20.csv`、`mods_frame_timing.csv`、`mods_build_cost.csv` |
| `mods_dead_code_evidence.tsv` | `mods_dead_code_candidates.csv`、リンク到達性一覧 |
| `mods_parity_matrix.md` | `mods_regression_matrix.md`、`mods_refactor_decisions.md`、変更ログ |

**ZIP再監査で同名の7つの正本ファイルを実際に生成済み**。ただし、現状のTSVは構造・字句候補や計測テンプレートであり、上表の**完了条件を満たしたのはファイルの全量棚卸しなど一部のみ**。`mods_hotpath_profile.tsv`には計測値がなく、`mods_dead_code_evidence.tsv`は全件削除未承認。`AUD-*`は担当別調査チケット、`EVD-*`は証拠の合格判定であり、混同しない。


### 根拠ベースの監査到達点（作成時点）
- **実施済み**：実コードに根拠を持つ重点監査、優先候補の特定、Phase 0～12の実施計画、主要候補の検証方法の具体化。**全量監査としての完了ではない**。
- **未完了**：全947ファイルの**ASTベース**意味依存・重複同値性・可変状態owner/reader/writerの確定、全体コンパイル／リンク到達性、全プラットフォーム実行、CPU/メモリ/描画/通信の実測。ZIPの機械的**字句索引は実施済み**。
- **更新**：受領したZIPの全ソースは実際に読み取り・一部単体検証できた。ただしQt/OpenGL開発環境が不足しフルビルドは未完、全関数の厳密なASTや性能実測も未完のため、「完全な調査が終わった」とは表現しない。今後の実行環境で上記7成果物を生成して初めて完遂とする。
- **変更範囲**：この追補はMarkdownのみ。C++ソースの変更・コミット・プッシュは行っていない。

### 構造監査で解消した問題と今も残る証拠
**解消**：Git tree APIの参照先を正しいツリーSHAへ修正し、全量947ファイルのパス・種別・ディレクトリ分布と大容量ファイルを再検証した。Network/Testingの43ファイルを含む対象範囲も確定。

**未解消（調査完遂の必須条件）**：

- 947ファイルの本文取得と字句索引は今回**済み**。残るのはAST／コンパイラ基準の全関数・全クラス・全可変状態と呼出し／書込者の**意味上の確定**
- `compile_commands.json` に基づく**プリプロセッサ条件を加味した推移include**依存と循環SCC（クォート付き直接includeの4,219件は確認済み）
- AST等による重複ロジックの同値性確認
- ビルドターゲット別の到達性、リンクマップ、dead-strip確認
- MSVC/Clangでの実ビルド、CTest、UDPマトリクス、実機RHI/Android検証
- CPU/アロケーション/ロック待機/フレーム時間のベースラインと改善後比較

**旧段階の環境上の制約（ZIP受領によって解消）**：当初は`git clone`のDNS障害によりGitHubコネクタの個別ソース取得のみ成功した。現在はZIPのGit Tree SHA一致を証明し、全ソースをローカル解析できている。よって、上記の未解消項目を「完遂」と報告することはできない。**全量メタデータ監査は完了、全量意味解析・実測監査は未完**と明示する。

### 調査完遂の最終判定条件
全ファイルのパスを数えただけでは「完全なリファクタリング調査」ではない。各ファイルについて所有責務、主要シンボル、参照元、ビルド対象、起動モード、可変状態、実行頻度、削除可能性の根拠が記録され、全ホットパスについて再現可能な測定値があり、既存のネットコード・言語・入力・レンダリング・Android・各CIを回帰テストで保護できたときのみ調査完遂とする。

**ファイル構造については全量メタデータを確認したが、関数・依存・性能の全量意味監査は未完了。未検証の性能改善や削除可能性を確定事実として記載しない。**

### 実装本文を読んだ対象と判定ルール
**検証基準:** `main` commit `66fdfd4f23d0e84837c1b16b5a91b8b48113708b`。以下は関数名だけでなく実装本文を読んだ結果。行番号はこのコミットに固定する。変更時は最新差分を確認する。**実測による性能改善は未確認**。本章の「確定」はコードの制御フロー・データフローについての確定であり、速度向上の確定ではない。

---

## 6. 担当者共通の運用と成果物
各チケットは差し戻し可能な最小変更セットとして扱い、同一条件での動作・性能比較を記録する。

### 作業チケットで提出する情報
各作業チケットは `Baseline SHA` / `変更ファイル` / `変更理由` / `現状の呼出し経路` / `新旧の所有者` / `保持する不変条件` / `前後の計測条件` / `回帰テスト` / `rollback` / `完了判定` を記録する。コード移動のみの場合でも、外部公開関数・シンボル・静的初期化・ヘッダ依存の差を点検する。

### チケットの標準テンプレート
各変更セットを次の形式で管理する。**完了後に記録する値と、計画時の期待値を混ぜない。**

```markdown
### [P?-NN] 変更内容
- Baseline commit:
- Change scope (C++ files only):
- Problem category (D1-D10):
- Evidence (call graph / profiler / test / source locations):
- Owner before -> after:
- SSOT before -> after:
- Invariants to preserve (frame / wire / GPU / save / authority):
- Refactoring steps (no simultaneous behavior change):
- Unit tests / integration / supported platforms:
- Performance measurement (scenario, warmup, repeats, results):
- Removal evidence / kept compatibility paths:
- Rollback strategy:
- Exit: PASS / FAIL / DEFER
```

**レビュー基準**：このチケットに「どんな無駄が消えたのか」「削減がなぜ安全か」「どのテストが旧動作を保証するか」が書けないなら実施しない。

### 実装着手前・完了後の記録テンプレート
各チケットは実装に着手する前に以下を記録する。作業後に数値とテスト結果を追記する。

```markdown
## [チケットID] 対象機能
- 基準コミット：
- 対象ファイルと関数（定義行）：
- 現在の入力／出力／副作用：
- 呼び出し元と呼び出し頻度（未計測なら未計測）：
- 状態の所有者と寿命：
- 既存の仕様・不変条件：
- 変更前の再現テスト：
- 最小変更案：
- 変更後の確認テスト：
- 性能 Before / After（同一条件）：
- 変更を見送る条件：
- 差し戻し方法：
```

### 調査と実装を分ける依存ルール
1. **即時着手可能な調査**：M-01、M-02、M-03、M-09、M-10、M-11の関数呼び出しとデータ寿命の図示。
2. **回帰試験を固定してから変更**：M-04～M-08。通信プロトコルやAuthorityの挙動を先に変更しない。
3. **現状維持を優先**：M-12。既にAOT化されている経路は、性能問題を測定できなければ変更しない。
4. **削除判断の基準**：未使用に見える関数も、ビルド設定・プラットフォーム条件・テスト・動的登録・関数ポインタ参照を調査するまで削除禁止。

### 担当者用・着手前後のチェックリスト
- [ ] 対象コミットと対象ファイルのSHAを記録
- [ ] 対象関数の呼び出し元、呼び出し先、所有者、実行スレッドを記録
- [ ] 観測可能な動作（戻り値・例外・ログ・wire bytes・イベント順）を列挙
- [ ] 変更前の再現テストと基準計測を保存
- [ ] 1コミット相当の変更範囲を1責務に限定（ただし指示がない限り実際のcommit/pushは禁止）
- [ ] 変更後の回帰試験・ベンチマーク・プラットフォーム確認
- [ ] 削除候補は全参照と条件付きビルドを確認
- [ ] 結果を「確認済み」「未検証」「仕様変更が必要」に区別

**旧監査時点では未実施／ZIP再監査で一部解消:** 当初はGitHub API経由の個別ソース読取だけだったが、現在はZIPの全量字句解析・独立単体9件・静的ゲート2件を実際に実行。Protocol旧Golden3件は既存の赤テストと判明。AST全量解析、フルビルド／CTest、UDPマトリクス、CPU/GPUプロファイリングは未実施。性能向上や全機能回帰PASSを主張しない。

### ビルド・CTest・Headless実行コマンド例
以下は **例示**。環境やCMake configure presetは各OSのCI設定に合わせる。ゲームファイル要件がある検査は別扱い。**以下の例示コマンド一式をそのまま通したわけではない**。ZIP再監査で一部の独立C++テストと静的スクリプトは実行済み（0章参照）。

```bash
# 1. 開始時点を固定し、差分・既存作業を把握
# （チェックアウトやfetchを無断で破壊しない）
git rev-parse HEAD
git status --short

# 2. 在庫・重複・状態ownerの候補抽出
find src/MphRead.Native/Mods -type f \( -name '*.cpp' -o -name '*.hpp' \) | sort
rg -n 'static\s+.*[=;]|std::function|std::shared_ptr|unordered_map|lock_guard' src/MphRead.Native/Mods
rg -n 'NetSession::|NetHitClaims::|NetSmoothing::' src/MphRead.Native

# 3. 既存の構成済みReleaseビルドを使用する場合の基本ゲート
cmake --build <build-dir> --config Release --parallel
ctest --test-dir <build-dir> -C Release --output-on-failure

# 4. テスト一覧とskip条件の監査
ctest --test-dir <build-dir> -C Release -N

# 5. ネイティブ実行の統合検査例（要ゲームファイル・paths.txt）
# 現行ModEntryで確認済みのフラグ名。
# 実際の値・室名・追加引数は固定fixtureの設定に合わせる。
./FruityPrime -netcombatcheck <room>
./FruityPrime -powerupsimtest <room>
./FruityPrime -homingspawncheck 30
./FruityPrime -movieheadlesscheck
```

Windowsでは実行ファイル名・ビルドパスが異なる。`-netcheck` や real UDP matrixを使う場合は **実ネットワーク/サーバー/複数クライアント実行**の構成を専用スクリプトで固定し、テスト終了時にプロセス・ポート・シーン・生成物を確実に解放する。マルチプロセス試験とクラッシュ再現試験はログが消えないよう単独実行の条件を設定する。

### Phase 11：コード削除の許可基準
削除チケットは次の**全項目**を満たす場合のみ実施する。

- シンボル/ファイルのcompile targetと全platform guardを確認した。
- CLI、テスト、headless、server、Android、desktopの動的入口を確認した。
- 同等の機能を提供する正本と移行先を特定した。
- 変更前の契約テストと削除後の回帰テストを記録した。
- 対象のバイナリ/リンク依存、サイズ、メモリ/CPU効果（該当時）を測った。
- 互換性が不要であることを確認し、削除前に移行済みである。
- 差し戻し単位を特定した。

「ファイルが大きい」「header-only」「参照が少ない」「似た関数がある」は単独では削除理由にならない。

### Phase 12：統合監査・正式な完了ゲート
**必須の成果物**：`mods_inventory.csv`、`mods_includes.csv`、`mods_cycles.md`、`mods_rebuild_impact.csv`、`mods_state_ownership.csv`、`mods_duplicate_rules.csv`、`mods_dead_code_candidates.csv`、`mods_cpu_top20.csv`、`mods_alloc_top20.csv`、`mods_frame_timing.csv`、`mods_build_cost.csv`、`mods_refactor_decisions.md`、`mods_regression_matrix.md`。

**完了判定**：

1. 対象ファイルの分類が100%。未確認を「問題なし」に読み替えない。
2. 全ての高リスク候補に、正本・owner・仕様・テスト・採否判断がある。
3. ネットワークauthority・wire互換・UI/入力・MapGen・OS間挙動の回帰がない。
4. 変更によるCPU/alloc/latency/build costを同一条件で比較し、悪化した場合は理由・許容判断を記録。
5. 依存の循環と不要な初期化を、機械的な結果と人手レビューで確認。
6. 旧互換層やdead codeを削除した場合、到達性と移行完了を証拠で示す。
7. すべての差分が小さく追跡可能で、機能修正とリファクタリングが区別されている。

**停止条件**：再現不能な性能改善、プロトコル差異、authorityの混線、UIの言語・glyph不一致、非同期破棄時のクラッシュ、ビルド/CIの回帰、実装を複雑にするだけの抽象化が出た場合は、その変更セットを止めて原因調査へ戻す。

### 最終報告に記載する証拠と完遂の定義
最終的に少なくとも次の情報を含む成果報告を作成する。

1. **Baseline / Final SHA**（基準と最終成果物の同定、ビルド条件）。
2. **全947ファイル由来のインベントリの処置**（新規増減を追跡し、変更後のファイル集合に置き換える）。
3. **改善された責務・依存図**（削除した循環、縮小した公共ヘッダ、残すstaticの理由）。
4. **SSOTの一覧**（authority、shot、weapon、frame、config、language、packet）。
5. **Golden/CTest/実UDP/画面/実機検証の結果**（PASS、FAIL、SKIPを分離）。
6. **前後性能比較**（CPU/GPU frametime、alloc、bytes、RSS、起動、リンクサイズ、ビルド時間）。
7. **実際に除去された無駄**（dead code、重複規則、常時実行、不要allocation、不要依存、条件付き機能）。
8. **意図的に残した複雑さ**（DS互換、history、ネット損失耐性、Qt fallback、AOT、platform差分など）とその理由。
9. **残課題 / DEFER** を機能・ソース・検証条件つきで開示。未解決を「全完了」に含めない。

#### 完全リファクタリングの定義

以下のすべてが満たされて初めて「完全リファクタリング完了」とする。

- `Mods/` のすべての下位領域が監査済みで、既知の重複・不要処理が `REMOVE/CONSOLIDATE/RETAIN/DEFER` に分類済み。
- 主要なpublic API、状態owner、SSOT、依存方向が明確。
- 毎フレーム、毎tick、毎packetの無駄は実測に基づき削減し、p95/p99・メモリ・startup・compileの回帰なし。
- 既存のゲーム動作、サーバーauthority、通信、音声、入力、UI、多言語、platform、描画の互換性がテストで保証される。
- C++20 best practices、DRY、SRP、SoC、DOD、KISS、Pay for What You Use、Zero-Cost Abstraction、AOT、Shift-Left Validationが**名前だけでなく設計判断と計測の結果に反映**される。
- 未使用と判定されたコードだけを証拠付きで削除し、必要な互換・診断・テスト機能を維持。

### この指示書自体の実施／未実施境界
過去のパスでは主要6実装ファイルの関数一覧/作業チケットを作成。本改訂では `main` 固定SHAの `Host.hpp`、`Compose.cpp`、`NetSession.*`、`SessionLifecycle.cpp`、`NetSmoothing.*`、`NetPlayerBridge.cpp`、`SessionSnapshotReceiver.cpp`、`NetHitClaims.cpp`、`GamepadManager.*`、`MenuLocalization.*`、`MenuUnicodeFont.cpp`、`CMakeLists.txt` と既存CTestソースなども本文で照合し、矛盾を修正した。**ZIP全量の字句的関数・include・状態候補抽出は実施済み**だが、全947ファイルのAST呼出しグラフ解析、全シンボル網羅、実機プロファイル、**フルソースのコンパイル／CTest**、UDPマトリクス実行は未実施であり、完了と記録してはならない。ソースコード変更・コミット・プッシュなし。

### 参照ソース・仕様資料
- [`main` 固定コミットの Mods ツリー](https://github.com/Zection6V/Fruity-Prime/tree/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods)
- [`ModEntry.cpp` 起動・CLI・診断](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/ModEntry.cpp)
- [`NetSession.hpp` / session静的API](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Session/NetSession.hpp)
- [`NetSession.cpp` / 静的Contextの定義](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Session/NetSession.cpp#L50-L77)
- [`NetHitClaims.cpp` / claim全体](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Claims/Hit/NetHitClaims.cpp)
- [`NetSmoothing.hpp` / history・遅延](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Replication/NetSmoothing.hpp)
- [`CMakeLists.txt` / ソース収集とテスト構成](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/CMakeLists.txt)
- [`ClassicHost.cpp` / Qt→C++サーバー・設定行変換とJoin](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native.Qt/Shell/ClassicHost.cpp)
- [`NetHooks.cpp` / simulation後のIntent・Tick・remote適用順](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Integration/Hooks/NetHooks.cpp)
- [`NetUnlagged.cpp` / shot launch時刻の優先順位](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/Network/Gameplay/Combat/NetUnlagged.cpp)
- [`ClassicMenu/MenuLocalization.cpp` / 既存AOT言語索引](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/ClassicMenu/MenuLocalization.cpp)
- [`ClassicMenu/MenuUnicodeFont.cpp` / k8x12L atlas](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/src/MphRead.Native/Mods/ClassicMenu/MenuUnicodeFont.cpp)
- [`CLAUDE.md` / Modsのowner境界](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/CLAUDE.md)
- [Software Design Philosophy and Best Practices](https://github.com/Zection6V/Fruity-Prime/blob/66fdfd4f23d0e84837c1b16b5a91b8b48113708b/.claude/design/Software_Design_Principles_and_Best_Practices.md)

*本書は現行ソースを読んだ上で作成した **実施指示用の計画**であり、リファクタリング実装・ベンチマーク実施・デッドコード確定を行ったとの報告ではない。*

---

## 7. 文書統合の整合性と参照方法
**最新結論**：ZIP一致・構造全量インデックスと実テストを根拠付きで完了し、以前の未確認だったソース可用性や再現性を解決した。ただし現行mainにはGolden改行バグの赤テスト3件が存在し、フルコンパイル/AST/性能/UDP/マルチOSは未完了。担当者は0章の優先問題を先に是正し、残りの証拠ゲートを満たすまでPhase12の完遂を宣言しない。C++ソース・コミット・プッシュなし。

本書は複数回の調査で後方に追加されていた章を、**分野別の一つの作業指示系統**へ再編した。各旧調査パスで記録されたコード位置・事実・仮説・実装ステップ・テスト要件・未完了証拠は上記章の対応する箇所に配置している。原文には複数時点の記述があるため、**同じ項目に違う完了表現がある場合は「未計測・未実行」を優先**する。

- 行番号はすべて固定SHA時点のものである。作業開始時は対象ブランチとの差分を確認する。
- 「改善候補」「要測定」は未確定。 `Confirmed` と `Not measured` を混同しない。
- 本文中の M-01～M-12、AUD-01～09、Phase 0～12は旧版から引き継いだ追跡ID。
- 本書の作成ではゲームのコード、CI、プロトコル、ビルド定義を変更していない。
