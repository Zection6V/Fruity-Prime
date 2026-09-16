# MphRead.Native 未実装ペア - 現在の推奨実装順

- 対象リポジトリ: `Zection6V/Fruity-Prime`
- 対象ブランチ: `develop2`
- 基準: 前回の dependency-first 77件リスト
- 現在ペア済みとして除外: **5件**
- 残り: **72件**
- 並び順: 前回の dependency-first 順を維持し、除外後に再採番

## 今回除外したペア済み 5件

- `src/MphRead/Metadata/Weapons.cs`  （旧 #1 / P1 / W7）
- `src/MphRead/Mods/Render/SmoothHudIcon.cs`  （旧 #4 / P2 / W8）
- `src/MphRead/Mods/Render/PlayerEntityProHud.cs`  （旧 #5 / P2 / W8）
- `src/MphRead/Mods/Render/HunterPreview.cs`  （旧 #6 / P2 / W8）
- `src/MphRead/Mods/ThumbnailGenerator.cs`  （旧 #29 / P5 / W8）

## 残りの推奨実装順

| # | Phase | 元Wave | C# source |
|---:|:---:|:---:|---|
| 1 | P2 | W8 | `src/MphRead/Selection.cs` |
| 2 | P2 | W8 | `src/MphRead/Renderer.cs` |
| 3 | P2 | W8 | `src/MphRead/Mods/ScreenCapture.cs` |
| 4 | P3 | W11 | `src/MphRead/Mods/Launcher/Portable/LauncherPrefs.cs` |
| 5 | P3 | W11 | `src/MphRead/Mods/Launcher/Portable/GameFiles.cs` |
| 6 | P4 | W9 | `src/MphRead/MemoryClasses.cs` |
| 7 | P4 | W9 | `src/MphRead/Memory.cs` |
| 8 | P4 | W9 | `src/MphRead/Utility/RepackEntity.cs` |
| 9 | P4 | W9 | `src/MphRead/Utility/RepackModel.cs` |
| 10 | P4 | W9 | `src/MphRead/Utility/RepackCollision.cs` |
| 11 | P4 | W9 | `src/MphRead/Mods/MapGen/RawStructs.cs` |
| 12 | P4 | W9 | `src/MphRead/Mods/MapGen/Q3Bsp.cs` |
| 13 | P4 | W9 | `src/MphRead/Mods/MapGen/MapReport.cs` |
| 14 | P4 | W9 | `src/MphRead/Mods/MapGen/MapTextureBake.cs` |
| 15 | P4 | W9 | `src/MphRead/Mods/MapGen/MapDefinition.cs` |
| 16 | P4 | W9 | `src/MphRead/Mods/MapGen/MapCollisionPacker.cs` |
| 17 | P4 | W9 | `src/MphRead/Mods/MapGen/MapNodePacker.cs` |
| 18 | P4 | W9 | `src/MphRead/Mods/MapGen/MapBuilder.cs` |
| 19 | P4 | W9 | `src/MphRead/Mods/MapGen/Q3Import.cs` |
| 20 | P4 | W9 | `src/MphRead/Mods/MapGen/MapBundle.cs` |
| 21 | P4 | W9 | `src/MphRead/Mods/MapGen/MapPacker.cs` |
| 22 | P4 | W9 | `src/MphRead/Mods/MapGen/CustomRooms.cs` |
| 23 | P4 | W9 | `src/MphRead/Mods/MapGen/Q3Convert.cs` |
| 24 | P5 | W8 | `src/MphRead/Mods/ThumbnailBatch.cs` |
| 25 | P5 | W8 | `src/MphRead/Mods/ThumbnailCapture.cs` |
| 26 | P6 | W10 | `src/MphRead/Mods/Update/UpdateCheck.cs` |
| 27 | P6 | W10 | `src/MphRead/Mods/Update/UpdateDownload.cs` |
| 28 | P6 | W10 | `src/MphRead/Mods/Update/UpdateInstall.cs` |
| 29 | P6 | W10 | `src/MphRead/Mods/Update/DesktopUpdate.cs` |
| 30 | P6 | W10 | `src/MphRead/Mods/Update/ServerUpdate.cs` |
| 31 | P6 | W10 | `src/MphRead/Mods/Update/Updater.cs` |
| 32 | P7 | W11 | `src/MphRead/Mods/Input/GamepadMappings.cs` |
| 33 | P7 | W11 | `src/MphRead/Mods/Input/GamepadDesktop.cs` |
| 34 | P7 | W11 | `src/MphRead/Mods/Input/GamepadProbe.cs` |
| 35 | P7 | W11 | `src/MphRead/Mods/DebugLog.cs` |
| 36 | P7 | W11 | `src/MphRead/Mods/LogShare.cs` |
| 37 | P7 | W11 | `src/MphRead/Mods/Launcher/Portable/AdventureSave.cs` |
| 38 | P7 | W11 | `src/MphRead/Mods/Launcher/Portable/MatchStart.cs` |
| 39 | P7 | W11 | `src/MphRead/Mods/Network/MapVote.cs` |
| 40 | P7 | W11 | `src/MphRead/Mods/EndScreen.cs` |
| 41 | P7 | W11 | `src/MphRead/Mods/Launcher/Portable/TextLauncher.cs` |
| 42 | P7 | W12 | `src/MphRead/Mods/WindowMode.cs` |
| 43 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/GuiTheme.cs` |
| 44 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/Rows.cs` |
| 45 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/MenuEntry.cs` |
| 46 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/SliderRow.cs` |
| 47 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/KeyRow.cs` |
| 48 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/PadRow.cs` |
| 49 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/ServerRow.cs` |
| 50 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/UpdateBadge.cs` |
| 51 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/DemoPickerView.cs` |
| 52 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/MapPickerView.cs` |
| 53 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/SettingsView.cs` |
| 54 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/HomeView.cs` |
| 55 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/SplashView.cs` |
| 56 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/UiCapture.cs` |
| 57 | P8 | W12 | `src/MphRead/Mods/Render/PlayerEntityEndScreen.cs` |
| 58 | P8 | W12 | `src/MphRead/Mods/Render/PreviewPass.cs` |
| 59 | P8 | W12 | `src/MphRead/Mods/PauseMenu.cs` |
| 60 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/PauseMenuView.cs` |
| 61 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/SettingsWindow.cs` |
| 62 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/PauseMenuWindow.cs` |
| 63 | P8 | W12 | `src/MphRead/Mods/Launcher/Gui/GuiLauncher.cs` |
| 64 | P9 | W13 | `src/MphRead/Testing/TestEffects.cs` |
| 65 | P9 | W13 | `src/MphRead/Testing/TestLogic.cs` |
| 66 | P9 | W13 | `src/MphRead/Testing/TestMisc.cs` |
| 67 | P9 | W13 | `src/MphRead/Testing/TestOverlay.cs` |
| 68 | P9 | W13 | `src/MphRead/Testing/TestParse.cs` |
| 69 | P9 | W13 | `src/MphRead/Testing/TestPlayer.cs` |
| 70 | P9 | W13 | `src/MphRead/Testing/TestPrint.cs` |
| 71 | P9 | W13 | `src/MphRead/Testing/TestWeapons.cs` |
| 72 | P9 | W13 | `src/MphRead/Test.cs` |

## Phase の意味

- **P2**: Renderer / Selection / HUD / preview / capture の基盤。
- **P3**: 後続 MapGen / thumbnail が直接参照する portable launcher 基盤。
- **P4**: W9 の Memory / Repack / MapGen closure。
- **P5**: Thumbnail の process / generator / capture closure。
- **P6**: UpdateCheck から install/orchestration へ積み上げる更新系。
- **P7**: W11 の input mapping、logging、portable launcher、vote/end-screen。
- **P8**: W12 の GUI と late Scene/PlayerEntity partial closure。
- **P9**: production 実装を消費するテスト / oracle 群。

## 注記

- 判定条件は、対応する Native `.cpp` と `.hpp` の **両方が現在の `develop2` に存在すること**。
- ペアの存在だけを判定しており、この一覧は strict parity 合格・ビルド成功・監査完了を意味しない。
- C# 側に相互参照がある SCC は、前回と同様にグループ全体が揃うまで closure 完了扱いにしない。
