# GPU mesh描画とOpenGL検索領域の改善

同じmesh・material・passで、DSのprimitive rangeごとに出していたGPU drawを
1つのtriangle listへまとめた。rangeごとの三角形化を維持し、stripの向き、
描画順、未完の末尾、terminal attribute stateを変えない。OpenGL desktopと
Vulkan共通のindex構築を使う。毎フレームの三角形化やGPU readbackは増やさない。

OpenGLのVAO cache検索は、各drawでkey vector、buffer名vector、attributeの
unordered_setを確保していた。keyの作業領域をcommand listで再利用し、
buffer名はcache miss時だけ構築する。attribute重複・範囲・buffer lifetimeの
検査と、buffer解放時のVAO retirementは維持する。

## 実際の対戦での測定

2026-10-04、最新MSVC Release、RTX 5070 Ti / driver 617.14、Qt、
AD2 ALINOS PERCH、borderless 2560x1439、Sylux＋3 bots、Low Latency Off、
通常設定Unlimited、Immediate、resolution scale 100、fog on、FPS counter off。
同一test directoryで、変更前→変更後→変更前→変更後の順に起動した。
Fire入力でlocal playerのActive・Spawned・Healthを確認し、出現後10秒を描画。
各runのwarmup後、`main_active=1,paused=0,focused=1` の最初の8区間の平均を比較した。

| 比較 | 変更前1 / 2 | 変更後1 / 2 |
|---|---:|---:|
| mesh batchingのみ、OpenGL | 388.58 / 365.66 FPS | 383.32 / 387.03 FPS |
| mesh batchingのみ、Vulkan | 430.08 / 437.59 FPS | 427.15 / 437.24 FPS |
| batching済みOpenGLでVAO検索領域を再利用 | 381.25 / 386.52 FPS | **518.93 / 518.01 FPS** |

batching単独のFPS差はばらつきの範囲。VAO検索のheap確保を除いたOpenGLは
2回とも約35%改善した。Unlimitedは出現後の通常設定でも500 FPSを超えた。
この条件でVulkanが1000 FPSに戻ったという証拠はない。
以前のFire入力前の結果は対戦中の性能値として使わない。

証拠はignored `tools/build/out/active-paired-{base1,new1,base2,new2}.csv`、
`active-paired-vk-{base1,new1,base2,new2}.csv`、
`vertex-cache-{base1,new1,base2,new2}.csv` と各log・capture。

## 検証

- MSVC Release full build、CTest 21/21、RHI isolation・frontend GL・legacy GL・shader interface audit PASS。
- mixed topology・strip winding・incomplete tail・不正range・empty meshの回帰検査 PASS。
- `-rhiconformance -noupdate` PASS（OpenGL/Vulkan、recording mutation、binding lifetime、session ownership、async readback）。
- 最新buildの`-vkvalidation`付きshell checkで、local player出現後のSettings Apply・renderer切替3回 PASS。
  各windowの256回左右mouseイベントは累積横0・縦0、縦入力も維持。
- OpenGL `-gpulifetime "AD2 ALINOS PERCH" -cycles 3 -frames 8` PASS。
  各release後は全GPU resource count・retiredが0、draw中の追加host waitは0。
- Vulkan resource checkはvalidation error 0、live/retired 0。
- Qtの既存golden capture harnessは`The current context has no OpenGL session.`で停止するため、
  この変更のpixel単位のgolden一致は未検証。対戦windowのcaptureでroom・gun・HUDを確認した。

ローカル検証logは`vertex-cache-build.log`、`vertex-cache-ctest.log`、
`vertex-cache-rhiconformance.log`、`vertex-cache-validation-switch.log`、
`vertex-cache-gpulifetime.log`、`final-vulkan-resource.log`。
