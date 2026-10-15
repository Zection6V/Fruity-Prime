# Fruity-Prime C++版：ムービー非同期タスクとScene寿命の競合によるクラッシュ修正指示

- 作成日：2026-10-14
- 重要度：**P0（Use-After-Free／セグメンテーション違反）**
- 対象：**C++版 Fruity-Prime**
- 調査基準：`Zection6V/Fruity-Prime` の `main`、`bc22f9db8985bafe301bcffd8666acbf8b2b5f73`
- C#比較対象：同リポジトリの `src/MphRead/`（仕様・既存動作の参照）
- 調査元：単独スポーンチェックで得られたクラッシュ解析報告、ならびにGitHub上の静的ソースコード確認
- 作成時の制約（2026-10-14）：当初は**修正指示書のみ作成し、ソース改変・コミット・プッシュ・CI起動・テスト実行をしない**依頼だった。その後の「これ読んで対応完遂」の依頼に基づく実装・ローカル検証記録を第14節に追記した。
- 実装上の既存方針：C#版を動作仕様の基準とする。ただしGCとC++オブジェクト寿命の差はC++側で補う。既存のC++20、DRY、SRP、同階層の `.hpp` / `.cpp`、`include/` ディレクトリ不使用の方針を守る。

## 1. 事象と根本原因

### 1.1 事象

`AlimbicTurret` 関連のスポーンチェック中、セグメンテーション違反が発生した。当初はタレット固有のホーミング／描画／生成処理が疑われたが、スポーンチェックをほかのテストと並列にせず単独で実行した結果、クラッシュの直接原因は**ストーリールームのカットシーンに付随する非同期オーディオ処理**と判明した。

報告されたクラッシュ経路は、`Scene::DoCleanup()` が停止要求を出した後にSceneの破棄が進み、スレッドプール／遅延スケジューラで実行中だったムービーの音声コルーチンが再開し、破棄済みのSceneを参照するもの。

**根本原因はタレットのホーミングではない。タレット側の動作を変更して症状を隠すことは禁止する。**

### 1.2 証拠の区分

| 区分 | 確認内容 | 確実性 |
|---|---|---|
| 実行時調査報告 | スポーンチェックを単独実行し、Scene破棄後の非同期オーディオ再開がクラッシュ原因と判明 | 報告済み（この指示書作成時に再実行はしていない） |
| ソースで確認 | `DoCleanup()` が `request_stop()` するのみでタスク完了を待機しない | 確認済み |
| ソースで確認 | `PlayMovie()` が `this` を捕捉した音声／映像処理を `TaskRun()` で起動する | 確認済み |
| ソースで確認 | `TaskRun()` は`void`で完了ハンドルを返さず、スレッドプール投入時以外にはトークンを検査しない | 確認済み |
| ソースで確認 | 音声側 `UpdateMovieAudio(std::stop_token)` が `token` を使わない | 確認済み |
| ソースで確認 | 音声ループが `_decoderCts` を毎回読み直す | 確認済み |
| ソースで確認 | `StopMovie()` が作業タスクの完了保証より先にOpenALソース／バッファを破棄する | 確認済み |
| ソースで確認 | `MapAudit::Dispose()` は `DoCleanup()` を呼ばずにSceneを破棄する | 確認済み |
| ソースで確認 | ストーリールームの着陸ムービー分岐は `Headless::Active()` を明示的に検査していない | 確認済み |
| ソースで確認 | `-maptest` は `TryHandleHeadless()` に存在するが `MapAudit` はウィンドウ／OpenGLを生成する | 確認済み |
| 要検証 | すべての実行入口でHeadlessが有効になるか、通常ムービー・連続ムービーにも同種の競合があるか | 未実機検証 |

### 1.3 クラッシュが成立する時系列

1. ストーリールームで着陸ムービー等が起動される。
2. `PlayMovie()` がデコード処理と音声・映像コルーチンを起動する。
3. 音声コルーチンが `co_await MovieTask::DelayOneMillisecond()` により中断される。
4. スポーンチェック終了などで `Scene::DoCleanup()` が呼ばれるか、監査側の `Dispose()` がSceneを破棄する。
5. 停止要求を発行しても、**すでに実行中または再開待ちの処理が終了した保証はない**。
6. コルーチンが再開して、`Scene*`、`_decoderCts`、`_audioHandle`、`_audioBufferIndex`、その他Sceneメンバにアクセスする。
7. **Use-After-Free／SIGSEGV／アクセス違反**となる。

`std::stop_source::request_stop()` は `join()` や完了待機の代替ではない。`stop_token` の検査を追加するだけでも、チェック直後に破棄が進むTOCTOU競合を排除できない。

## 2. 関連ソースと調査箇所

| ファイル | 主な確認箇所 | 役割／問題 |
|---|---|---|
| `src/MphRead.Native/Formats/Movie.cpp` | `Scene::PlayMovie()` 約3703～3815行 | 非同期デコード・音声・映像タスク起動。音声・映像が`this`参照を保持 |
| 同上 | `Scene::UpdateMovieAudio()` 約3821～3903行 | トークン未使用、OpenAL・Sceneメンバ参照 |
| 同上 | `Scene::StopMovie()` 約3905～3960行 | タスク完了待機より先にOpenAL資源解放 |
| 同上 | `Scene::UpdateMovieImage()` 約3983～4020行 | Sceneの映像状態とフェードを非同期更新 |
| 同上 | `MovieTask`／`TaskState`／`DelayScheduler` 約90～750行 | コルーチン継続・自律寿命管理 |
| `src/MphRead.Native/Formats/Movie.hpp` | `MPHREAD_SCENE_MOVIE_MEMBERS` 約78～124行 | Sceneムービー状態と `_decoderCts` |
| `src/MphRead.Native/Renderer.cpp` | `Scene::DoCleanup()` 約4080～4101行 | キャンセル要求のみ |
| 同上 | ストーリールームの着陸ムービー約580～600行 | Headless/監査除外がない |
| 同上 | `RenderWindow::EndScene()` 約6552～6565行 | Cleanup → GPU資源解放 → Scene破棄 |
| 同上 | `RenderWindow::OnClosing()` 約6650～6675行 | ウィンドウ終了側のCleanup |
| `src/MphRead.Native/NativeRuntime/System/Tasks.cpp` | `TaskRun()` 約155～163行 | `void`・fire-and-forget・キュー待機後のキャンセル非保証 |
| `src/MphRead.Native/Mods/Network/Testing/Maps/MapAudit.cpp` | `MapAudit::Dispose()` 約248～258行 | Scene破棄前のCleanup欠落 |
| 同上 | `MapAudit::Run()` 約1701～1763行 | 監査の生成・実行・Dispose |
| `src/MphRead.Native/Mods/ModEntry.cpp` | `-maptest`分岐 約2282～2343行 | テストへの入口。`TryHandleHeadless()`内でも監査は描画を使う |
| `src/MphRead.Native/Mods/Headless.cpp` | `Headless::Active()` | 単独フラグだけでは監査モードを必ず識別できない |
| `src/MphRead/Formats/Movie.cs` | `PlayMovie()`、`UpdateMovieAudio()`、`UpdateMovieImage()`、`StopMovie()` | C#側のゲームプレイ上の動作仕様 |
| `src/MphRead/Renderer.cs` | `DoCleanup()`、着陸ムービー条件 | C#の開始／終了条件参照 |

