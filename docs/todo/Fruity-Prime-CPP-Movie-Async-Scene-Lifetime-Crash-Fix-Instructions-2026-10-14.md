# Fruity-Prime C++版：ムービー非同期タスクとScene寿命の競合によるクラッシュ修正指示

- 作成日：2026-10-14
- 重要度：**P0（Use-After-Free／セグメンテーション違反）**
- 対象：**C++版 Fruity-Prime**
- 調査基準：`Zection6V/Fruity-Prime` の `main`、`bc22f9db8985bafe301bcffd8666acbf8b2b5f73`
- C#比較対象：同リポジトリの `src/MphRead/`（仕様・既存動作の参照）
- 調査元：単独スポーンチェックで得られたクラッシュ解析報告、ならびにGitHub上の静的ソースコード確認
- 作業制約：**修正指示書のみ作成する。現段階ではソース改変・コミット・プッシュ・CI起動・テスト実行をしない。**
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

- [ ] `Scene::DoCleanup()` 後およびScene破棄前に、当該Sceneを参照する非同期ムービータスクが0であることを確認できる。
- [ ] `StopMovie()` とムービー連続再生時に、旧世代の音声／映像／Decoder処理が完全終了する。
- [ ] OpenALリソースは利用タスクの完了後に解放される。
- [ ] 後続ムービーが旧ムービーのトークン／タスク／バッファを引き継がない。
- [ ] `MapAudit::Dispose()` を含む全破棄経路がライフタイム不変条件を満たす。
- [ ] `-maptest` と真正のHeadlessの双方で、カットシーンの非同期再生を抑制してもゲーム状態が進行する。
- [ ] GUIでのストーリームービー、フェード、サウンド、スキップ、連続再生の挙動が維持される。
- [ ] 故障時、キャンセル時、通常終了時、複数回Cleanup時にハング・デッドロック・UAFがない。
- [ ] 元のスポーンチェックを単独実行して正常終了し、ログに`movie-drained`と`remaining_tasks=0`を確認できる。
- [ ] ムービー以外のレンダリング・敵AI・ホーミング・スポーンに不必要な挙動変更がない。
- [ ] C++20、SRP、DRY、既存ディレクトリ規約を満たす。
- [ ] 新しいテストを含め、CIで再現性のある合否を出せる。

## 13. 最終指示

**本件は「AlimbicTurretの修正」ではなく、「カットシーンの非同期処理がSceneの寿命より長く存続する設計不備」の修正である。**

まずP0のタスク終了保証を完成させる。次に `MapAudit` とHeadless向けにムービーの副作用を抑制する。単独スポーンチェックで残存タスク数0を証明してから一般の回帰テストへ進む。

このMDは調査結果に基づく**実装修正指示**であり、修正実装・テスト実行・コミット・プッシュ済みであることを示すものではない。
