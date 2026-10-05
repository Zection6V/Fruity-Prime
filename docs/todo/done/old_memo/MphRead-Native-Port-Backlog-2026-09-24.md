# C# 側の新規コミットを C++ へ移す作業表（2026-09-24 時点）

`origin/newest` の 212 コミットを develop2 に取り込んだ時点（[PR #1](https://github.com/Zection6V/Fruity-Prime/pull/1)）で、
`src/MphRead.Native/` は `src/MphRead/` より 302 ファイル分古い。
その差分を、下から（依存される側から）順に埋めるための一覧。

列の意味 — **S**: C# 側の変更種別 (A 追加 / M 変更 / D 削除)、
**+/-**: C# の追加・削除行数、**対**: 既にある C++ の対応物
（`-` は新規に書き起こすもの）。
**進捗**: `完了` / `一部完了` / `監査待ち` / `監査中` / `保留` / `未反映（作業中）`。`監査待ち` は移植済みだが今回のC#対比監査に未着手の項目、`監査中` は着手済みで未完了の項目。移植の完了と今回の監査完了は分けて記録する。`—` は、現在の `develop2` でこのバックログ差分の移植を確認できていない項目。共通ランタイム化など別目的の変更だけでは進捗扱いにしない。

作業の規則は `MphRead-Native-CSharp-to-Cpp-Basic-Policy.md` と
`MphRead-Native-CSharp-to-Cpp-Pitfalls.md` のとおり。1つのバッチを終える
たびにネイティブをビルドし、緑のままコミットする。

**番号は依存順ではない。** 実際の順序は次のとおり（着手して分かった分を反映）:

1 → 3（`Mods` 直下の葉）→ 6（入力）→ 7（描画）→ 8（チーム）→ 9（ネット）
→ 10（マップ生成）→ 11（ランチャー可搬部）→ 12（ランチャー GUI）
→ 4・5（更新・チャット）→ **2（Diagnostics）** → 13（エンジン）→ 14（Android）。

`Mods/Diagnostics` は葉に見えて、`WindowGeometry`・`Render::UiOverlay`・
`GuiLauncher`・`MapGen::CustomRooms` を呼ぶ**利用側**なので最後に近い。

## 進捗ログ

作業中に更新する。コミットは develop2。

### 2026-09-28 移植差分監査（302項目）の状態

- 表の302項目はすべてC#対比監査済み。`GameFiles.cs`もC#全文・直接呼出し監査済みで、POSIX修正のmacOS/Clang・Linux/GCC CIとWindows Release buildを確認済み。個別監査の詳細は末尾ログを参照。最新Native C++ CI run `36348842034`（source `8c67ca37`）はWindows/MSVC・Android・Linux/GCC・macOS/Clangすべて成功。Windows Release `ninja -k 0`も成功。総合build run `36350578764`（source `6bfac073`）も全job成功。先行run `36348842072` で一度失敗したdedicated-server startup contractとbounded thumbnail worker regressionは再実行で成功し、失敗は再現しなかった。
- Section 14 のAndroid 17ファイルは監査完了。`NativeRuntime/Avalonia/Base.hpp` のMSVC対応後、Android arm64-v8a・x86_64をそれぞれ最新ソースで最終buildし、両方とも成功（各75段階、静的ライブラリをリンク）。runtime/device確認は未実施。
- 上記の `完了` はPR #1由来の302項目の移植差分監査を指す。別件のプレイ中不具合監査まで完了した意味ではない。C#版にはないとユーザーから報告された症状について、以前の「C#も同じ」とする9月27日の記録は結論として扱わず、現行ソースで確認した。現在の継続対象はメニュー性能と長時間メモリ挙動。
- 別件の不具合監査は継続中。カーソル表示・画面端エイムはユーザーより解消済みとの訂正があり、追加runtime調査の対象から外す。Online の `did not answer` は原因特定済みとの指定により再調査しない。残る優先対象は Online/Offline 等のメニュー描画性能と長時間メモリ挙動。既存の短時間測定ではC++のメモリ使用量がC#より低く、スクロール性能差は残る。これらの測定は通常プレイ時のPC全体フリーズを再現・解決確認したものではない。以下のカーソル/Online項目は当時の観測記録で、現在の作業予定ではない。
- `InputSettings.cs` と `.cpp/.hpp` を一ファイル単位で再照合。現行C#・C++とも `stylus_mode` の明示値だけでStylusModeを有効にし、`pointer_jump_guard` は独立設定として扱う。読み込み・保存に差異なし。
- `Renderer.cs` と `Renderer.cpp` のカーソル取得条件、PlayerInputへのpointer sample・acceptsInput引数を照合。条件と順序は一致し、C++ `CursorState::Grabbed` も OpenTK と同じ `GLFW_CURSOR_DISABLED` に対応する。コード上は同じ入力状態なら両版ともカーソルを隠し、画面端に制限されない。
- `NativeRuntime/OpenTK/RendererPlatform.cpp` をOpenTK 4.9.4の `NativeWindow` / `MouseState` と照合。C#はcallback差分用 `_lastReportedMousePos` と `MouseState.NewFrame` のポーリング位置を分けるが、nativeは `_mouse.X/Y` を両方に使い、`glfwGetCursorPos` を呼んでいなかった。nativeに別々の差分基準と、window作成時・event pump前の位置取得を追加。C#と同じくcallback差分と現在位置を別管理する。Windows Release `ninja -k 0` 成功（既存 `offsetof` 警告のみ）。
- `Renderer.cpp` は `-debuglog` が有効な場合にgrab要求・focus・各解除条件を状態遷移時に記録するようにした。設定読込、Rendererのgrab条件、`GLFW_CURSOR_DISABLED` への対応は現行C#と一致し、同じ状態ならカーソルは隠れて端に制限されない。位置ポーリング差は修正済みだが、これだけで可視カーソル症状が解消したとは未確認。次のruntime再現ではdebug logを使い、grab要求が外れているかを確認する。
- Windows上で試合中のマウス操作を自動再現できるUI操作手段がないため、カーソル症状のruntime確認は保留。既存のゲームデータは利用可能。ゲーム内で再現する際は `-debuglog` の `cursor grab=... focus=...` 行を採取し、grab解除条件を確認する。
- `RendererPlatform.cpp` を OpenTK 4.9.4 の `MouseState.NewFrame` / `NativeWindow.CursorPosCallback` の順序と再照合し、callback内で `_mouse.X/Y` を更新していた差を削除。C#同様、callbackは差分イベント用の位置だけを更新し、現在位置はevent pump前のpoll値を保つ。Windows Release `ninja -k 0` は成功（既存 `offsetof` 警告のみ）。`-shellshot` はmatch/pause画面まで到達したが入力ステップが成立せず、OSカーソル表示と画面端エイムのruntime確認は未完了。
- poll/callback位置を分離した後のイベント消費側もC#と照合。C# `Renderer.OnMouseMove` は Shell表示中に `MouseMoveEventArgs.X/Y` を使うが、native event argsに座標がなく、poll済み `MouseState` を読んでいた。native argsへcallbackの現在座標を追加し、native `Renderer::OnMouseMove` も `e.X/Y` を使うよう修正。GUI pointer座標をC#と揃えつつ、MouseStateはpoll値のままにする。`git diff --check`通過、Windows Release `ninja -k 0` 成功（既存 `offsetof` 警告のみ）。この補正後のshell UI runtime確認は未実施。
- Online監査開始: `PlayScreen.cs/.cpp` のサーバー一覧更新・個別status問い合わせと `NetMaster.cs/.cpp` の `Query`、`NetStatus.cs/.cpp` を一ファイルずつ比較。依頼処理・UDP query/response判定・タイムアウト時の「did not answer」表示に現時点で差異なし。Windows Release C# buildは成功（既存のobsolete警告4件）。Android buildは未実施。
- 同一Windows環境で既存C++ Release版とC# Release版の `-servers -debuglog` を続けて実行。両方とも `net.livetek.fr:27889` から「4 listed」を受け取り、4件すべての直接status queryが「did not answer」。この再現ではC#とC++に差は出ず、現環境で報告されたC++固有差を確認できなかった。サーバーまたは経路の一時的状態と区別するため、transport実装の残りと、既知のC#正常環境での結果を引き続き確認する。
- 9/28に最新Windows Release C++/C#実行ファイルで同じ `-servers -debuglog` を再実行。directoryは今回も「will start games」「4 listed」と返し、West US 2/West Europe/Pi/Japanの全status queryが両版とも `did not answer`。今回の実行では個別UDP replyのbyte長は採取していない。前回の131-byte旧サーバー観測と混同せず、ユーザー環境でのC#正常報告との差は引き続き未解決として扱う。
- `NativeRuntime/System/Net.cpp` の今回の Online 経路に関係するDNS→IPv4選択、endpoint生成/照合、UDP send/receive、timeout設定、socket破棄を `NetStatus.cs` の呼出し順と照合。query/JoinProbe双方で宛先・送信元一致判定とsocket寿命が一致し、差分修正なし。既知の旧サーバー応答長不足による非互換の記録は別に残し、同一環境でのC#正常結果は未確認。
- `NativeRuntime/Skia/Skia.cpp` を一ファイル監査し、C# `DeckTile.cs` の `PushClip(face)` 内での `DrawImage(_ground)` / 矩形描画と照合。nativeは角丸clipがあると軸平行bitmapの専用blitを使わず、pixelごとの逆変換・画像sampleへ落ちていた。専用blitで列/行のsample位置を再利用し、clip coverageを合成してからblendするよう修正。矩形fillもclip maskを保持したまま解析的coverage経路を利用する。Windows Release `ninja -k 0` 成功。
- `NativeRuntime/Avalonia/TopLevel.cpp` の `RenderVisual` をC# `UiTopLevelImpl` の可視clip/dirty領域の役割と照合。nativeは全描画時にsurface外・clip外のvisualもRenderしていたため、祖先のclip可視範囲を子へ伝えて描画不要なself/subtreeをskipするよう修正。Windows Release `ninja -k 0` 成功、C++/C#の `-uishot` は各26画面を出力し、`play-offline` のclip表示に崩れなし。offline map Scroll再計測はC++ 21.44 ms/47 fps、C# 8.55 ms/117 fpsで、今回のcullだけでは性能差はほぼ縮まらず、残るCPU raster/retained-render差を継続調査する。
- 同条件のmap Scroll中、C++ `DeckTile::ChromeAsks` は1026、C#は211。`DeckTile::Chrome` のcache lookup自体は両実装で一致する。C# Avalonia compositorが保持・再利用するvisual contentをnative custom rendererは可視viewport内で描き直しており、単なるclip外visual走査では説明しきれない差が残る。C#の不足map（`MK_BLOCKFORT`）が1件あるため呼出回数は参考値として扱う。
- 同じ `-uibench` 条件（2560x1440ウィンドウ、1920x1080描画面）のOffline mapsで、C++ Scrollは修正前71.40 ms/描画から21.67 ms（約46 fps）へ、Repaintは39.33 msから22.66 ms（約44 fps）へ短縮。C#の同条件はScroll 8.42 ms、Repaint 14.46 ms。描画差は大きく縮んだがまだ残り、これだけでユーザー報告のPC全体フリーズが解決したとは判定しない。
- 9/28 Windows Release版を同一端末で再計測。`-uibench maps -uibenchonly Scroll` のrender中央値は1280x720でC++ 8.15 ms/C# 3.81 ms、1920x1080で21.08/8.12 ms、2560x1440 window（raster surface 1920x1080）で21.33/8.30 ms。`settings` も1280x720で5.85/1.68 ms、1920x1080で14.77/3.12 msとなり、遅さはmap listだけではなく描画面積を増やすと強まる。mapsのChrome cache問い合わせ数はC++ 1098/1026、C# 211（各条件でbakeは4）。C#側は不足素材`MK_BLOCKFORT`を1件除外するため完全同一内容ではない。upload中央値は最大0.36 ms程度で、測定上はCPU render側が差の大半。これは`UiTopLevelImpl`の合成前surfaceを使うベンチで、GL compositeや実プレイ時の全体フリーズを再現した測定ではない。native `TopLevel::RenderVisual`はdamage内の可視controlを毎回Renderするため、C# compositorのretained visual contentとの差を次の性能監査候補とする。原因確定・修正はまだ行っていない。
- 9/28 `UiTopLevel.cs` と `NativeRuntime/Avalonia/TopLevel.cpp` の再照合で再描画経路を確認。C#はAvalonia `Compositor` とdirty-region対応の保持surfaceを使う。nativeは最終RGBA bufferとdamage rectangleを保持するが、damageに交差する可視Visualについて毎回 `Render()` を呼び直す。`Visual::RenderedContent` は描画有無の記録で、現状はhit test側だけが参照し、draw cacheではない。ScrollViewerのoffset変更はviewportをdamageにし、そこに見えるDeckTileも毎回Renderされる。この構造はベンチのChrome問い合わせ差とCPU render時間に整合するため、メニュー性能差の有力な原因と判定。Visual単位のraster cacheは無効化範囲と総メモリ上限を設計するまで未追加。ユーザーが挙げたOpenGL/メモリ破壊のfreeze修正とは別のUI性能経路で、実プレイ全体freezeの原因と同一とは断定しない。
- 9/28 `NativeRuntime/Skia/Skia.cpp` の `Canvas::SaveLayerAlpha` をC# `ServerRow.cs` のOpacityグループ利用と照合。nativeは各Opacity pushでsurface全体のRGBA bitmapを確保していた一方、合成時に読むのは現在のclip内だけだった。Onlineのserver rowは不透明度1の外側グループと、角丸clip内のmap画像用半透明グループを行ごとに作る。Opacity 1の外側pushはnative `ServerRow.cpp` で省略し、map画像はrounded clipを先に適用してからOpacity layerを開くようにして、見た目を変えずlayerの確保・合成範囲をclip内に限定。`Skia.cpp/.hpp` はlayerをclip rectangle寸法で確保し、BlendSpan/DrawBitmap/入れ子layer合成のdevice座標をcrop原点へ写す。これはフレームごとの一時メモリ/CPU churn対策であり、長期リークの証明やPC全体freeze解決とは扱わない。Windows Release `ninja -k 0` 成功。native/C#の `-uishot` は各26画面成功し、変更前後のnative `serverbrowser.png` はSHA-256一致。ほか3画面は出力差があり、今回のOpacity経路の回帰とは結論していない。`-uibench maps -uibenchsize 1920x1080 -uibenchonly Scroll` は21.31 ms/46 fpsで、map画面では有意な改善を確認せず。`-uishot` 26画面の10 ms採取ピークはC++ Working Set 98.1 MB / Private 87.0 MB、C# 199.8 MB / 141.9 MB。短時間の撮影では増え続けるメモリやPC全体freezeは再現していない。
- 9/28 `DeckTile.cpp` をC# `DeckTile.cs` のRender/Info/Badge、状態更新・直接呼出しと照合。静止・軸平行時だけ全カード内容を描く16 MiB上限LRUを追加し、room/map labels・ground bitmap・tally/chosen/leader/hover/focus・scale/render optionsをkeyに含める。animation中と回転transformでは従来描画へ戻す。C#の描画順とclipは維持。Windows Release build済み。1920x1080 maps Scrollは変更前21.31 ms/46 fpsから14.80 ms/66 fpsへ改善、C#は8.15 ms/119 fps。10 ms採取の同bench peakはC++ 106.9/98.7 MiB Working Set/Private、C# 159.2/112.4 MiB。`-uishot` は両版26画面を生成し、配置の崩れなし。短時間測定でcacheは予算内だが、長時間リークと通常プレイ時のPC全体freezeは未確認。C#のマップ入力にMK_BLOCKFORTがなく完全同一内容ではない。
- 9/28 `Rows.cpp/.hpp` をC# `Rows.cs` のCaption/ChoiceRow/ToggleRowと再照合。Caption・固定ラベルは行インスタンス内にFormattedTextを保持し、ChoiceRowの値レイアウトはIndex/SetItems/Stepまたは幅変更時に作り直す。描画位置・クリップ・イベント動作はC#から変更していない。全行の画像LRUも試したが、Settings Scrollは14.36 msでテキストレイアウト再利用のみの14.38 msと同等、短時間peakメモリは約2.4 MiB増えたため画像キャッシュを削除。現在の同条件はC++ 14.38 ms/68 fps、C# 3.14 ms/297 fpsで、行レイアウトcacheだけでは全体の差を縮められていない。Windows Release `ninja -k 0` 成功。PC全体freezeや長時間メモリ増加の再現・解決確認ではない。
- 9/28 current HEAD `3986c6b2` を同条件で再測定。`-uibench settings -uibenchsize 1920x1080 -uibenchonly Scroll` はC++ 14.63 ms render / 15.01 ms total（67 fps）、C# 3.23 / 3.48 ms（287 fps）、両方61/61 redraw。Skia textの各文字ごとに取得していたglyph/kerning/advanceを1行ごとのFreeType lockへまとめる試験も行ったが、C++ 14.51–14.67 msでbaseline 14.32–14.63 msの範囲から改善を確認できず、複雑さに見合わないためソース変更は破棄。試験後に元ソースでWindows Release `ninja -k 0` を再実行し成功。これはheadless測定であり、実ウィンドウやPC全体freezeを検証していない。
- 9/28 `DeckText.cpp/.hpp` をC# `DeckText.cs` と比較し、Online `ServerRow` を含む全呼出しを追跡。text/size/weight/family/color/widthのcache keyと、miss時に1024件超なら全消去する寿命は両方で一致（消去直後に1件追加されるため最大1025件）。server応答ごとの無制限cache増加や、C++だけの行キャッシュは見つからない。Skia glyph cacheは別のprocess-lifetime cacheとして引き続き区別する。
- `DeckText` の `std::map` lookupを同等のkey equalityを持つ `unordered_map` に置き換える試験も行った。Windows Release buildは通過したが、同条件Maps Scrollは14.84 msから14.83 msと差がなく、C#は8.33 msだったため変更は破棄。キャッシュ寿命/keyは変更していない。
- C++/C#双方で `-uishot` を実行し各26画面を生成。`play-offline` を目視比較し、clip境界の破綻は見られなかった。Onlineの `-uibench play ... -uibenchonly Scroll` は非同期サーバー一覧が揃わず描画0件のため、Online行の性能測定には使えない。
- 同じOffline maps Scrollベンチを20 ms間隔でプロセス採取したピークは、C++ Working Set 95.3 MB / Private 87.4 MB、C# 159.1 MB / 108.5 MB。`-uishot` の26画面を一プロセスで描画したピークも、C++ 99.9 MB / 89.0 MB、C# 199.0 MB / 141.4 MBだった。これら短時間の標準画面測定ではC++の使用量が多いとは言えないが、長時間の増加・実プレイ中のPC全体フリーズは再現も判定もしていない。
- `NativeRuntime/Skia/SkiaText.cpp` の静的監査を完了。`Typeface::Impl` はglyph coverage画像とadvanceを`(codepoint, 26.6 pixel size)`の`std::map`に保持し、上限・evictionはない。`Typeface::Default`のfaceはstatic cacheがprocess lifetime保持する。FaceMutexがFreeTypeとmapアクセスを直列化し、map挿入は既存要素の参照を無効化せず、現行コードは消去しないため、`Rasterize`の返すglyph pointerに現在のrace/UAF経路は見つからない。一方で動的Unicodeや多数のサイズを長時間使えばメモリが増える余地はある。C# `DeckText` のFormattedText cacheは1024件超でclearされるがSkiaSharp内部cacheは別物で、短時間のOffline scroll/26画面`-uishot`採取ではC++ Working Set/Privateの方が低かった。メモリ増加の実測・実害は未確認で、pointer寿命を壊すevictionや所有方式変更は根拠なしに行わず、長時間/動的Unicode入力の確認を保留。

- 9/28 `settings` 1920x1080 ScrollのSkia描画を一時in-process profilerで計測。`DrawBitmap` は73回/270 ms、`FillPath` は2974回/610 ms、`BlendSpan` は598621回/634 ms（inclusive計測なので重複し、合算不可）。WPRのCPU samplingはWindowsのsystem-performance policyに拒否されたため権限変更はせず、計測用コードは撤去した。背景bitmap（1952x1088、alpha 223を含む）を1.75倍で1920x1080へ描く限定Nearest/SrcOver経路を試し、既存と同じsample座標・整数premultiplied合成を使ったが、fast pathは3回/26.7 msで、Scroll renderは変更あり3回の中央値14.54 ms、clean HEAD 3回も14.54 ms（個別値14.49/14.54/15.01 ms対14.30/14.54/14.73 ms）。有意差がないため最適化・profilerとも破棄し、clean HEADでWindows Release `ninja -k 0` 成功。測定したのはheadless surfaceのCPU描画で、GL合成・長時間メモリ挙動・通常プレイ中のPC全体フリーズは確認していない。
- 9/28 `BakedBackdrop.cpp/.hpp` をC# `BakedBackdrop.cs` とキャッシュ寿命・寸法計算まで再照合。両側とも `(width,height,wash,part)` のLRU 6件、32px grain、1辺8192px上限で、通常の1920x1080 benchmark bitmap 1952x1088なら全6面で約48.6 MiB。上限だけを同時に6面使う理論値は約1.5 GiBであり、総byte budgetはない。C#はevict/Forget時に `Dispose`、nativeはcacheの `shared_ptr` を落とし、画面stack上のlive controlが持つ `_image` はそのcontrolが置換/破棄されるまで有効。これはdangling imageを避ける参照寿命で、同じサイズの再描画ごとに新規surfaceは作らず、通常使用時の無制限増加経路は見つからなかった。一方、高解像度や多数の異なるresize寸法では一時的にcache上限を超えるsurfaceがlive viewから保持される余地があり、長時間・高解像度の実測は未実施。C#も同じ6面と辺上限を持つため、変更は加えていない。
- 9/28 `MapShot.cpp/.hpp` のcacheをC# `MapShot.cs` と照合。両側ともOrdinalIgnoreCase room-key辞書に512px幅のdecoded previewを一度ずつ保持し、件数上限なし。公式27 mapsなら512x288 RGBA換算で約15.2 MiBだが、custom map数に応じて生涯使用量は増える。`Forget`は双方にあるが現行呼出し元は見つからず、キャッシュ寿命・null結果の保持も同じ。これはC#原本にもある拡張map数依存の増加余地で、C++固有の破損・漏れとは判定しない。長時間に多数のcustom mapを巡回するruntime測定は未実施。

### 2026-09-28 C++固有のフリーズ対策

- ユーザー指定のフリーズ対策コミットはすべて監査時点の `HEAD`（`229c35a282f0e44d895d5478ce97f137f7710217`）の祖先であることを再確認した。これらは今回のC#対比監査で差異として扱わず、意図的なC++固有修正として維持する。
- Windows watchdog の render thread 強制停止を除去: `852bc7a50f340f80af6575ad17399a82b0a60058`。shader の配列範囲修正: `09f469273d32eafed15a8f75a93f62b9e91021a8`、`c782a756fd72269e10f0a48408463ca64a8549a9`。DS matrix restore index のmask: `30966b618379f05e723f3e7bbc3ff6e4ff8b4c98`。matrix upload bounds: `deba61b4048e46abb4dfab732c7aea57f0a5fa6e`。
- Scene間のGL texture ID衝突修正: `5365698188263af96ab4c74d61fc84a088935568`、`54b17780f071e430e2d445da07984243466a18b3`。display list のScene所有化: `6bedab56b72ab088715b23855fcd5ed84fe0c250`。GL resource cleanup: `056f6eed4d51ef360ce990e03f2108eaf3542b12`、`d021335fc0a8eff380c167fcf078b4523de9be94`、`3137f43d2171b211d9bae4c730a6e3c4bcc6dbbb`、`4f88766a31b9f6061bff1a3a8aa5a98e1038b0a1`。
- miniaudio callback の寿命/data race 修正: `61c132f4eef5b89637872ac3850efa1870478ba7`、`deb50ccf627ca56f8e116507054c1773efeab3c9`。コミットと現HEADへの包含は確認済み。今回、通常プレイでの再発有無を実機確認したものではない。

- 済: 1 Platform helpers / 3 Mods leaves / 8 Multiplayer・teams
  （ea3398e9 まで）。
- 9 Network — 完了（56項目すべてC#対比監査済み。runtime/harnessは各行の記録を参照）:
  - 済 (f661cce0 ほか): NetProtocol（protocol 14、C#原典とnative実装の静的監査済）、
    NetSession・NetSessionLobby、
    SessionProtocol、LobbyRules、MatchDefinition、NetLifecycleTracker、
    NetPlayerLifecycle、ContinuousWeaponPhase、FormReconciliation、NetFaultQueue、
    NetMatchTimeSync、NetHealthSync、NetHudHealth、NetShotDiagnostics、
    NetTimingDiagnostics、NetSmoothing、NetHitClaims（C# と逐行照合済み）、
    NetHitPrediction、NetDamage、NetUnlagged、NetPlayerBridge、NetHooks、
    NetTransport＋NetLag、NetRoomChange、NetStatus、NetSlotManager、NetMatchSync、
    NetDiagnostics、NetFeatureCheck、DemoPlayback、MechanicsDump、MapRotation、
    MapAudit（＋Render/LockjawTrailProbe）、NetLog、ServerSim、ServerSimCheck、
    NetTestScript、HitRig、NetCheckClient、NetLaunch、
    Chat/NetChat、MapPick、PlayerEntityNetAim/NetHud の網関連分。
  - 付随: BeamProjectile / ItemSpawn / ItemInstance / PlayerEntity・Process・
    Collision・Draw の網関連差分、NativeRuntime に Guid・BinaryPrimitives・
    CharIsControl・StringSplit・StringReplaceOrdinalIgnoreCase・
    Console.KeyAvailable/ReadKeyInfo・EndPointEquals。
  - 済: DedicatedServer＋LobbyCommands（全面書き直し）、HostPool（新規）、
    NetMaster（HostCandidate・FindHosts・所有者トークン・CanHost フラグ）、
    NetHostSession、ModEntry の -server 部（-hostports・-affinityweapons）、
    HealthSimulationTest、NetHealthSyncTest、MapAuditTeams、SpireAltPoseCheck。
  - 済: NetLobbyTest（C#原典とnative実装・直接呼出し有無を監査済み）。
- 6 入力 前半: ゲームパッド層を実行時設定オブジェクト化（PadBindingState/GamepadOptionState/
  GamepadRuntimeConfig/GamepadManager/Profiles/Haptics/UiRouter ほか 29 ファイル）。NativeRuntime に
  Numerics(Vector2/3)・Event・ProcessExit・JsonWriteIndented・FileMove・EnvironmentTickCount64。
  GamepadProbe の `DesktopGlContext.PreserveWorkingDirectory` はSection 7の移植時に接続済み。PadRow は 12 まで暫定で新 API 呼び。
- 6 入力 後半: Stylus/Pointer/Pen/WeaponWheel/MouseFlick/AimAssist 一式と PlayerEntity 部分クラス
  （Haptics・MouseFlick・AimAssistWorld）、InputSettings、PlayerInput.cs 差分全部。保留だった
  TakeDamage の Telemetry/Feedback・着地フィードバック・ApplyGamepadAim も解消。
  Section 13で PlayerHud の `UpdateWeaponSelect`（WheelHeld/Absolute/Drag）と
  ModEntry の `-gamepadassisttelemetry` をC#と照合・配線済み。
- 6 完了: 検査系 6 本（PointerCheck は Scene/PlayerEntity に friend、GamepadUiChecks 呼び出しは 12 で）。
  NativeRuntime に DirectoryDelete・DirectoryGetFiles。
- 7 描画 完了: HunterPreview・ProHud・VoteHud・GlEs・Radar・TeamScoreboard・StylusHud と、それらが載る
  PlayerHud.cs 差分（13 の行）を移植。Scene に DrawFlat{Disc,Ring,Line,Square,Polygon}（Renderer.cs 分）。
- 7: MapThumbnail・PlayerEntityMapPick・EndScreen 部分クラス、EndScreen.cs（PanelUp・Tick・結果画面の
  パッド操作）。Scene::DrawHudTexture。呼び出し側（EndScreen::Tick・MapThumbnail::BeginFrame）は 13 の Renderer。
- 7: PreviewPass（ランチャー用プレビュー）・FrameTimingCheck・NoiseField・HunterShot。
  残り 6 本（LauncherPhoto・UiOverlay・LauncherHunter・LauncherNoise・AppIcon・DesktopGlContext）も
  12 と並行して `.cpp/.hpp` 化し、各 C# 原本との静的監査を完了。AppIcon は Renderer の生成経路、
  DesktopGlContext は RenderWindow・ThumbnailCapture・GamepadProbe から接続済み。
  LauncherPhoto・UiOverlay・LauncherHunter の描画呼び出しはSection 12のShell/Rendererへ接続済み。
- 10 マップ生成 前半: CollisionObj（新規）・MapDefinition（Collision・KeepItems・camelCase 出力）・
  MapPacker（ApplyCollision・面属性）・BuiltFace 属性・MapBundle・CustomRooms。
  NativeRuntime に JsonNamingPolicyCamelCase。
- 10: Q3Import（三角形法線・Clip クランプ・Weld 許容誤差・Pickups）・Q3Convert（-noitems・AddItems）・
  MapReport.ListItems。MapCheck（-mapcheck）・AltFormProbe（-altprobe）・MapReport.ListItems（-mapitems）は
  ModEntryから配線・監査済み。セクション10 完了。
- 11 Launcher portable: 移植済み。C#原本との再監査も完了。
  NativeFilePicker・LauncherPrefs・MatchStart・RomWhitelist・TextLauncher・LaunchPlan・GameFiles はC#全文監査完了。GameFilesはPOSIX修正のmacOS/Clang・Linux/GCC CI、Windows Release build済み。POSIX runtime・抽出runtime未実施。
  LaunchPlan（LobbyContext）、GameFiles（Root=AppPaths、RomWhitelist 照合）、TextLauncher（InputEnded・insane・StartupForced）、
  NativeFilePicker（NativeRuntime に ProcessRunCaptureOutput）は移植時の確認を完了。MatchStart は RenderWindow の1ウィンドウ API を使う形へ移植し、
  C#原本との静的監査とWindows Release buildを完了。
- 12 移植差分・C#対比監査は完了（移植作業の判定）。別件ではカーソル問題はユーザー報告で解消済み、Online `did not answer` は原因特定済みで再調査対象外。現在はメニュー性能差と長時間メモリ挙動の確認が残る。詳細は上記進捗ログを参照。C# は Avalonia headless + Skia CPU ラスタ → GL 転送（UiTopLevel/UiSurface/UiOverlay）。
  旧 NativeRuntime/Gui（Element ツリー + GL 直描画、グラデーション・楕円・パス・影なし）では足りないので、
  C# と同じ形で NativeRuntime に再現する:
  (A) NativeRuntime/Skia: CPU RGBA premul キャンバス（AA パス塗り、ストローク、線形/放射グラデーション、
      角丸、楕円、BoxShadow ぼかし、クリップ、変換、不透明度レイヤ、画像、FreeType 文字）。
  (B) NativeRuntime/Avalonia: Control/Panel/Grid/StackPanel/DockPanel/Border/Decorator/UserControl/
      TextBlock/ScrollViewer/Image 等のレイアウト・入力ルーティング・フォーカス・DrawingContext・
      Dispatcher/DispatcherTimer・TopLevel（UiTopLevelImpl 相当）。
  (C) Mods/Launcher/Gui の 50 新規ファイルを (B) の上に一対一移植、(D) 削除 9 ファイルと旧ホストを撤去。
  Tap・TapCheck 完了（GuiTheme に GuiSize）。
  (A) NativeRuntime/Skia 完了（Skia.hpp/.cpp・SkiaText.cpp、JPEG は libjpeg、Android は除外）。
  (B) NativeRuntime/Avalonia 新ツールキット完了: Base（幾何・AvaloniaProperty/StyledProperty/AvaloniaObject）、
      Media（ブラシ・ペン・FontFamily/Typeface・TextLayout/FormattedText・Geometry・Transform・BoxShadows・
      Bitmap・DrawingContext）、Platform（AssetLoader）、Input（Key 値は Avalonia と同一）、Controls
      （Visual/Layoutable/Interactive/InputElement/Control/TemplatedControl、ルーティング・フォーカス・キャプチャ）、
      Panels（Panel/StackPanel/Grid/DockPanel/Canvas/Decorator/Border/ContentControl/UserControl/
      LayoutTransformControl/Image）、Text（TextBlock/TextBox）、Scroll（ScrollViewer、Fluent のオーバーレイ
      スクロールバー込み）、Threading（Dispatcher/DispatcherTimer）、TopLevel（EmbeddableControlRoot・
      RenderTargetBitmap・ヒットテスト・ポインタオーバー）。単体描画テストで確認済み。
- Section 12 の NetLaunch 統合確認: `NetLaunch::TickTerminalLobby` を Renderer の全ビルド共通フレーム入口から呼び、
  HasScene/EndScene、persistent lobby の state reset、MatchStart::Begin、通信失敗時の終了を
  C# 原本の順序で接続。C# 原本・直接呼出し元との静的監査とWindows Release buildを完了。
  これは移植・統合の完了記録であり、別件として残るメニュー性能・長時間メモリ挙動の確認完了を示さない。

### 2026-09-26 進捗監査

`develop2` のコミット済み状態を、バックログ作成コミット（`75f30297`）以降の
対象ファイル履歴と C# 側差分に突き合わせて再確認した。単なる
`NativeRuntime` 共通化・文字列処理・数値処理などの横断リファクタは、
対象バックログ差分そのものを移植していない限り進捗には数えない。

- 追加で **完了** を確認: `Mods/Render/LockjawTrailNoise.cs`、
  `Utility/Console.cs`、`Mods/DebugLog.cs`、`Entities/NodeDefenseEntity.cs`。
- **一部完了 → 完了** に訂正: `Entities/BombEntity.cs`、
  `Mods/Launcher/Portable/LauncherPrefs.cs`。
- `DedicatedServer.cs` はバックログ作成後に専用のネットワーク移植コミットがなく、
  変更履歴は共通ランタイム化のみ。`LobbyCommands.cs` と `HostPool.cs` は
  対応する C++ ファイル自体がまだ存在しない。この3件は、作業中という記録は残しつつ
  表では **未反映（作業中）** とする。
- 追記（同日）: 上の3件は 3f7e8047 で移植済み。NetMaster・NetHostSession・
  健全性テスト4件も完了。BeamProjectile・ItemSpawn・ItemInstance・
  PlayerProcess・PlayerCollision・PlayerDraw は C# 差分の全ハンクを反映済みのため完了。

### 2026-09-26 Section 12 継続作業

- GUI の表にある70項目中、**56項目を C++ 反映・C# 原本監査済み**として更新した。
  `Tap`、`TapCheck`、Deck 系、入力行、一覧・ナビゲーション、背景・テーマ、地理・地図表示など。
- 表外の既存依存ファイルでは `TrackedText`、`ProgressRow`、`CrosshairPreview` も反映・監査済み。
- `ServerBadge.cs` と `ServerRow.cs` を移植し、各 C# 原本と照合済み。
  `ServerRow` の名前末尾判定は C# の UTF-16 長、描画丸めは .NET の ties-to-even に合わせた。
  バッジが使う `System.Net.IPAddress.TryParse` / `IsLoopback` 相当を `NativeRuntime/System/Net`
  に集約し、IPv4 の旧式表記、IPv6 のスコープ、IPv4-mapped loopback を反映した。
- `GamepadGlyph.cs` は形状描画、PlayStation の記号、ファミリー選択、TrackedText のサイズと配置を
  C# と照合済み。
- `LobbyPlayerRow.cs` は roster 配列の読み順、null 名の連結、チーム・READY 表示、列割当、文字スタイルを
  C# と照合済み。配列アクセスは managed runtime の null / 範囲例外経路を使用する。
- `ConfirmScreen.cs` は本文と yes/no mark の構成、初回フォーカス、Escape の false 応答を照合済み。
- `GamepadMonitor.cs` は attach/detach timer、状態差分更新、各スティック・トリガー・文字列の座標と書式を照合済み。
  `UiSurface` の dirty 通知はその未移植ヘッダーと接続するため、Section 12 の統合作業時に再確認する。
- `CreateServerScreen.cs` と内包する `PickRow`・`HostPicker`・`MapRotationPicker` を移植・監査済み。
  12ゲームモードとハンター選択、ホスト探索・再問い合わせ、マップの選択順と16件上限、
  Hosted/Dedicated の起動・接続、失敗時のセッション停止範囲、設定保存と `LaunchPlan` の生成順を照合した。
  `StartScreen` からの接続は完了。`GuiLauncher`/`UiCapture` 統合は後続。
- `PlayScreen.cs` の `.cpp/.hpp` を移植し、全メソッドを C# 原本と照合した。
  PR #1 の修正例も確認し、サーバー行の一押しを選択だけにする動作、応答数と稼働数の分離、
  ディレクトリ進捗行の保持、選択した行からの明示的な JOIN、デスクトップの `NativeFilePicker` 分岐を反映。
  C# の `ToolTip.SetTip` 相当として NativeRuntime に非操作 Popup tooltip を追加した。
  `StartScreen` からの接続は完了。`GuiLauncher`/`UiCapture` 配線は後続。Section 12 の作業中バッチのためビルドはしていない。
- `UiDesigns.cs` を `.cpp/.hpp` へ移植し、6案×4画面の生成順、1280×720 の capture、出力パス・ログ・終了コード、
  12マップ・5サーバー・設定値・各案の配置を C# 原本と PR #1 の追加差分に照合した。
  監査中に D案の一覧スクロールと A案の一時停止列配置の差を直した。
  `UiDesignsAdapter` の `GuiLauncher`/`UiCapture` 実装接続は後続の配線作業。ビルドはしていない。
- `StartScreen.cs` を `.cpp/.hpp` へ移植し、メイン画面のレイアウトと幅別切替、ground の着脱、画面 stack、
  Play/Create/Settings/Setup/Confirm の遷移、persistent lobby の復帰・一時停止、pause 操作と map vote、
  preview の追いつき、version/update の状態表示・進捗・installer 完了経路を C# 原本と PR #1 の差分に照合した。
  更新確認は `shared_from_this()` が有効になる `Create` factory の直後に開始する。
  `GuiLauncher`/`UiCapture` からの接続は後続。Section 12 の作業中バッチのためビルドしていない。
- `LobbyScreen.cs` と内包 `CustomTeamPicker` を `.cpp/.hpp` へ移植し、PR #1 の追加差分と C# 原本に照合した。
  roster/session revision によるプレイヤー再構築、1秒ごとの ping 更新、最新12件のチャット表示、
  owner/team 操作、match 定義検証と数値の invariant parsing、250ms 後の自動反映、map/custom-team picker、
  thumbnail の生成・解放経路を照合した。ネイティブ timer は attach 時に接続して start し、
  close callback 中も timer dispatch が戻るまで画面を保持する。`GuiLauncher`/`UiCapture` 配線と統合ビルドは後続。
- `InGameMenu.cs` を `.cpp/.hpp` へ移植し、PR #1 の追加差分と C# 原本に照合した。
  pause/settings/map-vote の stack 操作、spectate/rejoin/record/leave/quit の各 callback、投票時の二段 pop、
  Escape の handled 条件を確認した。イベント中に menu が UiSurface から外れても処理が戻るまで親を保持する。
- `EndPanelView.cs` を `.cpp/.hpp` へ移植し、PR #1 の追加差分と C# 原本に照合した。
  ballot order の snapshot、投票数/leader の差分描画、hunter/suit の commit と再読込、ready 状態、
  hunter stand の ballot 面での非表示、右側 panel の構成を確認した。
- `Shell.cs` を `.cpp/.hpp` へ移植した。ローカル C# 原本は PR #1 掲載ファイルと同一 blob。
  match/lobby/menu の遷移、例外報告、pause menu、入力変換、window capture script を監査した。
  監査で shellshot の `shell-server-side` 撮影漏れを見つけて修正し、24枚の撮影名・39個の待機値・
  38個の script action の順序が C# と一致することを再確認した。RenderWindow/MatchStart/ScreenCapture と
  Diagnostics の呼び出し先は各依存セクションの移植時に接続する。Section 12 の作業中バッチのためビルドしていない。
- `UiCapture.cs` を新しい `UiTopLevelImpl`/NativeRuntime の画面層へ移植し、26画面の名前と順序、
  画面サイズ、`Deck.Phone`/`Deck.Still`/`PlayScreen.Sample` の更新位置、fleet/browser の全サンプル値、
  PNGと同名JSONのVisual bounds出力、失敗ログ・終了コードを C# 原本と PR #1 差分に照合した。
  監査では旧3行サンプルとサーバーヘッダー、旧画面名、旧 Avalonia adapter を除去した。
  `ModEntry` の実行入口と `UiDesigns` の共通 capture 接続は後続の統合作業。ビルドはしていない。
- `GamepadUiChecks.cs` を `UiTopLevelImpl` 上の同一 UI 操作・入力・設定画面検査へ移植した。
  C# の35個の assertion 名と順序、コントローラー action 数、キャプチャ競合/取消/切断、preset・keyboard rebind、
  pause/map vote と任意 PNG 3枚を照合した。ネイティブの `GamepadSettingsPanel::Reload` で旧 Visual が破棄されるため、
  再読込後に Advanced/monitor と対象 PadRow を現行ツリーから取り直す。`GamepadChecks::Run` の
  `CheckPersistence` 直後に shell build のみで呼ぶ接続を追加した。
  C# の assertion 一覧は35/35が一致。ビルド・実行は未実施。
- `GuiLauncher.cs` を新しい `Shell::Run` と NativeRuntime の process-wide setup に移した。
  一度だけの setup、display probe、Android 分岐、fallback 文言、Linux の fontconfig 案内を照合し、旧 AppBuilder/
  HomeWindow adapter・別窓ループを除去した。PauseMenu から `EnsureSetup` を直接呼ぶようにし、旧 helper と Dispatcher pump を削除した。
  C#例外時の `PlatformDiagnostics.Report` は Section 2 の移植後に二経路とも接続した。
  `PlatformDiagnostics.Start` とsmoketest/GLFW/window diagnostic dispatchはSection 2で接続し、Windows Release build済。
- `UiBench.cs` を `.cpp/.hpp` へ移植し、5解像度・6シナリオ、10回 warm-up / 61回計測、中央値、surface寸法と倍率、
  スクロール・pointer・wheel操作、PNG出力をC#原本と照合した。NativeRuntime には旧Avalonia `Window` と managed GC がないため、
  Slow計測は全画面copyを行うheadless rigとして明示し、GC欄は `n/a` とした。layout数はNativeRuntimeが通知するTopLevelの
  layout passを数える。`LauncherPhoto` はSection 7から延期された依存だがSection 12で接続済み。Windows Release build済、実画面計測は未実施。
- `LauncherNoise.cs` を `.cpp/.hpp` へ移植し、既存 `NoiseField` による形状変更検出・33ms upload cadence、固定texture名、
  RGB upload と全unpack state、nearest/clamp sampler、失敗時fallbackと context-current `Release` をC#と照合した。
  desktop GL wrapper に必要な `PixelStoreParameter` 値を追加した。再監査で、C#のstatic field初期化はLauncherNoise初回アクセス時だが、
  C++ namespace staticはプロセス起動時に時計を始める差を検出。初回アクセスでNoiseField→upload clockの順にlazy初期化するStateへ修正した。
  `Texture` getterと`Release`もC#の型初期化を起こす順に揃えた。ビルド・実行は未実施。
- `Shaders.cs` のSection 7依存部分を先行移植し、`BackdropVertexShader` / `BackdropFragmentShader` の本文を
  C#と改行正規化後の全204/429文字で完全一致照合した。残りのShaders差分はこの2プロパティのみ。ビルドは未実施。
- `LauncherPhoto.cs` を `.cpp/.hpp` へ移植し、GL shader compile/link・固定texture名・RGBA JPEG upload、全unpack状態、
  texture sampler、UniformToFillの中央crop、noise shader時の2 texture座標と固定機能状態復元、失敗時の静止画fallbackを
  C#原本と照合した。NativeRuntimeの既存AssetLoader/JPEG decoderを使い、GLに足りなかったprogram query/log/delete、
  multitexture座標、matrix stack、texture environment、base/max level APIを追加した。RendererからのDraw接続はSection 12
  統合作業に残る。native実行ファイルの出力先でAssetLoaderが写真を見つけられるよう、CMakeのpost-build資産コピーにも
  `launcher-bg.jpg` を追加した。ビルド・実行は未実施。
- `LauncherHunter.cs` を `.cpp/.hpp` へ移植し、wanted/drawn と hunter/suit/矩形の状態、side scene の process lifetime、
  preview scene の初期化、Scene preview state 設定、実際に描けた場合だけ穴を開ける判定、失敗のsticky停止・ログを
  C#原本と照合した。`RenderWindow.HasScene`/`NewSideScene` はSection 13側で接続済み。Windows Release build済、画面runtimeは未実施。
- `UiOverlay.cs` を `.cpp/.hpp` へ移植し、固定texture名、同寸法でのTexSubImage2D、変更時のみTexImage2D、RGBA premul blend、
  unit-1無効化、window viewport / identity matrix、clear→photo→overlay→hunter の単独画面描画順、Release後の状態を
  C#原本と照合した。OpenTK薄いAPIに `BlendingFactor::One` と `TexCoord2` を加え、Renderer/ModEntryからSection 12で接続した。
  `LauncherHunter` の `HasScene`/`NewSideScene` もSection 13で解決済み。Windows Release build済、画面runtimeは未実施。
- `AppIcon.cs` を `.cpp/.hpp` へ移植し、埋め込みPNGの取得に対応するnative配布asset、RGBA decode、16/32/48/originalの順、
  alpha-weighted box filter、once-only cache、失敗時ログをC#原本と照合した。GLFW `Window::SetIcon` を追加しRendererから接続。
  Renderer呼び出し順も再監査し、C#と同じく未対応機能callbackを設定してからwindowサイズを問い合わせる順へ修正した。
  ビルド・実行は未実施。
- `ModEntry.cs` のSection 12入口を静的監査し、`-uishot`/`-uibench`/`-uidesign`/`-shellshot`/`-frametimingcheck`/
  `-tapcheck` をゲームファイル確認より前に配線した。`-uibenchscale` はC#の `NumberStyles.Float` に合わせて
  thousands separator を許さず解析する。`-uinativeres` と fullscreen/windowed の起動前適用も移した。
  `UiCapture::Run` の旧adapter呼び出しを現行公開APIに合わせ、CMake desktop target にC#相当の `MPHREAD_SHELL` を定義した。
  C#の `RunUi*` no-inline helper、例外文、返却値、コマンド順を確認した。ビルド・実行は未実施。
- 同じ `ModEntry.cs` の早期入口を追加照合し、`-pointercheck`、AimAssist debug/telemetry の設定、
  `-gamepadcheck`、`-gamepad`（`verbose` を含む）、`-frametimingcheck` を `TryHandleHeadless` に接続した。
  `GamepadChecks.cs` は C# と同じ `CheckPersistence` 直後に、desktop shell で `GamepadUiChecks::Run(shots)` を呼ぶ。
  ビルド・実行は未実施。
- `DesktopGlContext.cs` を `.cpp/.hpp` へ移し、GLFW error callback、macOSのworking-directory/menu-bar init hint、
  macOS 2.1 Any profile / 他desktop 3.2 Compatability settingsをC#と照合した。callback登録はGLFW初期化を早めない薄いadapterにし、
  `RenderWindow::Settings`、`ThumbnailCapture`、`GamepadProbe` の各C#呼び出しへ接続した。Android compileではGLFW APIを参照しない。
  ビルド・実行は未実施。
- C# 側で削除された旧 GUI 9 ファイル（`DemoPickerView`、`HomeView`、`HomeWindow`、`MapPickerView`、
  `MenuEntry`、`PauseMenuWindow`、`SettingsWindow`、`SplashView`、`UpdateBadge`）を1つずつ原本パスで確認した。
  すべて C# 原本がなく、Native 側の対応物も削除済みで、残存ソース参照もない。
- `Mods/PauseMenu.cs` をC#原本に合わせて再配線した。旧ウィンドウ矩形、追従、別窓pumpを削除し、
  `GuiLauncher::EnsureSetup` と `Shell::OpenPauseMenu`/`CloseMenu`/`Quit`/`LeaveMatch` を接続した。
  refocus、fullscreen toggle、topmost同期、leave/quit flag の順序を照合した。
  per-frame `Shell::TickUi` とシェル画面の overlay 描画は `Renderer.cs` と照合して接続した。
  `MatchStart.Begin` は既存窓へのシーン構築へ移行済み。ビルドはしていない。
- `HunterStand.cs` は7ハンターの色・84個の箱と順序、Suit再着色、描画投影・面順・ライト、33msタイマー、
  回転ドラッグ、エンジン描画との切替、スクリーンショットの寸法丸め・BGRA→RGBA変換・失効判定を照合済み。
  箱データ84件は原本から抽出した値を全件比較した。奥行きソートは C# `List.Sort` の比較順と
  `ManagedSort` を使い、同値時の並びも .NET と一致させた。`MPHREAD_SHELL` 分岐の描画接続と
  `NewSideScene` を追加し、`Scene.SideScene` の初期化抑止をC#と照合した。
- `Renderer.cs` のシェル依存部を静的監査し、`RenderWindow(bool shell)`、`HasScene`、
  `NewSideScene`/`BeginScene`/`LoadScene`/`EndScene`、シェル時の画面寿命・入力経路・
  フレーム描画合成・ウィンドウ位置保存をC++へ接続した。GLFW adapter はキー解放と横ホイールも渡す。
  C# の `SideScene` が抑止するプレイヤー初期化と端末プロンプト、描画設定ログも照合して反映した。
  続けて `MatchStart.cs` と `NetLaunch.cs` の保留差分を移植し、各C#原本と直接呼出し元を静的監査した。
  `NetLaunch.cs::TickTerminalLobby` は persistent lobby の scene 終了→match state reset→terminal message、
  scene の有無による早期 return、pump/input、timeout/refusal/leave 時の close、`ShouldLoadMatch` 後の定義取得と
  `MatchStart::Begin` の例外処理を照合した。`Renderer::OnRenderFrame` では frame-rate 設定直後に呼び、
  C#と同じ clear/swap/base-frame/return の順にした。`git diff --check` は通過。
  `Renderer.cs` の Windows pen / pointer、AimAssist debug、gamepad/focus、frame/draw更新順はnativeへ接続し、
  監査で見つかった保存・resize・maximize差分も修正済み。Section 12 完了後のWindows Release buildで確認した。
- Section 12 は全画面の差し替えと `ModEntry`/`PauseMenu` の再配線まで完了。後続のSection 13修正を含む
  Windows Release `ninja -k 0` も成功。画面runtimeは未実施。
- `GamepadSetupPanel.cs` は入力行の順序、Android で隠す項目、timer の開始・停止、mapping の neutral 待ち、
  20段階の wizard 呼び出し、校正の計測時間、取消・適用・例外表示を原本と照合済み。
  `DispatcherTimer` が native では callback 付き構築時に自動開始するため、C# と同じく未開始で生成し、
  `Start` 時だけ動くようにした。
- `GamepadProfilePanel.cs` は初期化と profile 名一覧、選択・入力欄、6操作の順序、操作ごとの変更通知、
  成功・例外ステータスを C# と照合済み。native の `InvalidDataException` は `IOException` と別型なので、
  両方を個別に捕捉する。
- `GamepadSettingsPanel.cs` は timer の可視性・revision 条件、フォーカス復元、端末選択、詳細設定の並びと値域、
  controller family / curve の表示名、変更後の再読込を照合済み。初期 slider 値は C# の double `Math.Round`
  と同じ計算精度・ties-to-even を使う。
- `ControllerKeyboard.cs` はキー配列・文字 ID・初期フォーカス、Shift / Delete / Done / Cancel、`MaxLength` と
  1024 UTF-16 code unit 上限、Popup の親・対象・閉じる順序、対象切断時の取消、確定後の focus と callback を照合済み。
  native 側は TextBox と最寄り Panel の参照を保持し、確定値を Popup を閉じる前に反映する。
  `Media::ToUtf32` / `ToUtf8` は UI 内 managed string の WTF-8 を往復させ、補助キーの Delete が補助文字の
  UTF-16 code unit 1つを消した場合も lone surrogate を保持する。
- `GamepadNavigation.cs` は root 変更時の router reset、PadRow への同一 snapshot 注入、KeyRow の controller press、
  Changed の発火位置、Accept / Back / tab / page scroll / 矢印キーの分岐順を照合済み。native 側では C# の
  管理参照寿命に合わせ、イベント中の root・focused control・tab・ScrollViewer を保持する。
- `UiTopLevel.cs` は Avalonia 実装の pixel buffer・Drawn/Painted・client size・mouse/touch/key/text input を
  `NativeRuntime::Avalonia::EmbeddableControlRoot` へ接続する薄い wrapper として移植・照合済み。
  `UiRenderTimer` はゲームフレームから native TopLevel を明示 pump し、TouchBegin 前にも同じ pump を行う。
  KeyEventArgs が `PhysicalKey` を保持するよう native Avalonia 入力型も補った。
- `UiSurface.cs` は生成・表示・非表示・resize/raster cap・scale bake・pending frame の実行順・dirty redraw の
  50/250/3000/16/66 ms 条件・計測ログ・pointer/button/wheel/key/text 入力・ClickOn/HoverOn の座標変換を
  C# と照合済み。`FrameClock` は C# の型初期化と同じく最初の static API 呼び出しで開始する。
  `UiOverlay` 呼び出しは、対応する Render ファイルを移植する Section 12 内で解決する。
- `UiScaleHost.cs` は decorator/transform の構成、screen の get/set、measure 前の factor 更新、DIP→point の
  `160/96` 換算、BakeScale に掛ける RenderScaling、factor 不変時の早期 return と変更ログを照合済み。
  `FactorFor` は C# の Infinity・非正値条件を保持し、`NaN` も .NET の算術結果どおり伝播させる。
- `PauseMenuView.cs` は menu の項目条件と順番、200ms の vote timer、Accept/Deny の即時応答、表示中の項目数に
  基づく NeededHeight、0.5〜1.0 の縮尺、Escape と初回 Resume focus を C# と照合済み。
  `UiCapture.cs` の移植で新しい capture 経路から直接生成するよう更新済み。`ModEntry` の入口配線は後続。
- `MapCardPicker.cs` は Factory の room code/metadata、OrdinalIgnoreCase の初期選択、空 room の診断、候補選択と
  use/back/Escape、選択名の表示を照合済み。native Avalonia は親を子より先に attach するため、初回 focus は
  grid の子が attach された後に dispatcher へ送る。`LobbyScreen` / `PlayScreen` からの呼び出しは接続済み。
- `SettingsView.cs` は旧 adapter を外し、現行 C# と同じ5ページの `UserControl` に置き換えた。Display/Audio/Controls/
  Profile/Credits の構成、FOV のライブ変更と Cancel 復元、保存時の設定適用、controller reset、touch/stylus の条件、
  debug log の共有・endpoint 検証を照合済み。`HunterStand` の移植と `UiCapture`・各画面の呼び出し接続は後続。
  Section 12 の作業中バッチなので、ビルドは行っていない。
- `SetupScreen.cs` は初回フォーカス、Escape/back 条件、ROM 選択、抽出中のログと進捗、成功後のプレビュー生成、再描画操作、
  末尾8行保持を C# と照合済み。native headless desktop の file picker を既存 `NativeFilePicker` に対応させた。
  `StartScreen` からの接続は完了。`GuiLauncher`/`UiCapture` 配線は後続。Section 12 の作業中バッチなので、ビルドは行っていない。
- `ThumbnailHost.cs` の native 対応を、削除される Avalonia Host adapter 経由から直接の C++ `shared_future` API に更新した。
  空一覧・登録済み host・batch 不可・`Task.Run` 相当の分岐、結果と例外を照合済み。`SetupScreen.cs` と `StartScreen.cs`
  が使う前提として追加した。Section 12 の作業中バッチなので、ビルドは行っていない。
- 全体の残り: Section 12 は一括交換が未完了で、作業ツリーはビルド不可。現状の未コミット差分を保持し、
  残りのGUIファイルをC#ごとに照合して完了させる。Androidは全Androidファイルの完了後にビルドする。
  このスナップショットは統合前の記録。Section 12は2026-09-27に完了し、その後Section 2・13・14と延期呼び出し元も完了した。

### 2026-09-27 再開後監査

- `UpdateCheck.cs` と `.cpp/.hpp`、直接呼び出し元を一ファイル単位で全体照合。HTTP設定・status/error分類・JSON/asset選択・RID/名前・version比較・`LastReason` の更新順を確認し、`CancellationToken` がnative HTTPで捨てられていた差を修正。`std::stop_token` を libcurl progress callback へ渡し、事前キャンセルと転送中断を `TaskCanceledException` 相当へ対応させた。Section 12一括交換が未完了のためbuild・実行確認は保留。
- `Updater.cs` と `.cpp/.hpp` を全体照合。Disabled時・例外時を含む`Available`/`Checked`更新、background `found`/`done` と例外境界、50ms wait・DateTime範囲、HTTPS限定とplatform/browser起動、Describe出力を確認した。`ModEntry -update`、StartScreen、TextLauncher、SettingsView、ServerUpdateの直接呼び出しも照合し、追加修正は不要。Section 12一括交換が未完了のためbuild・実行確認は保留。
- `DesktopUpdate.cs` と `.cpp/.hpp`、UpdateInstall/ServerUpdate/ModEntryの直接接続を照合。stage・展開・launch/apply引数・30秒終了待ち・400ms猶予・上書きcopy/retry・clean・実行属性を確認した。C#の失敗ログは例外型も含むためnativeのstage/launchログを `ExceptionToString` に合わせた。ModEntryのparse失敗値 `-1` をPOSIX `kill(-1, 0)` に渡していたため、非正PIDは無効PIDとして400ms後に続ける。download tokenはUpdateDownload監査で送信中・body読込中のC#相当pollを追加。Section 12一括交換が未完了のためbuild・実行確認は保留。
- `BuildVersion.cs` と `.cpp/.hpp`、直接参照元を照合。entry/fallback stamp選択、Lazyの一度だけ評価と例外保持、`+sha`切除、空白・v接頭辞・prerelease・2〜4成分・Int32境界、normalise/比較/表示を確認した。local buildはC# projectとnative compile-adapter双方で未stampをlocal扱いし、コード差はなかった。C++ release stampはcompile adapter defineで渡す設計だが、現行release workflowはC++ targetを作らないため未実ビルド。
- `NetChat.cs` と `.cpp/.hpp` を照合。64件の履歴更新、system/team行、Receive/Remember順、空白送信抑止と `NetSession`/`NetSessionLobby`/`LobbyScreen` の直接接続は一致。C#の `Revision++` はunchecked wrapなのでnativeの符号付きoverflowを `UncheckedAdd` に変更した。build・実行確認はSection 12統合後。
- `ThumbnailBatch.cs` と `.cpp/.hpp` を一ファイル単位で照合し、Macの並列数、既存キャッシュの再確認、batch/worker失敗状態、5分timeout、非0終了のリトライ抑止、worker終了処理、stdout/stderr排出、64行・2048 UTF-16 code unit制限、成功/失敗報告を反映した。直接呼び出し元は既定timeoutを使うためAPI変更の追加配線は不要。現在の Section 12 一括交換が未完了という引継ぎ指示に従い、build・実行確認は保留。
- `ScreenCapture.cs` と `.cpp/.hpp` を全体比較し、window back-buffer capture、GLバージョンによるcontext flags/profile取得、KHR_debug拡張とprocの可用性確認、Androidでの診断停止、再有効化時のmessage数reset、GL callback例外の遮断を反映した。`NativeRuntime/OpenTK/GLFW` に対応する2つの薄いAPIを追加。直接呼び出し元との引数・capture時点も確認し、`git diff --check` は通過。Section 12一括交換が未完了のためbuild・実行確認は保留。
- `ThumbnailCapture.cs` と `.cpp/.hpp` を一行ずつ照合し、診断開始/scene読込ログと実際の `ClientSize` を反映した。独自の手動ループを `RendererPlatform::Window::Run(WindowEvents&)` に置き換え、GLFW event polling、frame callback、close時のcleanupがC# `GameWindow.Run()` と同じライフサイクルを通るよう修正。`ModEntry`/`ThumbnailBatch` の呼び出し引数と公開 `WindowSettings` の診断呼び出しも照合。現在のSection 12作業中バッチではbuild・実行確認を保留。
- `ThumbnailLog.cs` と `.cpp/.hpp` を照合し、開始時に欠けていた informational source version と executable path を追加。並列workerの追記はC# `File.AppendAllText` 相当の共有モード・失敗分類を持つファイル内 `WriteBytes` に接続し、既存の5回/20ms再試行、パス再評価、UTF-8 no-BOM出力と例外時の無効化を確認した。build・実行確認はSection 12統合後。
- `ChatBox.cs` と `.cpp/.hpp` を全体比較。受信fallback・system/team表現・64件上限・10秒表示/1秒fade・送信・80文字ASCII入力・開閉とキー処理を確認した。Rendererのdesktop入力、Android `GameView` のkey/Unicode入力、NetChat/NetSession/NetCheckClientと直接呼び出しも照合し、挙動差と追加修正は不要。typed inputがASCII限定のためnativeのASCII space trim・byte countはC# `Trim`/UTF-16 countと同じ結果になる。Section 12統合前なのでbuild・実行確認は保留。
- `UpdateDownload.cs` と `.cpp/.hpp` を全体照合。URL許可、HTTP status/headers、redirect/cookie、10分のheader timeout、content length・progress、部分ファイル作成/cleanup・置換順を確認。C#の`Send(..., cancel)`と各read後の`ThrowIfCancellationRequested()`がnativeで無視されていたため、header転送中のlibcurl中断とbody chunk読込後の書込み前pollを実装。`DesktopUpdate.Stage`と`LocalServer.Install`の同期呼び出しも照合した。build・実通信確認はSection 12統合後。
- `ServerBadge.cs` と `.cpp/.hpp` を原本から照合し、`ServerRow`からの描画引数、IPv4/IPv6のendpoint切出し、loopback/LAN/Internet/Silent分類、country fallbackとFlags描画、色・pixel座標を確認。今回追加されたNativeRuntimeの `IPAddressTryParse`/`IPAddressIsLoopback`/`IPAddressGetAddressBytes` 部分だけも .NET 10 `System.Net.IPAddress` と照合し、IPv4-mapped loopbackを含め一致。差分修正は不要。Section 12統合前なのでbuild・描画確認は保留。
- `ConfirmScreen.cs` と `.cpp/.hpp` を再監査。questionとyes/noの配置、modal/nav scope、cancelを初期focusにするdispatcher順、Escapeでfalseを返してHandledにする動作、StartScreen/UiCapture/GamepadUiChecksの直接接続は一致。追加修正なし。
- `PlayScreen.cs` と `.cpp/.hpp` を全メソッド再監査。モード/ハンター順、5画面の再構築、オンライン一覧と非同期応答、endpoint解析、設定保存と `LaunchPlan`、story/demo/picker/vote/preview、StartScreen/InGameMenu/UiCaptureの接続を照合した。C# `Math.Round` の ties-to-even と同じにするためBodyGrid/BarRowの3箇所を `MathRoundToInt32` に変更し、非Online面の初回 focus はC#同様に同期実行、Onlineだけ dispatcher 経由に修正。Section 12一括交換のためbuild・実行確認は保留。
- `SettingsView.cs` と `.cpp/.hpp` を全体再監査。5ページ/Controlsの3 sub-page、FOVの即時反映とCancel復元、Display・Audio・入力・Profileの保存順、FPS stop、crosshair/radar表示、Android touch項目、stylus placement、ログ共有/endpoint検証を照合。StartScreen・InGameMenu・UiCapture・GamepadUiChecksの直接接続も一致し、今回の追加修正はなし。Section 12一括交換のためbuild・実行確認は保留。
- `PauseMenuView.cs` と `.cpp/.hpp` を全体再監査。項目の条件/順序、vote timerとAccept/Deny後のResume、200ms refresh、fullscreen label、Escape/初回focus、host高さに対する0.5〜1.0縮尺を照合。AndroidのStartScreen、desktopのInGameMenu、UiCaptureの呼び出しも一致し、追加修正はなし。Section 12一括交換のためbuild・実行確認は保留。
- `EndPanelView.cs` と `.cpp/.hpp` を全体再監査。ballot snapshot/order、カード生成と選択、leader/tally差分描画、hunter/suit commit・再読込、ready/eligible表示、Shellの表示/refresh/hide順とUiCaptureを照合。空ballotの説明文にC#指定の水平・垂直中央揃えが欠けていたためnativeへ追加。Section 12一括交換のためbuild・描画確認は保留。
- `LobbyScreen.cs` と内包 `CustomTeamPicker` をC#全体と `.hpp/.cpp` で再監査した。roster/session revision、ping更新、最新12件のチャット、owner/team操作、ゲーム形式・数値入力の検証、250ms後の自動反映、map/custom-team picker、thumbnailの生成・解放、timerのattach/detachと閉鎖経路を照合。`StartScreen` の生成・MatchRequested/Closed配線も確認し、差分なし（verified no-op）。統合ビルドはSection 12完了後。
- `InGameMenu.cs` と `.cpp/.hpp` をC#全体および直接呼び出し元 `Shell` と照合した。pause/settings/map-vote stack、Resume/Fullscreen/Spectate/Rejoin/Record/Leave/Quit、vote時の二段pop、EscapeのHandled、閉鎖中のshared lifetimeを確認し、差分なし（verified no-op）。Section 12統合buildは保留。
- `PauseMenu.cs` と `.cpp/.hpp` をC#全体、RendererのEscape/Poll呼び出し、ShellのTickUi/Open/Close/Quit/Leave接続と照合した。フラグと順序は一致。`window.Focus()`の保護がnativeだけcatch-allだったため、C# `catch (Exception)`相当の `std::exception` 捕捉へ限定した。Section 12統合build前の静的監査。
- Section 12統合後のWindows Release Ninja buildを再実行し、Avalonia/Skia・GUI・PauseMenuを含む全コンパイルと `FruityPrime.exe` のリンクを確認。PowerShell起動時に `mingw64/bin` がPATHに無く `cc1plus.exe` のDLL解決に失敗した初回診断を除き、MSYS2 binをPATHへ追加した実行ではエラーなし。増分確認は exit 0 / `ninja: no work to do`。画面runtimeは未実施。Android buildはユーザー指示どおり未実施。
- `AndroidGamepadProfile.cs` を `.cpp/.hpp` へ移し、`InputDevice.GetMotionRange` の有無で右スティック・trigger軸を選ぶ順、左スティックの両軸判定、未対応軸のゼロ値を照合した。`GamepadBridge::HandleMotion` へ接続し、C++旧実装がフレーム値0を根拠に別軸へ切り替えていた差を修正（対応軸が0を報告してもC#同様その軸を読む）。Android build・端末実行は未実施。
- `AndroidThumbnails.cs` と `.cpp/.hpp` を全体照合した。host がApplication時点で登録されActivityをrender時に引くこと、worker上限が `min(core数, heapMB/24)` であること、進捗ごとに60秒stall時計を戻すこと、全画像完了時にmarkerを待たないことを反映した。`MainActivity.RenderPreviews` のMovingBackdrop停止・keep-screen-on・fallback進捗/所要時間報告を照合し、存在しない `Thumbnail*Ref` 型をC#のrooms/reportと共通 `IThumbnailHost` に一致するnative vector/callback/shared_futureへ置換。Android ABI buildで検証中。
- `AndroidGamepadHaptics.cs` のnative対応をC#原本と照合。デバイス/vibrator不在時のno-op、`HasVibrator`、1〜500ms、API 26以上のone-shot/amplitude-control、未対応時のdefault amplitude、旧APIの`vibrate(ms)`、`Stop()`の`Cancel()`を対応させ、Android manifestへVIBRATE permissionを追加。JNI descriptorとAPI-level取得の例外確認も監査した。
- `GamepadBridge.cs` をPR #1と作業ツリーのC#原本に照らして監査し、端末ごとの状態/ID/name/family、Gamepad/Joystick source判定、profile/capabilities、haptics登録、key repeat、motion/hat、clear/remove処理を実装した。C++側がグローバル状態を更新していた差と、DPADだけのdeviceをgamepad扱いする差を修正し、trigger button判定はC#同様`GamepadManager`の設定依存thresholdへ任せた。`MainActivity.cs`の起動/終了/フォーカスと`InputManager`通知をActivity owner経由で接続した。
- `MainActivity.cs` と native の lifecycle/render-preview/start-wait/pause/end を全体監査。`StartMatch`のin-process preview停止待ちとhunter-shot retire、100ms end-panel tick/Hide、pause中のend-panel抑止、UI surface dispatcher/render pump、lobby保持終了と `NetSession.ResetMatchState`/`ResumeLobby` を反映し、`AtomicSharedPtr` をAndroid libc++対応へ修正。TouchOverlayViewからend-panelへのタッチ配送もC#原本どおり接続。C#にない `MainActivity.cpp::CustomizeAppBuilder` は `MainApplication.cs` のbuilder処理を担うnative host seamとして別途照合した。
- `AndroidHunterShot.cs` を `AndroidHunterShot.cpp/.hpp` に移し、PR #1 の追加差分とC#原本を照合した。最新要求だけを処理するqueue、semaphore相当のpermit、4秒のRetire待ち、pbuffer/Sceneの再利用とresize再生成、match中のnull応答、最大3回のpreview draw、RGB readbackから上下反転BGRA変換、例外ログと失敗後の無効化を反映。ビルド時に検出したConsole出力のnamespace誤りもC# `Console.WriteLine` 対応先へ修正。`MainApplication` installと`MainActivity` Retire接続済み。Android ABI build継続中。
- `AndroidWebLink.cs` を `.cpp/.hpp` 化し、Application Context保持、`MainActivity.Instance` 優先、ACTION_VIEW Intent、Activity以外のContextにだけ `FLAG_ACTIVITY_NEW_TASK` を付ける条件、例外時の `[android] could not open ...` とfalse応答をC#原本/PR #1と照合した。build時に見つけたConsole出力先namespace typoも修正。Application Contextをnative hostから渡し、`MainApplication`登録済み。Android ABI build継続中。
- `AndroidUiSurface.cs` と `.cpp/.hpp` を一ファイル単位で全体監査した。rootの透明背景・透明レベル・Dark theme、scale/density、focus post、hide時の状態順、premultiplied RGBA frameのcopy/version、touch routingとpointer IDを照合し、透明/theme設定とunchecked long相当のwrapを反映した。`std::atomic<std::shared_ptr>` は Android libc++ 対応の `AtomicSharedPtr` に置換。dispatcher/render timer pump、MainActivity/GameViewのUI tick・描画合成、およびTouchOverlayView経由の入力配線まで接続済み。Android build待ち。
- `AndroidUiOverlay.cs` と `.cpp/.hpp` をshader sourceからReleaseまで全体監査した。uploadの寸法/byte数判定、ES 3.0 shader link、quad/texture setup、上下反転UV、premultiplied blend、depth/state復帰の範囲、失敗時の無効化とcontext-current前提の解放を照合。C++のcatch-allはC# `catch (Exception)` にない捕捉を増やしていたため除去。GameView配線済み。Android build待ち。
- `GameView.cs` と `.cpp/.hpp` を全体監査し、固定更新後のsession拒否/timeout/lobby遷移、load成功/失敗通知、render後のUI texture合成とhunter preview hole、`B|Start`によるchat cancel、`EndMatchToLobby`へのUI thread dispatchを接続した。C#の`catch (Exception)`を越えるcatch-allを除き、同時に`Exception.ToString()`相当のnative診断へ修正。C#更新順に合わせるため`MainActivity::EndMatchCore(keepSession)`を追加。UI surfaceのUI thread tick・描画・タッチ配線まで接続済み。Android build待ち。
- `TouchOverlayView.cs` と `.cpp/.hpp` を全体監査し、色・Paint設定・描画・密度によるレイアウト・touch actionを照合。結果パネル表示中はタッチボタンを描かず、pointer down/upは消費し、それ以外のpointer 0をAndroidUiSurfaceへ渡すC#の経路を反映。Android build待ち。
- `MainApplication.cs` のbuilder処理はnativeで `MainActivity.cpp::CustomizeAppBuilder` に実装されているため、その対応範囲を照合。`WebLink` と `AndroidHunterShot` の登録漏れを追加し、ログ共有/WebLinkにはActivityではなく `getApplicationContext()` の結果を渡すよう修正。`AndroidUpdateInstaller.cs` も別途照合し、C#同様に構築時はActivityを保持せず、各操作時に最新の `MainActivity.Instance` を取得するよう変更。Android build待ち。
- Android NDK 27.2.12479018 / compile API 24で `arm64-v8a` と `x86_64` をconfigure済み。最初のcompileはMSYS2 include root全体を渡してNDK libc++の `stdlib.h` を隠したため失敗し、curl/libarchiveの必要ヘッダーだけを隔離したinclude rootへコピーして両ABIを再configureした。ユーザー指示により、Android全ファイルのC#照合完了までAndroid buildは実行しない。
- この監査後にWindows Release `ninja -k 0` を再実行し、終了コード0・`ninja: no work to do` を確認。対象にAndroid専用コードは含まれないため、Android buildは未確認。
- 上記の監査中にWindows Release Ninja buildをMSYS2 shellで再実行し、Section 12を含む全ターゲットが成功した。PowerShellからの直接実行ではMSYS2環境が設定されず診断なしで失敗したため、正しいMSYS2環境で再確認した。Android SDK/NDKは後続調査で存在を確認済み。Android buildは全Androidファイルの監査完了後に実施する。
- Section 12 の一括切替、`MatchStart`、RenderWindow/GL の依存を含む Windows Release Ninja build が
  `ninja -k 0` で成功した（ログ: `%TEMP%\fruity-prime-native-ninja-20260927-retry4.log`）。
  POST_BUILD の背景 JPEG コピー元だけ実在する C# asset path へ直した。これは build 成功の記録であり、
  画面実行・runtime 検証は未実施。Section 12 の依存接続を含むWindows Release buildは成功。
- 11 `MatchStart` も同 build に含めてコンパイル・リンク確認済み。

- `NetLaunch.cs::TickTerminalLobby` の移植後監査を完了。`Renderer::OnRenderFrame` のフレームレート設定直後へ接続し、
  persistent lobby 復帰・scene 有無・通信終了・match load と clear/swap/base frame の順を照合した。
  後続の Section 12 Release build でもコンパイル・リンクを確認した。
- `KeyRow.cs`、`PadRow.cs`、`ServerRow.cs`、`ServerBadge.cs` と各C++対応物を原本から再監査した。
  キー変換・pointer tap・pad capture/conflict・server status/selection・badgeの住所分類と描画で修正差分はなかった。
  対象C++の `git diff --check` は通過。
- `AndroidMatch.cs` と `.cpp/.hpp` を全体照合し、オンラインmatch開始時の `DisableCheatsForMatch()` 呼び出し漏れ、bot skill上限の `3` 対 `2`、build時に見つかった `RoomPlayerCount()` 呼出漏れを修正。demo/adventure/local/network各経路の初期化順、slot/team割当、map/mode選択、bot level、save確定も照合した。Android ABI build継続中。
- `AndroidApp.cs` と `.cpp/.hpp` を全体照合し、Activity lifetimeの遅延 `MainViewFactory`、SingleView fallback、両経路の `UiScaleHost`、`CrashReport` 初期化/Android unhandled exception報告、`StartScreen` のDone/MatchRequestedを接続。削除済み `HomeView` adapter参照を現行native `StartScreen` へ置換し、MainActivityのlifecycle参照とhost interfaceも追随させた。Android buildは全Androidファイルの監査完了後に実施する。
- `AndroidLogShare.cs` と `.cpp/.hpp` を比較監査し、cache staging pathと古いzipの best-effort 削除、FileProvider authority/URI、ZIP MIME、stream/subject extras、read grant、chooser title、Application Context用NEW_TASK、例外時のログとerror返却が一致することを確認。修正差分なし。Android buildは全Androidファイルの監査完了後に実施する。
- `TouchControls.cs` と `.cpp/.hpp` を全体照合し、button配置・visibility・touch/hudターゲット・aim/stick pointer処理・swipe boost/double tap・pad hide/force-visible状態を監査。`TakeAimDelta()` に欠けていた `AimInputSourceTracker::Pointer(x, y, true, TickCount64())` 呼び出しを追加。Android buildは全Androidファイルの監査完了後に実施する。
- Androidの全ファイル監査後に両ABIをbuildし、最初の共通コード障害として `SfxMixer.cpp` のNativeRuntime/OpenTK `using` 宣言が最終行にあり先行参照から見えない問題を検出。`SfxMixer.cs` のMath演算/通常unchecked算術との対応を確認して宣言をinclude直後へ移動。両ABIのbuildを再実行する。
- Android ABI buildの再実行で `HashCode.cpp` のentropy fallback lambdaに `std::uint64_t` と `0ULL` の推論型不一致が出たため、値を変えず `std::uint64_t{0}` に型を揃えた。標準 `HashCode.Combine` のno-entropy fallback動作と同じ。両ABIのbuildを再実行する。
- 次のAndroid ABI compile errorは `Stopwatch.hpp` の defaulted `<=>` が `std::strong_ordering` を必要とするのに `<compare>` が未includeだったこと。C# `Stopwatch`/`TimeSpan` の意味は変えず、標準依存ヘッダーを追加して再実行する。
- Android ABI buildで `GameView.cpp` のコンストラクター定義だけが `TouchControls*` になっていたため、C# `GameView`、native宣言、`RenderLoop::Create` の `TouchControls&` と照合して参照へ修正。両ABI buildを再実行する。
- 続くAndroid NDK compile errorを受け、既存 `PreviewService.cs` と `.cpp/.hpp` を照合。service intent、空room時の完了/StopSelf、workerの例外・finally、ゲームファイル初期化、offscreen render、10 worker宣言が対応していることを確認し、`AttachCurrentThread` だけをNDKの `JNIEnv**` signatureに合わせた。両ABI buildを再実行する。
- Android対象17ファイルのC#監査完了後、NDK target `fruity_mphread_native_android` を arm64-v8a / x86_64 の両方でbuildし、2 ABIとも exit 0・static library link成功を確認した。ログは `build/native-android-arm64-v8a/build-retry10.log` と `build/native-android-x86_64/build-retry10.log`。端末起動・実機動作は未確認。
- `CompatibilityCheck.cs` と `ModEntry.cs` の bundled OpenAL resolver をC++側で再監査した。Appleでは system frameworkへ直結せず、`AL.cpp` が C# と同じ executable directory の `libopenal.1.dylib` を初回呼出し時に読み、`CompatibilityCheck` の symbol-address照合でも同一handleを使うようにした。CMakeもAppleでの system OpenAL linkを外し、Windows/Linuxの直接linkは維持。
- 上記 `CMakeLists.txt` / `AL.cpp` の変更後、MSYS2 MinGW64 Release `ninja -k 0` が exit 0、全50 build stepを完了。ログ: `/tmp/fruity-prime-native-section2-openal-20260927.log`。macOS native buildと実行時smokeはこのWindows環境では未実施。
- Section 12統合後のWindows Release build成功を反映し、Section 3の `ThumbnailBatch` / `ScreenCapture` / `ThumbnailCapture` / `ThumbnailLog` をbuild待ちから完了へ更新した。
- `GuiLauncher.cs` と `.cpp/.hpp` の統合監査で `TryRun` / `EnsureSetup` / Android・display分岐 / Shell接続を照合した。
  二つのcatchで `PlatformDiagnostics.Report` を呼ぶよう接続し、C#例外時の診断内容を保持した。
  `ModEntry` の `PlatformDiagnostics.Start` と smoke/GLFW/window diagnostic dispatch も後続で接続し、
  Windows Release buildで確認した。

- `LauncherWindowCheck.cs` と `.cpp/.hpp` を原典と照合し、geometry設定のtry/finally復元、20/40 frame判定、
  4組のshader compile/link、back bufferのpixel thresholdとGL error、1100x740 resize後の再描画、
  callback内例外のcloseを確認した。`Shell::AfterDraw` のshot処理より前に接続した。Windows Release build済。
- `ThumbnailWindowCheck.cs` と `.cpp/.hpp` を照合し、workerと同じwindow settings、legacy GL 2.1条件、
  context診断、FBO/textureの生成・解放、quads描画、中央pixel判定を確認した。
  `ThumbnailCapture.WindowSettings` のnative可視性をC# `internal` に合わせ、OpenTK薄い層に
  `Rgba8`・`DrawBuffer`・`Vertex2` を同じGL定数/entry pointで追加した。
  `GlfwWindow`生成時にC#の`MakeCurrent`と同じcontext-current処理があることも確認した。
  外側のcatchをC# `catch (Exception)`相当へ修正し、Windows Release build済。`ModEntry`引数配線済。
- `GlfwPathCheck.cs` と `.cpp/.hpp` を一ファイル単位で移植した。temp fixture、working directory、
  `GameFiles.Root`、paths.txt、GLFW context policy、GLFW後のcwd/paths再確認、finally復元・recursive deleteを
  C#の順で実装し静的監査した。新たに必要だった `Directory.CreateTempSubdirectory`、
  `Directory.SetCurrentDirectory`、`Directory.Delete(path, true)` はNativeRuntime/System/IOへ追加した。
  外側のcatchをC# `catch (Exception)`相当へ修正し、Windows Release build・`ModEntry`配線済。
- `PlatformDiagnostics.cs` と `.cpp/.hpp` を比較し、起動環境の行・macOSのdylib一覧と永続化、
  platform別library名、native exception記録、`file -b` の2秒制限、IO/権限エラー時のログ通知を実装した。
  `GuiLauncher.cs` の二つの失敗経路も比較し、GLFW/Skiaの `PlatformDiagnostics.Report` 呼出しを接続した。
  `ModEntry` 起動時の `Start` と診断flagsもC#の分岐順で接続した。標準C++例外がC# stack traceを保持しない差は
  NativeRuntimeの例外仕様に従う。Appleのbundled OpenAL解決はC# `OpenALLibraryNameContainer.OverridePath` と同様に
  executable directoryの`libopenal.1.dylib`をnative bindingが遅延ロードするようにし、system OpenALへのlinkを外した。
- Section 2の診断pairと依存APIを含む Windows Release Ninja build が成功した
  （`ninja -k 0`、ログ: `%TEMP%\fruity-prime-native-section2-diagnostics-20260927-retry1.log`）。
  `CompatibilityCheck`、`GlfwPathCheck`、`PlatformDiagnostics`、GLFW/OpenAL binding、
  NativeLibrary、再帰ファイル判定の追加分をコンパイル・リンクした。smoketestや画面実行はしていない。
- 5つのDiagnostics原典を再監査し、C#が捕捉する通常例外とC++の`catch (...)`の範囲差を修正した。
  `CompatibilityCheck`、`LauncherWindowCheck` callback、`PlatformDiagnostics`の記録callback、
  `ThumbnailWindowCheck`、`GlfwPathCheck`の各catchを`std::exception`へ対応させた。
  例外後のGL資源解放・finally相当処理のcatch-allは維持した。Windows Release `ninja -k 0`は8 stepでexit 0
  （ログ: `%TEMP%\fruity-prime-section2-diagnostics-reaudit-20260927.log`）。既存の`offsetof`警告のみで、
  smoke test・画面実行・macOS buildは未実施。
- `Sfx.cs` と `Music.cs` の PR #1 差分を一ファイルずつ原本と照合し、診断呼出し、catch/filter、fallback 文言と
  状態更新順を確認した。`Sfx::Load` と MusicPlayer 初期化失敗の両native経路へ `PlatformDiagnostics::Report` を
  接続した。Windows Release の `ninja -k 0` が成功（ログ: `%TEMP%\fruity-prime-native-diagnostics-audio-20260927.log`）。
  `offsetof` の既存警告のみ。macOS build・起動は未実施。
- `ModEntry.cs` の Section 2 診断分岐は `smoketest` と shell 限定の GLFW/thumbnail/window checks を含め
  native側へ配線し、`PlatformDiagnostics::Start` の呼出し位置も `DebugLog::Attach` 後と照合した。
  同 build でコンパイル・リンク済み。runtime check は未実施。
- `ModEntry.cs` と `ModEntry.cpp` を引数・分岐・順序で全体比較した。`-teamprobe` の設定漏れを
  `MapAudit::TeamProbe` へ接続し、`-hosts` の20秒待機を超えて応答が返る場合も callback の参照先が残るよう
  共有状態へ変更した。render overrides、network controls、MapGen probes、server install/host、map vote/HUD capture
  を原本と照合し、CLI option inventory は完全一致。Windows Release `ninja -k 0` 成功
  （ログ: `%TEMP%\fruity-prime-modentry-audit-20260927-r2.log`）。GUI/runtime/macOSは未確認。

- `Shell.cs` を再開後に再監査した。Run/finally の停止・破棄、match/lobby/end-panel の遷移、settings再読込、pause menuの開閉、framebuffer resizeとpointer基準、入力変換、window capture の抑止とscript順をC#と照合し、追加修正はなかった。C#の `Exception.StackTrace` に相当する値は標準C++例外に保持されないため、match-start例外は既存の `DebugLog::Exception` がnative stackをログへ記録する。Section 12の一括交換が未完了なのでビルドはしていない。

- `UiCapture.cs` と `UiCapture.cpp` を一ファイル単位で照合した。26画面の順番・名前・寸法、固定server/host data、phone/sampleの設定時点、dispatcher jobとlayoutの順、bounds JSONの階層・丸め・escape、PNG保存、失敗時のreturnとconsole文を確認し、修正差分はなかった。C++はC#の非表示Windowをheadless `EmbeddableControlRoot`に置き換えるが、同じサイズで実画面を開かず描く役割を保つ。`ModEntry`の `-uishot` 呼び出しも引数・終了コードを照合済み。ビルド・撮影実行はしていない。

- `GamepadUiChecks.cs` と `.cpp` を一ファイル単位で再監査し、35 assertion の文言・順序、focus/navigation、keyboard/pad capture、preset変更、pause/map vote、optional capture と `GamepadChecks::Run` の呼び出し位置を照合した。SettingsView は attach 時に選択中タブへ focus する job を post するため、C#と同じく `ShowSection("Controls", 1)` を job pump より先に行う必要がある。native helper が attach 後すぐ job を実行して初期タブへ focus していた順序を修正した。残りの pump 箇所はC#の明示した `RunJobs` と同じ操作順で、他の attach callbackにも同型のずれは確認されなかった。静的監査のみで、Section 12 一括交換前のbuild・実行はしていない。
- 上記 `GamepadUiChecks` のSection 12 統合buildで、C#の `SliderRow` 参照に相当する `First<SliderRow>` のnative参照を `FocusNavigator::Focus(Control*)` に渡す際のポインタ変換漏れを修正した。C#と同じ要素をfocusする。
- `SettingsView.cs` と `.cpp/.hpp` の再監査で、`UiWord` のforward declaration欠落と `RequireReference(_settings)` が既に参照を返すのに再度dereferenceしていた点を修正した。保存対象はC#と同じ `_settings` の `MenuSettings` instance。
- `PauseMenuView.cs` と `.cpp/.hpp` のcompile再監査で、native版 `Deck::Face` factory の呼び出し括弧漏れを直し、Escape key handler のAvalonia型を `Av::Input` に明示した。C#と同じFace色とEscape時のResumeイベントを維持する。
- `CreateServerScreen.cs` と `.cpp/.hpp` の該当箇所を再照合し、ProgressRowをControlとして使うtranslation unitのinclude欠落と、`PickRow`・画面3種のpointer/key eventがゲーム入力namespaceへ誤解決される箇所を修正した。C#同様、Avalonia input event型を使い、PickRowはEnter/Space/Right、Escapeは各画面のclose/cancel経路へ入る。
- `HunterStand.cs` と `.cpp/.hpp` の入力箇所を比較し、cursorとpointer handlerのAvalonia型を明示した。ドラッグ開始・座標・capture・spin更新・release/capture loss時の状態解除はC#どおり。
- 再buildで `KeyRow`/`PadRow` にnativeには存在しないAvalonia `IsFocusedProperty` を登録していた誤りを修正した。C#のfocus renderingは両native controlのGotFocus/LostFocus overrideが同じく `InvalidateVisual()` し、Enabledはnative `InputElement` 本体がrender登録するため、専用static登録は不要。
- `UiSurface.cs` と `.cpp/.hpp` のbuild差分を監査し、GamepadNavigation event登録をnative Event APIの `+=` に合わせ、C#同様のinstance methodである `Covers` に誤った `const` を付けていた点を除いた。
- `Shell.cs` と `.cpp/.hpp` のWindows owner HWND、window title、`-shellshot` を照合し、GLFW native includeの順序、C#継承プロパティに当たる `RenderWindow::Title` forwarding API、およびsceneがないwindow captureの欠落を補った。追加 `ScreenCapture::SaveWindow(width,height,path)` はC#と同じback buffer RGB readback・pack alignment・black-frame判定経路を通す。
- `UiDesigns.cs` の既定実行adapter欠落をlink errorから追跡し、C#と同じ `GuiLauncher.EnsureSetup` → directory作成 → UI dispatcher → `UiCapture.Capture` → console出力のlive adapterを接続した。`UiCapture::Capture` はC#のinternal相当としてheader内公開。
- compile/link後のPOST_BUILDで背景画像だけnative Assetsに存在せずcopy失敗したため、CMakeのcopy元を実在するC# asset `src/MphRead/Assets/Backgrounds/launcher-bg.jpg` に修正した。native launcher resource URIはflat filename fallbackも持ち、ロゴ/markと同じく実行ファイル横へ配置される。
- `UiDesigns.cs` と `.cpp/.hpp` の該当構築箇所を比較し、`SliderRow` includeと `Opened` 戻り値を修正した。C#側の `Control` と同様、GridをBorderに包んだ開閉行も共通Controlとして親へ渡す。
- `UiCapture.cs` と `.cpp/.hpp` のcapture失敗経路を再照合し、ログ用 basenameを取得する際に `string_view` を `PathGetFileName(const std::string&)` へ渡していた型変換漏れを修正した。C#と同じく例外を画面名付きのcapture失敗ログへ変換する。
- `LobbyScreen.cs` と `.cpp/.hpp` のchat entry handlerを比較し、Enter eventをAvalonia `KeyEventArgs`/`Key` として扱うようnamespaceを修正した。Enter送信と `Handled` 設定は維持した。
- 再buildでnative routed eventのsender引数も必要と分かったため、LobbyScreenのC# `(_, e)` と同じ2引数callbackに揃えた。
- `PlayScreen.cs` と `.cpp/.hpp` のbuild露出箇所を照合した。BarRowのC# switch expressionをnativeのDock分岐へ正しく翻訳し、CornerRadius型のshadowing、Deck Face factory、Avalonia key/focus eventのnamespaceを修正した。各Dock位置の左右gapとC#のFace色・キー動作は同じ。
- Section 12のShellが使う `Renderer.cs::Scene.UnloadGl` をC#原本と照合し、one-window lifecycleに必要なGL解放をSection 13から先行移植した。palette texture map、cached model display listとcache、frame/render buffers、3 scene textures、4 shader programsを同じ順で解放しIDを0へ戻し、Headless時はreturnする。両model cacheを列挙する `Read::CachedModels` とGL `DeleteRenderbuffer` entry pointもC#/OpenTK APIに合わせて追加した。build未確認。
- Section 12のbuildエラーを `GamepadProfilePanel.cs` と対応pairで照合し、`LauncherPrefs.hpp` の相対includeを実ファイル位置に合わせて修正した。プロフィール操作・例外表示はC#と一致。
- 再buildで `GamepadProfilePanel` の `Path.Combine` に当たる `PathCombine` includeが抜けていたため、C#のファイル名・保存先と同じnative `System.IO` APIをincludeした。
- `UiScaleHost.cs` と `.cpp/.hpp` をbuildエラーと合わせて再監査した。Android dp→desktop pointの換算、factor・bake scale・measure順は一致。依存する `UiLayout.Factor` の `Math.Round` と `std::round` の中間値規則をmidpoint-to-evenへ修正し、Android native toolkitの `TopLevel.RenderScaling()==1` 固定を補うため、接続済みActivityのdisplay densityをbake scaleへ使うようにした。`DebugLog.hpp` の相対includeも配置に合わせて修正した。
- `UiSurface.cs` と `.cpp/.hpp` は先のC#比較記録に対し、初回buildで露出したnative includeを再点検した。`UiOverlay`・`DebugLog`・`Mods/Input` の相対パスを実配置へ修正した。入力・surfaceの動作順差分はなかった。
- `Shell.cs` と `.cpp` のbuild依存も監査した。`AfterDraw` の `LauncherWindowCheck` 呼出しはSection 2の未移植クラスに依存するため、順序表どおり同Sectionで実装・接続する記録を残し、Section 12中のinclude/callは除去した。shellshot経路は別の既存呼出しで維持。
- `StartScreen.cs` と `.cpp/.hpp` を全体照合した。Backdrop/wordmark/responsive bar、update/hint/gamepad timerのlifecycle、初回Setup、stack push/popとfocus、Play/Create/Settings/Lobby/Pause/Vote遷移、終了イベント、update installのpermission/progress/error/exit動作を確認し、差分修正はなかった。以前のbuildエラーに沿った `InputPrompt` / `InputSourceTracker` include修正と未使用 `GamepadUiRouter` include除去も維持した。Section 12一括交換が終わるまでbuildは保留。
- `ConfirmScreen.cs` と `.cpp/.hpp` をbuild診断と合わせて照合し、表示font accessorの呼出し漏れと親namespaceのInput名衝突を修正した。modal/no markの初期focus、Escapeのfalse応答、event順は一致。
- `FocusNavigator.cs` と `.cpp/.hpp` を再照合した。Avalonia `InputElement`・`NavigationMethod`・`Key`・key eventsが親のgame input namespaceに隠れる名前衝突を修正し、C#のfocus eligibility、default順、neighbor優先、方向距離とwrapの計算を確認した。
- `DeckTile.cs` と `.cpp/.hpp` のhash phase処理をbuildエラーに沿って再確認し、`Math.Abs(int.MinValue)` parity用 `OverflowException` をRuntimeの実際のglobal `System` namespaceへ修正して例外宣言をincludeした。
- `NativeRuntime/System/Net.cpp` のIPv6 named-scope変換をWindows SDKと照合し、MinGWで `if_nametoindex` を宣言する `netioapi.h` をWindows分岐へ追加した。リンク先 `iphlpapi` は既存設定済み。
- `LauncherHunter.cs` と `.cpp/.hpp` のrenderer dependencyを再確認し、forward declarationでは不足するScene state/preview呼出しの完全型として `Scene.hpp` を追加includeした。draw/fallback動作差分はなし。
- NativeRuntimeのAvalonia `Popup` はVisual baseの `enable_shared_from_this` と二重継承で曖昧になっていたため、独自の二重継承を除き、共通AvaloniaObject所有参照をControlへdynamic castしてoverlay layerへ渡すよう修正した。Panel親/overlay親の分岐は維持。
- `LobbyPlayerRow.cs` と `.cpp/.hpp` をbuild diagnosticsと照合し、`GuiTheme::Display` をfont-family accessorとして呼ぶ形に揃えた。名前・team・hunter/suit/pingの表示順と列構成は一致。
- `KeyRow.cs` と `.cpp/.hpp` をC#原本から再監査した。クリック後releaseでlisten開始、tap/drag判定、mouse/wheel/key binding、GLFW key変換、gamepadからPadRowへの移動、focus解除時のlisten解除、表示文言と描画を照合。Focused変更はTopLevelが対象Visualをinvalidateし、Enabled変更はInputElementの共通AffectsRender登録でC#の描画更新と一致。rounded clip typeはAvalonia root namespaceを参照する。
- `ServerRow.cs` と `.cpp/.hpp` を原本から全体照合した。列幅とnarrow判定、status/metadata/players/ping、hover・tap・focus・keyboard、map crop/scrim、名前tailのUTF-16長、各描画座標のties-to-even丸めとgetterを確認。buildエラー行のpointer position Visual引数とAvalonia Matrixも明示し、hover傾きとscale transformの数式はC#どおり。依存する変更済みNativeRuntimeのタッチ経路は `TouchBegin` が常に `ClickCount=1` だったため `DoubleTapped` が発火しない差を修正し、Avaloniaのdouble-tap時間・領域に沿ったtouch click countと、double tap後の `Tapped` 抑制を追加。Android固有の `ViewConfiguration` 値との一致はAndroid側の残ファイル監査で確認する。section 12統合前のためbuildは保留。
- `PadRow.cs` と `.cpp/.hpp` をC#原本から再監査した。button列挙順、held-button baseline、device/focus失敗、30ms capture timer、clear/picker/chord conflict解決、終了/visual-tree離脱時のcleanup、表示を照合。Avalonia `DispatcherTimer(interval, priority, callback)` は即時開始し、NativeRuntime版も同じ。Focused変更はTopLevel、Enabled変更はInputElement共通のrender invalidationが担う。既出のInput namespace/Managed helper/`RoundedRect` build修正も確認した。
- `Renderer.cs` の shell描画呼出し箇所をC++側と再照合した。シーンなしの `UiOverlay.DrawAlone → Shell.AfterDraw` と、マッチ中の `UiOverlay.Draw → LauncherHunter.Draw → Shell.AfterDraw`、寸法引数、swap前後の順は一致した。C#の `PixelSize` は `FramebufferSize` の別名。当時保留だったSection 13差分は後続監査・修正を完了し、Windows Release build済み。
- `LauncherHunter.cs` と `.cpp/.hpp` を全体照合した。window scene と専用 side scene の選択、side sceneの作成/ロード/resize、hunter/suit/正規化boundsの設定、preview drawの結果による `Drawn`、寸法/矩形不正時と例外時のフォールバック状態、初回成功ログを比較し、差分修正は不要だった。`NewSideScene` のnative実装はC#同様、windowのscene slotを変更しない。
- `UiOverlay.cs` と `.cpp/.hpp` を全体比較した。reserved texture ID、unit 0 upload、resize時のTexImage/SubImage、premultiplied blend、unit 1解除、固定機能quad/texture座標、GL stateの復元範囲、シーンなしのclear/photo/overlay/hunter順、releaseを照合し修正不要。build・画面実行はSection 12未完了のため未実施。
- `[PR #1](https://github.com/Zection6V/Fruity-Prime/pull/1/changes)` のC#追加差分も原本参照として確認した。`LauncherNoise`/`NoiseField`の宣言順と時計開始時点を照らし合わせ、nativeの初回アクセスlazy initializationを修正した。
- `LauncherPhoto.cs` と `.cpp/.hpp` を再照合した。アスペクト比crop、static/movingの切替、noise shader compile/linkと失敗時static背景へのfallback、uniform・multi-texture座標、embedded JPEGのdecode/upload、全unpack state、sampler level/filter/wrapを比較し、nativeのAvalonia asset/JPEG decoder利用は不透明JPEGの同じRGBA画素を渡すruntime adapterとして一致した。差分修正なし。build・描画実行は保留。
- `MapCardPicker.cs` と `.cpp/.hpp` を全体比較し、factoryのコード分割・metadata、選択のordinal-ignore-case、空リスト時の説明/無効化、選択表示、Done/Cancelled、Escapeを照合した。`LobbyScreen::OpenMapPicker` の呼出しとイベント後のdraft更新・preview・dirty通知・page復帰もC#と一致。nativeは子タイルのattach後に初期focusをdispatcherへ送るが、親 `OpenPage` のfocus jobとの順序もC#側の「タイルfocus後に親focus job」と同じ。修正なし。build・実行は保留。
- Section 12 の接続待ち記録をソース参照と照合し直した。Play/CreateServer/Setup は StartScreen と UiCapture、Lobby は StartScreen、Settings は StartScreen・InGameMenu・UiCapture、PauseMenuView は StartScreen・InGameMenu・UiCapture から参照されている。これらの画面接続は現ツリーで解消済み。GuiLauncher の PlatformDiagnostics 依存と Section 13 待ちは残す。

- `Renderer.cs` とnative実装を一ファイル単位で再監査した。60 Hz simulation内のfreeze/network/gamepad/spectator/hit-claim順、draw内のEndScreen/thumbnail/AimAssist/FOV/wireframe、one-window shell描画順、pen pointer/focus、gamepad cancelを照合。監査で見つけたnativeベクトル演算API差、`WindowGeometry::Flush`漏れ、0寸法framebufferの未ガード、最大化状態callback欠落を修正した。GLFW maximize callbackからshell geometryを記録し、C#と同じ復元動作にした。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-renderer-audit-20260927-r3.log`）。画面runtimeは未実施。
- `PlayerHud.cs` の残件メモを再確認し、`UpdateWeaponSelect` の `WheelHeld → Absolute → Drag` 優先順、使用可能武器の配列・selection marker更新、weapon switch音、`UpdateWeaponDrag` のstepと選択更新、`UpdateWeaponArc` のpointer入力がnativeに反映済みと確認。`ModEntry.cs` の `-gamepadassisttelemetry` も `AimAssistTelemetry.Configure(ValueAfter(...))` が両実装で一致。コード修正は不要。
- `GameState.cs` のPR #1差分をhunkごとにnativeと比較した。TeamCount/固定DamageLevel、チーム検証、正の残り時間のみのtempo/alarm、tie時winner camera抑止、survival集計・結果standings、BountyTeams comparator、resetを照合。唯一抜けていた `PlayPickedMap()` を追加し、offline/shell/選択roomの条件成立時だけ `Shell::PlayAnother` を呼び、対象外では従来fade/exitへ戻すようC#と同じ順序で接続。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-gamestate-audit-20260927.log`）。runtime未実施。
- `PlayerEntity.cs` のPR #1追加差分をnative部分クラス全体と照合した。boost-aim lockの保持・reset・入力/更新での消費、spawn lifecycle/continuous phase reset/bridge通知、予測damage scope、claim damage二重乗算防止、allied-team判定、projectile launch-frame付きdamage/hit prediction、AimAssist telemetry、haptic feedbackを確認し全て反映済み。差分修正不要。GameState修正後のWindows Release buildで当該native translation unitも再リンク済み。
- `PlayerSound.cs` の追加1行を `.cpp/.hpp` と照合した。`_timeBeforeLanding > 30` のときだけ `Landing` feedbackを先行させ、元のterrain landing SFX ID・volume式を続ける順序まで一致。変更不要で、入力待ち表記を完了へ更新。
- `PlayerEntityNetAim.cs` をC#全文と `.cpp/.hpp` で比較し、位置履歴、remote aim、camera補正、NodeRef解決、spawn/facing、form・weapon・ammo・shot state、status/affliction、vector修復と各aim経路を監査した。`PlayerInput`・`NetPlayerBridge` に加え、`NetHooks` の記録、`NetDamage` の銃口位置、`NetUnlagged` の退避/復元呼び出しも順序と引数が対応。修正不要で、入力待ち表記を完了へ更新。`git diff --check` 通過。
- `PlayerEntityNetHud.cs` 全体を `.cpp/.hpp` と照合した。ネット対戦時のhealth参照、score列のsolo/network座標とEndPanel幅に応じたclamp、ping列の表示条件・`--`/999表示・色境界（80/160 ms）・描画引数が一致。`PlayerHud`、pro HUD、team scoreboardの全直接呼び出し位置も対応し、変更不要。`git diff --check` 通過。
- `PlayerAi.cs` のPR #1差分をhunkごとにnative実装と比較した。Insane difficultyの乱数表・clamp、personality分岐、照準dot/偏差、Tracked Speedを60 Hz tickへ換算した反復lead、発射遅延、Judicator距離、health spawn-aware探索を照合してnativeへ反映。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-playerai-audit-20260927.log`）。runtime未実施。
- `Formats.cs` のPR #1差分をnative側の `Paths`/`CollectionExtensions` と比較した。未設定時の `Export` 空文字、`SetPath`/`paths.txt` 読込値の絶対化（rooted/空値維持、不正値は保持）、可変spanの `Slice(uint)` を反映。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-formats-audit-20260927.log`）。
- `Utility/Archive.cs` のPR #1差分をC#実装と照合し、path入力をbyte span overloadへ委譲する形と検証/展開処理をnativeに追加。Readのメモリ展開が使うAPIを用意した。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-archive-audit-20260927.log`）。
- `Read.cs` のPR #1各hunkをnativeと照合した。`CachedModels` はScene unload用に先行移植済み。Androidストレージ上の一時ファイル競合を避けるLZ10メモリ展開→Archiver byte span抽出、0件エラー、アーカイブ名/例外内容付き診断を移植。仮Archiver宣言を実ヘッダーへ置換してWindows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-read-audit-20260927.log`）。runtime未実施。
- `Features.cs` の追加設定3項目をC#と照合し、RadarEnabled/ShowBackground/ShowOutlinesのLoad/CommitをRadarの既定値・Boolean parse・lowercase保存まで反映。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-features-audit-20260927.log`）。
- `Platform/AppPaths.cs` 全文とnative peer、`ConsoleSetup.Run`ほかの直接呼び出しを照合した。既定root、macOS `.app/Contents/MacOS` のResources選択、UserProfile下のApplication Support、`paths.txt`、ディレクトリ作成条件は対応。nativeがmacOS判定前にParentを解決していた評価順をC#の短絡条件に合わせて修正。`git diff --check` と Windows Release `ninja -k 0` 成功。
- `Platform/WebLink.cs` 全文と `.hpp/.cpp`、`Updater.OpenUrl`、Android側のadapter登録を照合した。interfaceのURL引数とbool応答、static Currentのnull初期値・set/get、HTTPS検証後のadapter優先と拒否時の戻り、desktop fallback順が対応し、修正不要。Android両ABI buildは既存のSection 14証跡を確認。
- `MapPick.cs` 全文を `.hpp/.cpp` と照合し、label重複解消、票のserver順、case-insensitive比較、packet適用/close、offline・online選択、cursor/scroll境界、resend、hit-layoutを確認した。`EndScreen`・`NetSession`・`Renderer`・`GameState`・描画側の直接呼び出しも引数と順序が対応。`NoteLayout` のnativeコピーとmanaged配列参照は現在の唯一の呼び出しが描画後に同じ内容を公開し、次描画ではhover読取後に更新されるため観測差なし。修正不要。
- `WindowGeometry.cs` 全文をnative peerと照合し、保存/復元・fullscreen/minimized/maximized・monitor-fit・debounce・直接呼び出し順が対応することを確認。C# `MonitorInfo.ClientArea` に対してnativeがGLFW WorkAreaを使っていたため、全モニターと現在モニターのClientAreaをビデオモード寸法に修正し、`FitToScreen`用WorkAreaを分離。OpenTKのlargest-intersection monitor選択と外枠基準`Location`もnative adapterに反映。Fit境界演算はC# unchecked int32に一致。`WindowMode`・`Renderer`の直接呼び出しを監査し、`git diff --check` とWindows Release `ninja -k 0` 成功。
- `CrashReport.cs` 全文とnative peer、`Program.Main`/`AppDomain`/`ConsoleWindow`の接続を照合した。初回のみの登録・報告、起動例外とunhandled経路、書込み先の順序とfallback、Windows console表示/所有時pause、終了コードの対応を確認。修正不要（Windows Release build済）。
- `EndScreen.cs` 全文とnative peerを照合し、Available/Ready、ルーム表示、遷移時のMapPick・preview cleanup、hit box境界、クリック/キー/パッド選択、respawn choice保存を確認。`Renderer`のframe/input順、`Shell`のPanelUp、`RespawnChoice`・NetPlayerBridge・HUD描画側の呼び出しも対応し、修正不要（Windows Release build済）。
- `WindowMode.cs` 全文とnative peerを照合し、startup/force・保存したborder/location/size・F11/Alt+Enter・fullscreen遷移順・topmost同期・設定値parseを確認。`Renderer`・`PauseMenu`・Settings/Shell・`WindowGeometry`の直接呼び出しも対応。OpenTK 4.9.4の`GetMonitorFromWindow`がfullscreen monitor優先、通常時はClientAreaとの最大交差で選ぶ点と、`NativeWindow.Location`が外枠座標である点をnative adapterへ反映。Windows Release `ninja -k 0` 成功。
- `InputSettings.cs` 全文と `.hpp/.cpp`、`PlayerControls`・`SettingsView`・`ModEntry`・`CompatibilityCheck`・`Renderer`・`ChatBox`・入力診断側の直接呼び出しを照合。35 bindingの順序、OpenTK 4.9.4のenum名/別名、Load/Save/Reset・既定値・Apply順を確認。`Load`のPath再評価とmouse enum表示のunchecked int32加算を修正。Windows Release `ninja -k 0` 成功。
- `RenderOptions.cs` 全文と `.hpp/.cpp`、`GameSettings`・`ModEntry`・`Renderer/Scene`・`SettingsView`・`DebugLog`の直接呼び出しを照合。既定値、clamp境界、FOV倍率、整数スケールのunchecked積、parse/fallback/`%`処理と呼び出し順が対応し、修正不要（verified no-op、Windows Release build済）。
- `SceneSetup.cs` のC#差分をnativeと比較し、roomごとのNetHealthSync reset、offline/single-player/active sessionからのresource profile選択、MapResourceRulesによるentity listとItemSpawnDataの解決を同じタイミングで接続。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-scenesetup-audit-20260927.log`）。
- `Metadata.cs` の全差分をhunk単位で照合した。multiplayer entity layerの3人/4人以上選択とTeamColorsの青・紫を含む4要素化を反映。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-metadata-audit-20260927.log`）。
- `Utility/Extract.cs` のC#差分を比較し、生成ROM rootを `Paths.SetPath` に絶対化させるため `Extract.Setup` は相対Combine値を渡し、エラー終了時のキー待ちはredirect/no-console安全な `ConsoleSetup::PauseIfInteractive` に統一。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-extract-audit-20260927.log`）。
- `Menu.cs` の唯一の追加 `FieldOfView = "78"` をC#と照合。native `MenuSettings::FieldOfView` に同名・同じ既定値がすでにあり、変更不要。直前のWindows Release build済み。
- `Scene.cs` のC#変更を照合し、`GetFlagBaseEntities` が誤って `FhBomb` リストを参照していたnative実装を `FlagBase` に修正。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-scene-audit-20260927.log`）。

### 2026-09-27 試合中カーソル修正

- 症状: C++ 版で試合中にマウスカーソルが表示されたまま動き、画面端でエイムが止まる。
- 原因: InputSettings の読み込みで `stylus_mode` が無いとき `pointer_jump_guard` をスタイラスの切替として読んでいた。
  旧ビルドは既定値 true でこれを常に書いていたため、古い controls.txt は全員スタイラスモード（カーソル解放）になる。C# 側も同じ。
- 修正: 明示的な `stylus_mode=true` のときだけスタイラスモード。C++（Mods/InputSettings.cpp）と C#（Mods/InputSettings.cs）の両方を同じ形で変更し一対一を維持。

### 2026-09-27 Online の did not answer 調査

- 症状: Online のサーバー一覧が全部 did not answer。`FruityPrime -servers` で再現。
- 調査: 3 台（West US 2 / West Europe / Pi）とも StatusQuery に 131 バイトで応答している。
  現行プロトコル 14 は MatchStatePacket が 8 バイト増え、StatusReply は 138 バイト以上を要求するため短い応答を捨てる。C# も同一挙動で、移植のバグではない。
- 結論: サーバーが旧ビルド（Pi は v0.10.0）。本修正はサーバー再デプロイ。旧サーバーを「旧バージョン」と表示するクライアント側改善は任意。どちらにするかユーザー判断待ち。

## 1. Platform helpers — 2 ファイル (新規 2), C# +73 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +40/-0 | `Mods/Platform/AppPaths.cs` | — 新規 | 完了（C#全文・直接呼び出し監査済、短絡評価順修正、Windows Release build済） |
| A | +33/-0 | `Mods/Platform/WebLink.cs` | — 新規 | 完了（C#全文・呼び出し元監査済） |

## 2. Diagnostics — 5 ファイル (新規 5), C# +497 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +149/-0 | `Mods/Diagnostics/CompatibilityCheck.cs` | .cpp,.hpp | 完了（C#監査・ModEntry配線・Windows Release build済、bundled OpenAL pathをnative bindingへ反映。macOS build/smoke未実施） |
| A | +103/-0 | `Mods/Diagnostics/LauncherWindowCheck.cs` | .cpp,.hpp | 完了（C#監査・Shell/ModEntry接続・Windows Release build済、runtime check未実施） |
| A | +101/-0 | `Mods/Diagnostics/PlatformDiagnostics.cs` | .cpp,.hpp | 完了（C#監査・GuiLauncher/ModEntry接続・Windows Release build済、macOS runtime未実施） |
| A | +83/-0 | `Mods/Diagnostics/ThumbnailWindowCheck.cs` | .cpp,.hpp | 完了（C#監査・ModEntry接続・Windows Release build済、runtime check未実施） |
| A | +61/-0 | `Mods/Diagnostics/GlfwPathCheck.cs` | .cpp,.hpp | 完了（C#監査・ModEntry接続・Windows Release build済、runtime check未実施） |

## 3. Mods leaves — 13 ファイル (新規 3), C# +1597 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +516/-0 | `Mods/MapPick.cs` | — 新規 | 完了（C#全文・直接呼び出し監査済） |
| A | +339/-0 | `Mods/WindowGeometry.cs` | — 新規 | 完了（C#全文・WindowMode/Renderer直接呼び出し監査済、monitor選択/ClientArea/WorkArea/外枠Location/unchecked演算を修正、Windows Release build済） |
| M | +164/-89 | `Mods/ThumbnailBatch.cs` | .cpp,.hpp | 完了（C#原本監査済み、Section 12統合後のWindows Release build済） |
| A | +150/-0 | `Mods/CrashReport.cs` | — 新規 | 完了（C#全文・Program/AppDomain/ConsoleWindow呼び出し監査済、Windows Release build済） |
| M | +135/-28 | `Mods/EndScreen.cs` | .cpp,.hpp | 完了（C#全文・Renderer/Shell/RespawnChoice/NetPlayerBridge/HUD直接呼び出し監査済、Windows Release build済） |
| M | +76/-5 | `Mods/WindowMode.cs` | .cpp,.hpp | 完了（C#全文・Renderer/PauseMenu/Settings/Shell/WindowGeometry直接呼び出し監査済、monitor選択と外枠Location adapterを修正、Windows Release build済） |
| M | +63/-6 | `Mods/ScreenCapture.cs` | .cpp,.hpp | 完了（C#原本監査済み、Section 12統合後のWindows Release build済） |
| M | +47/-15 | `Mods/InputSettings.cs` | .cpp,.hpp | 完了（C#全文・PlayerControls/SettingsView/ModEntry/CompatibilityCheck/Renderer/ChatBox/入力診断の直接呼び出し監査済、保存パス再評価とunchecked演算を修正、Windows Release build済） |
| M | +43/-0 | `Mods/RenderOptions.cs` | .cpp,.hpp | 完了（C#全文・GameSettings/ModEntry/Renderer/Scene/SettingsView/DebugLog直接呼び出し監査済、verified no-op、Windows Release build済） |
| M | +40/-0 | `Mods/SpectatorMode.cs` | .cpp,.hpp | 完了（C#全文・Renderer/NetHooks/NetSession/NetSessionLobby/NetCheckClient/入力・画面の直接呼び出し監査済、verified no-op、Windows Release build済） |
| M | +10/-18 | `Mods/ThumbnailCapture.cs` | .cpp,.hpp | 完了（C#全文・ModEntry/ThumbnailBatch/GlfwPathCheck/ThumbnailWindowCheck直接呼び出し監査済、撮影待機・再試行・camera override・cleanup一致、verified no-op、Windows Release build済） |
| M | +8/-7 | `Mods/GameSettings.cs` | .cpp,.hpp | 完了（C#全文・Shell/SettingsView/TextLauncher/Renderer/RenderOptions/FrameTiming直接呼び出し監査済、verified no-op、Windows Release build済） |
| M | +6/-1 | `Mods/ThumbnailLog.cs` | .cpp,.hpp | 完了（C#原本監査済、Section 12統合後のWindows Release build済） |

## 4. Update — 4 ファイル (新規 0), C# +152 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| M | +139/-3 | `Mods/Update/UpdateCheck.cs` | .cpp,.hpp | 完了（C#原本・直接呼び出し監査済、CancellationTokenをlibcurl中断まで接続。buildはSection 12統合後） |
| M | +7/-0 | `Mods/Update/Updater.cs` | .cpp,.hpp | 完了（C#原本・直接呼び出し監査済、追加修正なし。buildはSection 12統合後） |
| M | +4/-1 | `Mods/Update/DesktopUpdate.cs` | .cpp,.hpp | 完了（C#原本・直接接続監査済、例外ログと無効PIDを修正。UpdateDownloadのcancel伝播も別途監査・修正。buildはSection 12統合後） |
| M | +2/-10 | `Mods/Update/BuildVersion.cs` | .cpp,.hpp | 完了（C#原本・直接参照元監査済、parser/stamp選択一致。C++ release stampはbuild adapter経由、buildはSection 12統合後） |

## 5. Chat — 2 ファイル (新規 1), C# +36 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +32/-0 | `Mods/Chat/NetChat.cs` | — 新規 | 完了（C#原本・直接呼び出し監査済、Revisionのunchecked wrapを修正。buildはSection 12統合後） |
| M | +4/-1 | `Mods/Chat/ChatBox.cs` | .cpp,.hpp | 完了（C#全文・Renderer/Android/NetChat/NetSession/NetCheckClient直接呼び出し監査済、差分なし。buildはSection 12統合後） |

## 6. Input and gamepad — 52 ファイル (新規 44), C# +4665 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +350/-0 | `Mods/Input/PointerCheck.cs` | — 新規 | 完了（C#全文監査済、Scene fixture/finallyを修正） |
| A | +309/-0 | `Mods/Input/PadBindingState.cs` | — 新規 | 完了（C#全文監査済、bounds/revisionを修正） |
| A | +301/-0 | `Mods/Input/GamepadChecks.cs` | .cpp,.hpp | 完了（shell build で GamepadUiChecks を C# と同じ位置で実行） |
| A | +264/-0 | `Mods/Input/WindowsPenInput.cs` | — 新規 | 完了（C#全文監査済、Win32 fallback/exception parity修正） |
| M | +261/-34 | `Mods/Input/StylusZone.cs` | .cpp,.hpp | 完了（C#全文監査済、NaN時のMathMin/Maxを修正） |
| A | +258/-0 | `Mods/Input/MouseFlick.cs` | — 新規 | 完了（C#全文監査済、Firedのunchecked加算を修正） |
| A | +210/-0 | `Mods/Input/GamepadManager.cs` | — 新規 | 完了（C#全文監査済、revision wrap/event valueを修正） |
| A | +199/-0 | `Mods/Input/GamepadProfiles.cs` | — 新規 | 完了（C#全文監査済、UTF-16/JSON/revision parity修正） |
| A | +158/-0 | `Mods/Input/GamepadEnhancementChecks.cs` | — 新規 | 完了（C#全文監査済、チェック順・条件・例外を照合、Windows Release build済） |
| A | +141/-0 | `Mods/Input/WeaponWheel.cs` | — 新規 | 完了（C#全文・HUD呼出元監査済、drag/availability境界を照合、Windows Release build済） |
| A | +117/-0 | `Mods/Input/GamepadOptionState.cs` | — 新規 | 完了（C#全文監査済、defaults/Load/Write/Clone/Resetを照合、Windows Release build済） |
| A | +116/-0 | `Mods/Input/GamepadUiRouter.cs` | — 新規 | 完了（C#全文監査済、repeat timerのunchecked long演算を修正、Windows Release build済） |
| A | +115/-0 | `Mods/Input/AimAssist/AimAssistWorld.cs` | — 新規 | 完了（C#全文・PlayerEntityNetAim呼出元監査済、default target/tick取得順を修正、Windows Release build済） |
| A | +111/-0 | `Mods/Input/PointerDevice.cs` | — 新規 | 完了（C#全文・Renderer/PlayerInput呼出元監査済、device/contact/primary/delta parity確認） |
| A | +101/-0 | `Mods/Input/ControllerRuntimeChecks.cs` | — 新規 | 完了（C#全文監査済、event timeoutの非block性を修正、Windows Release build済） |
| M | +99/-11 | `Mods/Input/GamepadMappings.cs` | .cpp,.hpp | 完了（C#全文監査済、mapping/GUIDのUTF-16長とunchecked countを修正、Windows Release build済） |
| A | +87/-0 | `Mods/Input/AimAssist/AimAssistTelemetry.cs` | — 新規 | 完了（C#全文・Hit/Shot/Record呼出元監査済、unchecked集計を修正、Windows Release build済） |
| A | +85/-0 | `Mods/Input/GamepadMappingWizard.cs` | — 新規 | 完了（C#全文・Desktop/Setup/Checks呼出元監査済、nameのUnicode control/UTF-16切詰めを修正、Windows Release build済） |
| M | +82/-117 | `Mods/Input/GamepadInput.cs` | .cpp,.hpp | 完了（C#全文・desktop Renderer呼出順監査済、native差分なし。Android GameViewのBeginFrame呼出有無は別途確認対象） |
| M | +79/-274 | `Mods/Input/GamepadDesktop.cs` | .cpp,.hpp | 完了（C#全文・Renderer/UiSurface/GamepadProbe呼出元監査済、GLFW接続通知とgeneration wrapを修正、Windows Release build済） |
| A | +77/-0 | `Mods/Input/WindowsGamepadHaptics.cs` | — 新規 | 完了（C#全文・GamepadHaptics/GamepadDesktop呼出元監査済、欠落XInputSetState export時の例外を修正、Windows Release build済） |
| A | +76/-0 | `Mods/Input/AimAssist/AimAssistChecks.cs` | — 新規 | 完了（C#全文・GamepadChecksからの接続監査済、全assertionと順序が一致、Windows Release build済） |
| A | +76/-0 | `Mods/Input/GamepadPlatformChecks.cs` | — 新規 | 完了（C#全文・GamepadChecks二箇所の接続監査済、全34assertion一致、Windows Release build済） |
| A | +72/-0 | `Mods/Input/AimAssist/AimAssist.cs` | — 新規 | 完了（C#全文・AimAssistWorld直結呼出監査済、native同式を確認、Windows Release build済） |
| M | +71/-47 | `Mods/Input/GamepadLayout.cs` | .cpp,.hpp | 完了（C#全文・GamepadDesktop/GamepadMappings呼出元監査済、layout定義とraw readが一致、Windows Release build済） |
| A | +69/-0 | `Mods/Input/GamepadAnalog.cs` | — 新規 | 完了（C#全文・Input/Manager/Layout/Calibration/Haptics/Checks呼出元監査済、演算と状態合成が一致、Windows Release build済） |
| A | +62/-0 | `Mods/Input/GamepadCalibration.cs` | — 新規 | 完了（C#全文・GamepadSetupPanel/Manager/EnhancementChecks呼出元監査済、sample・percentile・apply条件一致、Windows Release build済） |
| A | +56/-0 | `Mods/Input/GamepadGlyphs.cs` | — 新規 | 完了（C#全文・Desktop/Manager/PadBinding/InputPrompt/UI呼出元監査済、family判定とglyph優先順一致、Windows Release build済） |
| A | +55/-0 | `Mods/Input/AimAssist/AimAssistDebug.cs` | — 新規 | 完了（C#全文・Renderer/AimAssistWorld/Telemetry/ModEntry呼出元監査済、診断行のTargetSlotをcurrent-culture書式に修正、Windows Release build済） |
| A | +54/-0 | `Mods/Input/PadAction.cs` | — 新規 | 完了（C#全文・GamepadProfiles/PadBindingState/GamepadActions呼出元監査済、23値とenum名/Parse/ToString一致、Windows Release build済） |
| A | +54/-0 | `Mods/Input/PlayerEntityMouseFlick.cs` | — 新規 | 完了（C#全文・PlayerInput.ProcessAlt呼出順とnative実装を照合済、main/bot/aim/boost/sequence gatesとReset/Check/出力一致、Windows Release build済） |
| A | +51/-0 | `Mods/Input/GamepadHaptics.cs` | — 新規 | 完了（C#全文・Desktop/Android bridge/Renderer/settings/context/player feedback呼出元監査済、null backend呼出時をNullReferenceExceptionに修正。Windows Release build済、Android ABI buildはAndroid全ファイル完了後） |
| A | +40/-0 | `Mods/Input/GamepadOptions.cs` | — 新規 | 完了（C#全propertyとState/RuntimeConfig委譲を照合済、WheelOrderを設定所有者付きで返しC#配列参照の寿命を保持、Windows Release build済。Android ABI buildはAndroid全ファイル完了後） |
| A | +39/-0 | `Mods/Input/GamepadActions.cs` | — 新規 | 完了（C#全文・GamepadInput/GamepadEnhancementChecks呼出元監査済、invalid PadActionのshift countをC#の6bit maskに修正、Windows Release build済） |
| A | +32/-0 | `Mods/Input/WeaponSelectionDirection.cs` | — 新規 | 完了（C#全文・GamepadInput/GamepadEnhancementChecks/GamepadChecks呼出元監査済、float→intを.NET 9+互換変換にしてNaN時のC++未定義動作を除去、Windows Release build済） |
| A | +30/-0 | `Mods/Input/PlayerEntityHaptics.cs` | — 新規 | 完了（C#全文・PlayerInput/PlayerSound/PlayerHud/PlayerEntity呼出元監査済、feedback gates・weapon-selection slot/null/available-array処理一致、Windows Release build済） |
| A | +27/-0 | `Mods/Input/AimInputSourceTracker.cs` | — 新規 | 完了（C#全文・AimAssistWorld/GamepadInput/GamepadUiRouter/AimAssistChecks/AimAssistTelemetry/AimAssistDebug呼出元監査済、pointer判定・stick deadzone・120ms takeover・reset/revision semantics一致、Windows Release build済） |
| A | +25/-0 | `Mods/Input/AimAssist/AimAssistTuning.cs` | — 新規 | 完了（C#全文・AimAssist/AimAssistWorld呼出元監査済、定数/enum/profile factoryの値と順序一致、readonly record semanticsとfloat NaN等値比較をnativeへ反映、Windows Release build済） |
| M | +25/-18 | `Mods/Input/GamepadProbe.cs` | .cpp,.hpp | 完了（C#全文・ModEntry/GamepadChecks/GamepadEnhancementChecks/GamepadMonitor呼出元監査済、probe表示/終了判定一致、seconds表示のdouble精度と.NET UTF-16幅揃えを修正。Windows native library compile/link済、実行中FruityPrime.exeが出力先をロック中のためexe再リンク未確認） |
| M | +25/-213 | `Mods/Input/PadBindings.cs` | .cpp,.hpp | 完了（C#全文・全API委譲とInputSettings/GamepadManager/GamepadInput/Launcher/Probe呼出元監査済、Current.Bindings選択・action順・各委譲一致。native差分なし） |
| A | +24/-0 | `Mods/Input/GamepadRuntimeConfig.cs` | — 新規 | 完了（C#全文・GamepadManager/GamepadInput/GamepadOptions/PadBindings/GamepadProfiles/ControllerRuntimeChecks呼出元監査済、default/clone/Fallback/Selected/ThreadStatic Frame lifetimeとCurrent publication一致。AtomicSharedPtrはvolatile reference相当として確認、native差分なし） |
| A | +24/-0 | `Mods/Input/SpectatorInput.cs` | — 新規 | 完了（C#全文・Renderer呼出元監査済、focus/context gate・press消費順・deadzone/buttons/trigger/camera値一致、readonly record semanticsとfloat NaN等値比較をnativeへ反映。Windows native library compile/link済、実行中FruityPrime.exeのロックでexe再リンク未確認） |
| A | +23/-0 | `Mods/Input/GamepadDeviceSnapshot.cs` | — 新規 | 完了（C#全文・GamepadManager/GamepadProbe/Profiles/Launcher UI/直接呼出元監査済、init-only相当のreadonly APIとrecord/GamepadState/VectorのNaN等値比較を反映。Windows Release native library build済） |
| A | +20/-0 | `Mods/Input/HapticScheduler.cs` | — 新規 | 完了（C#全文・GamepadHaptics直接呼出監査済、undefined feedbackの範囲外アクセスをIndexOutOfRangeExceptionへ修正、Windows Release build済） |
| A | +19/-0 | `Mods/Input/InputPrompt.cs` | — 新規 | 完了（C#原本と呼び出し元を監査済、UiAction fallback/secondary slot/glyph/ToString一致、readonly/default Label nullをnativeに反映） |
| A | +19/-0 | `Mods/Input/InputSourceTracker.cs` | — 新規 | 完了（C#全文・Renderer/GamepadManager/HUD呼び出し元監査済、180ms切替とResetを照合、unchecked long差分を修正） |
| A | +17/-0 | `Mods/Input/AimAssist/AimAssistMath.cs` | — 新規 | 完了（C#全文・AimAssist/AimAssistWorld呼び出し元監査済、Smooth/Finite/Opposition/ScoreとNaN/Inf挙動一致、差分なし） |
| A | +15/-0 | `Mods/Input/ControllerLayoutState.cs` | — 新規 | 完了（C#全文・GamepadRuntimeConfig/GamepadOptions/PadBindings呼び出し元監査済、Bindings/Name/Southpaw/Applyと更新順一致、差分なし） |
| A | +13/-0 | `Mods/Input/StickCalibration.cs` | — 新規 | 完了（C#全文・GamepadCalibration/OptionState/GamepadMonitor呼び出し元監査済、readonly/NaN等値/Math.Maxを修正、Windows Release build済） |
| A | +12/-0 | `Mods/Input/AimAssist/AimAssistState.cs` | — 新規 | 完了（C#全文・AimAssist/AimAssistWorld/AimAssistChecks呼び出し元監査済、field defaults/型/Reset順一致、差分なし） |
| A | +10/-0 | `Mods/Input/AimAssist/AimAssistTarget.cs` | — 新規 | 完了（C#全文・AimAssist/AimAssistWorld/Debug/Telemetry/Checks照合、default値・readonly・等値・enum文字列を修正。Windows Release build済） |
| M | +10/-77 | `Mods/Input/PointerInput.cs` | .cpp,.hpp | 完了（C#全文・PointerDevice/Renderer/PlayerInput/PointerCheckの呼出元監査済、設定既定値・閾値・ログ・Reset一致。JumpsIgnoredのunchecked int wrapを修正。Windows Release build済） |

## 7. Render — 22 ファイル (新規 14), C# +2658 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +416/-0 | `Mods/Render/LauncherPhoto.cs` | .cpp,.hpp | 完了（C#原本監査済、Renderer呼び出しをSection 12で接続） |
| A | +277/-0 | `Mods/Render/PlayerEntityMapPick.cs` | — 新規 | 完了（C#全文・PlayerEntityEndScreen/MapPick/MapThumbnail呼出元監査済、描画順・座標・投票表示・hit layout一致。UTF-16切詰めとMath.Min/Max挙動を修正。Windows Release build済） |
| A | +256/-0 | `Mods/Render/MapThumbnail.cs` | — 新規 | 完了（C#全文・Renderer/EndScreen/PlayerEntityMapPick/Scene.BindTextureの接続監査済、RGB decode・box filter・per-frame cache lifecycle一致。OrdinalIgnoreCase comparerとunchecked texture-name incrementを修正。Windows Release build済） |
| A | +244/-0 | `Mods/Render/UiOverlay.cs` | .cpp,.hpp | 完了（C#原本監査済、Renderer呼び出しをSection 12で接続） |
| A | +226/-0 | `Mods/Render/NoiseField.cs` | — 新規 | 完了（C#全文・LauncherNoise/MovingBackdropの参照を監査済、seeded Random/Stopwatch・resize・noise/color計算一致。CellsForの.NET 10 double→int変換とPixels配列の変更可能性を反映。Windows Release build済） |
| A | +174/-0 | `Mods/Render/LauncherHunter.cs` | .cpp,.hpp | 完了（C#原本監査済、RenderWindow APIと描画呼出しを接続） |
| A | +160/-0 | `Mods/Render/LauncherNoise.cs` | .cpp,.hpp | 完了（C#原本監査済、GL unpack enum を追加） |
| M | +138/-3 | `Mods/Render/PreviewPass.cs` | .cpp,.hpp | 完了（C#全文・Rendererのstep/collect/draw順とAddRenderItem、LauncherHunter/HunterStand/EndScreen接続を監査済、preview state/座標/GL pass一致。単独描画時のcatch範囲をC#に合わせて修正。Windows Release build済） |
| A | +132/-0 | `Mods/Render/AppIcon.cs` | .cpp,.hpp | 完了（C#原本監査済、GLFW Window::SetIcon 経由で接続） |
| A | +116/-0 | `Mods/Render/Radar.cs` | — 新規 | 完了（C#全文・PlayerHud/Features/ModEntry/SettingsViewの参照を監査済、設定既定値・全weapon item分類・palette値一致。readonly Paletteをnativeのprivate storage/getter value型へ反映。Windows Release build済） |
| A | +97/-0 | `Mods/Render/HunterShot.cs` | — 新規 | 完了（C# interface/stateとnative `.hpp`を全文照合、nullable Task→optional shared_future・全state既定値一致。AndroidHunterShotのBGRA/top-down/tightly-packed契約も確認しnative API commentに反映。Android buildは未実施） |
| M | +77/-0 | `Mods/Render/FrameTimingCheck.cs` | .cpp,.hpp | 完了（C#全文・ModEntryの両headless入口を監査済、7 frame-rate case/2秒stall/Lockjaw noise invariant/出力とexit code一致。seeded Randomも照合、差分なし。Windows Release build green） |
| A | +70/-0 | `Mods/Render/PlayerEntityTeamScoreboard.cs` | — 新規 | 完了（C#全文・PlayerHud/TeamVisuals/DrawText2D接続監査済、current-culture整数表示と.NET 10 float→int変換を修正。Windows Release build済） |
| A | +62/-0 | `Mods/Render/LockjawTrailProbe.cs` | — 新規 | 完了（C#全文・MapAudit呼出時点/RenderItem/LinkedList列挙監査済、null参照時の例外とunchecked件数加算を修正。Windows Release build済） |
| M | +51/-0 | `Mods/Render/PlayerEntityStylusHud.cs` | .cpp,.hpp | 完了（C#全文・PlayerHudのWeaponSelect/UpdateWeaponArc・StylusZone/DrawHudFlatBox接続監査済、座標・scale・ellipse描画と.NET cast/Math MinMax一致。差分なし、Windows Release build green） |
| A | +43/-0 | `Mods/Render/DesktopGlContext.cs` | .cpp,.hpp | 完了（C#原本監査済、Renderer経由で接続） |
| M | +31/-0 | `Mods/Render/GlEs.cs` | .cpp,.hpp | 完了（C#全文監査済、Android ES state/primitive/list/texture/shader/framebuffer/uniform動作を照合。null/empty source/name、例外型、current-cultureログ書式を修正。Android nativeのGL dispatch接続は別途要確認。Android buildは全Android監査後） |
| A | +28/-0 | `Mods/Render/LockjawTrailNoise.cs` | — 新規 | 完了（C#全文とnative `.hpp/.cpp`・FrameTimingCheck呼出を監査済。tick/slot/bomb/segment/axisのbit混合、unchecked wrap、float変換・演算順が一致。修正なし） |
| M | +26/-0 | `Mods/Render/HunterPreview.cs` | .cpp,.hpp | 完了（C#全文・PreviewPass直接呼出・EntityBase.GetModels collectionを監査。shown/missing/recolor/idle-step/light/draw順が一致。catch範囲とモデル一覧の同一性を修正、Windows Release build green） |
| M | +17/-1 | `Mods/Render/PlayerEntityEndScreen.cs` | .cpp,.hpp | 完了（C#全文・EndScreen.NoteLayout/MapPick/PreviewPass接続を監査。配置・preview穴・HUD文字・ヒット領域・hunter/suit/ready表示が一致。修正なし） |
| M | +12/-9 | `Mods/Render/PlayerEntityProHud.cs` | .cpp,.hpp | 完了（C#全文・PlayerHudのDrawHudObjects/DrawModeScore呼出を監査。energy/ammo計算・閾値・配置・icon tint・score message IDが一致。修正なし、Windows Release build green） |
| M | +5/-0 | `Mods/Render/PlayerEntityVoteHud.cs` | .cpp,.hpp | 完了（C#全文・MapVote/EndScreen直接接続を監査。Android/desktop配置・touch/gamepad表示・panel/button hitboxとNoteLayout順が一致。修正なし、Windows Release build green） |

## 8. Multiplayer and teams — 7 ファイル (新規 7), C# +503 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +118/-0 | `Mods/Multiplayer/ResourceAudit.cs` | — 新規 | 完了（C#全文監査済、Math.Min/Maxを修正） |
| A | +99/-0 | `Mods/Multiplayer/MapResourceRules.cs` | — 新規 | 完了（C#全文監査済、評価順/null例外を修正） |
| A | +95/-0 | `Mods/Multiplayer/TeamGameplayTest.cs` | — 新規 | 完了（C#全文監査済） |
| A | +89/-0 | `Mods/Multiplayer/GameStateTeams.cs` | — 新規 | 完了（C#全文監査済、span boundsを修正） |
| A | +46/-0 | `Mods/Multiplayer/TeamLayout.cs` | — 新規 | 完了（C#全文監査済、span bounds/unchecked積を修正） |
| A | +37/-0 | `Mods/Multiplayer/TeamVisuals.cs` | — 新規 | 完了（C#全文監査済、Windows Release build済） |
| A | +19/-0 | `Mods/Multiplayer/MatchWorldProfile.cs` | — 新規 | 完了（C#全文監査済、Windows Release build済） |

## 9. Network — 56 ファイル (新規 26), C# +12807 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +1952/-0 | `Mods/Network/NetHitClaims.cs` | — 新規 | 完了（C#全文・native `.hpp/.cpp`とRenderer/NetSession/NetDamage/NetHitPrediction/Lifecycle接続を監査。claim検証・grace/ledger/死因順序・rescue抑止・cleanup一致。NearestLedgerOffsetのunchecked差分/Math.Abs境界を修正し、NetPlayerLifecycleの共有OldLifeClaims統計の直接加算もunchecked `IncrementInPlace`へ修正。Windows Release native library build green、実行ファイル再リンクは稼働中FruityPrime.exeにより未確認） |
| M | +934/-33 | `Mods/Network/NetProtocol.cs` | .cpp,.hpp | 完了（C#全文・native `.hpp/.cpp`・NetSession/DedicatedServer/NetMaster/NetHitClaims の直接送受信箇所を監査。PacketType・サイズ/offset・byte order・文字列置換・旧長さ互換・既定値/境界を照合し一致。修正なし、静的監査のみ） |
| M | +883/-78 | `Mods/Network/NetHitPrediction.cs` | .cpp,.hpp | 完了（C#全文・native `.hpp/.cpp`・PlayerEntity.TakeDamage/NetDamage/NetPlayerBridge/BeamProjectile/Renderer/PlayerHud/NetHitClaims/NetPlayerLifecycle/NetSession/ModEntry の直接接続を監査。hold/grace・リング寿命・claim settlement・体力補正・撃破/marker/reset条件一致。修正なし、静的監査のみ） |
| M | +704/-105 | `Mods/Network/DedicatedServer.cs` | .cpp,.hpp | 完了（C#全文・native `.cpp/.hpp`・ModEntry/NetHostSession/HostPool/NetMasterの直接接続を監査。loop順序、Hello/Welcome/refusal/status、authoritative/relay、snapshot/intent、投票/rotation、ping/roster、切断/cleanupが一致。修正なし、静的監査のみ） |
| A | +609/-0 | `Mods/Network/HitRig.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`・ModEntry/NetTestScript/NetCheckClient/PlayerEntityの直接接続を監査。役割/照準/距離/発射cadence/controls edge/reportが一致。nativeの符号付きカウンター加算をC# unchecked wrapにし、snapshot frameのuint→intをbit reinterpretへ修正。Windows Release build green、runtime未実施） |
| A | +558/-0 | `Mods/Network/NetSmoothing.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`・NetSession/NetHooks/NetPlayerBridge/NetPlayerLifecycle/NetUnlagged/NetHitClaims/NetLog/NetCheckClient/ModEntry の直接接続を監査。snapshot記録、playout tick、補間/hold条件、life/generation guard、subframe ack、reset/rebase、診断値と呼び出し順が一致。nativeの符号付きカウンターをC# unchecked wrapへ修正。Windows Release build green、runtime未実施） |
| A | +536/-0 | `Mods/Network/LocalServer.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`・ModEntryの`-installserver`/`-hostlocal`、CreateServerScreenのinstall/start/join接続を監査。実行ファイル選択、package取得/展開/実行bit、paths.txt/rotation、owner token、port選定、process継続/cleanup、cancelと起動待ちが一致。GUI installerからUpdateCheck/UpdateDownloadへstop tokenを渡し、progress callbackの例外cancelを除去。CanBindはC#同様SocketExceptionのみ処理。Windows Release build green、download/server runtime未実施） |
| A | +536/-0 | `Mods/Network/NetLobbyTest.cs` | — 新規 | 完了（C#全文・native `.hpp/.cpp`を照合し、packet/protocol境界、team layout、client state、全lobby/continuous/client-sessionシナリオとassert順を確認。`ModEntry`を含む直接呼出し元は両側にないことを確認。C# `IsBackground`/`Join(5000)`に対しnative Rigの無期限joinと起動失敗時のjoinable threadが不一致だったため、共有thread state・例外伝播・5秒join/detachを実装。Windows Release build green、シナリオ実行は未実施） |
| M | +489/-116 | `Mods/Network/NetSession.cs` | .cpp,.hpp | 完了（C#全文・native `.hpp/.cpp`を照合。接続/再接続、role・packet受理順、host roster、slot intent、authority handoff、session/match/rosterの世代検証、snapshot/health/time同期、demo記録とcleanupを確認。Renderer/NetHooksのtick・send/apply順、NetLaunch.Connect、DedicatedServer/ServerSimのauthority・roster・intent接続も照合。C# `Encoding.ASCII`の補助平面文字が2つの`?`になるのにnativeが1つだったため修正。C# unchecked `long++`相当の診断カウンター加算を`IncrementInPlace`へ変更。Windows Release build green、runtime未実施） |
| M | +481/-22 | `Mods/Network/NetUnlagged.cs` | .cpp,.hpp | 完了（C#全文・PlayerInputのBeginShot/Spawn/EndShot、NetSessionのReset/Record、NetHitClaims/NetSmoothing/NetPlayerLifecycle/NetPlayerBridge/NetShotDiagnosticsの直接接続を監査。履歴・subframe rewind・世代/life照合・補間・restore・beam catch-up・診断出力の順序と条件が一致。C# unchecked `long` カウンター加算をnativeの`IncrementInPlace`/`UncheckedAdd`へ修正。Windows Release build green、runtime未実施） |
| M | +384/-12 | `Mods/Network/NetMaster.cs` | .cpp,.hpp | 完了（C#全文・DedicatedServerのheartbeat/Farewell/hosted reporter、ModEntryの`-masterserver`/`-servers`/`-hosts`/`-hostgame`、TextLauncher・CreateServerScreen・PlayScreenのquery/probe/request接続を監査。heartbeat、expiry、host port cooldown/reap、list分割とCanHostの三状態、並列probe callback、request応答検証が一致。C#のfloat→int変換とprobe経過long→int unchecked wrapをnative helperへ合わせ、`-servers`のCanHost説明欠落も追加。Windows Release build green、runtime未実施） |
| M | +366/-389 | `Mods/Network/NetPlayerBridge.cs` | .cpp,.hpp | 完了（C#全文・NetHooksの入力/位置復元/ApplyState順、NetSessionのSendIntent metadata、PlayerEntity Spawn/PlayerProcess RespawnRequested、NetPlayerLifecycleのslot resetとNetRoomChangeを照合。intent edge履歴、life/generation検証、form reconciliation、authority/local/puppet別state適用、snapshot/intent位置復元、velocity・node/volume更新が一致。C# unchecked `long++` に対してnative signed `++`だったRejectedUpdates/Snapsと直接呼出し元PlayerEntityNetAimのNodeLookupsUnresolvedを`IncrementInPlace`へ修正。Windows Release build green、runtime未実施） |
| M | +307/-47 | `Mods/Network/NetDamage.cs` | .cpp,.hpp | 完了（C#全文・native `.hpp/.cpp`と`PlayerEntity.TakeDamage`/`ModNetDie`、BeamProjectile/Bomb/SpawnBomb、NetHitClaims/Lifecycleの直接接続を照合。Suppression/claim beam、room/slot/life reset、snapshot・damage replay・death/score復元の条件と順序が一致。C# unchecked wrapに対しnative signed overflowだった弾・爆弾・damage診断カウンター、PredictionScoreScope深度を`IncrementInPlace`/`DecrementInPlace`へ変更。uint damageのint変換・加算・resolve log減算をC#のunchecked bit/wrap semanticsへ修正。`git diff --check`通過、Windows Release native build green、runtime未実施） |
| A | +296/-0 | `Mods/Network/LobbyCommands.cs` | — 新規 | 完了（C#全文・native `LobbyCommands.cpp`/`DedicatedServer.hpp`と、packet dispatch/Remove/end-of-match、HostPool/NetMasterのlobby初期化を照合。session revision・command dedupe、owner/phase/revision権限順、team編成/容量、match/map検証、start失敗rollback/load barrier、lobby復帰・slot除去が一致。MapRotationのNaNをC#はClamp後も保持しunchecked変換結果は未規定。nativeのfloat→整数UBを避けNaNを0にするガードを追加。C# stackalloc team-count spanへ`Clear()`を追加し、native zero-initialized arrayに対応する定義済みcountにした。`git diff --check`通過、Windows Release native build green、C# Release build green（既存CS0618警告4件）、runtime未実施） |
| A | +284/-0 | `Mods/Network/NetCombatCheck.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`と`ServerSim`/`NetHitClaims`の依存を照合。両側ともproduction callerは未登録。初期state、dead held-fire、実weapon spawn/projectile lifecycle/ricochet、hit-claim拒否/同時kill/grace deadline、continuous-phase goldenとpeer間一致のケース順・条件が一致。native累積assertionのsigned `++`をunchecked `IncrementInPlace`へ変更し、catch-all failure出力を`ExceptionToString`へ変更。`git diff --check`通過、Windows Release native build green、harness runtime未実施） |
| A | +264/-0 | `Mods/Network/HostPool.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`とDedicatedServerのHostRequest/loop/shutdown、ModEntryの`-hostports`接続を照合。同一IPからの空ゲーム置換、使用中port保護、cooldown、mode/rotation/session options/owner token/reporter設定、listener待ち、未参加180秒・全員退出後45秒のreap、停止待ち/port解放/通知が一致。worker例外のC# `catch (Exception)`に対しnative `std::exception`のみ捕捉していたため`catch (...)`と`ExceptionMessage`へ変更。`git diff --check`通過、Windows Release native build green、runtime未実施） |
| A | +200/-0 | `Mods/Network/MapAuditTeams.cs` | — 新規 | 完了（C#全文・native `MapAuditTeams.cpp`/`MapAudit.hpp`と`ModEntry -maptest -teamprobe`設定を照合。8 slot/4 team配置、同士討ち規則、Standings/Survival/Nodes/Defender/Bountyの確認と画面キャプチャ条件が一致。`RunTeamProbe`/`CaptureTeamResults`はC#・native双方とも呼び出し元なし。native check counterのsigned `++`をunchecked `IncrementInPlace`へ、例外捕捉/表示をC#の`Exception`/Message/ToStringに合わせて修正。`git diff --check`通過、Windows Release native build green、runtime未実施） |
| M | +198/-11 | `Mods/Network/PlayerEntityNetAim.cs` | .cpp,.hpp | 完了（C#全文・直接呼び出し元監査済） |
| A | +194/-0 | `Mods/Network/NetSessionLobby.cs` | — 新規 | 完了（C#全文・native `NetSessionLobby.cpp`/`NetSession.hpp`とNetSessionのUpdate/packet dispatch/Stop、LobbyScreen/NetLaunch/NetSlotManager/Chatの直接呼び出しを照合。phase/owner/timeout判定、command ID・再送間隔と4回上限、revision/epoch/match stale guard、match resetとsynthetic state適用順、load ack/identity再送、roster作成とresetが一致。修正なし。`git diff --check`通過、Windows Release native build green、runtime未実施） |
| A | +180/-0 | `Mods/Network/NetPlayerLifecycle.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`とPlayerEntity Spawn/BeamProjectile ricochet、NetSession occupant/state/intent/roster、NetSlotManager、NetHitClaims/NetDamageの共有統計書込箇所を照合。generation/life guard、spawn/reset順、projectile launch identity、state/intent rejection、slot cleanupとlogが一致。native signed 64-bit統計加算をunchecked `IncrementInPlace`へ修正し、外部writerのOldLifeClaimsも同様に修正。`git diff --check`通過、Windows Release native library build green、exe再リンクは稼働中FruityPrime.exeにより未確認、runtime未実施） |
| A | +180/-0 | `Mods/Network/SessionProtocol.cs` | — 新規 | 完了（C#全文・native `.hpp/.cpp`、LobbyCommands/NetSessionLobby/NetSessionの送受信箇所、NetLobbyTestのProtocolChecksを照合。session stateの75-byte layout/各offset、little-endian、enum/rule/participant検証、command/result/load packetsの既定値・拒否条件・unchecked sbyte/short変換が一致。C#のSpan境界例外に対しnative writeが未チェックのindex/subspanで未定義動作となる差を、同じ評価順と先行書込を保つchecked accessへ修正。`git diff --check`通過、Windows Release native library build green、runtime未実施） |
| M | +160/-15 | `Mods/Network/NetHooks.cs` | .cpp,.hpp | 完了（C#全文・native `.hpp/.cpp`とRenderer/PlayerInput/PlayerProcess、SpectatorMode/ModEntry/NetPlayerBridgeの直接接続を照合。LocalSlotのoffline/connecting/server/demo分岐、puppet・slot保持・spawn、snapshot/intent復元とstale境界、shot origin/aim、AfterInput/AfterSimulationの順序と送信/適用条件が一致。C#配列添字と異なるnative `std::array::at()`例外を`ManagedAt`へ修正。`git diff --check`通過、Windows Release native library build green、runtime未実施） |
| M | +150/-2 | `Mods/Network/NetCheckClient.cs` | .cpp,.hpp | 完了（C#全文・native `.cpp/.hpp`・ModEntryの直接呼出しを照合。scene/window lifecycle、frame処理順、観戦・再接続・map vote、capture、各観測値、report、終了処理が一致。C#の`Double.TryParse(InvariantCulture)`を`NumberStyles.Float | AllowThousands`で呼ぶNativeRuntime parserへ統一し、独自近似を削除。signed `int`統計のunchecked wrapとframe差分をC#に合わせ、例外捕捉・表示もcatch-all/`ExceptionToString`へ修正。`git diff --check`通過、Windows Release native library build green、runtime未実施） |
| A | +138/-0 | `Mods/Network/SpireAltPoseCheck.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`・ModEntryの直接呼出しを照合。headless sim起動、Spire roster、240 frame input/press history、morph→alt attack、左右collision poseの移動・offset閾値、failure時の停止と例外再送出が一致。C#の再利用press配列とnativeのframe-local vectorはintent受理後すぐ同期stepするため同じ値を観測する。修正なし。`git diff --check`通過、Windows Release native library build green、runtime未実施） |
| M | +122/-2 | `Mods/Network/NetTestScript.cs` | .cpp,.hpp | 完了（C#全文・native `.cpp/.hpp`・ModEntry/NetHooks/MapAudit/NetCheckClient/NetFeatureCheck/PlayerEntityNetAim/WeaponDpsの直接接続を照合。16 phaseの順序・clock・target/input/aim・resetとcheck callerを確認。C# `Double.TryParse(InvariantCulture)`の`Float | AllowThousands`をNativeRuntime parserへ合わせ、phase quotientの.NET 10 `double`→`int`変換を`ConvertToInt32Net9`へ変更。MapAuditのNaN設定値はMapAudit自身の監査で扱う。`git diff --check`通過、Windows Release native library build green、runtime未実施） |
| A | +100/-0 | `Mods/Network/ContinuousWeaponPhase.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`とPlayerInput/BeamProjectile/PlayerEntity/NetSession/NetSessionLobby/NetSlotManager/NetCombatCheckの直接接続を照合。MaxIntentAge、ammo/damage cadence、slot clockのObserve/Advance/Resolve、ownerとremote intent分岐、reset条件と呼出し順が一致。負のslot数ではC#配列が`OverflowException`を投げるためnative constructorにも同じ例外を追加。`git diff --check`通過、Windows Release native library build green、runtime/harness未実施） |
| A | +98/-0 | `Mods/Network/NetHealthSync.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`とSceneSetup/ItemSpawnEntity/PlayerProcess/NetSession/DedicatedServer/NetSessionLobby/HealthSimulationTestの直接接続を照合。3-byte header/7-byte entry、health spawn上限、picker flags・reserved bits・重複ID・match検証、snapshot送受信順、replicaのspawn/pickup ownershipとroom resetが一致。native Writeのspan長を`int32_t`へ縮小して容量比較する差を除去。`git diff --check`通過、Windows Release native library build green、harness/runtime未実施） |
| A | +95/-0 | `Mods/Network/FormReconciliation.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`とNetPlayerBridge/ServerSimCheckの直接接続を照合。morph/unmorph遷移の開始・終了・90 frame上限、8/12 frame補正待機、ping latency grace、状態resetとFormCorrection値が一致。C#の既定unchecked `ping * 60 / 1000 + 8`をnative `UncheckedMultiply`/`UncheckedAdd`へ合わせ、enum underlying typeも既定Int32に修正。`git diff --check`通過、Windows Release native library build green、runtime/harness未実施） |
| A | +91/-0 | `Mods/Network/NetShotDiagnostics.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`とPlayerInput/BeamProjectile/NetDamage/NetHitClaims/NetHitPrediction/NetUnlagged/NetCheckClient/ServerSim/NetCombatCheckの直接接続を照合。ShotKey/enum/bucket、outcome・continuous統計、reset/describeの項目と順序が一致。C# unchecked `long` wrapに合わせ、native統計更新・Describe集計とNetHitClaims/NetHitPredictionの共有配列書込みをwrap-safe化。数値診断出力をNativeRuntime current-culture formatへ変更。`git diff --check`通過、Windows Release native library build green、runtime未実施） |
| A | +90/-0 | `Mods/Network/HealthSimulationTest.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`とModEntryの`-healthsimtest`接続、ServerSim/NetHealthSync/SceneSetup/ItemSpawn/NetLaunchを照合。authorityの180 frame起動・pickup・snapshot tail・respawn、replicaのentity数/状態収束/所有権、assert順・cleanupが一致。catch-all例外出力をC# `Exception.ToString()`相当へ、health名/countの数値書式をcurrent cultureへ修正。`git diff --check`通過、Windows Release native library build green、harness未実行） |
| M | +90/-35 | `Mods/Network/NetLaunch.cs` | .cpp,.hpp | 完了（静的監査済み、Section 12 後にビルド） |
| A | +82/-0 | `Mods/Network/NetLifecycleTracker.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`とNetPlayerLifecycle/NetSession/NetSessionLobby/DedicatedServer/LobbyCommands/NetSmoothing/NetDamage/NetHitClaims/NetLobbyTestの直接接続を照合。zero-reserved Next、ushort/uint/ulong Newer、occupant/life reset、death tombstone・resurrection rejection順が一致。native ushort/uint serial差分をC# unchecked signed bit reinterpretと同じ`std::bit_cast`に変更。`git diff --check`通過、Windows Release native library build green、harness未実施） |
| M | +82/-12 | `Mods/Network/PlayerEntityNetHud.cs` | .cpp,.hpp | 完了（C#全文・HUD呼び出し元監査済） |
| M | +80/-2 | `Mods/Network/ServerSimCheck.cs` | .cpp,.hpp | 完了（C#全文・native `.cpp/.hpp`、ModEntryの`-simcheck`解析、ServerSim/FormReconciliation/NetSession/NetPlayerLifecycle/NetProtocolの直接接続を照合。人数clamp、起動可否/失敗、roster、全slotのsynthetic intent、step計測、snapshot/終了統計、spawn判定、formcheck、cleanupとメモリ採取を確認。C#の`Math.Round`後.NET 10 double→Int32飽和変換に対しnative固有関数は正の範囲外/NaNを`int.MinValue`にしていたため`NativeRuntime::MathRoundToInt32`へ置換。`git diff --check`通過、Windows Release native library build green、runtime/harness未実施） |
| M | +77/-1 | `Mods/Network/NetLog.cs` | .cpp,.hpp | 完了（C#全文・native `.cpp/.hpp`、NetSession/NetHooks/NetConnectCommand/NetCheckClientのOpen/Snapshot/Close接続を照合。間隔設定、client名安全化、ファイル作成/上書き/flush、Event/CollisionRange、snapshot項目とhitreg集計、scene/node参照、失敗時のlogging継続を確認。native間隔parserをC# `NumberStyles.Float`相当の`DoubleTryParseInvariant`へ変更し、ログ整数の`std::to_string`をcurrent-culture formatterへ置換。`git diff --check`通過、Windows Release native library build green、runtime未実施） |
| A | +76/-0 | `Mods/Network/LobbyRules.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`、DedicatedServer/LobbyCommands/NetLaunch/LobbyScreen/SessionProtocol/NetLobbyTestの直接接続とTeamLayout/MatchWorldProfile依存を照合。mode/format/mapの定義検証順、exact/flexible roster数、team capacity/ready判定と理由文が一致。nativeのroom key長をUTF-8 byte数からC# `string.Length`相当のUTF-16 unit数へ変更し、Roster配列の参照をC#と同じ短絡条件内へ移動、理由中の数値をcurrent-culture表示へ統一。C# stackalloc count spanに`Clear()`を追加し、nativeのzero-initialized arrayと同じ確定値にした。`git diff --check`通過、Windows Release `LobbyRules.cpp.obj` compile green。全体build/runtime/harness未実施） |
| M | +75/-0 | `Mods/Network/MapRotation.cs` | .cpp,.hpp | 完了（C#全文・native `.cpp/.hpp`とDedicatedServer/LobbyCommands/HostPool/NetMaster/LocalServer/NetHostSession/ModEntry/NetLobbyTestの直接呼出しを照合。Current/Nextのfallback、FromList/SingleMatchのNone→Battle、コメント/空行/pipe/trim、enum・時刻(invariant Float)・point(current culture)解析、list/default出力、pending/override優先とcycle index再開が一致。修正なし。Windows Release `MapRotation.cpp.obj` up-to-date、runtime未実施） |
| A | +70/-0 | `Mods/Network/NetFaultQueue.cs` | — 新規 | 完了（C#全文・native template/.cppとNetLag::CreateQueue/NetTransportの直接接続を照合。引数検証、seed付きRandomの呼出し順、loss/jitter/reorder/duplicate、容量上限、同時刻のorder付き優先度、dequeue条件と失敗時default値が一致。nativeのsigned `++`によるDropped/Duplicated/Reordered/orderの未定義overflowを`IncrementInPlace`へ変更。`std::max`/heap比較のNaN・signed zero順がC# `Math.Max`/`Double.CompareTo`と異なるため.NET互換の比較へ修正。`git diff --check`通過、Windows Release `NetTransport.cpp.obj` compile green（既存offsetof警告のみ）、runtime未実施） |
| A | +69/-0 | `Mods/Network/NetTimingDiagnostics.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`とNetHooks/NetSession/NetSmoothing/NetDamage/NetPlayerLifecycle/NetCheckClient/ServerSimの直接接続を照合。50ms stall/250ms attribution、snapshot interval、frameごとのposition重複除外、snapshot対intent fallback、slot/global reset、診断文の順が一致。native signedカウンターとfallbackの`++`を`IncrementInPlace`へ置換し、`std::to_string`をcurrent-culture `Runtime::ToString`へ変更。`git diff --check`通過、Windows Release `NetTimingDiagnostics.cpp.obj` compile green（既存offsetof警告のみ）、runtime未実施） |
| M | +66/-2 | `Mods/Network/ServerSim.cs` | .cpp,.hpp | 完了（C#全文・native `.cpp/.hpp`とDedicatedServer/LobbyCommands/HealthSimulationTest/NetCombatCheck/ServerSimCheck/SpireAltPoseCheckの直接接続を照合。可用性判定、authority/roster/session初期化順、scene構築、固定step/上限/stall・drop計測、失敗時rollback、停止時のsession/cache cleanup、Room/Describeの診断出力が一致。nativeの`std::exception`限定catchをC# `catch(Exception)`相当のcatch-allへ広げ、例外Message/ToStringと初回・反復stepログを揃えた。計測は.NET `Stopwatch.GetElapsedTime().TotalSeconds`と同じTimeSpan tick変換へ、整数診断はcurrent-culture書式へ修正。`git diff --check`通過、Windows Release `ServerSim.cpp.obj` compile green（既存offsetof警告のみ）、runtime未実施） |
| A | +61/-0 | `Mods/Network/MatchDefinition.cs` | — 新規 | 完了（C#原本・native `.hpp/.cpp`とSessionProtocol/LobbyCommands/DedicatedServer/LobbyScreen/CreateServerScreen/NetLaunch/NetSession/NetMatchSync/NetHudHealth/ChatBoxの直接参照を照合。byte enum値、ushortのSessionRules bit値とRules合成、全GameModeのUsesLives/UsesTimeTarget/DefaultValue、MatchDefinitionの各既定値・値比較が一致。native `RoomKey`のoptionalはC# default structのnullを保持し、wire read/writeも対応。SessionPhase/MatchFormatのToStringは定義値・未知値ともC# enum表記に一致。修正なし。`git diff --check`通過、Windows Release `MatchDefinition.cpp.obj` up-to-date（再コンパイルなし）、runtime/harness未実施） |
| A | +58/-0 | `Mods/Network/NetHealthSyncTest.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`と唯一の呼出し元`NetLobbyTest.Run`を照合。開始/終了時のroom reset、手書きpacketの各byte offset、valid/invalid flags・picker・duplicate・truncation・stale match、snapshot容量、slot 7 objective clock、NaN拒否のassert順と値が一致。失敗時の`InvalidOperationException`と文言も一致。修正なし。`git diff --check`通過、Windows Release `NetHealthSyncTest.cpp.obj` up-to-date（再コンパイルなし）。harness/runtime未実施） |
| M | +41/-5 | `Mods/Network/MapAudit.cs` | .cpp,.hpp | 完了（C#全文・native `.cpp/.hpp`、`MapAuditTeams` partial・`ModEntry -maptest`・`NetTestScript`等の直接接続を照合。simulation/update/render/input、affliction・spawn/item/render probe、node/puppet検査、capture、report/終了処理の順序を確認。C# `Math.Max/Min`のNaN・signed zero伝播とnative `std::max/min`の差をRuntime helperに変更。scoreboardの.NET 10 double→Int32 NaN/飽和変換、整数`total * 2`と長時間カウンター/集計のunchecked wrap差を修正。`git diff --check`通過、Windows Release `MapAudit.cpp.obj` compile green（既存offsetofとcapture戻り値の警告あり）、runtime/harness未実施） |
| M | +39/-30 | `Mods/Network/NetLag.cs` | .cpp,.hpp | 完了（C#全文・native `.cpp/.hpp`とModEntry/NetTransport/DebugLog/NetCheckClient/NetLobbyTestの直接接続を照合。seed/jitter/rate/latency parser、失敗時の既存値保持、Active判定、queue作成値、説明文を確認。C# `unchecked(Seed + outbound)`をnative `UncheckedAdd`へ変更し、説明中の整数をcurrent-culture formatterに統一。`git diff --check`通過、Windows Release `NetLag.cpp.obj`/`NetTransport.cpp.obj` compile green（offsetof警告のみ）、runtime/harness未実施） |
| M | +37/-3 | `Mods/Network/NetMatchSync.cs` | .cpp,.hpp | 完了（C#全文・native `.cpp/.hpp`、NetHooks/NetSession/NetMatchEnd/MatchDefinition/NetLobbyTestの直接接続を照合。同期条件、goal/rules反映、intermission・無制限clockの優先順、1.5秒drift閾値とreset時の状態が一致。修正なし。`git diff --check`通過、Windows Release `NetMatchSync.cpp.obj` up-to-date（再コンパイルなし）、runtime/harness未実施） |
| A | +37/-0 | `Mods/Network/NetMatchTimeSync.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`とNetSession snapshot送受信/DedicatedServer検証/HealthSimulationTest/NetHealthSyncTestを照合。slotごとのTime/TeamTimeのlittle-endian offset、128-byte size、exact-length・finite/−1 validation、Receive順序とsnapshot tail offsetが一致。修正なし。`git diff --check`通過、Windows Release `NetMatchTimeSync.cpp.obj` up-to-date（再コンパイルなし）、runtime/harness未実施） |
| A | +31/-0 | `Mods/Network/NetHudHealth.cs` | — 新規 | 完了（C#全文・native `.cpp/.hpp`とPlayerEntityNetHud/PlayerHud/NetLogの直接利用を照合。HideOpponents/Visible条件、remote stateのslot/life/generation検証、fallback health/frame/life/authority値、sample field型・default/equalityが一致。native `ServerSession`二重取得をC# property patternと同じ単一取得へ変更。`git diff --check`通過、Windows Release `NetHudHealth.cpp.obj` compile green（offsetof警告のみ）、runtime未実施） |
| M | +27/-0 | `Mods/Network/NetFeatureCheck.cs` | .cpp,.hpp | 完了（C#全文・native `.hpp/.cpp`とNetCheckClientの直接呼出しを照合。観測・phase・scoreboard・feature verdict・failure集計・report順を確認。レポート整数3項目をC# current-culture書式へ統一し、配列/Span添字の例外を`ManagedAt`で.NET `IndexOutOfRangeException`に合わせた。`git diff --check`通過、Windows Release `NetFeatureCheck.cpp.obj` compile green（NOMINMAX再定義・offsetof警告あり）。runtime/harness未実施） |
| M | +23/-40 | `Mods/Network/NetTransport.cs` | .cpp,.hpp | 完了（C#全文・native `.hpp/.cpp`とNetSession/DedicatedServer/NetMaster/NetCheckClient/DemoPlayback/NetLobbyTestの直接接続を照合。UDP初期化、receive/lag worker、drop-oldest inbox、held FIFO、immediate Pong、playback injection、Drain、送受信統計とDispose順が一致。ReceivedPacketの配列/span例外と送信payload超過・二重Dispose時の例外型/内容をC#に合わせた。`git diff --check`通過、Windows Release `NetTransport.cpp.obj` compile green（offsetof警告あり）。runtime/harness未実施） |
| M | +21/-17 | `Mods/Network/NetRoomChange.cs` | .cpp,.hpp | 完了（C#全文・native `.cpp/.hpp`とRoomEntity.LoadRoom/NetHooks/NetPlayerBridgeの直接接続を照合。server room/match polling、unknown map retry、fade guard、slot再構築/occupied flags/reset順、score reset、intro camera reload条件が一致。再構築後のnative二重player初期化・Halfturret未初期化を修正し、IReadOnlyList index例外とintro読込時のcatch/MessageをC#に合わせた。`git diff --check`通過、Windows Release `NetRoomChange.cpp.obj` compile green（offsetof警告のみ）。runtime未実施） |
| M | +21/-29 | `Mods/Network/NetSlotManager.cs` | .cpp,.hpp | 完了（C#全文・native `.cpp/.hpp`とNetHooks/NetSession/NetPlayerLifecycleの直接接続を照合。active session gate、slot occupancy、遅延team/hunter補正、Weapons.Current待ち、flags/Initialize/PlayerCount、再接続slot解放とlifecycle/score cleanup順が一致。nativeの配列・IReadOnlyList添字と`.at()`を`ManagedAt`/`ManagedListAt`へ置換しC#の範囲外例外に統一。`git diff --check`通過、Windows Release `NetSlotManager.cpp.obj` compile green（offsetof警告のみ）。runtime/harness未実施） |
| M | +21/-4 | `Mods/Network/NetStatus.cs` | .cpp,.hpp | 完了（C#全文・native pairと全直接呼出しのprobe/timeout指定を照合。UDP status応答・deadline/latency・legacy JoinProbe・表示内容が一致。一般例外catchとメッセージをC#相当に修正し、未使用NetLaunch bridgeを削除。`git diff --check`・Windows Release `NetStatus.cpp.obj` compile通過。runtime/harness未実施） |
| M | +20/-0 | `Mods/Network/NetDiagnostics.cs` | .cpp,.hpp | 完了（C#全文・native pairとNetHooks/ModEntry直接呼出しを照合。1秒間隔、環境変数lazy cache、slot/team/form/damage診断と表示順が一致。PlayersのIReadOnlyList添字をManagedListAtに統一。`git diff --check`・Windows Release `NetDiagnostics.cpp.obj` compile通過（NOMINMAX/offsetof警告のみ）。runtime未実施） |
| M | +16/-1 | `Mods/Network/NetHostSession.cs` | .cpp,.hpp | 完了（C#全文・native pairとTextLauncher/Gui Shell/Lobby/CreateServerの直接呼出しを照合。server設定、listing、250ms待機、join失敗時のStop、background thread相当のdetachと停止順が一致。thread名設定、Console互換出力、C# `catch (Exception)`相当のcatch-all/例外文を修正。`git diff --check`・Windows Release `NetHostSession.cpp.obj` compile通過（offsetof警告のみ）。runtime未実施） |
| M | +5/-1 | `Mods/Network/DemoPlayback.cs` | .cpp,.hpp | 完了（C#全文・native pairとMatchStart/PlayScreen/DemoInfo/Renderer/NetSessionの直接接続を照合。protocol gate、frame 0からのpacket injection、20秒のmatch探索、roster用120 frame grace、rewind/reset、終端・停止・LastErrorの状態遷移が一致。nativeの`std::cout`分割出力をC# `Console.WriteLine`相当のatomic Console APIへ、protocol byte表示をcurrent-culture formatterへ修正。`git diff --check`・Windows Release `DemoPlayback.cpp.obj` compile通過（既存offsetof警告のみ）。runtime未実施） |
| M | +3/-2 | `Mods/Network/MechanicsDump.cs` | .cpp,.hpp | 完了（C#全文・native pairとModEntryの直接呼出しを照合。11節の呼出し順、静的Markdown文言、武器/ハンター/移動値の列順、enum表示・current-culture数値/小数書式が一致。C# `Console.Write`とUTF-8コンソール出力を揃えるため、nativeの`std::cout.write`を既存`ConsoleWrite`へ変更。`git diff --check`・Windows Release `MechanicsDump.cpp.obj` compile通過（既存offsetof警告のみ）。runtime未実施） |

## 10. MapGen — 11 ファイル (新規 3), C# +2034 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +609/-0 | `Mods/MapGen/MapCheck.cs` | — 新規 | 完了（C#全文対比監査済み、runtime/harness未実施） |
| A | +430/-0 | `Mods/MapGen/CollisionObj.cs` | — 新規 | 完了（C#全文対比監査済み、runtime/harness未実施） |
| A | +369/-0 | `Mods/MapGen/AltFormProbe.cs` | — 新規 | 完了（C#全文対比監査済み、runtime/harness未実施） |
| M | +213/-20 | `Mods/MapGen/Q3Import.cs` | .cpp,.hpp | 完了（C#全文対比監査済み、runtime/harness未実施） |
| M | +120/-0 | `Mods/MapGen/MapDefinition.cs` | .cpp,.hpp | 完了（C#全文対比監査済み、runtime/harness未実施） |
| M | +113/-2 | `Mods/MapGen/MapReport.cs` | .cpp,.hpp | 完了（C#全文対比監査済み、runtime/harness未実施） |
| M | +91/-8 | `Mods/MapGen/Q3Convert.cs` | .cpp,.hpp | 完了（C#全文対比監査済み、runtime/harness未実施） |
| M | +48/-1 | `Mods/MapGen/MapPacker.cs` | .cpp,.hpp | 完了（C#対比監査済、MapPacker/CustomRooms/ModEntry対象object compile green、runtime未実施） |
| M | +19/-0 | `Mods/MapGen/BuiltMap.cs` | .cpp,.hpp | 完了（C#対比監査済、Windows Release native library compile/link green、runtime未実施） |
| M | +19/-0 | `Mods/MapGen/MapBundle.cs` | .cpp,.hpp | 完了（C#対比監査済、Windows Release native library compile/link green、runtime未実施） |
| M | +3/-2 | `Mods/MapGen/CustomRooms.cs` | .cpp,.hpp | 完了（C#対比監査済、Windows Release native library compile/link green、runtime未実施） |

## 11. Launcher portable — 7 ファイル (新規 2), C# +615 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +320/-0 | `Mods/Launcher/Portable/NativeFilePicker.cs` | — 新規 | 完了（C#全文・SetupScreen/PlayScreen/Shell接続監査済、PATH/UTF-8出力/例外境界/COM cleanupを修正、Windows Release native build済。Linux/macOS picker runtime未実施） |
| M | +98/-42 | `Mods/Launcher/Portable/MatchStart.cs` | .cpp,.hpp | 完了（静的監査済み、Section 12 後にビルド） |
| M | +87/-8 | `Mods/Launcher/Portable/LauncherPrefs.cs` | .cpp,.hpp | 完了（C#全文・Load/Save/Directory直接接続監査済、catch(Exception)境界を修正、Windows Release native build済。runtime未実施） |
| A | +67/-0 | `Mods/Launcher/Portable/RomWhitelist.cs` | — 新規 | 完了（C#全文・Program/GameFiles直接呼出し監査済、MD5 file I/OをFile.OpenRead相当へ修正、Windows Release build済。実ROM runtime未実施） |
| M | +24/-7 | `Mods/Launcher/Portable/TextLauncher.cs` | .cpp,.hpp | 完了（C#全文・ModEntry/各設定/ネットワーク/GameFiles/MatchStart直接接続監査済、整数parse overflow・例外stack出力を修正、Windows Release build済。UI runtime未実施） |
| M | +15/-3 | `Mods/Launcher/Portable/GameFiles.cs` | .cpp,.hpp | 完了（C#全文・直接呼出し監査済。POSIX修正のmacOS/Clang・Linux/GCC CI、Windows Release build済。POSIX runtime・抽出runtime未実施） |
| M | +4/-0 | `Mods/Launcher/Portable/LaunchPlan.cs` | .cpp,.hpp | 完了（C#全文・直接呼出し監査済、LobbyContextのinit-only性をnativeにも適用、Windows Release build済。runtime未実施） |

## 12. Launcher GUI — 70 ファイル (新規 50), C# +22132 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +1941/-0 | `Mods/Launcher/Gui/PlayScreen.cs` | .cpp,.hpp | 完了（C#全メソッド再監査・rounding/focus差を修正、StartScreen/InGameMenu/UiCapture接続済。Windows Release build済（2026-09-27）） |
| A | +1529/-0 | `Mods/Launcher/Gui/UiDesigns.cs` | .cpp,.hpp | 完了（C# 原本監査済、`-uidesign`入口をModEntryへ接続） |
| A | +1126/-0 | `Mods/Launcher/Gui/CreateServerScreen.cs` | .cpp,.hpp | 完了（C#監査済、StartScreen・UiCapture接続済。LocalServer installのcancel token伝達を修正） |
| A | +1077/-0 | `Mods/Launcher/Gui/Shell.cs` | .cpp,.hpp | 完了（C# / PR #1原本監査済、`-shellshot`とSection 13 scene API接続済、Windows Release build済） |
| A | +1031/-0 | `Mods/Launcher/Gui/UiSurface.cs` | .cpp,.hpp | 完了（C#監査済） |
| A | +988/-0 | `Mods/Launcher/Gui/StartScreen.cs` | .cpp,.hpp | 完了（C#全文監査済、LobbyScreen・GuiLauncher・UiCapture接続済） |
| A | +921/-0 | `Mods/Launcher/Gui/LobbyScreen.cs` | .cpp,.hpp | 完了（C#全体監査済・verified no-op、StartScreen接続済。Windows Release build済（2026-09-27）） |
| A | +889/-0 | `Mods/Launcher/Gui/DeckTile.cs` | — 新規 | 完了（C#監査済） |
| A | +838/-0 | `Mods/Launcher/Gui/DeckButton.cs` | — 新規 | 完了（C#監査済） |
| A | +819/-0 | `Mods/Launcher/Gui/UiLayout.cs` | — 新規 | 完了（C#監査済） |
| A | +786/-0 | `Mods/Launcher/Gui/HunterStand.cs` | .cpp,.hpp | 完了（C#監査済、LauncherHunter状態・描画APIをSection 13で接続、Windows Release build済） |
| A | +665/-0 | `Mods/Launcher/Gui/UiBench.cs` | .cpp,.hpp | 完了（C#原本監査済、旧Window/GCはnative headlessでの測定差を明記、`-uibench`入口接続） |
| A | +574/-0 | `Mods/Launcher/Gui/UiList.cs` | — 新規 | 完了（C#監査済） |
| A | +535/-0 | `Mods/Launcher/Gui/UiTopLevel.cs` | — 新規 | 完了（C#監査済） |
| M | +493/-387 | `Mods/Launcher/Gui/SettingsView.cs` | .cpp,.hpp | 完了（C#監査済、HunterStand・StartScreen・InGameMenu・UiCapture接続済） |
| M | +491/-142 | `Mods/Launcher/Gui/ServerRow.cs` | .cpp,.hpp | 完了（C#監査済、Opacity 1 group省略・rounded clip先行でnative layer churn縮減。Windows Release buildとserverbrowser SHA一致確認済 2026-09-28） |
| A | +466/-0 | `Mods/Launcher/Gui/DeckChip.cs` | — 新規 | 完了（C#監査済） |
| A | +409/-0 | `Mods/Launcher/Gui/Deck.cs` | — 新規 | 完了（C#監査済） |
| A | +339/-0 | `Mods/Launcher/Gui/SetupScreen.cs` | .cpp,.hpp | 完了（C#監査済、StartScreen・UiCapture接続済） |
| A | +309/-0 | `Mods/Launcher/Gui/EndPanelView.cs` | .cpp,.hpp | 完了（C#全体監査済、空ballotのempty note alignmentを修正。Shell/UiCapture接続済。Windows Release build済（2026-09-27）） |
| A | +270/-0 | `Mods/Launcher/Gui/MovingBackdrop.cs` | — 新規 | 完了（C#監査済） |
| M | +268/-64 | `Mods/Launcher/Gui/UiCapture.cs` | .cpp,.hpp | 完了（C# / PR #1差分監査済、`-uishot`入口をModEntryへ接続） |
| A | +239/-0 | `Mods/Launcher/Gui/GamepadUiChecks.cs` | .cpp,.hpp | 完了（35 assertion を C# と順序照合、GamepadChecks から shell build で接続） |
| A | +227/-0 | `Mods/Launcher/Gui/Flags.cs` | — 新規 | 完了（C#監査済） |
| A | +222/-0 | `Mods/Launcher/Gui/TapCheck.cs` | — 新規 | 完了（C#全文・14ケースの順序/座標/期待値/出力とModEntryの`-tapcheck`接続を監査、Windows Release `-tapcheck` 14/14成功） |
| M | +220/-26 | `Mods/Launcher/Gui/Rows.cs` | .cpp,.hpp | 完了（C#監査済） |
| A | +202/-0 | `Mods/Launcher/Gui/InGameMenu.cs` | .cpp,.hpp | 完了（C#全体・Shell直接接続監査済、verified no-op。Windows Release build済（2026-09-27）） |
| A | +197/-0 | `Mods/Launcher/Gui/UiWord.cs` | — 新規 | 完了（C#監査済） |
| A | +196/-0 | `Mods/Launcher/Gui/BakedBackdrop.cs` | — 新規 | 完了（C#監査済） |
| A | +195/-0 | `Mods/Launcher/Gui/DeckText.cs` | — 新規 | 完了（C#監査済） |
| A | +193/-0 | `Mods/Launcher/Gui/GamepadSettingsPanel.cs` | — 新規 | 完了（C#監査済） |
| A | +181/-0 | `Mods/Launcher/Gui/DeckSide.cs` | — 新規 | 完了（C#監査済） |
| M | +181/-41 | `Mods/Launcher/Gui/PadRow.cs` | .cpp,.hpp | 完了（C#監査済） |
| M | +178/-123 | `Mods/Launcher/Gui/PauseMenuView.cs` | .cpp,.hpp | 完了（C#監査済、StartScreen・InGameMenu・UiCapture接続済） |
| A | +166/-0 | `Mods/Launcher/Gui/Tap.cs` | — 新規 | 完了（C#監査済み: 全文対比。Touch runtime未実施） |
| A | +165/-0 | `Mods/Launcher/Gui/UiMark.cs` | — 新規 | 完了（C#監査済） |
| A | +162/-0 | `Mods/Launcher/Gui/UiScaleHost.cs` | .cpp,.hpp | 完了（C#監査済） |
| A | +159/-0 | `Mods/Launcher/Gui/ServerBadge.cs` | — 新規 | 完了（C#原本・ServerRow/GeoCountry/Flags直接接続監査済、変更したIPAddress runtime APIも.NET 10と一致。Windows Release build済（2026-09-27）） |
| A | +158/-0 | `Mods/Launcher/Gui/DeckCard.cs` | — 新規 | 完了（C#監査済） |
| A | +155/-0 | `Mods/Launcher/Gui/DeckField.cs` | — 新規 | 完了（C#監査済） |
| M | +149/-21 | `Mods/Launcher/Gui/GuiTheme.cs` | .cpp,.hpp | 完了（C#監査済） |
| A | +147/-0 | `Mods/Launcher/Gui/DeckSheet.cs` | — 新規 | 完了（C#監査済） |
| A | +146/-0 | `Mods/Launcher/Gui/UiTabs.cs` | — 新規 | 完了（C#監査済） |
| A | +143/-0 | `Mods/Launcher/Gui/DeckWordmark.cs` | — 新規 | 完了（C#監査済） |
| A | +143/-0 | `Mods/Launcher/Gui/GeoCountry.cs` | — 新規 | 完了（C#監査済） |
| A | +142/-0 | `Mods/Launcher/Gui/MapCardPicker.cs` | .cpp,.hpp | 完了（C#監査済、LobbyScreenから接続済） |
| A | +126/-0 | `Mods/Launcher/Gui/GamepadSetupPanel.cs` | — 新規 | 完了（C#監査済） |
| M | +116/-10 | `Mods/Launcher/Gui/KeyRow.cs` | .cpp,.hpp | 完了（C#監査済） |
| A | +93/-0 | `Mods/Launcher/Gui/FocusNavigator.cs` | — 新規 | 完了（C#監査済） |
| A | +91/-0 | `Mods/Launcher/Gui/GamepadNavigation.cs` | — 新規 | 完了（C#監査済） |
| A | +89/-0 | `Mods/Launcher/Gui/ControllerKeyboard.cs` | — 新規 | 完了（C#監査済） |
| A | +88/-0 | `Mods/Launcher/Gui/MapShot.cs` | — 新規 | 完了（C#監査済） |
| A | +80/-0 | `Mods/Launcher/Gui/GamepadMonitor.cs` | — 新規 | 完了（C#監査済） |
| A | +78/-0 | `Mods/Launcher/Gui/ConfirmScreen.cs` | — 新規 | 完了（C#全文・StartScreen/UiCapture/GamepadUiChecks接続監査済、追加修正なし。Windows Release build済（2026-09-27）） |
| M | +70/-157 | `Mods/Launcher/Gui/GuiLauncher.cs` | .cpp,.hpp | 完了（C#監査・Launcher/PauseMenu・PlatformDiagnostics.Report接続・Windows Release build済） |
| M | +61/-10 | `Mods/Launcher/Gui/SliderRow.cs` | .cpp,.hpp | 完了（C#監査済） |
| A | +58/-0 | `Mods/Launcher/Gui/LobbyPlayerRow.cs` | — 新規 | 完了（C#監査済） |
| A | +51/-0 | `Mods/Launcher/Gui/GamepadProfilePanel.cs` | — 新規 | 完了（C#監査済） |
| A | +44/-0 | `Mods/Launcher/Gui/ControllerNav.cs` | — 新規 | 完了（C#監査済） |
| A | +37/-0 | `Mods/Launcher/Gui/GamepadGlyph.cs` | — 新規 | 完了（C#監査済） |
| D | +0/-151 | `Mods/Launcher/Gui/DemoPickerView.cs` | .cpp,.hpp | 完了（C#削除・native参照なしを確認） |
| D | +0/-2235 | `Mods/Launcher/Gui/HomeView.cs` | .cpp,.hpp | 完了（C#削除・native参照なしを確認） |
| D | +0/-41 | `Mods/Launcher/Gui/HomeWindow.cs` | .cpp,.hpp | 完了（C#削除・native参照なしを確認） |
| D | +0/-280 | `Mods/Launcher/Gui/MapPickerView.cs` | .cpp,.hpp | 完了（C#削除・native参照なしを確認） |
| D | +0/-317 | `Mods/Launcher/Gui/MenuEntry.cs` | .cpp,.hpp | 完了（C#削除・native参照なしを確認） |
| D | +0/-372 | `Mods/Launcher/Gui/PauseMenuWindow.cs` | .cpp,.hpp | 完了（C#削除を確認、旧PauseMenu参照も除去） |
| D | +0/-78 | `Mods/Launcher/Gui/SettingsWindow.cs` | .cpp,.hpp | 完了（C#削除・native参照なしを確認） |
| D | +0/-239 | `Mods/Launcher/Gui/SplashView.cs` | .cpp,.hpp | 完了（C#削除・native参照なしを確認） |
| D | +0/-157 | `Mods/Launcher/Gui/UpdateBadge.cs` | .cpp,.hpp | 完了（C#削除・native参照なしを確認） |
| M | +20/-62 | `Mods/PauseMenu.cs` | .cpp,.hpp | 完了（C#全体・Renderer/Shell直接接続監査済、catch範囲を修正。Windows Release build済（2026-09-27）） |

## 13. Engine and entities — 34 ファイル (新規 0), C# +3279 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| M | +1050/-99 | `Renderer.cs` | .cpp,.hpp | 完了（C#全文・直接呼出元再照合済、Scene.ShowCursorのWeaponWheel.Absolute条件漏れを修正してマウスホイール中のcursor captureを一致。Windows Release native library build済、runtime未実施） |
| M | +872/-49 | `Mods/ModEntry.cs` | .cpp,.hpp | 完了（C#全分岐・引数照合、`teamprobe`/network/MapGen/server/launcher配線、Windows Release build済） |
| M | +376/-94 | `Entities/Players/PlayerHud.cs` | .cpp,.hpp | 完了（C#監査済み: 全文・PR #1差分全hunk。team node countのnative範囲外アクセスをC#例外相当に修正。Windows Release build green、runtime未実施） |
| M | +185/-40 | `Entities/Players/PlayerInput.cs` | .cpp,.hpp | 完了（C#監査済み: PR #1差分全hunk・PlayerInput/NetSession/入力補助の直接接続と配列境界。BombSpawn診断counterもC# unchecked `++`相当のwrapに修正。runtime未実施） |
| M | +102/-126 | `GameState.cs` | .cpp,.hpp | 完了（C# PR #1差分監査・PlayPickedMap接続・Windows Release build済、runtime未実施） |
| M | +95/-11 | `Entities/Players/PlayerAi.cs` | .cpp,.hpp | 完了（C# PR #1差分監査・Insane AI移植・Windows Release build済、runtime未実施） |
| M | +71/-3 | `Formats/Formats.cs` | .cpp,.hpp | 完了（C# PR #1差分監査・Paths/Span対応・Windows Release build済） |
| M | +66/-33 | `Entities/BeamProjectileEntity.cs` | .cpp,.hpp | 完了（PR #1のC#差分全hunkとnative `.cpp/.hpp`、Spawn/ricochet・TeamRules・ContinuousWeaponPhase・NetPlayerLifecycle直接接続を照合。ModLaunchKeyのinternal setterをNetPlayerLifecycle限定に修正。Windows Release全体 `ninja -k 0` 成功、runtime未実施） |
| M | +54/-0 | `Shaders.cs` | .cpp,.hpp | 完了（LauncherPhoto依存の2 shaderをC#と完全一致照合） |
| M | +48/-6 | `Entities/Players/PlayerEntity.cs` | .cpp,.hpp | 完了（C# PR #1差分監査済、Windows Release build済） |
| M | +45/-12 | `Read.cs` | .cpp,.hpp | 完了（C# PR #1差分監査・memory archive展開/診断・Windows Release build済、runtime未実施） |
| M | +40/-6 | `Entities/Players/PlayerProcess.cs` | .cpp,.hpp | 完了（C#監査済み: PR #1差分全hunk・Respawn/NetHealthSync/ItemInstanceEntity/Spire直接接続と配列境界。修正不要。runtime未実施） |
| M | +40/-1 | `Entities/ItemSpawnEntity.cs` | .cpp,.hpp | 完了（C#監査済み: PR #1差分全hunk・NetHealthSync/PlayerProcess/ItemInstanceEntityの直接接続と配列境界。修正不要。runtime未実施） |
| M | +37/-8 | `Entities/BombEntity.cs` | .cpp,.hpp | 完了（C#監査済み: PR #1差分全hunk・PlayerEntity/TeamRules/LockjawTrailNoise直接接続と配列境界。監査counterのunchecked wrap差を修正。Windows Release build済、runtime未実施） |
| M | +36/-2 | `Program.cs` | .cpp,.hpp | 完了（C#監査済み: PR #1差分全hunk・CrashReport/ConsoleSetup/RomWhitelist直接接続と引数境界。unknown exceptionも報告するようcatch範囲を修正。Windows Release build済、runtime未実施） |
| M | +36/-2 | `Utility/Console.cs` | .cpp,.hpp | 完了（C#監査済み: Run/LaunchDirectory/PauseIfInteractiveとProgram/Extractの直接呼出し。AppPaths移行・入力redirect時の終了動作・Windows VT設定が一致。修正不要。runtime未実施） |
| M | +30/-1 | `Entities/Players/PlayerCollision.cs` | .cpp,.hpp | 完了（PR #1差分を全hunk照合。衝突補正stepをAlt/Biped radiusで`Math.Clamp`する式・係数・Y更新はnative `std::clamp`と一致。PlayerInputからの呼出し順も一致。Windows Release全体 `ninja -k 0` 成功、altprobe runtime未実施） |
| M | +26/-20 | `Entities/NodeDefenseEntity.cs` | .cpp,.hpp | 完了（C#全文・PR #1差分（+26/-20）とnative `.hpp/.cpp`、PlayerHud/PlayerAi/MapAuditTeams/TeamGameplayTestの直接参照を照合。NoTeam=-1、Active/aliveと有効team indexの条件、全slot走査、HUDのsentinel伝播が一致。差分修正なし、Windows Release build no work to do。runtime未実施） |
| M | +19/-1 | `Features.cs` | .cpp,.hpp | 完了（C#差分監査・Radar設定のLoad/Commit・Windows Release build済） |
| M | +11/-3 | `SceneSetup.cs` | .cpp,.hpp | 完了（C#差分監査・resource profile/health reset接続・Windows Release build済） |
| M | +6/-2 | `Mods/Credits.cs` | .cpp,.hpp | 完了（C#全文・PR #1差分（+6/-2）とnative `.hpp/.cpp`、ModEntryの`-credits`、SettingsView/StartScreen/TextLauncher参照を照合。全13 entry、Summary/Compact/Names、support URL、表示文字列と順序が一致。raw `std::cout`出力をC#互換`ConsoleWriteLine`へ修正し、Windows Release compile/link成功。実runtime未実施） |
| M | +6/-1 | `Mods/DebugLog.cs` | .cpp,.hpp | 完了（C#監査済み: PR #1差分・例外本文/stack trace/InnerExceptionの出力順を照合。AndroidにAPI 24対応の最大64フレーム取得を追加。Windows Release、Android arm64-v8a/x86_64 build成功。runtime未実施） |
| M | +6/-2 | `Utility/Archive.cs` | .cpp,.hpp | 完了（C#差分監査・byte span Extract overload・Windows Release build済） |
| M | +5/-3 | `Metadata/Metadata.cs` | .cpp,.hpp | 完了（C#差分監査・entity layer/4チーム色・Windows Release build済） |
| M | +4/-3 | `Utility/Extract.cs` | .cpp,.hpp | 完了（C#差分監査・相対root/interactive pause・Windows Release build済） |
| M | +3/-1 | `Sound/Sfx.cs` | .cpp,.hpp | 完了（C#差分監査・PlatformDiagnostics.Report接続・Windows Release build済） |
| M | +2/-2 | `Entities/Enemies/18_AlimbicTurret.cs` | .cpp,.hpp | 完了（C#全文・PR #1差分（+2/-2）とnative `.hpp/.cpp`、EnemySpawnEntity factory、MetadataのBehavior00/02登録を照合。両target filterでBountyTeamsかつ2 teamsの時だけteam 0を除外し、3/4 teamsでは含める条件が一致。残りのstate/shot/draw/valueも一致。修正なし、Windows Release `ninja -k 0` no work to do。runtime未実施） |
| M | +2/-2 | `Entities/ItemInstanceEntity.cs` | .cpp,.hpp | 完了（C#全文・native `.hpp/.cpp`・直接呼出しを照合。optional pickerを`Owner::OnItemPickedUp`へ渡す変更、PlayerProcessの`this`、HealthSimulationTestのnull既定値、回収者slot記録が一致。差異なし、Windows Release `ninja -k 0` no work to do。runtime未実施） |
| M | +2/-1 | `Entities/Players/HalfturretEntity.cs` | .cpp,.hpp | 完了（C#全文・native `.hpp/.cpp`・`TeamRules::AreAllies`を照合。target選別のself/health/allied team/alpha条件が一致し、TeamRules自体のC#/C++も同じ。`61 * uint damage`はC#数値昇格でlongとなるためnativeの64-bit計算と一致。差異なし、Windows Release `ninja -k 0` 成功） |
| M | +1/-9 | `Entities/Players/PlayerDraw.cs` | .cpp,.hpp | 完了（C#全文・native `.hpp/.cpp`・Spire alt-attackのsimulation/draw接続を照合。Drawは`AnimateSpireAltAttack`、simulationは両側で`UpdateSpireAltCollisionPose`から同じアニメーションとrock衝突位置更新を実行。描画・LOD・shadow・trail・死亡particle・double-damage texture matrixの対応も一致。差異なし、Windows Release `ninja -k 0` no work to do。runtime未実施） |
| M | +1/-0 | `Entities/Players/PlayerSound.cs` | .cpp,.hpp | 完了（C#差分監査済、着地feedbackの条件・種別・SFX順が一致） |
| M | +1/-0 | `Menu.cs` | .cpp,.hpp | 完了（C#差分監査・FieldOfView既定値の既反映を確認） |
| M | +1/-1 | `Scene.cs` | .cpp,.hpp | 完了（C#差分監査・FlagBase iterator修正・Windows Release build済） |
| M | +1/-0 | `Sound/Music.cs` | .cpp,.hpp | 完了（C#差分監査・PlatformDiagnostics.Report接続・Windows Release build済） |

## 14. Android head — 17 ファイル (新規 7), C# +1735 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +299/-0 | `(android) AndroidHunterShot.cs` | `.cpp,.hpp` | 完了（C# / PR #1監査済。install・MainActivity Retire配線。arm64-v8a / x86_64 build済） |
| A | +271/-0 | `(android) AndroidUiSurface.cs` | `.cpp,.hpp` | 完了（C#原本監査済。dispatcher・MainActivity UI tick・GameView描画・TouchOverlayView入力を接続。両ABI build済） |
| A | +257/-0 | `(android) AndroidUiOverlay.cs` | `.cpp,.hpp` | 完了（C#原本監査済。GameView render/preview配線済、両ABI build済） |
| M | +246/-131 | `(android) MainActivity.cs` | .cpp,.hpp | 完了（C#原本監査済。preview停止、hunter retire、end-panel/lobby保持、StartScreen lifecycle参照、UI tickとTouchOverlayView touch routing接続済。両ABI build済） |
| A | +167/-0 | `(android) MainApplication.cs` | `MainActivity.cpp` builder seam | 完了（C#原本監査済。WebLink/HunterShot登録、Application Context、root選択を反映。両ABI build済） |
| M | +112/-2 | `(android) GameView.cs` | .cpp,.hpp | 完了（C#原本監査済。session/load/ui合成/chat cancel差分を修正。MainActivity UI tick/touch配線済。両ABI build済） |
| M | +98/-127 | `(android) GamepadBridge.cs` | .cpp,.hpp | 完了（C#原本監査・MainActivity listener/lifecycle配線済。両ABI build済） |
| M | +55/-22 | `(android) AndroidThumbnails.cs` | .cpp,.hpp | 完了（C#原本監査済。Activity遅延参照、worker上限と60秒進捗watchを修正。MainApplication登録・両ABI build済） |
| M | +54/-0 | `(android) TouchOverlayView.cs` | `.cpp,.hpp` | 完了（C#原本/PR #1監査済。end-panel描画とpointer 0のUI touch routingを修正。両ABI build済） |
| A | +52/-0 | `(android) AndroidWebLink.cs` | `.cpp,.hpp` | 完了（C# / PR #1監査済。MainApplication登録、両ABI build済） |
| M | +47/-9 | `(android) AndroidApp.cs` | .cpp,.hpp | 完了（C#原本監査済。Activity/SingleView lifetime、UiScaleHost、crash handler、Done/MatchRequestedを接続。両ABI build済） |
| A | +39/-0 | `(android) AndroidGamepadHaptics.cs` | .cpp,.hpp | 完了（C#原本監査・GamepadBridge登録接続済。両ABI build済） |
| A | +22/-0 | `(android) AndroidGamepadProfile.cs` | .cpp,.hpp | 完了（C#原本監査済、GamepadBridgeへaxis選択を接続、両ABI build済） |
| M | +11/-14 | `(android) AndroidUpdateInstaller.cs` | `.cpp,.hpp` | 完了（C#原本監査済。Activityを保持せず各操作時に `MainActivity.Instance` を再取得。両ABI build済） |
| M | +2/-3 | `(android) AndroidLogShare.cs` | .cpp,.hpp | 完了（C#原本監査済。cache cleanup・FileProvider chooser・exception報告を照合、修正なし。両ABI build済） |
| M | +2/-1 | `(android) AndroidMatch.cs` | .cpp,.hpp | 完了（C#原本監査済。cheat無効化・bot skill上限差を修正。両ABI build済） |
| M | +1/-0 | `(android) TouchControls.cs` | .cpp,.hpp | 完了（C#原本監査済。TakeAimDeltaのTouch入力源通知を追加。両ABI build済） |

### 2026-09-27 checkpoint gates

- Section 12 launcher GUI は commit `12a7433b`、Section 2 diagnostics は `a535e260` で `develop2` にpush済み。
- Section 13 の34ファイルは移植・配線済みで、Windows Release buildのcompile/linkも完了。2026-09-28に行別の監査記録を照合した結果、当時の「34ファイル監査完了」という一括記録だけでは8行の個別C#監査を確認できなかった。BeamProjectileEntity、PlayerCollision、NodeDefenseEntity、Credits、18_AlimbicTurretを今回再監査し、残る3行は表で監査待ち。Section 13全体の監査完了とは扱わない。
- Androidは17ファイルすべてのC#監査・MainActivity/AppBuilder/Renderer呼び出し配線後にarm64-v8aとx86_64をbuildした。
  両ABIともstatic library link成功（`build/native-android-arm64-v8a/build-retry10.log`、
  `build/native-android-x86_64/build-retry10.log`）。端末・実機runtimeは未実施。
- この時点でAndroid側のソース更新は両ABI buildの後にない。Windows/macOSの画面runtimeとmacOS native buildは未検証。

### 2026-09-27 complete-only audit

- `Mods/Multiplayer/MatchWorldProfile.cs` をC#全文とnative `.cpp/.hpp` で照合。enum byte値、default struct値、
  `IsValid` のplayer/resource組合せとEnum.IsDefined相当、configured player数の2〜8 clamp、entity layer上限4、
  Low/Standard/Highの境界を確認した。差分修正なし。
- `Mods/Multiplayer/TeamVisuals.cs` をC#全文とnative `.cpp/.hpp` で照合。4 teamとneutralのlabel/RGBA/radar color/
  model team/recolor、負値・範囲外indexのneutral fallback、Apply時のteam設定と既存recolor保持/解除条件が一致。
  nativeの返却参照はstatic const tableへ限られ、呼び出し側はC# record structの値と同じ観測値。差分修正なし。
- `Mods/Multiplayer/TeamLayout.cs` をC#全文とnative `.cpp/.hpp` および`NetLobbyTest`/serverの直接呼び出しで照合。
  layout field/default、capacity、validation、ToString、allies、normalized occupancy/tie-break順は一致。
  span長不足時のC#例外をnativeでも`ManagedAt`で再現し、C#通常unchecked積を`UncheckedMultiply`へ変更した。
- `Mods/Multiplayer/TeamGameplayTest.cs` をC#全文とnative `.cpp/.hpp` で比較。初期化/全assertionの順、team standingsとtie、
  Survival勝者・FFA、PlayerEntity全slotの準備とfinally時の全state復元が一致。nativeのcatch/rethrowはfinally cleanup相当。
  差分修正なし。
- `Mods/Multiplayer/GameStateTeams.cs` をC#全文とnative `.cpp/.hpp` で照合。tie判定、active/team範囲filter、team/member sortの
  tie-break、represented teamからの順位計算、FFA rankと空配列初期化が一致。C# `stackalloc bool[4]` はinitializerなしで内容が
  未規定のため、nativeは未初期化読取を避けてゼロ初期化を維持。`represented`の両アクセスを`ManagedAt`にし範囲外例外も対応。
- `Mods/Multiplayer/MapResourceRules.cs` をC#全文とnative `.cpp/.hpp` で照合。27部屋の候補IDと順序、Health判定、profile別の
  Transfer Lock例外、元list複製、既存ID/親/Enabled/重複距離の条件、source順追加、spawn intervalの300上限と72-byte書換を確認。
  RoomMetadataの早期条件後に辞書lookupするC#の評価順へnativeを変更。適用対象でnullのoriginalはC# `List<Entity>` と同じ
  `ArgumentNullException("collection")` にし、未定義動作を除去。差分修正以外はなし。
- `Mods/Multiplayer/ResourceAudit.cs` をC#全文とnative `.cpp/.hpp` で照合。18シナリオのlabel/mode/player数/順、room/layer走査、healthと
  objective集計、重複・安定fingerprint検査、列出力と最終summaryが一致。距離集計のC# `Math.Min/Max` はNaNを伝播させるため、
  nativeの`std::min/max`を既存のC#互換`MathMin/MathMax`へ変更。差分以外の修正なし。
- `Mods/Input/PointerCheck.cs` をC#全文とnative `.cpp/.hpp` で照合。binding/movement/zone/player-input/settings/Win32 signatureの
  検査順とassertion内容は一致。C#が`RuntimeHelpers.GetUninitializedObject`で作るSceneに合わせ、通常Scene ctorのcache/GameState/Music初期化を
  行わないprivate test fixture ctorを追加し、Movie indexはC#の`-1`状態を保った。`Run`のcatch-all error formattingを`ExceptionToString`へ、
  PointerDevice cleanupをRAIIへ変更してC# catch/finally相当を合わせた。nativeの例外文字列はtype/messageを出し、CLR stack traceは持たない。
- `Mods/Input/PadBindingState.cs` をC#全文とnative `.cpp/.hpp` および直接呼び出しで照合。23個のdefault bindingsと23個のActionOrder、
  Set/SetSlot/LoadSlots、modifier chord、Evaluate/ChordButtons、conflict resolution、preset/clone/reset、text/setting keyの順序と値が一致。
  invalid action/slotのnative例外をC#配列と同じ`IndexOutOfRangeException`にし、SetSlotの引数検査順を修正。`Revision++`をC#のunchecked
  `long` wrapと同じ`IncrementInPlace`に変更。直接呼び出しのslotは0または1。
- `Mods/Input/WindowsPenInput.cs` をC#全文とnative `.cpp/.hpp`、`Renderer.cs`/`Renderer.cpp`のAttach/Read呼び出しで照合。
  Win32 message分岐、promoted mouse判定、接触/hover/release状態、座標scale、pen pressure/tilt、GLFW fallbackの内容と順序が一致。
  C# P/Invokeで欠落APIが例外になる箇所をnativeのnull関数ポインター呼出しにせず、`DllNotFoundException`/
  `EntryPointNotFoundException`相当のログ/fallbackへ変更。window callbackのcatchをcatch-allにし、現在例外のMessageを記録する。
  差分修正以外はなし。実機ペン入力runtimeは未実施。
- `Mods/Input/StylusZone.cs` をC#全文とnative `.cpp/.hpp`、renderer/settings/player-input/HUDの使用箇所で照合。
  状態遷移、ボタン定義と順序、配置ドラッグ、nudge/resize、capture/aim条件、遷移ログが一致。
  `SetRect`/`PlacementDrag`/`Nudge`の上限計算とドラッグ始点計算をC# `Math.Max/Min`同様のNaN伝播をする既存helperへ変更。
  差分修正以外はなし。実機タブレット入力runtimeは未実施。
- `Mods/Input/MouseFlick.cs` と対応する`PlayerEntityMouseFlick.cs`呼び出しをC#全文/native `.cpp/.hpp`で照合。
  sample ring、frame gap reset、rest arm、backward coherent burst、sensitivity閾値、重み付き方向、cooldownとlog値、およびmain-player/bot/input gatesと出力代入が一致。
  `Fired++`をC#既定unchecked時のwrapと同じnative `IncrementInPlace`に変更。差分修正以外はなし。
- `Mods/Input/GamepadManager.cs` をC#全文とnative `.cpp/.hpp`、状態・通知の直接使用箇所で照合。lock範囲とevent順、device追加/削除、
  selection fallback、active切替、profile publish、raw/calibrated state、trigger hysteresis、activity検出、snapshot/device revisionの更新順が一致。
  C# `long` revisionsの加算をunchecked wrapに合わせ、`Action<GamepadDeviceSnapshot>`相当の追加/削除eventを値渡しに変更。
  差分修正以外はなし。実機コントローラーruntimeは未実施。
- `Mods/Input/GamepadProfiles.cs` をC#全文とnative `.cpp/.hpp`、settings/profile UI・managerの直接使用箇所で照合。
  file size/count制限、profile validation、runtime構築、適用/保存/読込/import/export、assign/unassign、device key、atomic writeの順序を確認。
  .NET `string.Length` と `char.IsControl`に合わせ、name/line/keyをUTF-16単位で検査し、C1制御文字も拒否、import名の40-unit切詰めを修正。
  JSON `Version`はInt32範囲・整数表現で読み、revisionをunchecked wrapにした。差分修正以外はなし。
- `Mods/Input/GamepadEnhancementChecks.cs` をC#全文とnative `.cpp/.hpp`で照合。assertionの順序・条件・対象・メッセージ、synthetic calibration/mapping、実際のPlayerControls keybind、profile import/assign/cleanupの流れが一致。
  `GamepadProbe.Actions`は入力bindingを読むだけの処理であり、C#の2回評価とnativeの1回キャッシュによる結果差はない。native差分修正なし。Windows Release build済、check harness自体は未実行。
- `Mods/Input/WeaponWheel.cs` をC#全文とnative `.cpp/.hpp`、HUDの直接呼出元で照合。absolute-device判定、drag開始/close時の初期化、step既定値、累積移動と離散step、未所持武器のskip、端での停止、範囲外availabilityの拒否が一致。
  呼出元のcurrent slotは`-1..5`、availabilityは6要素配列で、native spanの受け渡しも対応。native差分修正なし。Windows Release build済。
- `Mods/Input/GamepadOptionState.cs` をC#全文とnative `.cpp/.hpp`で照合。field defaults、重複keyの後勝ち、culture/invariant parseの使い分け、finite/range fallback、legacy keys、enum/wheel-order validation、全fieldのWrite順、Clone/Resetを確認。
  native差分修正なし。Windows Release build済。
- `Mods/Input/GamepadUiRouter.cs` をC#全文とnative `.cpp/.hpp`で照合。context flags/revision、押下edge、analog trigger hysteresis、context切替・未接続時のneutral barrier、direction優先順とrepeat cadence、Accept/Back/tab/page action順を確認。
  repeat開始/次回時刻の加算と長押し時間差をC#既定unchecked `long`演算に合わせ、native signed overflowの未定義動作を解消。Windows Release build済。
- `Mods/Input/AimAssist/AimAssistWorld.cs` をC#全文とnative `.cpp/.hpp`、`PlayerEntityNetAim`直接呼出元で照合。state reset条件、eligibility/observation、武器profile、対象slot順/絞込/LOS、body/head geometry、assist・debug・telemetry出力を確認。
  C#の`default(AimAssistTarget)`は全field zeroであるため、nativeのmember defaultsによる`Eligible=true`/`UpperChest`を明示的なzero stateへ修正。PointerとStickに渡すtick countもC#同様に個別取得。Windows Release build済。
- `Mods/Input/PointerDevice.cs` をC#全文とnative `.cpp/.hpp`、Renderer/PlayerInputの直接呼出元で照合。active/accepting遷移、device identity/contact切替、aspect/座標正規化、StylusZone通知、primary/capture判定、pointer delta蓄積/消費とmouse fallback、Mouse Left binding edgesが一致。
  `PointerSample` defaultsとCurrent/PrimaryDownの利用方法も確認。native差分修正なし。Windows Release build済。
- `Mods/Input/ControllerRuntimeChecks.cs` をC#全文とnative `.cpp/.hpp`で照合。device-specific calibration/runtime切替、frame snapshot隔離、manager event再入、profile validation/library保護、haptic arbitration、mapping置換、UI trigger hysteresis、layout identity/promptのassertion順と条件が一致。
  event再入確認をC#のtimeout後も戻る`Task.Wait(1000)`に合わせ、timeout時にfuture破棄でblockする`std::async`をdetached packaged taskへ変更。Windows Release build済、check harness自体は未実行。
- `Mods/Input/GamepadMappings.cs` をC#全文とnative `.cpp/.hpp`で照合。resource/settings/environmentの読込優先順、override置換、platform filter、GLFWへの一括適用、capability解析、summaryとsuggestion出力を確認。
  C# `string.Length` とnative UTF-8 byte長の差が出るmapping上限/GUID判定を既存`Utf16Length`へ合わせ、C# unchecked `int` のfiles/lines集計をwrap演算へ変更。差分修正以外はなし。Windows Release build済。
- `Mods/Input/AimAssist/AimAssistTelemetry.cs` をC#全文とnative `.cpp/.hpp`、`AimAssistWorld`/`PlayerEntityHaptics`/`PlayerEntity`/`ModEntry`の呼出元で照合。opt-in・authority/player/spectator filter、weapon/input/range bucket、shot/hit対応、全統計値、process-exit保存、JSON項目と配列順を確認。
  `Shots`/`HitEvents`/`Samples`/`TargetSamples`/`Switches`と`ObservedDamage`の加算をC# unchecked wrapに合わせ、native signed overflowを解消。Windows Release build済。
- `Mods/Input/GamepadMappingWizard.cs` をC#全文とnative `.cpp/.hpp`、`GamepadDesktop`/`GamepadSetupPanel`/`GamepadEnhancementChecks`の呼出元で照合。20-step順、device/shape検証、release-to-rest、button/hat/axisの検出優先と閾値、重複排除、GUID/platformとmapping形式を確認。
  C# `char.IsControl`/`Take(100)`がUTF-16 code unit単位である点に合わせ、Unicode control除外とname切詰めをUTF-16経由へ修正。Windows Release build済。
- `Mods/Input/GamepadInput.cs` をC#全文とnative `.cpp/.hpp`、desktopの`Renderer`入出力呼出順で照合。snapshot/runtime反映、edge/reset/block処理、focus/disconnect、aim、press消費、移動・全binding合成、weapon wheel/last-weapon、keybind統合順が一致し、native差分修正なし。
  Android側は`GameView`から`TakePress`/`TakeMenuPress`を呼ぶ一方、`src/MphRead.Android`内に`GamepadInput.BeginFrame`呼出しが見つからなかったため、Android統合の別監査項目として記録。Android buildは実行していない。
- `Mods/Input/GamepadDesktop.cs` をC#全文とnative `.cpp/.hpp`、`Renderer`/`UiSurface`/`GamepadProbe`呼出元で照合。GLFW slot走査、mapped/raw read、軸変換、button indices、capability/name/family、wizard snapshot、haptics同期が一致。
  C# `OnJoystickConnected`がconnect/disconnect両通知で`DeviceChanged`を呼ぶ点に対しnativeの通知配線がなかったため、対象のGLFW callback APIだけを追加して同じcleanupを接続。slot generationの加算もC# unchecked wrapに合わせた。Windows Release build済。
- `Mods/Input/WindowsGamepadHaptics.cs` をC#全文とnative `.cpp/.hpp`、`GamepadHaptics`/`GamepadDesktop`の呼出元で照合。XInput struct layout、接続indexの一意判定、登録・解除・dispose順、finite振幅、1〜500 msのoneshot停止とtimer競合時の順序を確認。
  `XInputGetState`のみ存在して`XInputSetState`が欠落する環境でC# P/Invokeは例外になるがnativeが黙って振動を捨てていた差を、`System::EntryPointNotFoundException`で合わせた。Windows Release build済。
- `Mods/Input/AimAssist/AimAssistChecks.cs` をC#全文とnative `.cpp/.hpp`、`GamepadChecks`からの接続で照合。23個の判定、状態の準備・変更順、30/120 Hz比較、入力ソース切替の時刻と期待値が一致し、差分修正なし。C#のGC割当計測はmanaged heap専用のためnativeでは実行できないが、`AimAssist::Apply`と呼出先を見てspan/value演算だけで割当経路がないことを確認した。Windows Release build済。
- `Mods/Input/GamepadPlatformChecks.cs` をC#全文とnative `.cpp/.hpp`、`GamepadChecks.Run`内の二箇所の接続順で照合。macOS Xbox Bluetooth fixtureの軸/トリガー/10物理button/diagonal hat、mapping許可・拒否、Linux/generic fallback、4 preset、secondary slotを含むconflict swapの全34 assertionとreset順が一致。修正なし。Windows Release build済、check harness未実行。
- `Mods/Input/AimAssist/AimAssist.cs` をC#全文とnative `.cpp/.hpp`、`PlayerEntity::ApplyControllerAssist`/`AimAssistWorld`の直接呼出元で照合。invalid入力/eligibility時のreset、intent閾値、target走査順・retain/challenger hysteresis、角速度補償、head delay/blend、距離/inner cone friction、opposition、rotation clamp、result scoreまで同式・同順序で、差分修正なし。Windows Release build済。
- `Mods/Input/GamepadLayout.cs` をC#全文とnative `.cpp/.hpp`、`GamepadDesktop` raw-readおよび`GamepadMappings`の選択/compatibility呼出元で照合。Xbox/flat/macOS Bluetooth各index、GUID/形状条件、axis-capability、finite軸/trigger floor、Y反転、button・hat bit mappingと境界処理が一致。修正なし。Windows Release build済。
- `Mods/Input/GamepadAnalog.cs` をC#全文とnative `.cpp/.hpp`、`GamepadInput`/Manager/Layout/Calibration/Haptics/Checks呼出元で照合。finite clamp、radial deadzone、4 response curve、trigger hysteresis、8方向quantize、curve enum parse/format、key＋motion button合成が一致。quantizeのnearbyintはC#のties-to-evenと同じ既定rounding modeで、repoにmode変更がないことも確認。修正なし。Windows Release build済、gamepadcheck未実行。
- `Mods/Input/GamepadCalibration.cs` をC#全文とnative `.cpp/.hpp`、`GamepadSetupPanel`のRawState採取・`GamepadManager`のtrigger変換・`GamepadEnhancementChecks`の接続で照合。2048件上限、dirty cache、rest/range各10件条件、NaN先頭のfloat percentile、左右stick range/center判定、deadzone、trigger min/maxの0.4幅条件、Apply順、Summaryのcurrent-culture書式が一致。修正なし。Windows Release build済、check harness未実行。
- `Mods/Input/GamepadGlyphs.cs` をC#全文とnative `.cpp/.hpp`、`GamepadDesktop`/`GamepadManager`/`PadBindingState`/`InputPrompt`/launcher glyph viewの呼出元で照合。vendor ID優先順、GUID offset、name token順、設定family→device family→genericの選択、PlayStation/Nintendo remap、fallback labelsとflags `ToString`が一致。修正なし。Windows Release build済。
- `Mods/Input/InputPrompt.cs` をC#全文とnative `.cpp/.hpp`、`GamepadUiRouter::ToString`/`PadBindings`およびStartScreen/ControllerRuntimeChecksの呼出元で照合。UiActionのbutton mappingと未知値の数値Label、PadActionのprimary未割当時のsecondary選択、modifier/glyph/ToStringが一致。C# `readonly record struct` に対してnativeが公開可変fieldだった点と、default structの`Label == null`をprivate getter/optionalへ修正し、文字列連結時はC#同様空文字として扱う。
- `Mods/Input/InputSourceTracker.cs` をC#全文とnative `.cpp/.hpp`、Rendererのmouse/key入力・GamepadManagerのactivity通知・menu/HUDの読み取り元で照合。初期値、Reset、180msの切替抑制、同一source時の早期returnと通知位置が一致。C#既定uncheckedの`milliseconds - _changed`をnativeで直接計算していたため、境界値での符号付きoverflow未定義動作を`UncheckedSubtract`へ置換した。
- `Mods/Input/AimAssist/AimAssistMath.cs` をC#全文とnative inline `.hpp`、`AimAssist`/`AimAssistWorld`の呼び出し元で照合。Smoothの割算・clamp・式順、Vector2 finite判定、Oppositionの符号判定、Scoreの係数とclamp順が一致。NaN/InfもC# `Math.Clamp`とnative `std::clamp`で伝播・飽和が一致し、差分修正なし。
- `Mods/Input/ControllerLayoutState.cs` をC#全文とnative `.cpp/.hpp`、GamepadRuntimeConfigの生成・GamepadOptions/PadBindingsの委譲・ControllerRuntimeChecksの参照で照合。Bindingsの共有参照、Preset名、Southpaw setterのoptions→Custom preset順、Apply時のpreset→Southpaw更新と`Custom`保持条件が一致。差分修正なし。
- `Mods/Input/StickCalibration.cs` をC#全文とnative inline `.hpp`、GamepadCalibration/OptionState/GamepadMonitor/EnhancementChecksの直接使用箇所で照合。6値の順序/default、readonly record、Normalizeの方向ごとの分母/clamp、等値とNaNを確認。nativeの公開可変fieldと既定float比較はC# recordと異なるためprivate getter化・NaN同士を等値化し、`std::max`ではNaNを捨てるため正規化をC# `Math.Max`互換helperへ変更した。全callerを読み取りproperty相当に更新。Windows Release build成功。
- `Mods/Input/AimAssist/AimAssistState.cs` をC#全文とnative inline `.hpp`、`AimAssist::Apply`/`AimAssistWorld`/`AimAssistChecks`の参照経路で照合。TargetSlot/TargetLifeと各float/Vector2 fieldの型・初期値、同じstate objectを連続Applyへ渡す方法、Resetの代入値・順序が一致。native実呼出しにstateの値コピーはなく、差分修正なし。
- `Mods/Input/AimAssist/AimAssistTarget.cs` をC#全文とnative `.cpp/.hpp`、AimAssist/AimAssistWorld/Debug/Telemetry/Checksおよび`PlayerEntityNetAim`の直接使用箇所で照合。default値とpositional constructorのoptional defaultを分離し、readonly record propertiesをprivate storage + getterへ変更。C# `float.Equals` / `Vector2.Equals` に合わせNaN同士の等値を実装し、`with`更新を値の再生成に置換。未定義enum値の`ToString`もC#同様に数値化。Windows Release build済。
- `Mods/Input/PointerInput.cs` をC#全文とnative `.cpp/.hpp`、PointerDevice/Renderer/PlayerInput/PointerCheckの呼出元で照合。設定既定値、jump閾値・whole-sample rejection、current-cultureログ書式、初回ログ、Reset範囲が一致。C#通常uncheckedの`JumpsIgnored++`に対してnative signed overflowが未定義動作となるため`IncrementInPlace`へ修正。Windows Release build済。
- `Mods/Render/PlayerEntityMapPick.cs` をC#全文とnative `.cpp/.hpp`、`PlayerEntityEndScreen`/`MapPick`/`MapThumbnail`の直接接続で照合。panel計算、描画順、色・文字列・投票count、scrollbar、thumbnail呼出し、4 hit領域のpublishが一致。文字名の切詰めはC# string.Length/RangeのUTF-16 code unit単位、`Math.Min/Max/Clamp`はNaN伝播を含む.NET helperへ変更し、UTF-8 byte切詰めと`std::min/max/clamp`との差を修正。Windows Release build済。
- `Mods/Render/MapThumbnail.cs` をC#全文とnative `.cpp/.hpp`、Rendererの`BeginFrame`、EndScreenの両edgeでの`Clear`、PlayerEntityMapPickの呼出し、Scene.BindTextureまで照合。PNG RGB decode、縮小box filter、1 decode/frame、cache miss/failure、reserved texture-name ringの接続が一致。`StringComparer.OrdinalIgnoreCase`を大文字化キーからNativeRuntime comparerに置き換え、texture-name counterのunchecked wrapを`IncrementInPlace`に変更。Windows Release build済。
- `Mods/Render/NoiseField.cs` をC#全文とnative `.cpp/.hpp`、LauncherNoise/MovingBackdropの直接参照で照合。固定seedの`Random.NextDouble`、Stopwatch elapsed、CellsFor/clamp、resize falloff、domain-warp noise、RGB byte計算が通常入力で一致。`CellsFor`のdouble→intを.NET 10のNaN/範囲外規則を持つ`ConvertToInt32Net9`に変更し、C#のbyte[] `Pixels` getterに合わせてnativeも非const instanceからmutable bufferを返す。Windows Release build済。
- `Mods/Render/PreviewPass.cs` をC#全文とnative `.cpp/.hpp`、Rendererのsimulation step/item collection/draw順、AddRenderItem、LauncherHunter/HunterStand/EndScreenの接続で照合。state既定値、preview model更新、collection時のfinally、scissor/depth/camera/GL復元、MathF.RoundのToEven conversionが一致。`ModDrawPreviewAlone`の例外処理をC# `catch (Exception)`相当のcatch-allとNativeRuntime message helperへ変更。Windows Release build済。
- `Mods/Render/Radar.cs` をC#全文とnative `.cpp/.hpp`、PlayerHud/Features/ModEntry/SettingsViewの直接参照で照合。Enabled/ShowBackground/ShowOutlinesの既定値、Range、IsWeaponItemの全15項目、7色のpalette値が一致。C# readonly structをnative側で書き換えできたためprivate storage/getterのvalue型にし、HUD参照を更新。Windows Release build済。
- `Mods/Render/HunterShot.cs` をC#全文とnative `.hpp`、HunterStandのproducer/consumer接続、AndroidHunterShotの戻り値形式で照合。Current/InFrame/hole/frame propertiesの既定値、Hunter.Samus、nullable Taskとoptional shared_futureが一致。BGRA・上から下・tight packingの戻り値契約をnative interface commentへ明記。Android buildは全Androidファイル監査後のため未実施。
- `Mods/Render/FrameTimingCheck.cs` をC#全文とnative `.cpp/.hpp`、ModEntryのheadless dispatch接続で照合。7 frame-rate caseと上限step数、jitter seed、2秒stall、LockjawNoise invariants/Rng1不変、diagnostic reset、出力とexit codeが一致。seed付きRandom sequenceも既存NativeRuntime実装で照合、修正なし。Windows Release build green。
- `Mods/Render/PlayerEntityTeamScoreboard.cs` をC#全文とnative `.cpp/.hpp`、PlayerHudのモード判定・列位置・ping表示、TeamVisuals、DrawText2DのUTF-16 maxLength処理まで照合。勝者/チーム/プレイヤー行の順序、mode別TIME/POINTS・DEATHS/KILLS、色・尺度・間隔は一致。整数値のToStringをcurrent-culture helperへ、名前幅のfloat→intを.NET 10 cast helperへ変更。
- `Mods/Render/LockjawTrailProbe.cs` をC#全文とnative `.cpp/.hpp`、MapAuditの呼出タイミング、RenderItem保持キュー、BombEntity列挙を照合。対象条件・順序・FNV-1a入力とfloat bit pattern、pool返却前の採取タイミングが一致。C#でnull参照例外となるRenderItem/BombEntity/Pointsをnativeでも`RequireReference`経由にし、`trailCount++`をC# unchecked wrap helperに変更。
- `Mods/Render/PlayerEntityStylusHud.cs` をC#全文とnative `.cpp/.hpp`、PlayerHudの武器ホイール初期座標・描画・UpdateWeaponArc、StylusZoneの寸法/5ボタン配列、Scene.DrawHudFlatBoxを照合。ゾーン矩形、6位置の変換、scale、輪郭/楕円spanの順序とalpha、.NET cast・Math Min/Maxの特殊値処理が一致。差分修正なし。
- `Mods/Render/GlEs.cs` をC#全文とnative `.cpp/.hpp`、Android shared rendererのGL利用箇所と照合。primitive分解、display list/dynamic buffer、current color/alpha-test、texture-name map、shader translation、GL state、framebuffer、uniform配列の値・順序が一致。null shader sourceの例外、空shader/uniform名のpointer、ref相当引数のnull参照、compile/link diagnosticsと未知primitive enumのcurrent-culture数値表示を修正。Android buildは全Androidファイル監査後のため未実施。別途、C# Android headはglobal `GL` aliasで本クラスを使う一方、native shared `Renderer` は `OpenTK::Graphics::OpenGL::GL` を使い、Android CMakeはdesktop `GL.cpp` を除外する。native Android側の直接呼出しは `GlEs::Reset` と `PreviewRun` の `Viewport` のみ確認できたため、実Android renderer dispatch/link接続はプラットフォームcloseoutで別途解決・確認する。
- `Mods/Render/LockjawTrailNoise.cs` をC#全文とnative `.hpp/.cpp`、`FrameTimingCheck`の直接呼出しで照合。64-bit tickのlow/high word、各signed IDのunchecked uint変換、FNV系mixとavalanche定数、下位16-bitからのfloat変換・式順が一致。出力は全経路で決定的、native差分修正なし。
- `Mods/Render/HunterPreview.cs` をC#全文とnative `.hpp/.cpp`、`PreviewPass`のSetUp/Ready/Step/GetDrawInfo呼出および`EntityBase.GetModels` collection契約で照合。hunter fallback/missing guard、LOD0取得、Idle設定、recolor、animation step、固定light/facingとrender-item生成順が一致。C# `catch (Exception)` に対してnativeは`std::exception`しかcatchしなかったためcatch-allと`ExceptionMessage`を使用。またC# `_models.Clear()`が公開中の一覧を同じlist objectで更新するのに対しnativeは`ModelList`自体を置換して保持中のvector参照を失効させるため、`ModelList::Clear`を追加してlist identityを保った。Windows Release buildは`ninja -C tools/build/out/msys2-mingw64-Release -j 4`で成功。
- `Mods/Render/PlayerEntityEndScreen.cs` をC#全文とnative `.hpp/.cpp`、`EndScreen.NoteLayout`/hit testing、MapPick描画、PreviewPassの直接接続で照合。panel/preview geometry、portrait fallback、hunter名、4 suit swatches、Ready/Gamepad glyph、次roomの不変大文字化、クリック領域の更新順が一致し修正なし。
- `Mods/Render/PlayerEntityProHud.cs` をC#全文とnative `.hpp/.cpp`、`PlayerHud.DrawHudObjects`および`DrawModeScore`の呼出条件で照合。health/ammoのclamp・色閾値・ammo cost換算・HUD icon tint/配置、mode別score message ID、ProHud時のstock表示抑止が一致し修正なし。Windows Release全体build green。
- `Mods/Render/PlayerEntityVoteHud.cs` をC#全文とnative `.hpp/.cpp`、`MapVote.NoteLayout`/`EndScreen` pointer hit testingとの接続で照合。touch/gamepad表示条件、Android/desktop geometry、文字列置換、button hover描画とhitbox publish順が一致し修正なし。Windows Release全体build green。
- `Mods/Network/NetHitClaims.cs` をC#全文とnative `.hpp/.cpp`、Renderer tick順・NetSession packet send/receive・NetDamage/NetHitPrediction・slot/room lifecycle接続で照合。claim stream/life検証、outbox retry/verdict、damage/geometry判定、ledger duplicate、fire-frame arbitration、rescued-hit抑止、reset/cleanupが一致。`NearestLedgerOffset`のuint→int bit reinterpretとunchecked int差分をnativeで明示し、C# `Math.Abs(Int32.MinValue)`相当の`OverflowException`境界も保った。後続のlifecycle共有統計監査で見つけた`OldLifeClaims`のsigned overflowも4書込箇所を`IncrementInPlace`へ修正。Windows Release native library build green。
- `Mods/Network/DedicatedServer.cs` をC#全文とnative `.cpp/.hpp`で照合し、ModEntryの専用server起動、NetHostSession/HostPool/NetMasterの生成・設定・停止、PeerCount/Listening/EverOccupiedの直接参照も確認。loop順序、Helloの再接続/slot割当/Welcome、status/refusal、authority通知と昇格、snapshot/intent検証とfan-out、match clock/rotation/vote、ping/roster、timeout/cleanupの条件とpacket内容は一致。修正なし、静的監査のみ（build/runtime未実施）。
- `Mods/Network/HitRig.cs` をC#全文とnative `.cpp/.hpp`で照合し、ModEntryのconfigure、NetTestScriptのdriver dispatch、NetCheckClientのreport、PlayerEntityのscript aim/input hookを確認。role割当、runner/sniper/duel/volleyの条件、照準・距離制御、発射周期、controls edgeと測定値は一致。nativeのsigned frame/stat counters `++`はC#のunchecked wrapと違い未定義動作なので`IncrementInPlace`へ変更し、snapshot `uint`からclockへの明示castをC# unchecked castと同じbit reinterpretへ変更。Windows Release `ninja -C tools/build/out/msys2-mingw64-Release -k 0 -j 4`成功。runtime未実施。
- `Mods/Network/NetLobbyTest.cs` をC#全文とnative `.hpp/.cpp`で照合。packet/protocol境界、team layout、client state、全lobby/continuous/client-sessionシナリオとassert順を確認し、`ModEntry`を含む直接呼出し元は両側にないことを確認。C# `IsBackground`/`Join(5000)`に対しnative Rigの無期限joinと起動失敗時のjoinable threadが不一致だったため、共有thread state・例外伝播・5秒join/detachを実装。Windows Release build green、シナリオ実行は未実施。
- `Mods/Network/NetSession.cs` をC#全文とnative `.hpp/.cpp`で照合。接続/再接続、role・packet受理順、host roster、slot intent、authority handoff、session/match/rosterの世代検証、snapshot/health/time同期、demo記録とcleanupを確認。Renderer/NetHooksのtick・send/apply順、NetLaunch.Connect、DedicatedServer/ServerSimのauthority・roster・intent接続も照合。C# `Encoding.ASCII`の補助平面文字が2つの`?`になるのにnativeが1つだったため修正。C# unchecked `long++`相当の診断カウンター加算を`IncrementInPlace`へ変更。Windows Release build green、runtime未実施。

### 2026-09-27 native launcher regression audit

- `NativeRuntime/System/Tasks.cpp` の `TaskRun` を `Tasks.hpp` とC# `Task.Run`、全native直接呼出元（特に `NetMasterClient::FindHosts` のサーバー行ごとの並列問い合わせ）で照合。C#は共有ThreadPoolへ投入するがnativeは呼出しごとにOSスレッドをdetachしていた。hardware concurrencyまで遅延拡張する共有キューに変更し、行数に比例するスレッド・スタック生成を止めた。Windows Releaseのnative static library compile/link成功。ネットワーク実サーバーでの応答確認は未実施。
- Launcher CPU/memory診断: `-uibench maps -uibenchsize 1280x720 -uibenchonly Paint` はrender 97.09 ms、約10 fps。併せたプロセスメモリ採取はprivate bytes最大57.5 MB、working set最大65.4 MB、5 threadsで、この測定内に増え続けるメモリは観測しなかった。1920x1080ではrender 229.08 ms、約4 fps。従ってOffline画面の再描画はCPU描画の重さが直接確認できたが、この診断だけではユーザーPC全体のフリーズ原因や1080pのメモリ状態は確定しない。UI描画の修正とOnline画面の実サーバー確認は継続。
- `Mods/Network/NetUnlagged.cs` をC#全文とnative `.cpp/.hpp`で照合。履歴・subframe rewind・世代/life照合・補間・restore・beam catch-up・診断出力を確認し、PlayerInputの`BeginShot`→`Spawn`→`EndShot`、NetSessionのreset/record、NetHitClaims/NetSmoothing/NetPlayerLifecycle/NetPlayerBridge/NetShotDiagnosticsの直接接続も照合。C# unchecked `long` カウンターの加算・共有rewind統計の加算をnative `IncrementInPlace`/`UncheckedAdd`に変更。`git diff --check`通過、Windows Release native build green。runtime未実施。
- `Mods/Network/NetMaster.cs` をC#全文とnative `.cpp/.hpp`で照合。DedicatedServerのheartbeat/Farewell/hosted reporter、ModEntryの`-masterserver`/`-servers`/`-hosts`/`-hostgame`、TextLauncher・CreateServerScreen・PlayScreenのquery/probe/request接続を確認。heartbeat、expiry、host port cooldown/reap、list分割とCanHostの三状態、並列probe callback、request応答検証は一致。C#のfloat→int変換とprobe経過long→int unchecked wrapをnative helperへ合わせ、`-servers`のCanHost説明欠落も追加。`git diff --check`通過、Windows Release native library/executable build green。runtime未実施。
- `Mods/Network/NetPlayerBridge.cs` をC#全文とnative `.cpp/.hpp`および`FormReconciliation`依存で照合。NetHooksの入力・位置復元・state適用順、NetSessionのSendIntent metadata、PlayerEntity Spawn/PlayerProcess RespawnRequested、slot/room lifecycleを確認。intent edge履歴、life/generation検証、form reconciliation、authority/local/puppet別state適用、位置復元、velocity・node/volume更新は一致。C# unchecked `long++` と違いnative signed `++`が未定義動作だったRejectedUpdates/Snapsと、直接呼出し元PlayerEntityNetAimのNodeLookupsUnresolvedを`IncrementInPlace`へ変更。`git diff --check`通過、Windows Release native build green。runtime未実施。
- `Mods/Network/NetDamage.cs` をC#全文・native `.cpp/.hpp`と`PlayerEntity.TakeDamage`/`ModNetDie`、BeamProjectile/Bomb/SpawnBomb、NetHitClaims/Lifecycleの直接接続で照合。Suppression/claim beam、room/slot/life reset、snapshot・damage replay・death/score復元の条件と順序が一致。C# unchecked wrapに対しnative signed overflowだった弾・爆弾・damage診断カウンター、PredictionScoreScope深度を`IncrementInPlace`/`DecrementInPlace`へ変更。uint damageのint変換・加算・resolve log減算をC#のunchecked bit/wrap semanticsへ修正。`git diff --check`通過、Windows Release native build green。runtime未実施。
- `Mods/Network/LobbyCommands.cs` をC#全文・native `LobbyCommands.cpp`/`DedicatedServer.hpp`と、packet dispatch/Remove/end-of-match、HostPool/NetMasterのlobby初期化を照合。session revision・command dedupe、owner/phase/revision権限順、team編成/容量、match/map検証、start失敗rollback/load barrier、lobby復帰・slot除去が一致。MapRotationのNaNをC#はClamp後も保持しunchecked変換結果は未規定。nativeのfloat→整数UBを避けNaNを0にするガードを追加。直接呼出しの`ChooseTeam`にも未初期化stackalloc countがあったためC# spanを`Clear()`し、nativeのzero-initialized arrayと揃えた。`git diff --check`通過、Windows Release native build green、`dotnet build src/MphRead/MphRead.csproj -c Release --no-restore` green（既存CS0618警告4件）。runtime未実施。
- `Mods/Network/NetCombatCheck.cs` をC#全文・native `.cpp/.hpp`と`ServerSim`/`NetHitClaims`の依存で照合。両側ともproduction callerは未登録。初期state、dead held-fire、実weapon spawn/projectile lifecycle/ricochet、hit-claim拒否/同時kill/grace deadline、continuous-phase goldenとpeer間一致のケース順・条件が一致。native累積assertionのsigned `++`をunchecked `IncrementInPlace`へ変更し、catch-all failure出力を`ExceptionToString`へ変更。`git diff --check`通過、Windows Release native build green。harness runtime未実施。
- `Mods/Network/HostPool.cs` をC#全文・native `.cpp/.hpp`とDedicatedServerのHostRequest/loop/shutdown、ModEntryの`-hostports`接続で照合。同一IPからの空ゲーム置換、使用中port保護、cooldown、mode/rotation/session options/owner token/reporter設定、listener待ち、未参加180秒・全員退出後45秒のreap、停止待ち/port解放/通知が一致。worker例外のC# `catch (Exception)`に対しnative `std::exception`のみ捕捉していたため`catch (...)`と`ExceptionMessage`へ変更。`git diff --check`通過、Windows Release native build green。runtime未実施。
- `Mods/Network/MapAuditTeams.cs` をC#全文・native `MapAuditTeams.cpp`/`MapAudit.hpp`と`ModEntry -maptest -teamprobe`設定で照合。8 slot/4 team配置、同士討ち規則、Standings/Survival/Nodes/Defender/Bountyの確認と画面キャプチャ条件が一致。`RunTeamProbe`/`CaptureTeamResults`はC#・native双方とも呼び出し元なし。native check counterのsigned `++`をunchecked `IncrementInPlace`へ、例外捕捉/表示をC#の`Exception`/Message/ToStringに合わせて修正。`git diff --check`通過、Windows Release native build green。runtime未実施。
- `Mods/Network/NetSessionLobby.cs` をC#全文・native `NetSessionLobby.cpp`/`NetSession.hpp`とNetSessionのUpdate/packet dispatch/Stop、LobbyScreen/NetLaunch/NetSlotManager/Chatの直接呼び出しで照合。phase/owner/timeout判定、command ID・再送間隔と4回上限、revision/epoch/match stale guard、match resetとsynthetic state適用順、load ack/identity再送、roster作成とresetが一致。修正なし。`git diff --check`通過、Windows Release native build green。runtime未実施。
- `Mods/Network/NetPlayerLifecycle.cs` をC#全文・native `.cpp/.hpp`とPlayerEntity Spawn/BeamProjectile ricochet、NetSession occupant/state/intent/roster、NetSlotManager、NetHitClaims/NetDamageの共有統計書込箇所で照合。generation/life guard、spawn/reset順、projectile launch identity、state/intent rejection、slot cleanupとlogが一致。native signed 64-bit統計加算をunchecked `IncrementInPlace`へ修正し、外部writerのOldLifeClaimsも同様に修正。`git diff --check`通過、Windows Release native library build green。実行中の`FruityPrime.exe`が出力ファイルをロックしたため最終exe再リンクは未確認。runtime未実施。
- `Mods/Network/SessionProtocol.cs` をC#全文・native `.hpp/.cpp`、LobbyCommands/NetSessionLobby/NetSessionのpacket send/receiveとNetLobbyTestのProtocolChecksで照合。session stateの75-byte layout/各offset、little-endian、enum/rule/participant検証、command/result/load packetsの既定値・拒否条件、unchecked sbyte/short変換が一致。C#のSpan境界例外に対しnative writeが未チェックのindex/subspanで未定義動作となる差を、同じ評価順と先行書込を保つchecked accessへ修正。`git diff --check`通過、Windows Release native library build green。runtime未実施。
- `Mods/Network/NetHooks.cs` をC#全文・native `.hpp/.cpp`とRenderer/PlayerInput/PlayerProcess、SpectatorMode/ModEntry/NetPlayerBridgeの直接接続で照合。LocalSlotのoffline/connecting/server/demo分岐、puppet・slot保持・spawn、snapshot/intent復元とstale境界、shot origin/aim、AfterInput/AfterSimulationの順序と送信/適用条件が一致。C#配列添字と異なるnative `std::array::at()`例外を`ManagedAt`へ修正。`git diff --check`通過、Windows Release native library build green。runtime未実施。
- `Mods/Network/NetCheckClient.cs` をC#全文・native `.cpp/.hpp`とModEntryの直接呼出しで照合。scene/window lifecycle、frame処理順、観戦・再接続・map vote、capture、各観測値、report、終了処理が一致。C# `Double.TryParse(InvariantCulture)`の`Float | AllowThousands`をNativeRuntime parserに合わせ、独自近似を削除。signed `int`統計のunchecked wrapとframe差分をC#に合わせ、例外捕捉・表示をcatch-all/`ExceptionToString`へ修正。`git diff --check`通過、Windows Release native library build green、runtime未実施。
- `Mods/Network/SpireAltPoseCheck.cs` をC#全文・native `.cpp/.hpp`とModEntryの直接呼出しで照合。headless sim起動、Spire roster、240 frame input/press history、morph→alt attack、左右collision poseの移動・offset閾値、failure時の停止と例外再送出が一致。C#の再利用press配列とnativeのframe-local vectorはintent受理後すぐ同期stepするため同じ値を観測する。修正なし。`git diff --check`通過、Windows Release native library build green、runtime未実施。
- `Mods/Network/NetTestScript.cs` をC#全文とnative `.cpp/.hpp`、ModEntry/NetHooks/MapAudit/NetCheckClient/NetFeatureCheck/PlayerEntityNetAim/WeaponDpsの直接接続で照合。全16 phaseの順序、server/local clock、入力・照準・target選択、offline/map helper、resetとcheck callerの接続を確認。C# `Double.TryParse(InvariantCulture)`は`Float | AllowThousands`既定のためnativeをNativeRuntime parserへ合わせ、phase quotientの.NET 10 double→Int32変換を`ConvertToInt32Net9`へ変更。MapAuditがNaNの`seconds`を渡す場合の`Math.Max`/`std::max`差はMapAudit自身の監査時に処置する。`git diff --check`通過、Windows Release native library build green。runtime未実施。
- `Mods/Network/ContinuousWeaponPhase.cs` をC#全文とnative `.cpp/.hpp`、PlayerInput/BeamProjectile/PlayerEntity/NetSession/NetSessionLobby/NetSlotManager/NetCombatCheckの直接接続で照合。MaxIntentAge、ammoは境界を超えた時・damageは境界を含めた時に加算するcadence、slot clockのObserve/Advance/Resolve、owner/remote-intent/local-frame選択と各resetの順序が一致。C# `new Clock[slots]`の負数時`OverflowException`に対しnative vectorが別例外になる差をnative constructorで合わせた。`git diff --check`通過、Windows Release `libMphRead.Native.a` build green。runtime/harness未実施。
- `Mods/Network/NetHealthSync.cs` をC#全文とnative `.cpp/.hpp`、SceneSetup/ItemSpawnEntity/PlayerProcess/NetSession/DedicatedServer/NetSessionLobby/HealthSimulationTestの直接接続で照合。3-byte header/7-byte entry、health spawn上限、picker flagsとreserved bits、重複ID・picker・match検証、snapshotの検証/受信/送信順、replicaのspawn・pickup ownership、room resetが一致。native `Write`のspan長を`int32_t`へ縮小して容量比較する差を取り除いた。`git diff --check`通過、Windows Release `libMphRead.Native.a` build green。harness/runtime未実施。
- `Mods/Network/FormReconciliation.cs` をC#全文とnative `.cpp/.hpp`、NetPlayerBridge/ServerSimCheckの直接接続で照合。morph/unmorph遷移のtarget/開始/終了、stale transition grace、90 frame timeout、8/12 frame correction waitと全reset条件が一致。ping latency graceのC# unchecked `ping * 60 / 1000 + 8`に対してnative signed overflowが未定義だったため`UncheckedMultiply`/`UncheckedAdd`を使用。native FormCorrection enum幅もC#既定のInt32へ合わせた。`git diff --check`通過、Windows Release `libMphRead.Native.a` build green。runtime/harness未実施。
- `Mods/Network/NetShotDiagnostics.cs` をC#全文・native `.cpp/.hpp`およびPlayerInput/BeamProjectile/NetDamage/NetHitClaims/NetHitPrediction/NetUnlagged/NetPlayerBridge/NetSession/NetCheckClient/ServerSim/NetCombatCheckの直接接続で照合。ShotKey identity/format、enumとbucket、attempt結果・continuous診断・reset・Describeの出力順と条件が一致。native signed `long`相当の`++`/`+=`とDescribeの集計加算をC# unchecked wrapへ合わせ、`NetHitClaims`/`NetHitPrediction`のこの配列への直接writerも同じwrap helperへ変更（NetDamage/NetUnlagged writerは既に対応済み）。整数表示をNativeRuntime current-culture `ToString`へ揃えた。`git diff --check`通過、Windows Release `libMphRead.Native.a` build green。runtime未実施。
- `Mods/Network/HealthSimulationTest.cs` をC#全文・native `.cpp/.hpp`とModEntryの`-healthsimtest`、ServerSim/NetHealthSync/SceneSetup/ItemSpawnEntity/NetLaunchの接続で照合。authorityの初期session/rosterと180 frame simulation、pickup後のunavailable tail・respawn、replica構築と3段階snapshot適用、収束確認・終了処理の順序が一致。nativeの`catch(std::exception)`/`what()`だけではC# `catch(Exception)`/例外ToStringを満たさないためcatch-all/ExceptionToStringへ変更し、health名とcount表示をcurrent-culture formatへ合わせた。`git diff --check`通過、Windows Release `libMphRead.Native.a` build green。harnessは実行していない。
- `Mods/Network/NetLifecycleTracker.cs` をC#全文とnative `.cpp/.hpp`、NetPlayerLifecycle/NetSession/NetSessionLobby/DedicatedServer/LobbyCommands/NetSmoothing/NetDamage/NetHitClaims/NetLobbyTestの直接接続で照合。zero-reserved generation/life `Next`、ushort/uint serial arithmeticとulongの順序比較、occupant/reset/BeginLife、Acceptの拒否優先順とdead tombstoneが一致。C# unchecked castに対するnative `uint16/uint32`→signedの直接castは結果が実装依存なので、wrap後のbit patternを`std::bit_cast`で保持するよう変更。`git diff --check`通過、Windows Release `libMphRead.Native.a` build green。harness未実施。
- `Mods/Network/ServerSimCheck.cs` をC#全文・native `.cpp/.hpp`、ModEntryの`-simcheck`引数処理、ServerSim/FormReconciliation/NetSession/NetPlayerLifecycle/NetProtocolとの直接接続で照合。人数clamp、起動可否/失敗、roster、slotごとのintentとack frame、step/snapshot/match-end統計、spawn判定、formcheck、cleanup、working set/peak採取と出力順が一致。C# `Math.Round`後の.NET 10 double→Int32変換はNaNを0、範囲外を飽和させるが、native固有処理がNaN・正の範囲外を`int.MinValue`にしていたため、共通`NativeRuntime::MathRoundToInt32`へ置換。`git diff --check`通過、Windows Release `libMphRead.Native.a` build green（既存`offsetof`警告のみ）。runtime/harness未実施。
- `Mods/Network/NetLog.cs` をC#全文・native `.cpp/.hpp`、NetSession/NetHooks/NetConnectCommand/NetCheckClientのOpen/Snapshot/Close接続で照合。間隔設定、client名の安全化、ユーザーデータ先へのtruncate作成と逐行flush、Event/CollisionRange、snapshot全項目とhitreg集計、scene/node参照、書込失敗時の継続を確認。C# `NumberStyles.Float`とnative stream parserの差を`DoubleTryParseInvariant`へ統一し、ログ整数の`std::to_string`をNativeRuntime current-culture formatterへ変更。`git diff --check`通過、Windows Release `libMphRead.Native.a` build green。runtime未実施。
- `Mods/Network/LobbyRules.cs` をC#全文・native `.cpp/.hpp`、DedicatedServer/LobbyCommands/NetLaunch/LobbyScreen/SessionProtocol/NetLobbyTestの直接接続とTeamLayout/MatchWorldProfile依存で照合。mode/format/map検証順、exact/flexibleの必要人数、team capacity/ready条件と理由文が一致。native room key長のUTF-8 byte比較をC# `string.Length`相当のUTF-16 unit比較へ変更し、roster配列参照をC#と同じ短絡条件内へ移動、理由文の数値をcurrent-culture表示へ統一。C#の未初期化stackalloc count spanへ`Clear()`を追加し、nativeのzero-initialized arrayと同じ定義済み状態にした。`git diff --check`通過、Windows Release `LobbyRules.cpp.obj` compile green。全体build/runtime/harness未実施。
- `Mods/Network/MapRotation.cs` をC#全文・native `.cpp/.hpp`とDedicatedServer/LobbyCommands/HostPool/NetMaster/LocalServer/NetHostSession/ModEntry/NetLobbyTestの直接呼出しで照合。Current/Nextのfallback、None→Battle、コメント/空行/pipe/Unicode trim、enum・時刻(NumberStyles.Float/invariant)・point(Int32/current culture)解析、list/default出力、pending/overrideの優先とcycle index再開が一致。修正なし。`git diff --check`通過、Windows Release `MapRotation.cpp.obj` target up-to-date（再コンパイルなし）。runtime/harness未実施。
- `Mods/Network/NetFaultQueue.cs` をC#全文・native template/.cpp、NetLag::CreateQueueとNetTransportの直接接続で照合。constructor検証、seed付き乱数の呼出し順、loss/jitter/reorder/duplicate、容量上限、同時刻のorder付き優先度、TryDequeueの時刻境界と失敗時default値を確認。native signed `++`の未定義overflowをDropped/Duplicated/Reordered/orderで`IncrementInPlace`に置換し、`Math.Max(double)`のNaN/符号付きzeroと`Double.CompareTo`のNaN順をC#へ合わせた。`git diff --check`通過、Windows Release `NetTransport.cpp.obj` compile green（既存`offsetof`警告のみ）。runtime/harness未実施。
- `Mods/Network/NetTimingDiagnostics.cs` をC#全文・native `.cpp/.hpp`とNetHooks/NetSession/NetSmoothing/NetDamage/NetPlayerLifecycle/NetCheckClient/ServerSimの直接接続で照合。50ms stall/250ms attribution、snapshot interval、frameごとのposition重複除外、snapshot対intent fallback、slot/global reset、診断文の順とStopwatch elapsed→TimeSpan millisecond変換を確認。native signedカウンター/fallbackの`++`を`IncrementInPlace`へ置換し、`std::to_string`をcurrent-culture `Runtime::ToString`へ変更。`git diff --check`通過、Windows Release `NetTimingDiagnostics.cpp.obj` compile green（既存`offsetof`警告のみ）。runtime未実施。
- `Mods/Network/ServerSim.cs` をC#全文・native `.cpp/.hpp`とDedicatedServer/LobbyCommands/HealthSimulationTest/NetCombatCheck/ServerSimCheck/SpireAltPoseCheckの直接接続で照合。可用性判定、authority/roster/session初期化順、scene構築、固定step/上限/stall・drop計測、失敗時rollback、停止時のsession/cache cleanup、Room/Describeの診断出力が一致。nativeの`std::exception`限定catchをC# `catch(Exception)`相当のcatch-allへ広げ、例外Message/ToStringと初回・反復stepログを揃えた。計測を.NET `Stopwatch.GetElapsedTime().TotalSeconds`と同じTimeSpan tick変換へ、整数診断をcurrent-culture書式へ修正。`git diff --check`通過、Windows Release `ServerSim.cpp.obj` compile green（既存`offsetof`警告のみ）。runtime未実施。
- `Mods/Network/MatchDefinition.cs` をC#原本・native `.hpp/.cpp`とSessionProtocol/LobbyCommands/DedicatedServer/LobbyScreen/CreateServerScreen/NetLaunch/NetSession/NetMatchSync/NetHudHealth/ChatBoxの直接参照で照合。byte enum値、ushortのSessionRules bit値とRules合成、全GameModeのUsesLives/UsesTimeTarget/DefaultValue、MatchDefinitionの各既定値・値比較が一致。native `RoomKey`のoptionalはC# default structのnullを保持し、wire read/writeも対応。SessionPhase/MatchFormatのToStringは定義値・未知値ともC# enum表記に一致。修正なし。`git diff --check`通過、Windows Release `MatchDefinition.cpp.obj` up-to-date（再コンパイルなし）。runtime/harness未実施。
- `Mods/Network/NetHealthSyncTest.cs` をC#全文・native `.cpp/.hpp`と唯一の呼出し元`NetLobbyTest.Run`を照合。開始/終了時のroom reset、手書きpacketの各byte offset、valid/invalid flags・picker・duplicate・truncation・stale match、snapshot容量、slot 7 objective clock、NaN拒否のassert順と値が一致。失敗時の`InvalidOperationException`と文言も一致。修正なし。`git diff --check`通過、Windows Release `NetHealthSyncTest.cpp.obj` up-to-date（再コンパイルなし）。harness/runtime未実施。
- `Mods/Network/MapAudit.cs` をC#全文・native `.cpp/.hpp`、`MapAuditTeams` partial、ModEntryの`-maptest`、NetTestScript/ScreenCaptureとの直接接続で照合。simulation/update/render/inputの順、probeとaffliction phase、spawn/item/render sweep、node/puppet検査、report内容・失敗数・cleanupを確認。native `std::max/min`はC# `Math.Max/Min`とNaN/signed zeroの結果が異なるためRuntime helperに変更。scoreboardのdouble→Int32変換を.NET 10のNaN=0/範囲外飽和へ、`total * 2`と実行時間依存カウンター・集計の整数演算をC#既定unchecked wrapへ合わせた。`git diff --check`通過、Windows Release `MapAudit.cpp.obj` compile green（既存offsetofとScreenCapture::Save戻り値の警告あり）。runtime/harness未実施。
- `Mods/Network/NetLag.cs` をC#全文・native `.cpp/.hpp`、ModEntry/NetTransport/DebugLog/NetCheckClient/NetLobbyTestの直接接続で照合。seed/jitter/rate/latencyのNumberStyles・invariant parser、境界、失敗時の設定保持、Active判定、outbound/inbound queueの設定値とreportを確認。C# `unchecked(Seed + (outbound ? 1 : 0))`に対してnativeの符号付き変換に依存していたseed生成を`UncheckedAdd`へ変更。Describe内のRTT/jitter/seed整数もC# current-culture表示へ統一し、百分率書式・文言・順序は一致。`git diff --check`通過、Windows Release `NetLag.cpp.obj`/`NetTransport.cpp.obj` compile green（既存offsetof警告のみ）。runtime/harness未実施。
- `Mods/Network/NetMatchSync.cs` をC#全文・native `.cpp/.hpp`、NetHooks.AfterInput/NetSessionのserver match・reset/NetMatchEnd/MatchDefinition/NetLobbyTestとの接続で照合。active/server state/empty room gate、mode fallback、TimeGoal対PointGoal、friendly-fire/shadow-freeze/StatesRules付きaffinity、intermission中のclock保護、無制限match sentinel、zero clock gate、room切替・drift計測と1.5秒超のsnap条件、同期ログを確認。ResetがLastDriftを保持する点も一致し、修正なし。`git diff --check`通過、Windows Release `NetMatchSync.cpp.obj` up-to-date（再コンパイルなし）。runtime/harness未実施。
- `Mods/Network/NetMatchTimeSync.cs` をC#全文・native `.cpp/.hpp`とNetSession snapshot送受信、DedicatedServer snapshot検証、HealthSimulationTest/NetHealthSyncTestのtail offsetで照合。8 slotのTime/TeamTimeを各4-byte little-endian floatで交互に置く128-byte layout、Write時に先頭Sizeだけ更新する動作、Validateのexact length/finite/`-1`下限、Receive順と呼出元のvalidate後適用が一致。修正なし。`git diff --check`通過、Windows Release `NetMatchTimeSync.cpp.obj` up-to-date（再コンパイルなし）。runtime/harness未実施。
- `Mods/Network/NetHudHealth.cs` をC#全文・native `.cpp/.hpp`とPlayerEntityNetHud/PlayerHud/NetLogの直接利用で照合。HideOpponentsのactive/session/policy短絡、local・spectator・demo時のVisible、remote stateのslot/life/generation一致判定、fallback entity health・snapshot frame・life ID・authority flag、record sampleのfield型/default/equalityを確認。nativeが`ServerSession` accessorを2回呼んでいたため、C# property patternと同じくActiveの後で一度取得して判定するよう変更。`git diff --check`通過、Windows Release `NetHudHealth.cpp.obj` compile green（既存offsetof警告のみ）。runtime未実施。
- `Mods/Network/NetFeatureCheck.cs` をC#全文・native `.hpp/.cpp`とNetCheckClientの直接呼出しで照合。Observeのplayer/entity計測、phase/path cadence、spawn・damage・weapon・teleport・replication比較、23 featureのmachine-readable出力、verdict/failure集計、board/report順を確認。own respawnの整数3項目にある`std::to_string`をC#補間と同じcurrent-culture formatterへ変更し、Record・Player・beam/bomb/turret・remote state・NetDamage・GameState配列と2次元overlap配列の添字を`ManagedAt`へ変更して.NET `IndexOutOfRangeException`に統一。`git diff --check`通過、Windows Release `NetFeatureCheck.cpp.obj` compile green（NOMINMAX再定義・offsetof警告あり）。runtime/harness未実施。
- `Mods/Network/NetTransport.cs` をC#全文・native `.hpp/.cpp`とNetSession/DedicatedServer/NetMaster/NetCheckClient/DemoPlayback/NetLobbyTestの直接接続で照合。UDP初期化、receive/lag worker、drop-oldest inbox、held FIFO、immediate Pong、playback injection、Drain、送受信統計とDispose順が一致。ReceivedPacketのbyte[]/Span境界、送信payload超過、CTS二重Disposeの例外型・文面をC#に合わせた。Windows Release `NetTransport.cpp.obj` compile green（既存offsetof警告のみ）、`git diff --check`通過。runtime/harness未実施。
- `Mods/Network/NetRoomChange.cs` をC#全文・native `.cpp/.hpp`とRoomEntity.LoadRoom/NetHooks/NetPlayerBridgeの直接接続で照合。room/match polling、unknown map retry、fade guard、全slot再構築/occupied flags/reset順、score reset、intro camera reload条件を確認。AfterRebuildでnativeがplayerを二重初期化しHalfturretを初期化していなかった差を修正。`IReadOnlyList` indexの例外を`ManagedListAt`で、intro load failureのcatch/MessageをC#に合わせた。`git diff --check`通過、Windows Release `NetRoomChange.cpp.obj` compile green（offsetof警告のみ）。runtime未実施。
- `Mods/Network/NetSlotManager.cs` をC#全文・native `.cpp/.hpp`とNetHooks/NetSession/NetPlayerLifecycleの直接接続で照合。session gate、slot occupancy、team/hunter roster補正、Weapons.Current待ち、activate/deactivate flags・Initialize・PlayerCount、reconnect解放とlifecycle/score cleanup順を確認。nativeの内部/NetSession配列とPlayerEntity.Playersの添字・`.at()`を`ManagedAt`/`ManagedListAt`に変更しC#の配列/IReadOnlyList例外へ統一。`git diff --check`通過、Windows Release `NetSlotManager.cpp.obj` compile green（offsetof警告のみ）。runtime/harness未実施。
- `Mods/Network/NetStatus.cs` をC#全文・native `.cpp/.hpp`、NetMaster/NetLaunch/NetLobbyTestとLauncher/ModEntry/LocalServerの直接呼び出しで照合。IPv4解決、StatusQueryの送受信元・長さ判定、deadline/latency、JoinProbeのWelcome/MatchState/Bye順・slot数補正、default値、Describe/ModeName表示と各callerのprobe/timeout指定が一致。Queryの一般例外捕捉をC# `catch (Exception)`相当のcatch-allにし、`ExceptionMessage`でメッセージを揃えた。参照がない古い`NetLaunchQueryStatus` bridgeを削除し、NetLaunchの直接`Query`呼び出しに統一。`git diff --check`通過、Windows Release `NetStatus.cpp.obj` compile green（既存`offsetof`警告のみ）。runtime/harness未実施。
- `Mods/Network/NetDiagnostics.cs` をC#全文・native `.cpp/.hpp`とNetHooks/ModEntryの直接呼出しで照合。lazy environment flag、session gate、1秒間隔と`_lastReport`更新時点、slot/scoreboard/remote-state/team/form、NetDamage・NetPlayerBridge counterと条件付き警告、match表示、current-culture数値書式・出力順が一致。`PlayerEntity.Players`はC# `IReadOnlyList`なのでnativeの6つの`.at()`を`ManagedListAt`に置換。`git diff --check`通過、Windows Release `NetDiagnostics.cpp.obj` compile green（既存NOMINMAX/offsetof警告）。runtime未実施。
- `Mods/Network/NetHostSession.cs` をC#全文・native `.cpp/.hpp`、TextLauncherのStartAndJoinと終了処理、Gui Shell/Lobby/CreateServerのStop呼出しで照合。dedicated server設定・listing/Reporter、250ms後のerror確認、localhost join、失敗時Stop、background threadとcleanup順が一致。C#が設定する`MphRead host server`名を既存thread adapterへ接続し、listing文をcurrent-culture数値書式とConsole互換APIへ移した。workerの`catch (Exception)`との差はcatch-allと`ExceptionMessage`で合わせた。`git diff --check`通過、Windows Release `NetHostSession.cpp.obj` compile green（既存`offsetof`警告のみ）。runtime未実施。
- `Mods/Network/DemoPlayback.cs` をC#全文・native `.cpp/.hpp`とMatchStart/PlayScreen/DemoInfo/Renderer/NetSessionの直接接続で照合。protocol gate、frame 0からのpacket injection、20秒のmatch探索、roster用120 frame grace、rewind/reset、終端・停止・LastErrorの状態遷移が一致。nativeの`std::cout`分割出力をC# `Console.WriteLine`相当のatomic Console APIへ、protocol byte表示をcurrent-culture formatterへ修正。`git diff --check`通過、Windows Release `DemoPlayback.cpp.obj` compile green（既存`offsetof`警告のみ）。runtime未実施。
- `Mods/Network/MechanicsDump.cs` をC#全文・native `.cpp/.hpp`とModEntryの直接呼出しで照合。11節の呼出し順、静的Markdown文言、武器/ハンター/移動値の列順、enum表示・current-culture数値/小数書式が一致。C# `Console.Write`とUTF-8コンソール出力を揃えるため、nativeの`std::cout.write`を既存`ConsoleWrite`へ変更。`git diff --check`通過、Windows Release `MechanicsDump.cpp.obj` compile green（既存`offsetof`警告のみ）。runtime未実施。

### 2026-09-27 C#対比監査

- `Mods/MapGen/MapCheck.cs` を全文とnative `.cpp/.hpp`、`ModEntry` の `-mapcheck` 分岐で照合。判定順、サンプル・セル走査順、重複面の保持順、安定ソート、診断文と終了コードを確認。C# の unchecked `int` 演算と float→int 変換、`MathF.Max`、`Vector3.ComponentMin/Max`、current-culture 数値表示、`Console.WriteLine` のUTF-8出力、`catch (Exception).Message` に合わせて修正。ローカルのOpenTK 4.9.4でも `Vector3.Equals` のNaNと符号付きzeroを確認し、`HashSet<Vector3>` のdistinct件数に合わせた。C#の遅延列挙に対し一時配列を作っていた三角形分割・被覆サンプルも逐次処理に変更。`git diff --check`通過、Windows Release `MapCheck.cpp.obj` compile green。マップ実行・runtime/harnessは未実施。
- `Mods/MapGen/CollisionObj.cs` を全文とnative `.cpp/.hpp`、`MapPacker.ApplyCollision` の直接接続で照合。OBJ の ReadLine 境界、UTF-8/16/32 BOM検出と不正UTF-8置換、Unicode whitespace tokenization、負の頂点index、winding、縮退面、Newell double sum、terrain/attributeの初期値・順序を確認。C# `StreamReader`/`Char.IsWhiteSpace`、`MathF.Round` ToEven、`Math.Max` のNaN伝播、uncheckedカウンター/index加算、current-cultureの行番号・index・material出力へ修正。全行を先に保持していた一時配列も逐次処理へ変更。`git diff --check`通過、Windows Release `CollisionObj.cpp.obj` compile green。実マップ読込・runtime/harnessは未実施。

### 2026-09-28 C#対比監査

- `Mods/MapGen/MapDefinition.cs` を全文とnative `.hpp/.cpp`、`CustomRooms`/`MapBundle`/`MapPacker`/`MapCheck`/`Q3Import`/`MapReport`/`Q3Convert` の直呼び出しで照合。全プロパティ、既定値、null/ignore、camelCase、case-insensitive読込、コメント・trailing comma、Load後の基準path設定、Import/Collisionの候補順とbundle優先を確認。`collision.source: null` はC#既定JsonSerializerで保持されるがnativeが読込時点で拒否していたため、optional化し、シリアライズ省略、`ReadBytes`/`Resolve`/`MapPacker`/`MapBundle`のnull参照例外と`MapCheck`の連結結果をC#に揃えた。`git diff --check`通過、Windows Releaseの `MapDefinition.cpp.obj`/`MapBundle.cpp.obj`/`MapPacker.cpp.obj`/`MapCheck.cpp.obj` compile green（既存警告のみ）。実map読込・runtime/harness未実施。
- `Mods/MapGen/Q3Convert.cs` を全文とnative `.hpp/.cpp`、`ModEntry` の `-q3convert` オプション解析・直接呼出しで照合。map選択と既定名、出力path、Bounds/scale、skyを含めたScaleFactor、clip判定、spawn候補と向き、pickup変換・`-noitems`、definition保存と全終了条件を確認。処理の差はなく、全出力をC# `Console.WriteLine` 相当のUTF-8・環境別改行・行単位書込みへ変更し、数値をcurrent-culture formatterに統一。C# `catch (Exception)` とnative `std::exception` の差は `Q3Bsp::Load` と `ModEntry` の直接呼出しでcatch-all/message helperへ修正。`git diff --check`通過、Windows Release `Q3Convert.cpp.obj` と `ModEntry.cpp.obj` compile green（既存警告のみ）。実.pk3変換・runtime/harness未実施。
- `Mods/MapGen/MapPacker.cs` を全文とnative `.hpp/.cpp`、`MapCheck`/`CustomRooms`/`ModEntry` の直接呼出しで照合。モデル素材の初出順・重複排除、面のmaterial別順序、三角形fan、opcode padding、packed座標・法線・texcoord、collisionの属性・面順、出力順と失敗境界を確認した。`Console.WriteLine` のUTF-8/環境改行/current-culture書式、null参照時の例外順、vertex/countのunchecked加算を合わせ、C++ TU内の `Repack`/`CollisionDataEditor` の重複宣言をcanonical header参照へ置換してODR違反を除去。`ModEntry -mapgen` と `CustomRooms.GenerateMissing` の直接呼出しもcatch-all/message・console出力・unchecked countへ合わせた。`git diff --check`通過、Windows Release `MapPacker.cpp.obj`/`CustomRooms.cpp.obj`/`ModEntry.cpp.obj` compile green（既存警告のみ）。mapgen実行・runtime/harness未実施。
- `Mods/MapGen/BuiltMap.cs` を全文とnative `.hpp/.cpp`、`MapBuilder`/`Q3Import`/`MapPacker`/`CollisionObj` の生成・消費契約で照合。定義・面・entityの既定値と可変属性、Faces/Solidの共有順、Definition/face配列の寿命を確認。C#の強参照に対しnativeが面・点/UV配列・entityを裸ポインターで保持して解放していなかったため、BuiltMap/BuiltFaceに所有権を持たせて生成元からRAII移送し、OBJ読込途中の例外でもfaceを解放するよう `CollisionObj::Result` もRAII化した。Definitionはshared ownership時にBuiltMapが生存を保持する。`git diff --check`通過、Windows Release `fruity_mphread_native` compile/link green（既存警告のみ）。実mapgen・runtime/harness未実施。
- `Mods/MapGen/MapBundle.cs` を全文とnative `.hpp/.cpp`、`MapDefinition.Load`/`MapImport.ReadBytes`/`ReadBundledTextures`/`CustomRooms`/`ModEntry -mapbundle` の直接呼出しで照合。zip内recipe・level・textures・collisionのentry名と順序、BOM付きrecipe読込、ordinal-ignore-case検索、欠落時fallback、既定出力path、overwrite移動を確認。CLIのcatch範囲と件数加算をC#の全例外/unchecked `int` に揃え、ログをUTF-8の `ConsoleWriteLine` とcurrent-culture整数書式へ統一。`git diff --check`通過、Windows Release `fruity_mphread_native` compile/link green（既存NOMINMAX警告のみ）。bundle実作成・読込、runtime/harness未実施。
- `Mods/MapGen/CustomRooms.cs` を全文とnative `.cpp/.hpp`、`Metadata.Rooms` のAppendIds/AppendRooms、ModEntryのmapgen/mapbundle、MatchStartのGenerateMissing/WhyUnplayable、AndroidMapsのMapDirectory設定呼出しで照合。bundle優先・JSON同名除外、current-culture安定ソート、name変換、room IDとmetadata値、出力path、再生成の三ファイル/UTC時刻判定、missing source時の除外、WhyUnplayableとGenerateMissingの失敗隔離を確認。配列範囲外を `IndexOutOfRangeException` に、map読込のcatch境界をC#の全例外へ、診断行をUTF-8 `ConsoleWriteLine` に合わせた。`git diff --check`通過、Windows Release `fruity_mphread_native` compile/link green（既存`offsetof`警告のみ）。実mapgen・runtime/harness未実施。Android buildは全Androidファイルの監査完了後。
- `Mods/MapGen/AltFormProbe.cs` を全文とnative `.hpp/.cpp`、`ModEntry` の `-altprobe` 引数・`TraceDelay` 設定で照合。window/scene/player初期化、slot 1 の入力駆動、reset・settle・jump/morph・trial/reportの順序、終了条件と終了コードを確認。nativeの `std::min/max` を `MathF.Min/Max` 相当の `Runtime::MathMin/Max` にし、delay/frameの加算・減算・incrementをC#のunchecked `int` と同じにした。数値表示と列幅をcurrent-culture `ToString` / `StringPadLeft` に揃え、出力をUTF-8・行単位の `ConsoleWriteLine` に統一。Player/Keybindの参照取得もC#の `IReadOnlyList` / 配列境界・null例外へ合わせた。`git diff --check`通過、Windows Release `AltFormProbe.cpp.obj` compile green（既存 `offsetof` 警告のみ）。実マップ実行・runtime/harnessは未実施。native例外はC# `Exception.StackTrace` を保持しないため、その行は空出力のまま。
- `Mods/MapGen/MapReport.cs` を全文とnative `.hpp/.cpp`、`ModEntry` の `-q3shaders`/`-mapitems`/`-mapmaterials` 分岐・引数変換で照合。shader件数の first-seen 順と安定降順、pickup列挙・GroupBy/安定ソート、materialsの添字・表示順、例外報告と戻り値を確認。C#のUTF-16列幅/current-culture数値/UTF-8行出力、`IReadOnlyList`境界、unchecked件数加算、`.NET 10 MathF.Round(value, 2)`のfloat丸め経路を合わせた。再監査で見つけた集計行の`TrimEnd()`差（Unicode空白）と `Resolve() ?? Source` のlazy評価差も修正。`git diff --check`通過、Windows Release `MapReport.cpp.obj` compile green（既存`offsetof`警告のみ）。実マップ読込・runtime/harness未実施。
- `Mods/MapGen/Q3Import.cs` を全文とnative `.hpp/.cpp`、`MapCheck`/`MapPacker`/`MapBundle`/`Q3Convert`/`MapReport` の直接呼出しで照合。surface/patch生成順、collision brushのshell・clip・buried判定、texture bake、spawn/jump pad/item変換、pickup列挙を確認。shader prefixの最長判定をC# `string.Length`相当のUTF-16長へ、`Weld`の`Vector3.ComponentMin/Max`と`MathF.Max`を.NETのNaN/符号付きzero動作へ修正。LINQ `Min`のNaN先頭・途中時の選択順、unchecked件数加算、float floor→int変換も照合。verbose出力をcurrent-culture formatterとUTF-8行出力に統一し、texture bake失敗のcatch/messageをC#相当にした。`git diff --check`通過、Windows Release `Q3Import.cpp.obj` compile green（既存`offsetof`警告のみ）。実マップ読込・runtime/harness未実施。

### 2026-09-28 Section 11 Launcher portable監査

- `Mods/Launcher/Portable/NativeFilePicker.cs` 全文をnative `.hpp/.cpp`と照合。WindowsのSTA dialog/owner/filter/flags、LinuxのPATH検索順とzenity/kdialog引数、macOS osascript escaping、cancel時null・既存file確認・失敗ログを確認。`SetupScreen`/`PlayScreen`のpicker選択後処理と`Shell`のOwner/Suppressed設定も照合。nativeのPATH読込を `EnvironmentGetVariable`、tool stdoutを.NET互換UTF-8 decode、例外捕捉をC# `catch (Exception)`相当へ修正し、COM apartment cleanupをRAIIで保証した。`git diff --check`通過、Windows Release `fruity_mphread_native` compile/link green。Linux/macOS picker実行は未実施。
- `Mods/Launcher/Portable/LauncherPrefs.cs` 全文とnative `.hpp/.cpp`を照合。全既定値、launcher.txtのpath、行/キー/value trim、key別の空値・範囲条件、Invariant整数/Boolean/Hunter enum parsing、color clamp、window mode/geometry parser、保存キーと順序を確認。Shell/StartScreen/ModEntry/TextLauncher/各GUI・WindowGeometry/Diagnostics/Android MainActivity・PreviewService・AndroidApp のLoad/Save/Directory直接接続も照合。C# `catch (Exception)` に対しnative `Load`/`Save` が `std::exception` のみだったためcatch-allを追加。`git diff --check`通過、Windows Release `fruity_mphread_native` compile/link green。runtime未実施。Android buildは全Androidファイル監査完了後。
- `Mods/Launcher/Portable/RomWhitelist.cs` 全文、native `.hpp/.cpp`、`Program` のROM drag-and-drop入口、`GameFiles::RunSetup` の共通setup入口を照合。7件のMD5/label、lowercase化、認識時のlabel・不一致/読込失敗時のnullと拒否文を確認した。NativeRuntime監査は今回追加されたMD5 file helperだけに限定。`std::ifstream`を使っていたためC# `File.OpenRead`のUTF-8 path、`FileShare.Read`、open/read例外分類と異なっていた箇所を、同じ設定の `FileStream` に変更。`git diff --check`通過、Windows Release全体の `ninja -k 0` 成功。実ROM runtime未実施。Android buildは全Androidファイル監査完了後。
- `Mods/Launcher/Portable/TextLauncher.cs` 全文とnative `.hpp/.cpp`、`ModEntry` のGUI/text fallback・`-launcher`分岐、GameFiles setup、Updater、LauncherPrefs、room/mode/hunter選択、directory browse、online join、host request/start、AdventureSave、MatchStartの直接接続を照合。menu loop・EOF終了、設定の保存順、endpoint parse、各既定値と失敗分岐は一致。C# invariant `Int32.TryParse` に対しnativeのuint64桁あふれがwrapして有効値になる差を、乗算前の上限判定で修正。C#例外stack行がnativeで常に空だった差は、既存のnative stack captureを共通API化してconsoleへ出すよう修正。`git diff --check`通過、Windows Release全体の `ninja -k 0` 成功（既存`offsetof`警告）。UI runtime未実施。Android buildは全Androidファイル監査完了後。
- `Mods/Launcher/Portable/GameFiles.cs` 全文、native `.hpp/.cpp` と直接呼び出しを一ファイル単位で照合。paths.txtの読込・version境界・Paths初期化、ROM whitelist、desktop子プロセスの引数/作業ディレクトリ/標準入出力/10分timeout、Android in-process setup、ReportWriter、SetupScreen/TextLauncher/MatchStart/Shell/Android root設定の接続を確認した。`Version.TryParse` が受理する `-0` をnative parserが拒否する差を修正。POSIX timeoutを子孫プロセスにも適用する専用process groupを設け、入力pipeの `SIGPIPE` を全thread共通のsignal dispositionで抑止していた箇所を、呼出しthreadだけのsignal maskへ変更した。`git diff --check`通過、Windows Release全体の `ninja -k 0` 成功（既存 `offsetof` 警告）。POSIX build/runtime、実ROM抽出runtimeは未実施。Android buildは全Androidファイル監査完了後。
- `Mods/Launcher/Portable/LaunchPlan.cs` 全文、native `.hpp/.cpp`、Hunterのenum値とResolve/Reroll呼出し、LobbyContext/LaunchPlanの生成・保持・MatchStart利用を照合。Random hunterをNetLaunchのIdentify前に一度だけ確定する順序、front screen再表示時の再抽選、全Plan項目の既定値・enum値・copy/WithRoomKey相当を確認。C# recordのinit-only性に対してnative LobbyContextの値が変更可能だったため、3フィールドをconst化。`git diff --check`通過、Windows Release全体の `ninja -k 0` 成功（既存`offsetof`警告）。画面runtime未実施。Android buildは全Androidファイル監査完了後。
- macOS/Clang CIでGameFilesのDarwin signal-set macrosに対する`::`修飾と、macOS未提供の`sigtimedwait`を検出。`sigwait`を使うPOSIX共通処理へ変更し、再ビルドで確認中。
- Windows/MSVC CIで`Globalization.hpp::CharIsWhiteSpace`の非ASCII character literalがC2015になることを検出。該当コードポイントだけをC# `char.IsWhiteSpace`と照合してUnicode escapeへ置換し、NativeRuntimeの確認を変更範囲に限定した。`git diff --check`とWindows Release全体の `ninja -k 0` 成功。MSVC CIで再確認中。
- `Mods/Launcher/Gui/TapCheck.cs` 全文とnative `TapCheck.hpp/.cpp` の14ケースを順序・gesture・座標・期待値・出力まで照合し、C#/native両ModEntryの`-tapcheck`接続を確認。差異なし。Windows Release `FruityPrime.exe -tapcheck` は14/14成功。
- `Mods/Launcher/Gui/Tap.cs` 全文とnative `Tap.hpp/.cpp`、`TapCheck`のplain-value呼出しを照合。pointer種別、8点slopの境界、取消後の非復活、Releaseのpointer identityとinclusive bounds、Sidewaysのtie処理、Cancel後状態は一致。直接利用する行・slider・list・wordの呼出しとTopLevelのpointer所有/TouchEnd dispatch寿命も確認し、native pointerはgesture内でidentity比較のみ。差異修正なし（verified no-op）。Touch device runtimeは未実施。
- `Entities/BeamProjectileEntity.cs` をPR #1 base `dcdc900f` からC# head `fe453ce6` までの差分（+66/-33）で全hunk照合し、native `.hpp/.cpp` と直接接続を比較。projectile shot identity、stale ricochet parent拒否、continuous weapon phase/ammo・damage cadence、team ally時のlife drain/homing除外、diagnostic、pooled beam spawn transformはnative実装と一致。`ModLaunchKey`だけC# `internal set` に対しnative setterがpublicだったため、`NetPlayerLifecycle` friendだけが書けるprivate setterへ修正。`git diff --check`とWindows Release全体 `ninja -k 0` 成功（既存`offsetof`警告）。ゲームruntime未実施。
- `Entities/Players/PlayerCollision.cs` を同じPR base/head間の差分（+30/-1）で全hunk照合。衝突平面Y・penetration・factorからstepを作り、form別 `AltColRadius`/`BipedColRadius` の対称境界へclampしてposition.Yへ加える式はnativeの`std::clamp`と一致。PlayerInputからの`CheckCollision`呼出し箇所・前後の状態処理も照合し、native差分修正なし（verified no-op）。`git diff --check`通過、Windows Release全体 `ninja -k 0` 成功（no work to do）。ゲームruntime/`-altprobe`未実施。
- `Entities/NodeDefenseEntity.cs` をC#全文とPR #1 base/head間の全差分（+26/-20）、native `.hpp/.cpp`、PlayerHud/PlayerAi/MapAuditTeams/TeamGameplayTestの直接参照で照合。`NoTeam=-1`への変更とHUD伝播、team indexのunsigned境界判定、Active/aliveのcapture条件、`IsOccupied`・前回占有状態・`Complete`の全8 slot走査を確認し、native実装は一致。TeamVisualsの中立owner処理も両側で一致し、古いliteral 4 sentinel参照は残っていない。コード修正なし（verified no-op）。`git diff --check`通過。Windows Release `ninja -k 0` はno work to do。ゲームruntime/harness未実施.
- `Mods/Credits.cs` をC#全文・PR #1 base/head間の差分（+6/-2）、native `.hpp/.cpp`、ModEntryの`-credits`分岐、SettingsView/StartScreen/TextLauncherの直接参照で照合。全13 entryの名前・説明・URL・順序、`Summary`/`Compact`/`Names`、NoneGiven除外、SupportUrl click/fallbackとconsole出力を確認。nativeの独自`std::cout` writerはC# `Console.WriteLine`アダプターを迂回し、UTF-8処理・OS改行・行単位同期が一致しないため既存`ConsoleWriteLine`へ接続した。`git diff --check`通過、Windows Release `Credits.cpp.obj` compile・static library link・`FruityPrime.exe` link成功。`-credits` runtime未実施.
- `Entities/Enemies/18_AlimbicTurret.cs` をC#全文・PR #1 base/head間の差分（+2/-2）、native `.hpp/.cpp`、EnemySpawnEntity factoryとMetadataのEnemy18Subroutines callback登録で照合。Behavior00/02双方のtarget filterはBountyTeamsでTeamCount==2の時だけteam 0を除外し、3/4 teamsでは対象に含める。その他state timer、target acquisition/switch、shot count/damage、draw transform、Enemy18Values field type/layoutも一致。コード変更なし（verified no-op）。`git diff --check`通過、Windows Release `ninja -k 0` はno work to do。runtime未実施.
- `Entities/ItemInstanceEntity.cs` をC#全文・native `.hpp/.cpp`・PR #1差分（+2/-2）で照合。data/rotation、item typeとaffinity置換、artifact effect、parent追従、despawn/owner/story state、SFX、charged weapon attraction、scan更新、FH itemと描画transformを確認。変更点のoptional nullable pickerはnativeのdefault `nullptr` とownerへの転送に一致し、PlayerProcessは両側で`this`、HealthSimulationTestは両側で引数省略。ItemSpawnEntityのhealth pickup slot記録（null時`-1`）と通知も一致。差異なし・コード変更なし（verified no-op）。`git diff --check`通過、Windows Release `ninja -k 0` はno work to do。runtime未実施。
- `Entities/Players/HalfturretEntity.cs` をC#全文・native `.hpp/.cpp`・PR #1差分（+2/-1）で照合。コンストラクタ/health分割、状態timer、damage/freeze/burn、nearest-target探索、照準/発射、fall collision、HUD、描画/animation/light、double-damage material overridesを比較。変更されたself/死亡/allied-team/alphaのtarget filterは一致し、`TeamRules::AreAllies` と native 側実装もC#と同じ条件。`61 * uint damage`はC# binary numeric promotionでlongとなるため、nativeのint64積は同じ。差異なし・コード変更なし（verified no-op）。`git diff --check`通過、Windows Release `ninja -k 0` 成功（既存`offsetof`警告のみ）。runtime未実施。
- `Entities/Players/PlayerDraw.cs` をC#全文・native `.hpp/.cpp`・PR #1差分（+1/-9）で照合。DrawのSpire alt-attackは両側で`AnimateSpireAltAttack`を呼び、collision poseは両側のsimulation `Process`から`UpdateSpireAltCollisionPose`を呼んでanimation後の左右rock位置を更新する。描画分岐、LOD、biped/alt-form、shadow、render-item traversal、double-damage texgen、morph-ball trail、death particle、volume表示も照合し一致。コード変更なし（verified no-op）。`git diff --check`通過、Windows Release `ninja -k 0` はno work to do。runtime未実施。
- `Entities/Players/PlayerHud.cs` の全メソッドとPR #1差分（+376/-94）をnative `.hpp/.cpp`・直接呼出しと照合。基底C# blobはnative移植開始時の`992f4884`およびPR base `dcdc900f`と同一、現行C# blobはPR head `fe453ce6`と同一で、移植後のC#変更分はPR差分に限定される。weapon wheelの初期位置/close・gamepad/absolute/drag優先順、radarの描画条件・座標変換・blip、team scoreboard/node表示、weapon order、health visibility、opponent health、HUD draw順と補助実装の接続を照合。`ProcessHudNodes`/`DrawNodesBonuses`のnative `_teamNodeCounts`動的添字が`std::array::operator[]`で範囲外時UBとなりC#配列の`IndexOutOfRangeException`と異なるため、全動的添字を`ManagedAt`へ変更。`git diff --check`通過、Windows Release `ninja -k 0` 成功（PlayerHud compile・native library / FruityPrime.exe link、既存warnings）。runtime未実施。
- `Entities/Players/PlayerInput.cs` のPR #1差分（+185/-40）全hunkをnative `.cpp/.hpp`および直接接続と照合。RemoteIntentのfresh判定と`ContinuousPhase.Observe`、stylusの独立WeaponMenu hold/TakePressed、weapon menu終了時のwheel close、射撃結果・`NoteFired`・haptic feedback、boost flick方向/aim lock/速度投影、spectatorのGameplay限定Back入力、PointerBindingsの主ボタン解決と`UpdatePointer`を確認。RemoteIntent配列はslot境界確認済みで、受信threadはinboxへenqueueし`Pump`がgame thread上で状態を更新するためこの読出しとのdata raceなし。nativeの他の動的配列参照も範囲確認・範囲内loop・固定5要素配列のindex 4、または`ManagedAt`/`.at()`で保護され、C#配列アクセス相当の境界動作を維持。PR base/current C# blobは`992f4884`/`fe453ce6`の対応する移植前・PR headと一致。監査後のnative変更であるBombSpawn診断5 counterの`IncrementInPlace`も、C#の既定unchecked `++`と同じwrapを保ちsigned-overflow UBを避けることを確認。差異・memory-safety問題は見つからず、コード修正なし（verified no-op）。`git diff --check`通過。runtime未実施、今回の監査後build未実施。
- `Entities/Players/PlayerProcess.cs` のPR #1差分（+40/-6）全hunkをnative `.cpp/.hpp`および直接接続と照合。remote respawn要求条件、`_boostAimLock`のtick減算、Spire alt attackのdraw非依存collision pose更新、pickupのauthority/despawn filter、health pickup音、pickerを渡す`OnPickedUp(this)`、Spire animation/pose更新の順序を確認。`RespawnRequested`のslot checkと`NetHealthSync::OwnsPickup`/`MapResourceRules::IsHealth`もC#条件と一致。全C#配列参照のnative側は`ManagedAt`等のchecked access、固定index、同じ長さで制限したloopを使用し、respawn候補の`valid[index]`は非空確認後に`FrameCount % valid.size()`で範囲を限定。C#移植前/PR base blobは`992f4884`/`dcdc900f`で一致、現行C# blobはPR head `fe453ce6`と一致。差異・範囲外アクセスは見つからず、コード修正なし（verified no-op）。`git diff --check`通過。runtime・今回の監査後build未実施。
- `Entities/ItemSpawnEntity.cs` をPR #1 base `dcdc900f`からhead `fe453ce6`まで監査。C#移植前blobは`992f4884`とbaseの両方で`95034a77`、head blobは`c3f626ad`。実差分は`+40/-1`（表記を`+39/-1`から訂正）。Initialize時のNetHealthSync登録、available state field order、replica state適用・item再生成・local pickerだけへのhealth音、spawnCount/cooldown/last-picker更新、picker付き`OnItemPickedUp`を照合した。NetHealthSyncのWrite/Process呼出し、PlayerProcessの`OnPickedUp(this)`、HealthSimulationTestの省略picker（native default null）まで確認。唯一の動的添字`Players[localSlot]`は双方で`0 <= localSlot < Count/size`の確認後に参照し、nativeはさらにnullを`RequireReference`でC# null dereference相当に処理。unchecked index・data race・差異は見つからず、コード修正なし（verified no-op）。`git diff --check`通過。build/runtime未実施。
- `Entities/BombEntity.cs` のPR #1差分（+37/-8）全hunkをnative `.cpp/.hpp`・PlayerEntity/TeamRules/LockjawTrailNoise直接接続と照合。allied-team爆弾判定、Lockjaw visual tickのProcess更新とSpawn reset、RNG draw audit、tick/slot/source/target/segment/axis依存noiseを確認。C#移植前blobは`992f4884`とPR base `dcdc900f`で`dc680892`、head blobは`a28b562c`。native `ModLockjawDrawRngChanges`だけ通常の`++int`がC# unchecked incrementと異なり符号付きoverflow UBだったため、`NativeRuntime::IncrementInPlace`へ変更してwrap semanticsを一致。Sylux爆弾のnative配列アクセスはサイズclamp後のindex比較、`ManagedAt`、またはC#固定index相当の`BombAt`で範囲/Nullを検査し、描画vertex配列も要求count・segment loop内。ほかの差異・範囲外アクセスはなし。`git diff --check`通過、Windows Release `ninja -k 0`成功。runtime未実施。
- `Program.cs` のPR #1差分（+36/-2）全hunkをnative `.cpp/.hpp`、Main entrypoint、CrashReport/ConsoleSetup/RomWhitelistの直接呼出しと照合。CrashReport install-before-Run、startup error/exit code、paths version failureとROM whitelist拒否時のpause、認識label表示後のExtract接続が一致。C#移植前blobは`992f4884`とPR base `dcdc900f`で`8defcec7`、head blobは`2bc3ffa0`。nativeの起動catchだけ`std::exception`限定でC# `catch (Exception)`より狭かったため`catch (...)`へ変更し、unknown exceptionも`CrashReport::Report(current_exception())`へ渡すよう一致させた。Program.cppの配列参照はcomponentCount/args.size()/valid.size()のガード内、PairRangeはindex<sizeの範囲。`git diff --check`通過、Windows Release `ninja -k 0`成功。runtime未実施。
- `Utility/Console.cs` のPR #1差分（+36/-2）をnative `.cpp/.hpp`と直接呼出し先に照合。移植前blobは`0fc8e2b5`、PR head/current blobは`770f94ac`。InvariantCulture設定、変更前のcwdを`LaunchDirectory`に保存してから`AppPaths::PrepareUserData`とUserDataDirectoryへ移動する順序、Windows標準出力のVT設定が一致。`LaunchDirectory`はimmutable stringをatomic shared pointerで公開し、PauseIfInteractiveはredirect判定後にReadKeyし、C#と同じInvalidOperation/IO例外を無視して終了する。ProgramとExtractのC#呼出しもnative側で対応するPauseIfInteractiveへ接続されている。今回使うNativeRuntimeのConsoleIsInputRedirected/ConsoleReadKeyだけを追跡し、redirect・端末なし・読み取り失敗で待機処理が異常終了しないことを確認。差異・範囲外アクセスは見つからず、コード修正なし（verified no-op）。`git diff --check`通過。runtime/build未実施。
- `Mods/DebugLog.cs` のPR #1差分（+6/-1）と現行C#例外出力をnative `DebugLog.cpp/.hpp`と照合。C# base blobは`af7560c9`、PR head/current blobは`78a6d603`。ログ名への`Environment.ProcessId`追加位置と10進値は一致。C# `Exception`の本文・`ex.StackTrace`・InnerException再帰順、null stack trace時の1行出力も照合。C++標準例外はCLRと異なりthrow-site stackを保持しないため、Androidはcatch/log境界のnative call stackを最大64フレーム取得し、`dladdr`でsymbolまたはmodule offsetを出し、解決できない場合もaddressを記録する方式とした。NDK API 24で使えない`backtrace()`には依存せず、`_Unwind_Backtrace`/`dladdr`を使用。例外stack traceの行位置・改行とInnerException順を維持し、Androidの`CaptureStack`/`StackFrom`/`Stack`も実装。Windows fixed buffer、Android fixed capture buffer、POSIX symbol bufferの範囲・所有を確認。C#監査で他の差異・範囲外アクセス・寿命切れ参照なし。Windows Release `ninja -k 0` 成功、Android arm64-v8a/x86_64 `tools/build/build-android-cpp.bat all` 成功。`git diff --check`通過。runtime未実施。

- Windows/MSVC CI run `36342122622` の失敗を確認し、`NativeRuntime/Avalonia/Base.hpp` の `Rect(Point, Point)` からMSVCでconstexpr評価できない `std::abs` を含むコンストラクターの `constexpr` 指定だけを外した。幾何計算式は変更なし。`git diff --check`、Windows Release `ninja -k 0`、Android arm64-v8a・x86_64各75段階の最終build成功。MSVC修正後CI待ち。

- 修正後のMSVC run `36344368855` は `Rect` のC3615を越え、次に `NativeRuntime/System/Net.cpp` のWindows SDK `netioapi.h` includeで失敗した。Windowsの `if_nametoindex` は `iphlpapi.h` 経由で宣言されるため、`windows.h` 後に `<iphlpapi.h>` をincludeする形へ変更。C#側のIP scope parsingや実行時の式は変更していない。`git diff --check`・Windows Release `ninja -k 0` 成功。MSVC再CI待ち。

- MSVC run `36346606444` は `Number.cpp` のUnicode非ASCII文字リテラル（C2015）と `Runtime.cpp` のソース内NUL文字リテラル（C2137）で失敗。Numberのdash/NBSP/NNBSPは同一Unicode値のescape表記へ変更し、RuntimeのNUL値は `\0` 表記へ変更。C#相当の数値token比較・command-line null separatorの値は不変。`git diff --check` とWindows Release `ninja -k 0` 成功。修正後MSVC CI待ち。

- 修正後のNative C++ CI run `36348842034`（commit `8c67ca37`）はWindows/MSVC成功。run `36348842030` Android、`36348842046` Linux/GCC、`36348842020` macOS/Clangも成功。総合build run `36348842072` はWindows dedicated-server startup contract（プロセスが起動拒否後も稼働しexit 1にならない）とLinux bounded thumbnail worker regression（C# `ThumbnailBatch.StopWorker`内の`Process.Dispose`でNullReferenceException）の2 step失敗。今回のNativeRuntime portability修正との因果は未確認。

- 総合build run `36350578764`（source `6bfac073`）は全job成功。直前run `36348842072` で一度失敗したWindows dedicated-server startup contractとLinux bounded thumbnail worker regressionも、再実行ではそれぞれ成功し、今回のNativeRuntime portability修正との関連は認められなかった。Native C++ platform CI run `36348842034`（MSVC）・`36348842030`（Android）・`36348842046`（Linux/GCC）・`36348842020`（macOS/Clang）もすべて成功。
