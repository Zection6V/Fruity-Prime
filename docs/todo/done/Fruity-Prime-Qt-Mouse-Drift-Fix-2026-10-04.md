# 横方向のマウス移動による照準の下降

Qtのカーソル固定は、各移動後にウィンドウ中央へカーソルを戻す。
奇数の幅・高さで中央を小数として保持すると、整数へ丸めた実際の戻し先との
0.5ピクセル差が、次の移動すべてに加算されていた。
基準を実際の整数の戻し先に統一した。

最新MSVC Release、ALINOS PERCHの実際の対戦ウィンドウ（borderless 2560x1439、
DPI倍率1）で `FRUITY_MOUSECHECK=1 -shellshot ...` を実行した。
QtのMouseMoveイベント経路へ256回の左右移動と各re-centreイベントを渡す検査は、
修正前に横0・縦128の累積でFAIL、修正後はOpenGL/Vulkan両方で横0・縦0となりPASS。
縦方向の入力が4ピクセル分正しく届き、逆方向へ戻せることも確認した。
Settings Applyによる対戦中renderer切替3回もPASSし、各新ウィンドウで入力検査がPASS。
これはイベントを注入した回帰検査であり、手動でマウスを振った実測とは区別する。

証拠は `tools/build/out/mouse-before.log`、`mouse-after-{opengl,vulkan}.log`、
`mouse-vertical-switch.log`。通常起動では診断を実行しない。

最新のFire入力でlocal playerの出現を待つshell checkでも再検査した。
`vertex-cache-validation-switch.log`はsimulation frame 6で出現を確認し、
その後のSettings Apply・3回の切替・各新windowの入力検査がPASSした。
