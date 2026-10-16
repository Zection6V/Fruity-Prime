# Fruity-Prime develop13：Normal / Alt 変形ネットワーク同期の実装調査と改善設計

- 調査日：2026-10-16
- 対象：[`Zection6V/Fruity-Prime` / `develop13`](https://github.com/Zection6V/Fruity-Prime/tree/develop13)
- 固定参照コミット：[`0bdbd611442a1092a97f40d5bd3db7f18e29abb5`](https://github.com/Zection6V/Fruity-Prime/commit/0bdbd611442a1092a97f40d5bd3db7f18e29abb5)
- 範囲：C++版 `src/MphRead.Native`。Normal → Alt（Morph）と Alt → Normal（Unmorph）の**形態変化**。座標・姿勢の一般的な Transform 行列レプリケーションの調査ではない。
- 作業：GitHub上の実装・テスト・仕様文書の静的調査。コード改変、ビルド、実プレイでの測定、コミット、プッシュは**実施していない**。

## 1. 結論

提案されている「**変形方向・変形開始時刻をサーバー権威で確定し、他のクライアントが同じ開始時刻から同じアニメーションを再生する**」方式は、Fruity-Primeで採用する価値がある。ただし、現在も方向と量子化された進行度を**サーバー確定スナップショット**で配信しており、変形同期そのものが未実装という状態ではない。

**最も効果が見込める変更は、進行度の断片的なスナップショット再現から、サーバー確定の `StartFrame` + `TransitionId` に基づく再現への移行である。** ただし、ネットワークゲームの物理・当たり判定・攻撃権威までアニメーション時刻で置き換えてはいけない。

推奨構成は次のようにする。

1. **クライアントの操作は予測入力**：ローカルの変形をすぐ開始する。既存 `IntentButtons::Morph` と押下履歴を送信する。クライアントが申告する時刻は、予測との対応付け用であって権威ではない。
2. **サーバーの実行結果を正本にする**：入力を受け取った瞬間ではなく、サーバー側 `PlayerEntity` が実際に変形を受理・開始した60 Hzシミュレーションティックで `StartFrame` と単調増加する `TransitionId` を採番する。
3. **サーバーから変形イベントを配信する**：方向、開始ティック、世代・ライフ、イベントIDを一体として送り、重複・逆順・取りこぼしに耐える。現行の形態ビット・進行度スナップショットは整合性回復の保険として維持する。
4. **他プレイヤーの描画は共有プレイアウト時計で再現**：`NetSmoothing` が読んでいるサーバーフレーム時刻に基づき、位置と変形を**同じ時刻で表示**する。描画間の小数時刻も使い、4ビット整数への再量子化による段階表示を避ける。
5. **状態確定はサーバーに従う**：`IsAltForm`、当たり判定、射撃、ボム、Weavelの砲台・HPなどはイベントの見た目から推測しない。現行 `PlayerState` とサーバーシミュレーションの権威を維持する。

### 先に把握しておくべき実装上の制約

- **現行プロトコルは33**、`PlayerState::Size` は **140バイト**。`NetConfig::MaxPacketSize` は **1232バイト**。
- 8人のスナップショットでは、プレイヤー状態 `8×140=1120`、ヘッダー23、試合タイム情報64、アイテム状態ヘッダー3の**最小合計が1210バイト**。さらにアイテム状態3件（7バイト×3）なら**1231バイト**。したがって `StartFrame`（4バイト）と `TransitionId`（たとえば2バイト）を**全プレイヤーの `PlayerState` に無条件追加する方式は1232バイト制限に収まらない**。現行のアイテム巡回同期も圧迫する。
- よって**追加の小型イベントパケット**を優先候補とする。あるいはスナップショットを構造的に分割・再設計する必要がある。パケットを増やさずに済むと決めつけてはいけない。

参照：[`PacketTypes.hpp`（プロトコル33、最大サイズ、入力間隔）](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Protocol/Packets/PacketTypes.hpp#L56-L65)、[`RealtimePackets.hpp`（PlayerStateとSnapshotHeaderサイズ）](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Protocol/Packets/RealtimePackets.hpp#L412-L427)、[`NetMatchTimeSync.hpp`](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Session/NetMatchTimeSync.hpp#L10-L17)、[`NetItemState.hpp`](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Gameplay/Pickups/NetItemState.hpp#L42-L50)。

## 2. 現行ネット同期：実際のデータ経路

```text
[操作プレイヤー / Client]
   │  Morph押下、押下履歴、現在Altビット、Frame、LifeId等
   │  IntentPacket（送信間隔：1シミュレーションティック）
   ▼
[専用サーバー / ServerIntentChannel]
   ├─ MatchId / AuthorityEpoch / SlotGeneration / LifeId を検証
   ├─ 自身のサーバーシミュレーションへ入力を渡す
   │     └─ TrySwitchForms → EnterAltForm / ExitAltForm
   └─ 入力を SlotIntent として別プレイヤーにも中継
          ※「その入力が実際に変形成功した」という確定イベントではない

[サーバー60 Hzシミュレーション]
   └─ PlayerStateSnapshotCapture
        ├─ FlagAltForm
        ├─ WeavelFlagMorphing / WeavelFlagUnmorphing
        └─ non-Weavel：FormTransitionProgress = 0～15
             │
             ▼
        SnapshotHeader.Frame 付き Snapshot を配信
             │
             ▼
[他の Client / SessionSnapshotReceiver]
   └─ NetSmoothing::Record(Frame, PlayerState)
         └─ NetSmoothing::Tick(): 遅延付き readFrame を更新
               └─ NetPlayerBridge::ApplyState
                     ├─ Sample(position)
                     ├─ SampleForm(direction, progress)
                     ├─ PlayerFormReplication::ApplyPresentation
                     └─ ApplyWeavelState（Weavel専用砲台ロジック）
```

**ここがポイント**：サーバーは入力の中継もしているが、補間有効な通常クライアントは中継された `Morph` 押下をリモートプレイヤーの変形開始に直接使わない。変形は遅延付きプレイアウト時計で読み出した**権威スナップショット**が担当する。一方、サーバー側と `-nointerp` 経路は即時の入力再生を残している。

根拠：
- [`NetPlayerIntent.cpp`（Morph、AltFormStateを収集）](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Gameplay/Player/NetPlayerIntent.cpp#L26-L70)
- [`NetInputHistory.cpp`（Morph押下エッジを履歴として記録）](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Gameplay/Player/NetInputHistory.cpp#L13-L35)
- [`NetHooks.cpp`（入力キャプチャと送信、毎ティックスナップショット）](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Integration/Hooks/NetHooks.cpp#L360-L424)
- [`ServerIntentChannel.cpp`（入力の検証、シミュレーション渡し、中継）](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Server/Match/ServerIntentChannel.cpp#L32-L85)
- [`NetPlayerBridge.cpp`（中継Morphエッジを補間クライアントで抑止）](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Gameplay/Player/NetPlayerBridge.cpp#L248-L263)
- [`SessionSnapshotPublisher.cpp`（サーバー状態の配信）](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Session/SessionSnapshotPublisher.cpp#L49-L113)
- [`SessionSnapshotReceiver.cpp`（検証、履歴記録）](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Session/SessionSnapshotReceiver.cpp#L22-L110)

### 2.1. 今すでに同期される3種類の情報

| 情報 | 現状 | 備考 |
|---|---|---|
| Normal → Alt | `PlayerState::WeavelFlagMorphing` | 権威コピーがMorph中ならセット |
| Alt → Normal | `PlayerState::WeavelFlagUnmorphing` | 権威コピーがUnmorph中ならセット |
| 現在のAlt形態 | `PlayerState::FlagAltForm` | **変形進行中でも**方向から見た目の形態と一致するとは限らない |
| 変形フェーズ | `PlayerState::WeavelFlags` の上位4ビット | **Weavel以外のみ**0～15に量子化 |
| 変形開始ティック | **独立したフィールドなし** | `SnapshotHeader.Frame` は各スナップショットの**観測時刻**であり開始時刻ではない |
| 変形エピソードID | **独立したフィールドなし** | 同じ方向・同じ進行度の別イベントをIDで判別できない |

`PlayerState::HeadingAlt()` は、Morph中ならtrue、Unmorph中ならfalse、それ以外は `FlagAltForm` を採用する。これは見た目の遷移先判断として有用だが、サーバー上で形態変更が法的に許可・確定された時刻そのものではない。

参照：[`RealtimePackets.hpp` L452～514](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Protocol/Packets/RealtimePackets.hpp#L452-L514)、[`PlayerStateSnapshotCapture.cpp` L19～62](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Gameplay/Replication/PlayerStateSnapshotCapture.cpp#L19-L62)。

### 2.2. スナップショット側の補間と同期タイミング

- シミュレーションの正本は `SimulationRate::Hz = 60`。DS固有処理の基準は `DsHz = 30`。レンダリングFPSとは独立。
- `NetSmoothing` はリモートプレイヤーを最新スナップショットより**2～8シミュレーションフレーム**過去で再生する（約33～133ms相当。通信遅延とは別）。`_readFrame` は1ティックずつ進み、最新フレームからの誤差を補正する。
- `NetSmoothing::SampleForm` は変形方向・Alt状態が両側のサンプルで一致する場合のみ進行度を線形補間し、**最後に `lround` で0～15の整数へ戻す**。方向やAlt状態が変わる境界は補間しない。
- `PlayerFormReplication::ApplyPresentation` はその進行度でリモートの変形アニメーションを保持する。変形の終了は同じ読取時刻で到達した安定状態で確定する。
- ローカル操作本人にはその遅延時計を適用せず、既存の入力予測による変形アニメーションを維持する。

従って、**「受信した最新パケットをすぐ描く」設計ではない**。位置・形態・ボム等のビジュアルがずれるのを防ぐため、同じプレイアウト時刻へそろえている。新方式でもこの性質は原則維持する。

参照：[`SimulationRate.hpp`](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/SimulationRate.hpp#L7-L27)、[`NetSmoothing.hpp` L16～69](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Gameplay/Replication/NetSmoothing.hpp#L16-L69)、[`NetSmoothing.cpp` L218～266](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Gameplay/Replication/NetSmoothing.cpp#L218-L266)、[`NetSmoothing.cpp` L404～444](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Gameplay/Replication/NetSmoothing.cpp#L404-L444)、[`NetPlayerBridge.cpp` L490～580](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Gameplay/Player/NetPlayerBridge.cpp#L490-L580)。

### 2.3. Morph / Unmorph は対称ではない

原作由来の状態遷移であり、単純な `AltForm = !AltForm` に統合してはならない。

- **Morph**：`EnterAltForm()` によって `Morphing` を開始。通常は変形アニメーション終了時に `UpdateForm(true)` され、そこでゲームプレイ上のAlt状態が確定する。
- **Unmorph**：`ExitAltForm()` によって `Unmorphing` を開始し、`UpdateForm(false)` は開始側ですでに呼ばれる。つまり、見た目のUnmorphアニメーションが進行中でも `IsAltForm()` はfalseになりうる。
- `TrySwitchForms()` は凍結、移動・カメラ制約、他の変形処理中、`NoFormSwitch`、Guardianなどの条件で拒否する。**Morphボタンが来たことだけを変形成功と扱うのは誤り**。
- Weavelは砲台の生成・HP分配・砲台消失・再合体が絡む。見た目のリプレイで `EnterAltForm()` / `ExitAltForm()` を無条件に再実行すると、砲台とHPを二重更新する危険がある。

参照：[`PlayerProcess.cpp` L1263～1285](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Entities/Players/PlayerProcess.cpp#L1263-L1285)、[`PlayerProcess.cpp` L2052～2072](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Entities/Players/PlayerProcess.cpp#L2052-L2072)、[`PlayerProcess.cpp` L2356～2463](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Entities/Players/PlayerProcess.cpp#L2356-L2463)、[`PlayerWeavel.cpp` L157～230](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Entities/Players/PlayerWeavel.cpp#L157-L230)。

## 3. 現状の評価：何が良く、どこを改善できるか

| 観点 | 現行実装の評価 | 新方式の期待値 |
|---|---|---|
| サーバー権威 | **良好**。クライアントは世界スナップショットを発行できず、専用サーバーがシミュレーション結果を配信する | 維持必須 |
| 変形方向 | **実装済み** | 再送を含むイベント情報として明確化 |
| 進行度 | 4ビット、スナップショット間の補間後も整数 | `StartFrame` を基準とした**連続時間の位相**で描画可能 |
| イベント識別 | 方向と進行度から推定 | `TransitionId` により重複、取りこぼし、逆順、再起動を明確に判定 |
| 時刻一致 | 位置と形態を共通プレイアウト時計で表示 | **維持**し、開始ティックも同じ時計系へ統合 |
| ローカル操作感 | 入力予測が残る | **維持**。権威の確認を待ってからアニメーションを始めない |
| 拒否された変形 | 変形後の状態で修正・整合 | 権威の受理／拒否をイベント単位で特定しやすい |
| Weavel | 砲台ライフサイクル専用ロジックあり、進行度の4ビット同期はなし | 動画再生の位相を共有しつつ砲台の正本を保持 |
| `-nointerp` | 入力中継ベースの経路あり | 独立の旧経路を残すか、イベント再生へ統一するか段階的に検証 |

現在の `FormTransitionPlaybackPolicy` は、進行度の後退を新しい遷移の徴候として扱い、終了済みアニメーションが古いスナップショットで再生されないよう工夫している。しかしこれは**明示的なイベントIDがないことに対する推論**であり、次の変形と同じ方向の古い情報の識別にはIDが適している。

`FormReconciliation` にも最大90フレームの不一致修正期限がある。新方式導入後も**権威状態へのフォールバック**として活用できるが、イベントが正常な場合は強制状態修正を常態化させないこと。

参照：[`FormTransitionPlaybackPolicy.cpp` L5～60](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Gameplay/Replication/FormTransitionPlaybackPolicy.cpp#L5-L60)、[`FormReconciliation.cpp` L12～79](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Gameplay/Replication/FormReconciliation.cpp#L12-L79)。

## 4. 推奨設計：サーバー確定 FormTransition イベント

### 4.1. イベントモデル

概念的には次の情報を持つ。これは**提案構造**であり、現行プロトコルに存在するフィールドではない。

```cpp
// 設計概念。既存ヘッダーへそのまま貼り付ける実装コードではない。
struct AuthoritativeFormTransition
{
    uint16_t matchId;           // MatchId
    uint64_t authorityEpoch;    // サーバー権威の世代
    uint8_t  slot;              // プレイヤースロット
    uint16_t slotGeneration;    // スロット再使用の識別
    uint16_t lifeId;            // 死亡・リスポーンの境界
    uint16_t transitionId;      // そのライフ内の一意な変形イベントID（周回比較が必要）
    uint32_t startFrame;        // サーバー上で変形が実際に始まった60 Hzティック
    uint8_t  direction;         // Morph / Unmorph
    uint8_t  status;            // Started / Completed / Cancelled / Rejected（必要に応じて）
};
```

- **`startFrame` はサーバーのフレーム番号**。クライアントの壁時計時刻（UNIX time、`steady_clock` の値）を直接比較・信用しない。
- **`transitionId` は実際に成立した変形ごとに一意**。`matchId`・`authorityEpoch`・`slotGeneration`・`lifeId` と組み合わせてイベントの所属を確認する。カウンター周回時の新旧比較ルールを定義する。
- **変形方向はサーバーが決める**。クライアントが送った `AltFormState` や独立の方向申告をそのまま採用せず、`TrySwitchForms` の成功とサーバー側の現在形態から決定する。
- **確定済みの状態も送る**。遅れて参加するクライアントは変形中かどうか、すでに完了しているかを把握できなければならない。
- 既存の `IntentPacket.Frame` と `Presses` は操作発生元のフレーム・予測イベントの照合に利用可能。ただし**クライアントのFrameとサーバーのFrameが同じ時間軸であるという前提を置かない**。対応付けは既存のセッション時刻同期・履歴仕様を踏まえて別途定義する。
- `Completed` や `Cancelled` の意味、送信条件、取りこぼし復旧方法も規約化する。イベント通知だけを状態の唯一の正本にしてはいけない。

### 4.2. 開始ティックの採番位置

**推奨：権威側で `PlayerEntity` の変形開始が実際に成立したことを観測したタイミング**。候補は以下の順。

1. `ServerSim` のシミュレーションに連動した、責務を限定した `FormTransitionAuthorityTracker` を設け、各スロットの開始前／開始後の状態変化と60 Hz権威フレームを監視する。
2. あるいは、すでに毎フレーム状態を取り出す `PlayerStateSnapshotCapture` / `SessionSnapshotPublisher` 側で前回状態と比較して遷移開始を確定する。これは取りこぼしや順序のない設計にしやすい。ただしその時点のフレーム番号が実際の開始シミュレーションティックであることをテストする。
3. 入力受付の `ServerIntentChannel::Receive()` の中で即時に確定する方法は**避ける**。そこではまだ変形がゲーム内条件で許可されるか決まらない。

設計上、**ServerIntentChannel（通信入口）**、**FormTransitionAuthorityTracker（状態遷移の事実）**、**イベント送信（配送）**、**FormPresentation（描画）**を分離する。C++20の型・バッファで小さく閉じ、ゲーム本来の `PlayerEntity::TrySwitchForms()` ロジックはなるべく重複実装しない。SRP / SoC / DRY / SSOT を維持する。

### 4.3. 受信側の表示時刻とアニメーション位相

サーバー側開始フレームを `S`、クライアントが描こうとしている**サーバー時刻**を `R`（整数部＋小数部分、`NetSmoothing` のプレイアウト時計に準ずる）、変形アニメーションの正本の長さを `D`（60 Hzステップ換算）とすると、表示専用の進行度は概念的に次の通り。

```text
elapsed = R - S
phase   = clamp(elapsed / D, 0, 1)
```

**ただしこの式だけでネイティブアニメーションの実装を置き換えない**。Morph/Unmorphの実際のモデルアニメーションはハンター・クリップ・DS側30 Hzのアニメーション更新規則を参照する。ゲームプレイのタイミングは60 Hzの固定ティックであり、描画だけ小数時刻を使用する。完了点や見た目の中間段階、カットシーン・強制変形等はオリジナルの仕様に合わせる。

画面描画時の `R` は、実装済みの `FrameTiming::StepPhase()` や描画時間補正を介し、**最新スナップショットと同じ時計上の描画時刻**にする。`SampleAhead` は現在、位置について最新フレームより先へ進まない制限を備えている。この制限を意図せず破り、**位置は止まるのに変形アニメーションだけ進む**表示を作らないこと。

現行の `SampleForm` の0～15丸めを描画フェーズの最終出力として使い続けると、開始ティック導入後も高FPSで16段階に見える問題が残る。フェーズ計算では `double` / `float` を持ち、`AnimationBlend` / presentationレイヤーに渡す。ネット上の4ビットはフォールバック検査値として残せる。

### 4.4. 受信・再生の状態機械

```text
Idle(StableNormal / StableAlt)
  └─ 新規 Started(direction, transitionId, startFrame)
        ├─ match/epoch/slotGeneration/life を照合
        ├─ eventId が既知なら二重開始しない
        ├─ 旧イベントなら捨てる
        └─ PresentationTransition
              ├─ 同じ共有時計で位置とクリップを表示
              ├─ 同一イベントの再送は進行度更新のみ
              ├─ Completed → 確定形態に遷移
              ├─ Cancelled → 権威の確定形態で修復
              └─ 時間超過・欠落 → 最新権威スナップショットで復旧
```

- イベントを受信したそのフレームに「0%から開始」してはいけない。届いたときに `R > S` なら**経過済みの位相**から描く。
- 開始イベントがスナップショットより後から届いても、同一 `transitionId` に対して時間を巻き戻さない。
- 死亡、リスポーン、スロット再利用、観戦切替、部屋移動、AuthorityEpoch変更では未完成の変形を破棄し、新ライフの権威状態へ切り替える。
- サーバーが入力を拒否した場合、入力者の予測変形は必要に応じて修復する。**拒否を「逆方向の変形イベント」と偽装しない**。
- `-nointerp` と専用サーバーのモードでは、同じ確定イベントを受けても二重の `TrySwitchForms()` / `EnterAltForm()` を起こさない。
- 効果音・パーティクル・カメラ切替は「同じイベントの再送」で繰り返さない。一方、変形途中の遅延参加で**いきなり開始音を鳴らすべきか**は別ポリシーとする。

### 4.5. パケット配送と回復性

**優先案：小型のサーバー権威 `FormTransition` イベントパケット＋現行の`PlayerState`スナップショット。**

- サーバー→他クライアントへ遷移の開始と状態確定を送る。単発送信だけではパケット欠損で永遠に変形が見えないため、**受信ACKまたは有限回の再送と、参加時の最新状態再送**を定義する。
- 複数のイベントを遅れて受信しても、`transitionId`・世代・ライフ・権威Epochによって冪等に処理する。再送中に別の変形が発生した場合も順序逆転を処理する。
- 現行 `PlayerState.FlagAltForm`、Morphing/Unmorphingビット、進行度は**権威の状態回復・失敗検出**に使用し、イベントの後に矛盾が続く場合のみ補正する。
- 変形中に接続した参加者には、単発イベントの過去送信履歴だけでなく、**現在進行中の遷移レコード**または最新確定状態を提供する。開始済みで完了も済んだ変形を遅れて最初から再生しない。
- イベント専用パケットを作る場合、`PacketType`、エンコード／デコード、サイズ・整合性・不正入力の検証、ネットデモ・再生、セッションロール、CIテストまで一組で設計する。**現行プロトコル33との互換性管理・バージョン更新が必須**。新旧パケットの無検証混在を許さない。
- スナップショットへの可変長追記も理論上は可能だが、**8人時にほとんど余裕がない**。採用するならスナップショット分割・上限計算・アイテム配信の到達性・ヘッダー再設計を先に完了させること。

この案は「イベントさえ送れば全員完全同期」と主張するものではない。ネットワークの欠損・サーバー側の受理タイミングのずれ・ローカル予測の差分は残るため、**権威スナップショットによる収束が必要**。

## 5. Weavel / カメラ / 攻撃の特殊対応

### Weavel

- `PlayerStateSnapshotCapture` は Weavel にMorphing/Unmorphingの方向ビットを載せるが、**上位4ビットのクリップ進行度はセットしない**。`NetPlayerBridge` 側も Weavelの上位4ビットが立つと不正として扱う。
- 現行 `PlayerWeavel.cpp` の `ModApplyWeavelState` / `FinalizeWeavelForm` / `ModApplyOwnWeavelTurret` と、`WeavelReplicaTransition` の90フレーム上限は別責務として残す。
- **イベント方式の共通化対象は映像上の「方向・開始時刻・位相」**。砲台の出現、生存、HP分配、`_weavelAltLife` は従来どおりサーバー確定のライフサイクルが正本。
- 検証対象：変形途中で砲台が破壊される、Alt中死亡、Alt→Normalへの回復、未確定砲台の遅延到着、連続Morph、観戦／途中参加。

参照：[`PlayerStateSnapshotCapture.cpp` L38～62](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Gameplay/Replication/PlayerStateSnapshotCapture.cpp#L38-L62)、[`NetPlayerBridge.cpp` L415～429](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Gameplay/Player/NetPlayerBridge.cpp#L415-L429)、[`WeavelReplicaTransition.hpp`](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Entities/Players/WeavelReplicaTransition.hpp#L7-L58)。

### カメラ・観戦・描画FPS

- 変形アニメーションは共通プレゼンテーション時刻で描くが、**ローカルの視点・操作エイム・フリーカメラにはリモート用遅延時計を直接適用しない**。
- 観戦中の対象プレイヤーの**位置・姿勢・形態と観戦カメラが別時刻**にならないよう確認する。視点追従カメラとMorphカメラ切替のタイミングは特に重要。
- 30 / 60 / 120 / 144 / 240 / 540 FPSで、開始から終了までアニメーションの所要時間が一致し、描画間の動きが連続し、シミュレーション結果が変わらないことを確認する。
- 新しいイベントを再描画ごとにゲームプレイ状態へ再適用してはならない。**状態更新と表示専用の小数位相評価を分離**する。

### 弾・ボム・当たり判定

- Morph中にBoostが入るケース、Morphからのボム設置、射撃とMorphの同時操作、Altから戻る最中の被弾は、イベント表示だけで解決しない。
- 現行 `PlayerFormReplication::AdvanceForAltFormAction()` は、ボム等のAltアクションに追いつくため変形状態を先行補正する。イベント同期導入時にこの整合性回復を消してよいかは**別途検証が必要**。
- ダメージ判定・ヒットリワインドを映像の推測位相から求めることはしない。サーバーでの権威形態と発射時刻を優先し、演出がゲームプレイ状態を上書きしない。

参照：[`PlayerFormReplication.cpp` L233～305](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Mods/Network/Gameplay/Player/PlayerFormReplication.cpp#L233-L305)。

## 6. 段階的な実装計画（リスクを抑える順序）

### Phase A：可視化・根拠収集（既存挙動を変更しない）

1. 権威サーバーでの変形要求時刻、受理／拒否、実際の `IsMorphing` / `IsUnmorphing` 遷移時刻、`FlagAltForm` 確定時刻をログに採る。
2. 受信側の `SnapshotHeader.Frame`、`NetSmoothing` の `_readFrame`、受信 `Direction` / `Progress`、描画上のクリップ進行時刻を対応付ける。
3. 現行の2～8フレームの補間バッファ遅延と、実際の変形開始のズレを測る。変形途中に16段階の表示が目視可能かも測定する。
4. Weavelと他のハンターを分け、Morph / Unmorph / 変形中のボム / 凍結 / 死亡を計測する。

### Phase B：サーバー確定イベント台帳・テスト

1. `FormTransitionAuthorityTracker` のような小さな責務の型を導入する。既存 `PlayerEntity` の受理ロジックを再実装しない。
2. `MatchId` / `AuthorityEpoch` / `SlotGeneration` / `LifeId` / `TransitionId` / `StartFrame` / `Direction` の確定条件とリセット条件をテストする。
3. スナップショット不一致、入力拒否、連続変形、アニメーション完了、死亡によるキャンセルをテストする。
4. この時点では送信・描画方式は変えず、**サーバー確定イベントと現行スナップショットが一致するか**を検証する。

### Phase C：イベントプロトコルと冪等配送

1. 小型イベントのWrite / Read / Validate、型ID、上限サイズ、既存プロトコル33とのバージョン交渉を整備する。
2. 欠損・順序逆転・重複・途中参加の台帳処理を実装する。
3. Weavel専用状態と一般のMorphタイムラインを混同しない。Netデモ・Bot・専用サーバー・ホスト経路を対象にする。
4. スナップショットの冗長な4ビット進行度を**この時点で削除しない**。復旧経路を保持する。

### Phase D：プレゼンテーション置換（最初は比較・診断モード）

1. `NetSmoothing` のサーバー時刻に `StartFrame` を載せる。位置・ボム等と同じ `readFrame` を使用する。
2. アニメーションのフェーズは描画専用の小数値で評価する。60 Hz固定ゲームプレイ、30 Hz原作アニメーションの長さ・終端意味を保つ。
3. 既存 `PlayerFormReplication` のネイティブ再生・4ビット補間と、新しいイベント方式を同一テストケースで比較する。実際の再生方式の切替は**フラグ制御**して回帰可能にする。
4. `-nointerp`、観戦、フリーカメラ、Weavelの砲台・HP、死亡・リスポーン、サーバー負荷時の確定タイミングを検証する。

### Phase E：整理

1. 収束・復旧テストが通り、旧形式を受信しないプロトコル移行が成立してから、使われなくなった進行度推論や重複状態管理を削減する。
2. ただし `FormReconciliation` のような**権威の不一致検知／強制修正機構は残す**。役割を「通常の変形開始」から「異常時の収束」へ明確化する。
3. スナップショット・デモ・テスト・診断ログの時刻ソースをSSOT化し、画面の変形とゲームプレイのフォームは別の責務として維持する。

## 7. 最低限必要な回帰テスト

| テスト | 期待条件 |
|---|---|
| Normal → Alt / Alt → Normal | 方向とStartFrameが権威側で一致し、最終フォームが一致する |
| クライアント入力拒否（凍結、禁止、死亡） | 入力パケットだけから確定イベントを発行しない |
| 変形イベントの重複受信 | 変形開始、サウンド、砲台生成が二重実行されない |
| イベント消失・スナップショットだけ到着 | 破綻せず現在フォーム・進行度へ収束する |
| 先に完了通知、後から開始イベント | 古い開始でアニメーションを巻き戻さない |
| 2連続変形、方向の反転 | TransitionIdで前の遷移を区別し、旧情報で復元しない |
| 遅延100 / 250 / 500ms、ジッター、順序逆転 | 位置とアニメーションが同一プレイアウト時刻を表示する |
| `NetSmoothing` 2～8フレーム、OFF | 二重開始がなく、それぞれの時計規約どおりに動く |
| 30 / 60 / 144 / 240 / 540 FPS | アニメーション実時間が一致し、描画フェーズは連続になる |
| 8人 + アイテム同期 | `MaxPacketSize=1232` 超過なし、アイテム同期が途切れない |
| Alt中ボム設置、変形中Boost | サーバーの衝突・アクション受理・状態と表示が矛盾しない |
| Weavel砲台／HP分配／破壊 | 変形イベントの再生で砲台・HPが二重更新されない |
| 死亡／リスポーン／別プレイヤーが同一slotを再使用 | 古いライフのイベントを適用しない |
| 部屋移動、試合再開、AuthorityEpoch変更 | 前試合の時刻とイベントを持ち込まない |
| 観戦の開始・離脱、観戦対象の退出 | カメラやフォームが停止・巻戻り・旧プレイヤーへ残留しない |
| Host / dedicated server / bot / Net demo | プレイヤーの権威、再生結果、イベント順が一致する |
| 固定ティック決定性 | ネットイベントや描画FPSによりサーバーのゲームプレイ結果が変化しない |

既存テスト基盤の候補：[`TestNetSmoothing.cpp`](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Testing/TestNetSmoothing.cpp)、[`TestFormReconciliation.cpp`](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Testing/TestFormReconciliation.cpp)、[`TestWeavelAltFormParity.cpp`](https://github.com/Zection6V/Fruity-Prime/blob/0bdbd611442a1092a97f40d5bd3db7f18e29abb5/src/MphRead.Native/Testing/TestWeavelAltFormParity.cpp)。これらの**存在とソースを確認した**のであって、今回の実行結果ではない。

## 8. 変更候補ファイル一覧

| 区分 | 既存ファイルまたは追加先 | 対応 |
|---|---|---|
| 原作の変形条件 | `Entities/Players/PlayerProcess.cpp` | 権威状態の観測点。不要な侵入や重複ロジックは避ける |
| 権威状態の抽出 | `Mods/Network/Gameplay/Replication/PlayerStateSnapshotCapture.cpp` | StartFrame確定用の状態変化検出候補 |
| イベント台帳 | `Mods/Network/Gameplay/Replication/` 配下に小型専用型を追加 | エピソードID、開始ティック、期限、ライフサイクル |
| 権威側サーバー | `Mods/Network/Server/Match/ServerSim.cpp`、`ServerIntentChannel.cpp` | 結果確定と単なる入力受信を分離 |
| 配信 | `Mods/Network/Server/Match/ServerSnapshotChannel.cpp`、`ServerPacketDispatcher.cpp` | イベントの配送と再送計画（役割の重複を避ける） |
| プロトコル | `Mods/Network/Protocol/Packets/PacketTypes.hpp`、`RealtimePackets.hpp`、`Codec/` | 独立イベントのWire DTO・Codec・Version |
| クライアント受信 | `Mods/Network/Session/SessionPacketDispatcher.cpp`、`SessionSnapshotReceiver.cpp` | 重複・順序・ライフ・権威の照合、復旧 |
| 表示時計 | `Mods/Network/Gameplay/Replication/NetSmoothing.cpp` | 共通readFrameの分数時刻をイベントへ適用 |
| 形態の表示 | `Mods/Network/Gameplay/Player/PlayerFormReplication.cpp` | 描画専用位相を使用し、従来修復ロジックを保持 |
| フォーム/座標結合 | `Mods/Network/Gameplay/Player/NetPlayerBridge.cpp` | 形態と位置を同じ時刻で反映 |
| Weavel | `Entities/Players/PlayerWeavel.cpp`、`WeavelReplicaTransition.hpp` | タイムライン導入でも砲台ライフサイクルは別管理 |
| テスト | `Testing/TestNetSmoothing.cpp`、`TestFormReconciliation.cpp` 等 | 欠損・重複・境界・パケット長・多FPS |

※この表は「変更推奨の対象・確認先」であり、全部のファイルを書き換えることを要求するものではない。現状の構造を保ち、小さな段階で改修する。

## 9. 最終判断

**採用推奨。ただし全面置換よりハイブリッド方式を優先。**

現在のFruity-Primeは、すでにサーバー権威で変形方向と進行度を配信し、Net Smoothingで位置とアニメーションの時刻をそろえる設計になっている。その方向性は正しい。提案された開始時刻同期は、それを覆すものではなく、**一つの変形に一つのサーバー開始ティックとイベントIDを持たせ、全表示端末で同じ再生位相を再構築する**改善として取り込むのがよい。

とくに次の3点を守ることが成功条件となる。

1. **権威開始ティックはサーバーの実際の変形成立時点**。クライアントの自己申告時刻をそのまま配る設計にはしない。
2. **表示用アニメーションと権威ゲームプレイを切り離す**。既存のフォームの非対称性、Weavel、被弾、ボムを壊さない。
3. **配送耐性とパケット上限を初期設計に含める**。方向＋時刻だけの1回限りのイベントでは、通信欠損時に同期できない。

これを満たせば、アニメーション開始位相の整合性、途中参加時の復元、古い変形の誤再生、高FPSでの段階的な進行度描画の改善を期待できる。一方、**実際にどれだけ滑らかになるか、通信遅延が何ms短縮されるかは、実装後の実測で確認する事項**である。開始時刻だけを追加してもネット遅延そのものやプレイアウトバッファの遅延が消えるわけではない。

---

**調査方法について**：GitHub接続で上記固定コミットのC++ソース、既存ユニットテスト、ネットワーク仕様書を照合。読み取り専用で実施。上記構造・工程・パケット案は**新規実装の提案**であり、既に実装・試験完了したという意味ではない。