GitHub（基準コミット）：
- [Movie.cpp](https://github.com/Zection6V/Fruity-Prime/blob/bc22f9db8985bafe301bcffd8666acbf8b2b5f73/src/MphRead.Native/Formats/Movie.cpp)
- [Movie.hpp](https://github.com/Zection6V/Fruity-Prime/blob/bc22f9db8985bafe301bcffd8666acbf8b2b5f73/src/MphRead.Native/Formats/Movie.hpp)
- [Renderer.cpp](https://github.com/Zection6V/Fruity-Prime/blob/bc22f9db8985bafe301bcffd8666acbf8b2b5f73/src/MphRead.Native/Renderer.cpp)
- [Tasks.cpp](https://github.com/Zection6V/Fruity-Prime/blob/bc22f9db8985bafe301bcffd8666acbf8b2b5f73/src/MphRead.Native/NativeRuntime/System/Tasks.cpp)
- [MapAudit.cpp](https://github.com/Zection6V/Fruity-Prime/blob/bc22f9db8985bafe301bcffd8666acbf8b2b5f73/src/MphRead.Native/Mods/Network/Testing/Maps/MapAudit.cpp)

## 3. 修正要件（P0）：ムービータスクのスコープ管理

### 3.1 必須のライフタイム不変条件

**Invariant A**：Sceneが破棄される時点で、そのSceneにアクセスする可能性のある、実行中・実行待ち・遅延再開待ちのムービータスクは**0件**である。

**Invariant B**：新しいムービーを開始する前に、古いムービーの全タスクが終了しているか、古いセッションが新セッションに一切触れられないことを厳密に保証する。特に共有 `VxDecoder::Instance1/2` に対する同時Reset/Decodeは許可しない。

**Invariant C**：ムービーの音声タスクがOpenALソースやバッファにアクセスしている間、それらのOpenALリソースを破棄しない。

**Invariant D**：通常再生、スキップ、連続ムービー、部屋移動、アプリ終了、レンダラー切替、MapAudit終了、初期化失敗など、**すべての終了経路**が同一の終了手順を通る。

**Invariant E**：停止・同期にSceneの所有するロックを使用する場合、そのロックを待つタスクと終了待機の間にデッドロックが発生しない。

### 3.2 専用の `MoviePlaybackSession` / `MoviePlaybackLifetime` を導入

SRPを守るため、汎用タスクスレッドプールの挙動を場当たり的に変更せず、**ムービー再生の所有者をScene単位に明示**する。

推奨責務：
- ムービー再生世代／セッションIDの払い出し
- 各再生セッション固有の `std::stop_source`
- 起動予定・実行中・コルーチン再開待ちを含む作業件数の追跡
- **デコード上画面・デコード下画面・音声ループ・映像ループの全4系統**（下画面がない場合は3系統）の完了追跡
- 終了要求と完了待機を分離
- 例外の保存と呼び出し元への観測／ログ出力
- ムービーごとの稼働状態（Created, Starting, Running, Stopping, Stopped 等）の一元管理
- 同一Scene内での再生切替と冪等な終了

`MovieTask` はコルーチンがサスペンド後も生存する実装である。**外側の `TaskRun` ラムダが戻ったことだけを完了と見なしてはいけない。** `UpdateMovieAudio()`／`UpdateMovieImage()`／`Decode()` のコルーチン本体の `final_suspend` 相当まで追跡すること。

`MovieTask::GetAwaiter().GetResult()` は待機機構として参照できるが、同期コンテキストとコルーチン実行スレッドの関係を検証すること。UI/SceneGate保持中に無条件で待機する実装は不可。代わりに専用の非同期終了フェーズ＋終了確認でもよい。

採用するライフタイム管理は、必要最小限の補助クラスで実装する。クラス名は役割が分かるものとし、汎用 `TaskRun` APIの全面再設計は要求しない。

### 3.3 `this` の捕捉とセッションID

現状の `TaskRun([this] { ... })` はそのまま残さない。

- Sceneメンバへのアクセスが必要なタスクは、Sceneの生存保証が成立する間だけ実行可能とする。
- キャンセルトークンは**起動時の再生セッションから一度取得して固定**し、ループごとに `_decoderCts.load()` で新しいトークンを読み直さない。
- 非同期処理は旧世代のトークン／セッションIDを持ち、前の再生が停止した後に次の再生の状態を変更できないようにする。
- 新規起動処理と終了処理は同一のセッション状態管理を経由し、**終了開始後には新しいタスクを登録・起動しない**。
- 破棄処理はタスクの終了確認までSceneを保持し、単なる`weak_ptr`失敗時returnを同期の代替にしない。
- Sceneを強参照させる方式を採る場合は、Sceneと再生セッション間の参照循環を作らず、確実に解消する。

### 3.4 早期リターン・失敗処理

- ムービーファイルの欠落、デコード例外、初期フレーム不足、OpenAL初期化失敗などでも、起動済みのタスクを全て停止し、完了確認を行う。
- `PlayMovie()` に存在する `while (FramesQueued() < 4)` の無期限待機は、停止要求・デコード失敗・終了を観測して抜けられる設計にする。固定sleep時間だけのタイムアウト対策ではなく、進行・失敗・キャンセルを区別する。
- 停止済みのタスクが新しいムービーで共有デコーダの状態を書き換えないよう保証する。
- 開始途中で例外が発生した場合も、OpenAL、デコーダ、フェード、Muteカウンタが中途半端に残らないようにする。

## 4. 修正要件（P0）：安全な停止・解放順序

### 4.1 共通の終了プロトコル

ムービー終了とScene終了で呼び出せる「ムービーの物理停止」専用処理を設ける。**ゲーム進行を伴う `StopMovie()` とリソース後始末を明確に分離**する。

推奨フェーズ：

1. 当該再生セッションをStoppingに変更して新規タスク登録を禁止する。
2. セッションの停止トークンにキャンセル要求を送る。
3. 音声・映像・デコードのすべての作業が最終的に完了するまで待機する。キュー投入済みだが未開始の作業も必ず完了扱いになること。
4. コルーチン／タスクがOpenALとSceneメモリへアクセスしなくなったことを確認する。
5. 有効な音声デバイス／コンテキスト上でOpenALソース／バッファを解放する。
6. セッション所有のリソースと完了ハンドルを解放し、停止済みにする。
7. **Scene全体の終了時に限り**、残りのSFX・テクスチャ・GPUリソース等を既存の規約に従って解放する。

待機の間、呼び出しスレッドが保持している `SceneGate` や、コルーチン再開に必要なスケジューラのロックによってデッドロックを作らない。必要なら Sceneの終了開始と最終破棄を二段階にする。

### 4.2 `Scene::DoCleanup()`

現在の `_decoderCts->request_stop()` のみという後始末を置き換える。

- `_exiting` を先にセットしてゲーム側の新規ムービー起動を阻止する。
- **SFX／OpenAL等の終了より先にムービーの停止・タスク完了を確定**する。
- 通常の `StopMovie()` を直接呼ぶだけの修正は禁止。`StopMovie()` は `AfterMovie::LoadRoom`、`AfterMovie::EndGame` 等のゲーム進行を実行し得るため、Scene破棄中に新たな部屋読み込み・再帰Cleanupを始める危険がある。
- `DoCleanup()` の再入／重複呼び出しに対して安全であること。
- `DoCleanup()` を経由しないScene破棄でも、最終的にInvariant Aを満たす設計にする。デストラクタは最低限の防衛線とし、破棄後にキャンセルを頼むことはしない。

### 4.3 `Scene::StopMovie()` と連続再生

- 音声を停止する際は、旧音声コルーチンの完了後にOpenALバッファ／ソースを削除する。
- `AfterMovieId` によって次のムービーを始める場合、前のセッションを**完全に停止してから**共有デコーダをResetし、新しいセッションを起動する。
- Muteカウンタと `SfxMute` の復元が二重に実行されないこと。
- `AfterMovie::StartGame`、`AfterMovie::LoadRoom`、`AfterMovie::EndGame`、連続ムービー、SkipMovieなどの既存の結果を保持する。
- コルーチン側でフェード完了を検知した際に直接Sceneの汎用状態を書き換える部分（例：`SetFade()`）は、Sceneのゲーム更新スレッドへ**世代ID付き完了イベントを通知**する方式を優先する。終了済みの世代のイベントは破棄する。

### 4.4 `UpdateMovieAudio()` と `UpdateMovieImage()`

- 音声更新側の `(void)token;` を廃止し、停止条件を正しく消費する。
- `co_await` の**前後**、OpenAL呼び出しや共有Scene状態に触れる直前に停止／世代無効化を確認する。
- チェック単体では競合を防げないので、最終的な安全性は「所有者がタスク終了を確認してからSceneを解放する」不変条件で担保する。
- 音声バッファ配列、オーディオハンドル、映像フレーム、Fade状態のアクセススレッドと同期規約を明示する。SceneGateを新たに取得する場合も待機順序を整理し、デッドロックを避ける。
- 停止後に `AL::SourceQueueBuffers` や `AL::SourcePlay`、`SetFade` を実行してはいけない。

## 5. 修正要件（P1）：監査実行・Headless時のムービー抑制

### 5.1 `-maptest` とHeadlessフラグを混同しない

`ModEntry::TryHandleHeadless()` は起動時の分岐名であり、そこで走る `-maptest` が常に `Mods::Headless::Active() == true` とは限らない。

`MapAudit` は `RendererPlatform::CreateWindow()` とOpenGL Swapchainを生成している。したがって、**`Mods::Headless::Active()` のみのガードでは単独スポーンチェックを保護しきれない場合がある。**

推奨：
- Scene単位の **ムービー再生方針**（例：Normal / SuppressForAudit）を明示的に持たせる。
- `MapAudit` はScene生成直後・部屋読み込み前に SuppressForAudit を設定する。
- 実際のHeadless経路も同じ再生方針のSkip動作へ集約する。
- グローバルフラグの切替によって通常プレイ／ほかのSceneへ設定が漏れる設計は避ける。
- 監査対象の描画、スポーン、敵AI、武器、当たり判定、時間進行まで無効にしてはいけない。

### 5.2 ゲーム状態の意味を保持してスキップ

**ムービーだけを省略し、後続ゲーム状態を省略しない。**

着陸ムービーについては、`_playingLandingMovie`、Music::Pause、開始時のFade、`AfterMovie::StartGame` をセットしたままムービー処理だけ停止すると、`Scene::UpdateScene()` が永久に早期returnしてスポーンチェックが進まない危険がある。

推奨動作：
- 監査／Headlessでは着陸ムービーを最初から開始しないか、意味的に「ムービー完了」を即時／次のScene更新で適用する。
- `AfterMovie::StartGame` では `_playingLandingMovie` を確実に解除し、ゲームの進行とスポーンが継続する。
- `AfterMovie::LoadRoom` 等については従来と同じ状態遷移を引き起こすが、Scene破棄や部屋読み込みの再入が起きない安全な更新フェーズで処理する。
- 再生が抑制される場合、`VxDecoder`、ムービー用OpenALソース、テクスチャ、音声／映像コルーチンを生成しない。
- 通常のGUIプレイではムービーを再生する。C#版と同じムービー順序、音声、Fade、スキップ、終了後の状態を保つ。
- アドベンチャー以外にもムービー起動経路があれば、共通の開始地点で抑制できるようにする。

## 6. 修正要件（P1）：Scene破棄経路の統一

### 6.1 `MapAudit::Dispose()`

現行コードはおおむね次の順序：

```cpp
_itemPickupAudit.reset();
if (_scene)
{
    _scene->ReleaseGpuResources();
    _scene.reset();
}
_swapchain.reset();
_window.reset();
```

ここに `DoCleanup()` を追加するだけでも、**その `DoCleanup()` が全ムービータスクの完了を保証しない限り根本修正にはならない**。

監査側の修正仕様：
- Sceneの停止・ムービータスク終了を保証してからGPUリソースを解放し、`_scene.reset()` を行う。
- `Run()` の正常終了だけでなく、例外からの終了、部分初期化失敗、ウィンドウクローズ、監査対象の途中離脱でも同じ不変条件を守る。
- `MapAudit::Dispose()` を冪等にする。
- Sceneの破棄が完了した後で元のセッションのタスクが実行されることを許容しない。
- 終了処理のログ／ステータスは、クラッシュ時にも上書きされないようテストラン単位で保存する。

### 6.2 その他の終了経路

次のすべてを監査する：

- `RenderWindow::EndScene()`
- `RenderWindow::OnClosing()`
- `Scene::QuitGame()`
- ムービー途中のESC／スキップ
- 続けて次の部屋／ムービーをロード
- レンダラーバックエンド切替
- 監査・テストがSceneを直接管理／破棄する経路
- `PlayMovie()` の初期化途中の例外・途中終了
- アプリ終了／音声デバイス終了

終了処理の責務を複数場所で重複実装せず、ムービーセッション所有クラスの共通停止操作へ集約する。

## 7. C#版との比較と移植上の取り扱い

C#版 `Movie.cs` も `Task.Run(...)`、`CancellationTokenSource`、非同期音声・映像更新を使用する。`Renderer.cs` の `DoCleanup()` は `_decoderCts?.Cancel()` を呼び、厳格な完了待機をしていない。

ただし、C#の非同期クロージャはSceneを参照している間、GCによるScene回収を防ぐ。一方、C++版では `TaskRun([this]...)` の `this` はSceneを所有せず、`std::unique_ptr<Scene>` がリセットされると参照先が解放され得る。

**ここはC#の処理手順を字義通り移植してはならない箇所である。** 再生結果はC#に合わせる一方、C++固有の所有権・スレッド同期・解放順序を明示的に補う。

既存 `MovieTask` の自律生存（`SelfKeepAlive`）は「コルーチン自身を維持する」仕組みであり、「コルーチンが参照するSceneを維持する」ことを意味しない。

## 8. テスト方針：原因と回帰を分離する

### 8.1 最初はスポーンチェックのみ単独実行

ユーザーの原因特定手順と同じく、**ほかのテストと並列実行しない**。ログローテーションによりクラッシュ証拠が消えることを避ける。

- 修正後のビルドで単独スポーンチェックを起動する。
- 対象のストーリールームを含める。失敗した部屋、タレット出現地点を優先する。
- クラッシュログ、スタック、exit code、対象部屋、最後のエンティティ、ムービー世代、起動／停止した作業数を記録する。
- `MoviePlaybackSession` の停止完了ログを確認する。
- テスト終了時点で `in_flight_movie_tasks == 0` とする。
- 「たまたまクラッシュしなかった」だけではPASSにしない。解放順序・完了件数・後続タスク不存在を証明する。

### 8.2 決定的な非同期回帰テスト

可能な限り実時間の `sleep` に依存しない制御可能なスケジューラ／フェイク音声バックエンドで再現する。

| ID | ケース | 期待結果 |
|---|---|---|
| MOV-01 | 音声更新が1ms遅延待機中にSceneを終了 | コルーチン完了後にSceneが破棄され、UAFなし |
| MOV-02 | `TaskRun` に投入したが未起動の処理がある状態で終了 | キュー内の作業も安全にキャンセル・完了 |
| MOV-03 | 音声更新のOpenAL呼び出し中にStopMovie | 終了完了後に音声リソースが解放される |
| MOV-04 | 映像更新がフレーム完了通知を出す瞬間に終了 | 停止済みSceneへFade通知しない |
| MOV-05 | 連続ムービー A → B | AのタスクがBのトークン／デコーダ／OpenAL状態を参照しない |
| MOV-06 | `SkipMovie()` 直後に部屋切替 | タスク／Mute／Fadeの後始末が一度だけ成立 |
| MOV-07 | デコード中の例外・ファイル欠落・初期フレーム不足 | ハング・UAF・ハンドルリークなし |
| MOV-08 | `DoCleanup()` 二重呼び出し | 冪等でデッドロック、二重解放なし |
| MOV-09 | GUIで通常ムービー再生・連続再生・スキップ | C#版と同等の順序と音声・映像・Fade |
| MOV-10 | `Headless::Active()` のストーリールーム | ムービーなし、開始処理完了、シミュレーション継続 |
| MOV-11 | 描画ウィンドウを使う `-maptest` のストーリールーム | ムービーなし、GPU監査機能は維持、スポーン継続 |
| MOV-12 | MapAudit正常終了・異常終了・途中破棄 | Scene破棄前に全タスク完了 |
| MOV-13 | 何度も異なるストーリールームへ切替 | セッション間の旧タスク／Decoder状態の混入なし |
| MOV-14 | Linux ASan／Windows ASan等でMOV-01～13 | UAF、二重解放、データ競合に起因する不正メモリ操作なし |
| MOV-15 | シーン後始末中にエラーが発生 | 終了順序を維持し、診断情報を失わない |

**注意**：AddressSanitizerはデータ競合のすべてを検出するわけではない。可能な環境ではThreadSanitizerやスレッド制御テストを別途使用する。ASanとTSanの同時利用は前提としない。

### 8.3 テスト実行順序

1. P0修正の単体テスト（個別・逐次実行）。
2. クラッシュを起こしたスポーンチェック**単独実行**。
3. Headless監査のケースを単独実行。
4. GUIムービー再生・スキップ・連続ムービーの回帰テスト。
5. ASan等のメモリ検証。
6. すべての問題が解消した後に既存の総合テストへ展開。

**同一ログファイルに複数のテストを並列出力しないこと。** 各実行の標準出力／標準エラー／クラッシュダンプは一意の名前で保存し、CIでの既存ログローテーション条件を確認する。

## 9. 診断ログ要件

最低限、以下を**再生セッションIDおよびScene ID付き**で出力できるようにする。常時のフレームログではなく、開始／終了／失敗の低頻度イベント中心とする。

- `movie-start`: Scene、movie ID、headless/audit policy、セッションID
- `movie-task-start`: audio/image/decoder-top/decoder-bottom、作業ID
- `movie-stop-request`: 理由、要求元、進行中の作業数
- `movie-task-complete`: 作業ID、正常／cancelled／failed
- `movie-drained`: セッションID、残存タスク数0、経過時間
- `movie-resource-release`: OpenAL資源、decoder状態
- `scene-destroy`: 終了済みセッションID、残存ムービータスク数0
- `movie-suppressed`: Headless/Audit、後続アクションの種類

ポインタアドレスだけに依存して同一Sceneを識別せず、再利用による誤認を防ぐため世代付きIDを持つこと。

## 10. 禁止する場当たり的修正

以下は根本解決にならないため不可。

- `request_stop()` のみ追加して終える。
- `sleep_for(10ms/100ms/1s)` で破棄を遅延させる。
- `try/catch` でSIGSEGVのような不正メモリアクセスを握りつぶす。
- `this` を捕捉したタスクを `detach()` して、Scene破棄後も走らせる。
- タスクの「起動用ラムダ」の終了を、生成したコルーチン本体の終了とみなす。
- `_decoderCts` を次のムービー用に上書きして旧ループを動かし続ける。
- OpenAL音声リソースをタスク完了前に削除する。
- ムービーの終了時にゲーム進行を担う `StopMovie()` を破棄処理から無条件に呼ぶ。
- Headless時の `PlayMovie()` に単純な早期returnを置き、着陸ムービーのフラグ／Fade／ゲーム進行を未完了にする。
- `-maptest` が必ず `Headless::Active()` であると仮定する。
- `AlimbicTurret` のホーミング／敵AIロジックを本件の対処として変更する。
- 監査ログが競合・ローテーションする状態でスポーンチェックをほかのテストと並列実行する。

## 11. 推奨実装順序

1. 現行タスクの起動／コルーチン継続／破棄の正確な責務図を確定し、`TaskRun`から起動された全系統を識別する。
2. 再生セッション固有の停止トークンと**コルーチン本体まで含む完了追跡**を導入する。
3. Sceneが破棄される前に全タスクの完了が保証されるようにし、二段階の停止・終了待機を実装する。
4. 音声／映像更新で停止状態を消費し、終了済み世代からの状態更新を排除する。
5. `StopMovie()`・次ムービー開始・OpenAL解放の順序を修正する。
6. `Scene::DoCleanup()` と `MapAudit::Dispose()` その他の破棄経路を共通手順に統一する。
7. 監査・Headlessのムービー抑制とゲーム状態完了を実装する。
8. MOV-01～15を実装し、原因となったスポーンチェックを**単独で**再実行して確認する。
9. 通常GUI動作、音声・ムービー・メモリリークとCIの回帰確認を行う。
10. 変更理由、タスク所有権、停止／待機の同期規約、検証結果をMDに記録する。

## 12. 完了条件（Definition of Done）

- [x] `Scene::DoCleanup()` 後およびScene破棄前に、当該Sceneを参照する非同期ムービータスクが0であることをログと単体／ASan検査で確認した。
- [x] `StopMovie()` とムービー連続再生時に、旧世代の音声／映像／Decoder処理が完了してから次を開始することを確認した。
- [x] OpenALリソースが利用タスクの完了後に解放されることを通常／スキップ／連続再生ログで確認した。
- [x] セッション単体テストとMovieId 32→35検査で、後続ムービーが旧ムービーのトークン／タスク／バッファを引き継がないことを確認した。
- [x] MapAuditの実Scene cleanup例外とコンストラクタ部分初期化をRelease/ASanで注入し、Scene/GPU/windowの後始末が続行されることを確認した。cleanup段階ランナー単体でも後続段階実行・報告処理失敗後の継続・最初の例外保持を確認した。
- [x] `-maptest` とHeadlessの双方で抑制後に更新が進むことを確認した。Headless検査は240 simulation framesに加え、テスト用StorySaveでPlayerSpawnを有効化したUNIT4_LAND fixtureからslot 0が1/1 spawn。MapAudit SinglePlayerは874 frames・1/1 spawn。
- [x] GUIでのストーリームービー、フェード、スキップ、連続再生の映像をスクリーンショットで確認した。実音声の聴取は未確認。
- [ ] 実音声を人の耳で聴取して確認する。
- [x] 通常終了・診断スキップ・連続再生・ムービー中window close／重複CleanupをRelease/ASanで確認した。Releaseで `movies\\08.vx` 欠落を使ったScene統合の初期フレーム失敗も注入し、全タスクdrain、OpenAL/decoder解放、`AfterMovie::StartGame` 完了を確認した。
- [ ] OpenAL native function bodyの実行中にstop要求を重ねる故障注入は未実施。呼び出し境界での停止検査とは分けて扱う。
- [x] `-maptestmoviealcall` の実Scene統合で、audio taskを実 `AL::SourceQueueBuffers` 呼び出し境界に停止し、window closeから `OnClosing()` → `Scene::DoCleanup()` の停止要求を通した後にgateを解放した。実AL関数の呼び出し・return、task完了、全task drain、OpenAL解放の順をRelease/ASanで確認した。native関数の内部で停止したとは扱わない。
- [x] 一つのプロセス内でUNIT4_LAND → UNIT4_RM1 → UNIT4_RM2のSceneを順に作成・破棄し、各movie sessionのdrain後に次のSceneへ進む診断サイクルをRelease/ASanで確認した。RM1/RM2のMovieId 7はこの診断経路が直接開始するため、通常のStory room遷移確認とは区別する。
- [x] `-mapteststorymovietransition` でLANDの起動MovieをStartGameまで進めた後、Movie skip → 実 `AfterMovie::LoadRoom` → `RoomEntity::LoadRoom(false)` の順でRM1/RM2へ遷移し、3 sessionのdrain／OpenAL解放とScene破棄時の残task 0をRelease/ASanで確認した。各地点でMute acquire/releaseが1/1→2/2→3/3、SFX mute global全0、Fade/update pending 0もRelease/ASan双方で確認した。RM1/RM2の遷移後シーン画像も取得・読取した。door/teleporter入力を起点にする通常操作は別条件として未確認。
- [x] 実Sceneを使ったMapAudit cleanup例外／部分初期化の統合検査をRelease/ASanで実行した。
- [x] 元のスポーン検査コマンド `-homingspawncheck 15` を単独実行し、最新Release/ASanでexit 0・`SPAWN PASS 81/81 checks` と `remaining_tasks=0` を確認した。更新後fixtureではUNIT4_LANDのslot 0もspawnした。AlimbicTurret row 2の発射は15秒以内に再現できていない。
- [x] ムービー以外の敵AI／ホーミング挙動は変更していない。`HomingCheckScene` の差分は診断ハーネスの例外時Cleanupのみ。
- [x] C++20、SRP、DRY、既存ディレクトリ規約に沿って、セッション管理を `Mods/` に分離した。
- [ ] 新しいテストを含め、CIで再現性のある合否を出せる。

## 13. 最終指示

**本件は「AlimbicTurretの修正」ではなく、「カットシーンの非同期処理がSceneの寿命より長く存続する設計不備」の修正である。**

まずP0のタスク終了保証を完成させる。次に `MapAudit` とHeadless向けにムービーの副作用を抑制する。単独スポーンチェックで残存タスク数0を証明してから一般の回帰テストへ進む。

このMDは当初の調査結果と実装修正指示に、後日の実装・ローカル検証記録を追記したもの。デリバリ状態はGit履歴と最終報告を参照。

## 14. 実装・検証記録 (2026-10-15)

### 実装

- `Mods::MoviePlaybackSession` を追加し、Scene ID／ムービーセッション ID、停止トークン、decoder/audio/image コルーチンの完了を追跡する。タスク数は起動ラムダの戻りではなく、各 `MovieTask` の `final_suspend` 通過時に減算する。
- `Scene::DoCleanup()`、通常のムービー差し替え／停止、`MapAudit::Dispose()` で停止要求後に全タスクをdrainしてからdecoderとOpenAL資源を解放する。`Scene` デストラクタにも不変条件の検査を置き、残存タスクがあれば診断後に終了を止める。
- Headless／Auditでは映像・音声タスクを起動せず、`AfterMovie::StartGame` 等の後続アクションを通常の更新経路で処理する。Headless判定とMapAudit方針を分け、描画監査をHeadless扱いしない。
- セッション／Scene ID付きで開始、タスク完了、停止、drain、資源解放、抑制、Scene破棄を記録する。デコーダの進捗待機を条件変数化し、共有フレームカウンタと画像更新を同期する。
- MapAuditに明示的な `-maptestmovies` 実行経路を設け、通常のMoviePlaybackPolicyで映像・音声を再生できるようにした。`-hudshots` 時はムービー合成後のウィンドウ画像を15 Hzで保存する。
- `-maptestmoviechain` と `-maptestmovieskip` の診断経路を追加し、連続ムービーと30フレーム後のスキップ時にセッションdrain／次状態への遷移を再現できるようにした。通常プレイの既定動作は変えない。
- `-maptestmoviealcall` はaudio taskを実 `AL::SourceQueueBuffers` 呼び出し境界で止め、可視window closeで実Scene cleanupを開始する。停止要求の後にgateを解放して実APIを呼び出し、そのreturnとtask drainより前にOpenAL資源を解放しない順序を検査する。native APIのfunction body内でpauseする検査ではない。
- `-mapteststorymoviecycle` は一つのプロセスでUNIT4_LAND、UNIT4_RM1、UNIT4_RM2のSceneを順に作り直し、MovieId 7とcleanupを実行する。LANDは通常のroom-start経路でmovieを開始し、後続2室は診断専用に開始する。これはStoryの通常room遷移を再現する検査ではない。
- `-mapteststorymovietransition` はLANDの起動Movieを通常の `AfterMovie::StartGame` で終えた後に診断Movieを開始し、30 movie framesでskipしてproduction `AfterMovie::LoadRoom`／`RoomEntity::LoadRoom(false)` を通し、UNIT4_RM1とUNIT4_RM2へ同一Scene内で続けて移動する。各遷移の30 simulation frames後にwindow画像とscene target画像を保存する。後者は遷移後の部屋を表示した。door/teleporter入力でMovieを開始する操作自体はシミュレートしない。
- `-maptestmovieclose` は最初のムービー画像を可視windowに描画した直後、自身のWindow.Closeを呼んで `OnClosing()` → `Scene::DoCleanup()` → `MapAudit::Dispose()` を通すclose診断。decoder/audio/imageがin-flightの状態からdrainする経路を再現する。
- `-movieheadlesscheck` と `FruityPrime.MovieHeadless` を追加。UNIT4_LANDのHeadless経路でムービー抑制後に240 simulation framesが進み、movie/updateが残らないことを検査する。Headless用StorySaveでPlayerSpawn stateを有効化し、`AfterMovie::StartGame`後にslot 0が実際にspawnするところまで検査する。
- `MoviePlaybackSession::StartTask()` でタスク登録後のログ・完了処理準備・スケジューラ投入の例外も捕捉し、登録済みタスクを必ず完了扱いにしてdrainが永久待機しないようにした。Movie cleanupでは進捗通知解除とdecoder resetの失敗を個別記録し、OpenAL解放まで試行する。
- 映像コルーチンのFade完了は現在の `MoviePlaybackSession` にだけ通知し、Scene更新側がそのセッションから一度だけ消費する。停止中の既存イベントは破棄し、停止後に届く遅延イベントも受け付けない。セッション分離・一度だけの消費・停止後通知拒否を単体テストし、成功時の消費回数が1のまま増えないことも数える。
- `MapAudit::Dispose()` は後始末の各段階を個別に試行する。Scene停止・GPU解放・Scene破棄のどこかで例外が出ても後続解放を試み、最初の例外を最後に再送出する。`MapAudit::Run()` はcleanup失敗を `MAPCLEANUP` として記録して非ゼロ結果を返す。
- MapAuditコンストラクタにscope guardを置き、Scene/Swapchain作成後の初期化例外でも`Dispose()`を呼んでGPU/windowを解放する。`-maptestpartialinit`は`Scene::AddRoom`後に例外を注入する。`-maptestcleanupexception`は`Scene::DoCleanup()`の実例外を注入し、`MapAudit::Dispose()`がGPU解放とScene破棄を継続する経路を検査する。
- `HomingCheckScene` の構築途中で `SpawnOthers()` が失敗するとデストラクタは呼ばれないため、例外経路でも共通Cleanupを通すようにした。ASanで検出したテストフィクスチャ側のBeamProjectile→旧Scene参照を解消するための診断ハーネス修正であり、敵AI／ホーミング挙動は変更していない。
- 決定的な `FruityPrime.MoviePlaybackSession` テストを追加。制御したコルーチンの停止可視性、final-suspendまでdrainが完了しないこと、模擬OpenAL呼び出し完了前にリソース解放へ進まないこと、正常完了、非同期例外の捕捉・観測、セッション間の停止分離、停止後の新規登録拒否、完了通知の重複無視、兄弟タスクの停止・drainを検証する。さらに、TaskRunの全ワーカーを制御下で占有し、停止後にキューから開始した作業のstarterが実行されず、残タスク数が0になることを検証する。

### 検証結果

| 検証 | 結果 |
|---|---|
| `tools\\build\\build-cpp-and-log.bat msvc Release` | 成功、exit code 0。Native C++の `FruityPrime.exe` を生成 |
| クリーンMSYS2 MinGW Release build（GCC 16.2.0） | 新規build dirでCMake buildがexit code 0、`FruityPrime.exe`を生成。ログ `tools/build/out/msys2-movie-lifetime-validation-configure-20261015.log`、`tools/build/out/msys2-movie-lifetime-validation-build-20261015.log`。Qt deploymentは `catalogs.json` がなく翻訳を配置できない旨を警告した。 |
| クリーンMSYS2 MinGWのmovie lifetime選択CTest | `FruityPrime.MoviePlaybackSession` と `FruityPrime.MapAuditCleanup` が2/2 pass、0.78秒。隔離build dirにgame dataがないため `MovieHeadless` は未実行。ログ `tools/build/out/msys2-movie-lifetime-validation-ctest-20261015.log` |
| 既存MSYS2 MinGW incremental build | 既存の `.obj` 出力を上書きできず失敗。コンパイラ診断はなく、別の新規build dirでのclean buildと対象CTestは上記のとおり成功したため、この失敗はソース不具合の検証結果として扱わない。 |
| 指定Release build（MapAudit fault injection追加後、2026-10-15 06:24） | `tools\\build\\build-cpp-and-log.bat msvc Release` 成功、exit code 0。Native C++ `FruityPrime.exe` を再生成 |
| Release `FruityPrime.MoviePlaybackSession` | CTest 1/1 pass。final-suspend drain、模擬OpenAL呼び出し完了前の解放禁止、セッション分離、停止後登録拒否、重複完了通知、兄弟タスクdrain、未起動キュー項目のキャンセルを確認 |
| Release `FruityPrime.MapAuditCleanup` | CTest 1/1 pass。最初と2番目のcleanup段階を失敗させても全段階が順に実行され、診断handler自身の例外後も継続し、最初の例外を返すことを確認 |
| Release CTest全件（HomingSpawn Story fixture更新後） | 119/119 pass、Total Test Time 183.44秒。`MapAuditCleanup`、強化後の `MovieHeadless`、更新後の `HomingSpawn` を含む。テストは逐次実行。ログ `tools/build/out/msvc-Release/logs/ctest-full-homingspawn-fixture-20261015.log` と `tools/build/out/msvc-Release/Testing/Temporary/LastTest.log` |
| Release CTest全件（Movie close診断追加後、2026-10-15 06:13） | 119/119 pass、Total Test Time 186.88秒。`HomingSpawn` 52.51秒、`MovieHeadless` 0.48秒を含む逐次実行。ログ `tools/build/out/msvc-Release/logs/ctest-full-movie-close-probe-20261015.log` と `tools/build/out/msvc-Release/Testing/Temporary/LastTest.log` |
| 最新Release CTest全件（実Scene cleanup fault injectionとmovie scene-cycle追加後、2026-10-15 07:04） | 119/119 pass、Total Test Time 187.18秒。`HomingSpawn` 52.71秒、`MovieHeadless` を含む逐次実行。`tools/build/out/msvc-Release/Testing/Temporary/LastTest.log` |
| 最新Release CTest全件（Mute/Fade exact-once診断追加後、2026-10-15 08:13） | 119/119 pass、Total Test Time 187.00秒。`MoviePlaybackSession`のFade consume count testを含む逐次実行。`tools/build/out/msvc-Release/Testing/Temporary/LastTest.log` |
| Movie Fade通知／stop競合テスト追加後のRelease全CTest | 指定ビルド成功後、逐次実行119/119 pass、Total Test Time 186.89秒。`MoviePlaybackSession` は64回のbarrier同期競合を含む。ログ `tools/build/out/msvc-Release/logs/ctest-full-movie-fade-race-20261015.log` |
| 指定Release build（Headless fixture更新後、2026-10-15 05:30） | `tools\\build\\build-cpp-and-log.bat msvc Release` 成功、exit code 0。Native C++ `FruityPrime.exe` を再生成。 |
| Release `FruityPrime.MovieHeadless` / `-movieheadlesscheck`（fixture更新後） | CTest 1/1 pass。テスト用StorySaveでUNIT4_LANDのPlayerSpawn stateを有効にし、`MOVIE HEADLESS PASS room=UNIT4_LAND suppressed_completions=1 frames_advanced=240 movie_playing=0 update_pending=0 slot0_in_play=1 player_spawn_count=1 active_player_spawns=1`、stderr空を確認。ログ `tools/build/out/msvc-Release/logs/FruityPrime-20261015-053112-11120-native.txt` |
| Windows ASan `FruityPrime.MoviePlaybackSession` | CTest 1/1 pass。模擬OpenAL呼び出しdrainを含む |
| Windows ASan cleanup/lifetime選択セット（Movie.hpp変更後のclean rebuild） | `vcvars64.bat` 環境でASanターゲットをclean rebuild後、`MoviePlaybackSession`、`MapAuditCleanup`、`MovieHeadless` が3/3 pass。ASan報告なし。build log `tools/build/out/msvc-asan-movie/logs/asan-clean-rebuild-20261015.log`、CTest log `tools/build/out/msvc-asan-movie/logs/asan-targeted-ctest-20261015.log` |
| 最新Windows ASan対象テストとStory transition（2026-10-15 08:20） | ヘッダー変更を反映したclean targeted rebuild後、`MoviePlaybackSession`／`MapAuditCleanup`／`MovieHeadless` が3/3 pass。`-mapteststorymovietransition` もexit 0で、Mute取得/解放1/1→2/2→3/3、SFX mute globals 0、pending 0。native stderr 0 byte、ASan診断なし。console stderrのOpenGL 1282割当警告は2行。build log `tools/build/out/msvc-asan-movie/target-build.log`、CTest log `tools/build/out/msvc-asan-movie/Testing/Temporary/LastTest.log`、runtime log `tools/build/out/msvc-asan-movie/logs/FruityPrime-20261015-082108-16708.log` |
| Windows ASan fixture更新後 | `vcvars64.bat` 環境で `fruity_prime` を再ビルド成功。`MoviePlaybackSession`、`MapAuditCleanup`、更新後の `MovieHeadless` が3/3 pass、ASan報告なし。CTest `tools/build/out/msvc-asan-movie/Testing/Temporary/LastTest.log` |
| Windows ASan選択CTest（Movie close診断追加後、2026-10-15 06:13） | 3/3 pass。`LastTest.log` にfinal-suspend／模擬OpenAL drain、cleanup失敗時の全段階実行、UNIT4_LANDの240 frame更新とplayer spawnを記録。MovieHeadlessのnative stderrは0 byte。ログ `tools/build/out/msvc-asan-movie/Testing/Temporary/LastTest.log` |
| 最新Windows ASan選択CTest（MoviePlaybackSession / MapAuditCleanup / MovieHeadless、2026-10-15 07:00） | 3/3 pass、Total Test Time 1.83秒。ログ `tools/build/out/msvc-asan-movie/Testing/Temporary/LastTest.log` |
| Movie Fade通知／stop競合テスト追加後のWindows ASan選択CTest | `MoviePlaybackSession`、`MapAuditCleanup`、`MovieHeadless` 3/3 pass、1.55秒。VS ASan runtime DLLを含むPATHで実行。ログ `tools/build/out/msvc-asan-movie/Testing/Temporary/LastTest.log` |
| Release `-homingspawncheck 30 -noupdate -debuglog` 単独実行（2026-10-15 06:31） | exit code 0、`SPAWN PASS 84/84 checks`、stderr 0 byte。AlimbicTurret row 2はUNIT4_LAND／UNIT4_RM1／UNIT4_RM2の各30秒でも発射せず、元報告の発射条件は再現できていない。stdout `tools/build/out/msvc-Release/logs/homingspawn-30s-20261015-stdout.txt`、debug log `FruityPrime-20261015-063111-4844.log` |
| Windows ASan `FruityPrime.HomingSpawn`（Story fixture更新後） | CTest 1/1 pass、374.53秒。`SPAWN PASS 81/81 checks`、native stderr 0 byte、ASan診断なし。UNIT4_LANDはプレイヤーspawn後に検査可能になったが、AlimbicTurret row 2は15秒間発射なし。ログ `tools/build/out/msvc-asan-movie/Testing/Temporary/LastTest.log` |
| Release `-homingspawncheck 15 -noupdate -debuglog` 単独実行（2026-10-15 03:12） | exit code 0、`SPAWN PASS 80/80 checks`、stderr空。ログに `UNIT4_LAND` MovieId 7のHeadless抑制と全Scene `remaining_tasks=0` を記録 |
| Windows ASan `-homingspawncheck 15 -noupdate -debuglog` 単独実行（clean rebuild後） | CTest 1/1 pass、376.86秒。ASan native stderr 0 byte。ログ `tools/build/out/msvc-asan-movie/logs/asan-homingspawn-20261015.log`、`FruityPrime-20261015-051459-6688-native.txt` |
| Release `-maptest UNIT4_LAND -players 8 -seconds 1 -noupdate -debuglog -rhi opengl` | exit code 0、932 frames、8/8 spawn |
| Windows ASan 同 `-maptest UNIT4_LAND` | exit code 0、8/8 spawn、stderr空 |
| Release `-maptest UNIT4_LAND -mode SinglePlayer -players 1 -seconds 3 -noupdate -debuglog -rhi opengl -hudshots -shots <dir>` | `policy=audit after_action=start_game remaining_tasks=0` を記録し、874 frames・1/1 spawn。14枚を可視ウィンドウから取得し、00でARCTERRAルーム画面を確認。lit fraction最小0.0%のためMapAuditはexit code 1、総合PASSではない |
| Release `-maptest UNIT4_LAND -mode SinglePlayer -players 1 -seconds 15 -noupdate -debuglog -rhi opengl -hudshots -shots <dir> -maptestmovies` | MovieId 7の通常再生。decoder/audio/imageが完了し `remaining_tasks=0`、OpenAL解放を記録。movieフレーム40枚をウィンドウswapchainから取得し、05/20/35を目視。MapAudit全体はteleporter移動0/2とlit fraction最小0.0%の監査結果でexit code 2のため、総合PASSではない |
| Releaseの最新可視Movie 7再生・部屋遷移キャプチャ（2026-10-15 04:42） | 更新後のRelease exeで38 movie画像と27枚の部屋画像を取得。`movie-00/20/35` は異なる位置の惑星を描画し、`UNIT4_LAND-10/26` はムービー後の部屋、Ship、HUDを描画。`-shots` のworking directory解決後の画像先は `tools/build/out/msvc-Release/tools/build/out/msvc-Release/manual-land-movie-visible-20261015-0445/` |
| 最終Release可視Movie 7再生・部屋遷移キャプチャ（2026-10-15 05:13） | `-hudshots` の可視windowからムービー37枚、部屋20枚を保存。`UNIT4_LAND-movie-00/20` の惑星位置変化、`movie-36` の終端Fade、`UNIT4_LAND-03/12` の後続画面を読み取った。Movieのdecoder/audio/image task完了、OpenAL解放、Scene破棄 `remaining_tasks=0` を記録。ログ `FruityPrime-20261015-051356-30104.log`。`-shots` のworking directory解決後の画像先は `tools/build/out/msvc-Release/tools/build/out/msvc-Release/manual-land-movie-visible-final-20261015/` |
| Windows ASan 同通常ムービー再生 | exit code 2（同じMapAudit条件）、18フレーム取得。decoder/audio/image完了、`remaining_tasks=0`、OpenAL解放、stderr 0 byte、AddressSanitizer診断なし。フレーム05を目視 |
| Release `-maptestmoviechain` 可視キャプチャ | `tools/build/out/msvc-Release/manual-land-movie-chain-final-20261015/` のmovie-20/110/240を開いて読み、MovieId 32の宇宙船、MovieId 35の生物、終端Fadeを確認。最新clean ASan runtime log `FruityPrime-20261015-070550-20396.log` では32の4 taskが完了・drain・OpenAL解放してから35を開始し、35も4 task完了・drain・解放後に終了。MapAuditのplayer spawn / minimum lit率はfailとなるrunであり、MapAudit総合PASSとして扱わない。ASan native stderrは0 byte |
| ASan `-maptestmovieskip`（2026-10-15 07:07） | 30 movie frames後のskipでaudio/image完了、decoder cancel、drain 0、OpenAL解放を記録。994 simulation frames、slot 0は1/1 spawn。MapAuditはminimum lit 0.0%判定でexit 1のため総合PASSではない。`UNIT4_LAND-movie-00/05` と `UNIT4_LAND-10` をスクリーンショットから読み、Landing movieの惑星／船とskip後の部屋/HUDを確認。log `tools/build/out/msvc-asan-movie/logs/FruityPrime-20261015-070749-21688.log`、native stderr 0 byte、画像 `tools/build/out/msvc-asan-movie/manual-land-movie-skip-visual-20261015/` |
| Release 可視MapAuditへの左クリック入力 | `MovieId 7` の通常再生中にWin32の左ボタンdown/upメッセージをゲームwindowへ送り、直接 `SkipMovie()` を呼ばず `PlayerInput` のShoot押下経路を通した。start 04:00:13.633、stop 04:00:14.005、decoderはcancel、15 ms後にdrain 0・OpenAL解放。Movie画像3枚後に部屋／HUDへ移り、15秒後にslot 0が1/1 spawn。stderr空。MapAuditはteleporter／照明条件でexit code 2 |
| Release 可視MapAuditのmovie中window close | MovieId 7のmovie画像を撮影中、movie開始から約0.55秒でwindowに `WM_CLOSE` を送り、exit code 4で早期終了。3タスクin-flightから decoder/audio/image がcancel完了し6 msでdrain 0、OpenAL解放、Scene破棄も残0を記録。`OnClosing()` と `MapAudit::Dispose()` の連続Cleanup経路。exit 4は途中終了した監査の値であり、MapAudit総合PASSではない。`tools/build/out/msvc-Release/manual-land-window-close-20261015b/UNIT4_LAND-movie-03.png` を読み、惑星を含む実movie frameを確認 |
| Windows ASan 可視MapAuditのmovie中window close | 同じ `WM_CLOSE` をmovie画像撮影中に送り、decoder/audio/imageが全てcancel完了、drain 0、OpenAL解放、Scene破棄時も残0。process exit code 4、stderr 0 byte、AddressSanitizer診断なし。`tools/build/out/msvc-asan-movie/logs/FruityPrime-20261015-041423-14072.log` と `movie-window-close-asan-20261015-stderr.txt` に記録 |
| Release / Windows ASan `-maptestmovieclose`（2026-10-15 06:11 / 06:12） | MovieId 7のdecoder/audio/imageがin-flight=3のフレームでMapAudit自身がWindow.Closeを呼ぶ。`OnClosing()`→`Scene::DoCleanup()`で3件cancel、drain 0（Release 5 ms / ASan 2 ms）、OpenAL解放、Scene破棄時残0を確認。両プロセスは早期closeでMapAuditのspawn／描画条件を満たさずexit 1。ASan native stderr 0 byte。Release画像 `tools/build/out/msvc-Release/manual-land-movie-close-probe-20261015/UNIT4_LAND-00.png` を読み、close要求時に青い惑星を含むLanding Movieが描画済みと確認。Release log `tools/build/out/msvc-Release/logs/FruityPrime-20261015-061158-26480.log`、ASan log `tools/build/out/msvc-asan-movie/logs/FruityPrime-20261015-061228-19576.log` |
| MapAudit実Scene部分初期化失敗（Release / ASan、2026-10-15 07:04） | `-maptestpartialinit`で`Scene::AddRoom`後に例外を注入。コンストラクタguardが`Dispose()`を実行し、GPU資源0・Scene破棄時`remaining_tasks=0`を記録してから`MAPCRASH`を報告。両構成exit 1は意図した注入、stdoutに注入理由、native stderr 0 byte。Release log `tools/build/out/msvc-Release/logs/FruityPrime-20261015-070421-6852.log`、ASan log `tools/build/out/msvc-asan-movie/logs/FruityPrime-20261015-070444-25368.log`。stdout/stderrは同じ`logs/`内の`mapaudit-partialinit-*-final-*` |
| 実Scene `Scene::DoCleanup()` 例外後のMapAudit cleanup（Release / ASan、2026-10-15 07:04 / 07:04） | `-maptestmovieclose -maptestcleanupexception`でMovieId 7のdecoder/audio/imageをclose時に停止し、drain 0、OpenAL解放後に`MapAudit::Dispose()`内の実Scene cleanupへ一度限りの例外を注入。`cleanup-error operation=scene-stop`後もGPU解放とScene破棄（残0）まで継続し`MAPCLEANUP`を報告。ASan native stderr 0 byte。Release log `tools/build/out/msvc-Release/logs/FruityPrime-20261015-070430-10232.log`、ASan log `tools/build/out/msvc-asan-movie/logs/FruityPrime-20261015-070458-15692.log`。Release画像 `tools/build/out/msvc-Release/manual-land-movie-cleanup-exception-20261015/UNIT4_LAND-00.png` |
| `-mapteststorymoviecycle`（Release / ASan、2026-10-15 07:09 / 07:00） | 一つのプロセスでUNIT4_LAND、UNIT4_RM1、UNIT4_RM2のSceneを順に作成・破棄。全室で `SourceQueueBuffers` 境界gate → window close/stop → gate release → 実API return → task drain 0 → OpenAL解放 → Scene破棄を確認。roomごとにScene/session ID 1/1、2/2、3/3。Release exit 0、ASanは `MAPMOVIECYCLE PASS` / `process exiting`、native stderr 0 byte。後続2室のMovieId 7は診断経路から開始。Release log `tools/build/out/msvc-Release/logs/FruityPrime-20261015-070904-27200.log`、stdout `story-movie-cycle-release-final-stdout.txt`、ASan log `tools/build/out/msvc-asan-movie/logs/FruityPrime-20261015-070015-28976.log` |
| Release / Windows ASan `-mapteststorymovietransition`（2026-10-15 08:00） | 起動Movie 7を30 movie framesでskipし、`AfterMovie::StartGame` を完了してから診断Movie 7を開始。RM1とRM2への各Movie skipで実 `AfterMovie::LoadRoom` → `RoomEntity::LoadRoom(false)` を通り、session 1/2/3ごとにtask drain 0・OpenAL source/buffer解放、Scene破棄時も残0。Release/ASanとも `MAPMOVIETRANSITION PASS`、Start-Processで待機して取得したexit 0。ASan native stderr 0 byte。Release log `tools/build/out/msvc-Release/logs/FruityPrime-20261015-080005-15536.log`、ASan log `tools/build/out/msvc-asan-movie/logs/FruityPrime-20261015-080018-30076.log`。各遷移後30 simulation framesのscene target画像 `manual-story-movie-transition-20261015-final/UNIT4_RM1-story-scene.png` / `UNIT4_RM2-story-scene.png` を開き、両部屋の地形・武器表示を確認。window画像は遷移Fade表示のため白く、scene target画像を視覚確認に使用した。door/teleporter操作を起点にした実プレイ入力は未確認。 |
| Release / Windows ASan Story movie Mute/Fade one-shot監査（2026-10-15 08:10 / 08:21） | `-mapteststorymovietransition` がLANDのStartGame後、RM1、RM2の各地点で取得/解放数 `1/1`、`2/2`、`3/3` を検査。各点で `SfxMute=0`、Long/Timed/ForceField Mute counters=0、Mute held=0、movie/fade update pending=0。Release/ASanとも3室transition exit 0、同じカウントを記録。Release log `tools/build/out/msvc-Release/logs/FruityPrime-20261015-081058-23952.log`、ASan log `tools/build/out/msvc-asan-movie/logs/FruityPrime-20261015-082108-16708.log`。Fade単体テストでは完了を初回だけ消費しcount=1のまま、2回目および停止後イベントでは増えない。Release/ASanの惑星movie frame、RM1/RM2 scene target画像を開いて読取済み。ASan native stderr 0 byte、console stderrにはOpenGL 1282割当警告が2行。 |
| Release `-maptest UNIT4_RM1 -mode SinglePlayer -players 1 -seconds 1 -noupdate -debuglog -rhi opengl -hudshots -shots manual-story-transition-room-baseline-20261015` | 725 frames・1/1 spawn、lit first/min 92.3%、mean 99.0%。MapAudit全体はteleporter移動0/2のMAPFAIL。`manual-story-transition-room-baseline-20261015/UNIT4_RM1-05.png` を開き、室内・プレイヤー・Energy HUDを確認した。 |
| Release Scene統合のムービー初期化失敗（MovieId 7、`movies\\08.vx`欠落） | 3717ファイルをハードリンクした一時AMHP1ツリーからmovie fileだけ除いてMapAuditを起動。decoder taskがFileNotFoundでfailed、4初期frame不足を検知し `startup-failure` でdrain 0、OpenAL/decoder解放、`after_action=start_game` の抑制完了、874 frames・1/1 spawnまで進行。process exit 1はMapAuditの既知lit-fraction判定（min 0.0%）で、クラッシュではなく正常な `process exiting`。log `tools/build/out/msvc-Release/logs/FruityPrime-20261015-053321-14236.log`、stderr `FruityPrime-20261015-053321-14236-native.txt`。元の `paths.txt` は実行後ハッシュ一致で復元 |
| ASanの中間incremental実行で出た `Scene::~Scene` access violation | `Movie.hpp` のSceneメンバー変更後、`Movie.cpp.obj` だけが更新され、`Scene.cpp`・`MapAudit.cpp`・`HomingCheckScene.cpp` は変更前の時刻だった。Sceneの異なるクラスレイアウトを含む古い `.obj` の混在と判断し、ASan構成をclean rebuildした。clean後はMoviePlaybackSession/MapAuditCleanup/MovieHeadlessが3/3、HomingSpawnが1/1で通過し、native stderrも空。再現するコード上のアクセス違反としては扱わない |
| MapAuditの段階別cleanup再検証（2026-10-15 04:24） | MovieId 7再生中のWM_CLOSEでOnClosing→Disposeを再実行し、task drain後のOpenAL解放とScene破棄時残0を確認。Release画像 `tools/build/out/msvc-Release/manual-land-window-close-20261015c/UNIT4_LAND-movie-03.png` を読み、停止直前も惑星を含むLanding Movieが表示中と確認。この時点ではcleanup例外を未注入。後のRelease/ASan注入結果は上記2026-10-15 07:04の行を参照 |
| 中間incremental Windows ASan run | `Movie.hpp`変更後に古いobjectが混在したrunはclean ASan構成の根拠にしない。clean rebuild後は選択CTest 3/3、Story movie cycle、MovieId 32→35 chain、MovieId 7 skipをASanで再実行し、native stderr 0 byteを確認した。詳細は上記の最新runtime rowsと `Testing/Temporary/LastTest.log` |
| `git diff --check` | pass。CRLF変換に関するGit警告のみ |
| セッション分離／停止後登録拒否／重複完了通知／失敗時の兄弟タスクdrain／未起動キュー項目キャンセル／模擬OpenAL呼び出しdrainテスト（Release / Windows ASan） | CTest 1/1 pass |
| 最新Release CTest内の `FruityPrime.HomingSpawn` | 逐次実行で52.71秒、`SPAWN PASS 81/81 checks`。UNIT4_LANDのStory fixtureがslot 0をspawnし、旧 `room not usable here` は解消。row 2の単独30秒延長検査は3部屋とも発射なし。全119件を通した同一実行で確認 |

### MOV-01～15 coverage audit（2026-10-15）

| ID | 判定 | 現在の証拠と残る差分 |
|---|---|---|
| MOV-01 | 部分確認 | `MoviePlaybackSession` の制御コルーチンで停止から `final_suspend` までdrainし、実Sceneのwindow-closeでも3 taskを停止・drainした。実audio loop固有の1 ms待機を狙って停止する専用注入はない。 |
| MOV-02 | 確認済み | `CheckQueuedTaskCancellation` が全TaskRun workerを占有し、停止後にqueueから始まるtaskのstarter未実行と残数0を検証。 |
| MOV-03 | 部分確認 | mock active-call drainと実 `AL::SourceQueueBuffers` 呼び出し境界でのScene stopをRelease/ASanで確認。OpenAL native関数内部で停止する故障注入は未実施。 |
| MOV-04 | 確認済み | session単体テストでFade完了通知のsession分離、一度だけの消費、stop後および遅延通知の拒否を検証。追加でFade通知とstop要求をbarrier同期で64回競合させ、Release/ASanで停止後のpending通知なしを確認。 |
| MOV-05 | 確認済み | MovieId 32完了・drain・OpenAL解放後に35を開始するRelease/ASan chain logと別クリップのスクリーンショットを確認。 |
| MOV-06 | 部分確認 | 可視windowへの通常左クリック入力と診断skipの双方でMovieId 7を停止し、drain後にStartGame／LAND画面へ進んだ。さらに起動MovieのStartGame後にMovie skip → production `AfterMovie::LoadRoom` → `RoomEntity::LoadRoom(false)` をRelease/ASanで通し、RM1/RM2への部屋切替、task drain、OpenAL cleanupを確認。LAND後、RM1後、RM2後にMute取得/解放 `1/1`、`2/2`、`3/3`、SFX mute globals 0、update pending 0をRelease/ASan双方で確認。Fade完了のsession単体検査は消費カウントが一度だけ1になることを確認。door/teleporter操作を起点にする入力は未確認。 |
| MOV-07 | 確認済み | decoder task例外の単体テスト、`movies\\08.vx`欠落、初期4 frame不足を実Sceneで注入し、drain・decoder/OpenAL解放・StartGame継続を確認。 |
| MOV-08 | 確認済み | movie中のWM_CLOSEが`OnClosing()`と`MapAudit::Dispose()`のcleanupを重ねてもdrain 0、資源解放、Scene破棄まで進むことをRelease/ASanで確認。 |
| MOV-09 | 部分確認 | 通常再生、fade、chain、skip、skip後画面は可視window screenshotで読取済み。人耳による音声確認とC#版との直接A/B比較は未実施。 |
| MOV-10 | 確認済み | `FruityPrime.MovieHeadless` が240 simulation frames、movie/update残0、slot 0の1/1 spawnを検証。 |
| MOV-11 | 部分確認 | SinglePlayer MapAuditでaudit policy抑制後にroom renderと1/1 spawnを確認。MapAudit全体はlighting/teleporter基準のため総合PASSではない。 |
| MOV-12 | 確認済み | MapAudit正常なclose、movie中途中破棄、部分初期化例外、Scene cleanup例外をRelease/ASanで確認。 |
| MOV-13 | 確認済み | 一つのScene内でLAND→RM1→RM2のproduction LoadRoom遷移を行い、session 1/2/3が順にdrain・OpenAL解放してから次へ進むことをRelease/ASanで確認。遷移後のscene target画像も両部屋で読み取り済み。遷移を始めるdoor/teleporterのプレイヤー操作はシミュレートしていない。 |
| MOV-14 | 部分確認 | 最新clean targeted Windows ASan buildで選択CTest 3/3と3-room Story transitionを実施。MOV-01～13全ケースをASan一括実行したものではなく、TSanもこの環境では実行不可。 |
| MOV-15 | 確認済み | cleanup段階handler例外と実Scene `DoCleanup()` 例外を注入し、後続GPU解放／Scene破棄と最初の診断保持を確認。 |

主な実行ログ：最新Releaseビルド `tools/build/out/build-cpp.log`、最新Release全件CTest `tools/build/out/msvc-Release/Testing/Temporary/LastTest.log`（119/119 pass、186.46秒）、最新ASan選択CTest `tools/build/out/msvc-asan-movie/Testing/Temporary/LastTest.log`（3/3 pass）。Story transitionのRelease／ASan runtime logとscene target画像は上記各行を参照。

fixture更新後のHeadless Releaseログは `tools/build/out/msvc-Release/logs/FruityPrime-20261015-053112-11120-native.txt`、ASanの選択CTest最新結果は `tools/build/out/msvc-asan-movie/Testing/Temporary/LastTest.log`。MovieId 7のmissing-file統合検査ログは `FruityPrime-20261015-053321-14236.log` と `FruityPrime-20261015-053321-14236-native.txt`。

最終Releaseの可視movie画像は `manual-land-movie-visible-final-20261015/` と `manual-land-movie-chain-final-20261015/` に保存した。Movie chainの最新ASan runtime logは `FruityPrime-20261015-070550-20396.log`、skipのASan runtime logと画像は `FruityPrime-20261015-070749-21688.log` と `manual-land-movie-skip-visual-20261015/` にある。3室cycleのRelease logは `FruityPrime-20261015-070904-27200.log`、ASan logは `FruityPrime-20261015-070015-28976.log`。全119件の最新Release CTest結果は `Testing/Temporary/LastTest.log` にある。

LAND部屋の画像出力も `-maptest UNIT4_LAND -players 8 -seconds 1 -noupdate -debuglog -rhi opengl -shots <dir>` で取得し、画面を読み取った。`tools/build/out/msvc-Release/manual-land-map-shot-20261015-025349/UNIT4_LAND-00.png` は青緑色の室内と岩場を描画している。これはMapAudit後の部屋画像で、監査方針によりムービーを抑制した結果であり、Landing Movieの再生フレームではない。

スポーン検査ログでは `UNIT4_LAND` がroom id 77として読み込まれ、`Renderer.cpp` の分岐が `Movie::ArcterraLanding` を選び、`Metadata/FrontendMeta.hpp` で値7に対応することを確認した。実行ログは `movie-suppressed scene_id=8 session_id=1 movie_id=7 policy=headless after_action=start_game remaining_tasks=0`、続けて `scene-destroy ... remaining_tasks=0` を記録している。したがって、LANDは今回の経路でArcterra Landing Movieを要求しており、Headlessでは非同期再生せず開始処理へ進む。

通常再生の目視確認では、MapAuditに `-maptestmovies` を指定して抑制を解除し、`-hudshots` で可視ウィンドウを開いた。movie ID 7でdecoder/audio/imageの3タスクが開始・完了し、movie停止時に `remaining_tasks=0`、OpenALソース／バッファ解放、decoder resetを記録した。可視ウィンドウのswapchainから15 Hzで保存した40枚のうち、`tools/build/out/msvc-Release/manual-land-movie-visible-20261015-030751/UNIT4_LAND-movie-05.png`、`-20.png`、`-35.png` を開き、青い惑星が動く実映像とフレーム39の白い終端画面を確認した。これはWindows Graphics Captureではなくゲームのウィンドウ画像を読む撮影経路である。更新後のRelease exeでも再撮影し、`tools/build/out/msvc-Release/tools/build/out/msvc-Release/manual-land-movie-visible-20261015-0445/UNIT4_LAND-movie-00.png`、`-20.png`、`-35.png`、`UNIT4_LAND-10.png`、`-26.png` を開いた。ムービー内で惑星の位置が変わり、終端後にUNIT4_LANDの部屋とHUDへ移ることを確認した。最新runではmovie-00～37の38枚と部屋画像27枚を保存し、ログ上も3 task drainとScene破棄時の残タスク0を確認した。

連続再生ではMovieId 32をdrainしてから35へ切り替えた。`tools/build/out/msvc-Release/manual-land-movie-chain-20261015/UNIT4_LAND-movie-80.png`、`-100.png`、`-240.png` 等を読み、2本目の異なる映像と終端の白フェードを確認した。直接 `SkipMovie()` を呼ぶ診断ハーネスでは30 movie frames後に、通常の `PlayerInput` を通る可視検査では再生開始後にゲームwindowへの左クリックメッセージでスキップした。後者はMovieId 7が0.37秒で停止、decoder cancel後にdrain 0となり、`tools/build/out/msvc-Release/manual-land-movie-key-skip-20261015-0430/UNIT4_LAND-movie-00.png` から `UNIT4_LAND-05.png`、`-10.png`、`-24.png` への撮影画像遷移でLAND画面／HUDとゲーム進行を確認した。15秒後のMapAudit集計は1/1 spawn。これは物理マウスではなく、実windowの通常入力経路へ送ったOS入力メッセージである。MapAudit終了コード2はteleporterと照明条件によるもので、movie sessionはdrain済み。

別に `-maptestmovies` を付けないSinglePlayer MapAuditも実行した。`policy=audit after_action=start_game remaining_tasks=0` の後に874 frames進み、1/1 spawnを記録し、可視画像 `tools/build/out/msvc-Release/manual-land-suppressed-singleplayer-20261015-032147/UNIT4_LAND-00.png` にARCTERRAルーム画面を確認した。これはAudit方針での抑制後にMapAuditがルームを進める証拠であり、真正Headlessの受け入れとは分けて扱う。

HomingSpawn Story fixtureをテスト用StorySaveでPlayerSpawn stateが有効になるよう更新した。最新のRelease/ASan単独検査ではUNIT4_LANDをロードでき、slot 0のspawn後に検査を完了した。追加のRelease 30秒検査ではAlimbicTurret row 2の配置を含むUNIT4_LAND、UNIT4_RM1、UNIT4_RM2のいずれも発射なしだったため、元報告のタレット発射条件そのものは再現したとは扱わない。別途、可視SinglePlayer MapAuditのスキップ後検査はslot 0が1/1 spawnしたが、タレットの発射確認とは別である。Battleモードの8-player `-maptest` もLanding Movie再生の検証には使っていない。

初回のASan全スポーン検査では、`HomingCheckScene::SpawnOthers()` がspawn失敗でコンストラクタから例外を投げた後、SceneのCleanupをしないままBeamProjectileがSceneを参照する別のUse-After-Freeを検出した。構築失敗Cleanupに加え、StorySaveのPlayerSpawn stateを診断fixtureで有効化した後の全スポーン検査は、Release/ASanとも81/81で完走し、ASan診断はなかった。

### 残る受け入れ確認

- Release/ASanでmovie、fade、skip、chain、window closeと遷移後の映像を撮影し、画像を開いて読取済み。`-mapteststorymovietransition` のscene target画像はRM1/RM2の地形と武器を描画している。対応するwindow画像は白い遷移Fadeを写すため、scene target画像を部屋の視覚確認に使った（scene targetにはHUDを含まない）。RM1の通常MapAudit画像では室内とEnergy HUDを確認した。実音声の人による聴取は未確認。
- native OpenAL関数本体の実行中にstopする故障注入、door/teleporterのプレイヤー操作を起点にするStory遷移、C#版との直接A/B、MOV-11のMapAudit全体PASS、MOV-01～13全ケースの一括ASan、ThreadSanitizer、CI置換runは未実施。TSanはWSLにLinuxディストリビューションがなく、MSYS2 MinGW/UCRT g++の `-print-file-name=libtsan.a` が `libtsan` のまま、clang64の `clang++.exe` も見つからなかったため、このWindows環境では実行できない。CIの結果は今回のpush後に確認する。
- `MoviePlaybackSession` の決定的テストは中断中のコルーチンdrain、模擬OpenAL呼び出し完了前の解放禁止、正常／例外完了、セッション分離、停止後登録拒否、重複完了通知、Fade完了通知のセッション分離・一度だけの消費・停止後通知拒否、兄弟タスク停止、未起動キュー項目のキャンセルを確認する。Movie中の `WM_CLOSE` と `-maptestmovieclose` により `OnClosing()` と `MapAudit::Dispose()` の複数Cleanupも検証した。`MapAuditCleanup` 単体テストは2段階のcleanup失敗とfailure reporter例外を注入し、後続段階の実行と最初の例外保持を確認する。Release Scene統合でmovie file欠落／初期4フレーム不足も検証し、今回MapAudit実Sceneの部分初期化とcleanup例外もRelease/ASanで再現した。`SourceQueueBuffers` 呼び出し境界でstop要求を挟み、実関数return後にtask drainとOpenAL解放する統合検査も実行した。未確認なのはnative AL関数本体の実行中停止、通常のStory room間遷移、人による実音声聴取。
- Headless `UNIT4_LAND` では `movie-suppressed ... policy=headless after_action=start_game remaining_tasks=0` 後に、テスト用StorySaveのspawn stateを有効化したSceneで240 frames後のslot 0 `inPlay` と1/1 active spawn pointを確認した。HomingSpawnにも同じStory fixture設定を適用してroom not usableを解消したが、AlimbicTurret row 2の発射そのものは15秒以内に再現していない。SinglePlayer MapAuditの1/1 spawnは引き続き別のAudit経路の証拠である。

Native C++の非同期Scene寿命修正と診断を反映した。指定Release build成功、全CTest 119/119 pass、clean targeted Windows ASan CTest 3/3 pass、clean MSYS2 MinGW Release buildと対象CTest 2/2 pass。Release/ASanのmovie/skip/cleanup例外/3室Story transition runtime検査でtask drain 0と資源解放を確認した。Story transitionではMute acquire/releaseが1/1→2/2→3/3となり、SFX mute globalsとmovie/fade pendingが全て0。Fade完了のunit testはconsume countが一度だけ1になる。LAND MovieId 7、RM1/RM2 scene targetをスクリーンショットで撮影して読取し、RM1のHUD画像も確認した。残る受け入れ確認は人による実音声の聴取、AlimbicTurret row 2の元報告発射条件（30秒×3部屋でも未再現）、MOV-03のnative OpenAL関数本体での停止、通常プレイヤー入力から始まるdoor/teleporter遷移、MOV-09のC#版との直接A/B、MOV-11のMapAudit全体PASS、MOV-14の全ケースASan/TSan、push後のCI結果。Windows TSan runtimeは利用できない。コミット／プッシュ状態はこの実装記録の更新後にGit履歴と最終報告で確認する。
