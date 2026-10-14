# Mods クラシックメニューの多言語対応

## 責務分離

- `Languages/en.json` / `ja.json` / `fr.json`: **翻訳定義のみ**。同じ安定IDを各言語で持つ。英語の表示文言を変更しても翻訳IDを変更しない。
- `Languages.qrc` と CMake: **配布のみ**。3つのJSONをQtリソースとしてアプリ・テストに組み込み、OS固有のファイル探索に依存しない。
- `MenuLocalization.hpp/.cpp`: **翻訳解決のみ**。JSONロード、キー整合性検証、ID検索、ROM英語文字列互換の検索、英語フォールバック。
- `LauncherPrefs`: **保存のみ**。`launcher.txt` の `menu_language=en|ja|fr`。ゲーム本体の `MenuSettings::Language` は参照・変更しない。
- `Composer`: **UI操作のみ**。Optionsのメニュー言語項目で English → 日本語 → Français を切り替える。保存処理はHostに委譲する。
- `MenuStrings`/`MenuEngine`: **レンダリング用の表示文言取得のみ**。ROM英語文字列と内部IDは変更しない。
- `MenuUnicodeFont`: **非ASCII文字の描画のみ**。Qtのプラットフォームフォントで初回だけグリフを作り、テクスチャキャッシュを再利用する。

## 翻訳IDの規則

`menu.begin_game` のように意味を表す固定IDを使う。JSONのキーを日本語訳や英語の文章にしてはならない。

`Languages/en.json` は `ID → 英語原文`、`ja.json`・`fr.json` は同じIDから訳文への対応。既存ROMの描画箇所では、英語原文からIDへの索引を**初回のみ**作ることで操作ロジックを壊さずに移行できる。新しいUIは `TranslateMenuKey("menu.some_action", language)` を直接呼ぶ。

翻訳の欠落・キー数の不一致・JSON破損・英語原文の重複はテストで検出する。未登録の動的テキストは入力そのままを表示し、名前や通信プロトコルの値を翻訳しない。

## テスト

`ctest --test-dir <build> -R FruityPrime.MenuLocalization --output-on-failure`

この単体テストは Qt Core のみ使用し、実際の組込JSON、翻訳ID、英語互換検索、フォールバック、改行とUnicodeを確認する。Release構成でも検査が無効にならないようassertではなく終了コードで判定。

画面確認項目: タイトル、Options、設定、サーバー一覧、ロビー、ポーズ、リザルトの各ページで3言語を確認し、再起動して保存が反映されることを確認する。ゲーム本体の言語が変わらないことも確認する。
