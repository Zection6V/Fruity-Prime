# C# 側の新規コミットを C++ へ移す作業表（2026-09-24 時点）

`origin/newest` の 212 コミットを develop2 に取り込んだ時点で、
`src/MphRead.Native/` は `src/MphRead/` より 302 ファイル分古い。
その差分を、下から（依存される側から）順に埋めるための一覧。

列の意味 — **S**: C# 側の変更種別 (A 追加 / M 変更 / D 削除)、
**+/-**: C# の追加・削除行数、**対**: 既にある C++ の対応物
（`-` は新規に書き起こすもの）。

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

- 済: 1 Platform helpers / 3 Mods leaves の大半 / 8 Multiplayer・teams
  （ea3398e9 まで）。
- 9 Network — 進行中:
  - 済 (f661cce0 ほか): NetProtocol（protocol 14）、NetSession・NetSessionLobby、
    SessionProtocol、LobbyRules、MatchDefinition、NetLifecycleTracker、
    NetPlayerLifecycle、ContinuousWeaponPhase、FormReconciliation、NetFaultQueue、
    NetMatchTimeSync、NetHealthSync、NetHudHealth、NetShotDiagnostics、
    NetTimingDiagnostics、NetSmoothing、NetHitClaims（C# と逐行照合済み）、
    NetHitPrediction、NetDamage、NetUnlagged、NetPlayerBridge、NetHooks、
    NetTransport＋NetLag、NetRoomChange、NetStatus、NetSlotManager、NetMatchSync、
    NetDiagnostics、NetFeatureCheck、DemoPlayback、MechanicsDump、MapRotation、
    MapAudit（＋Render/LockjawTrailProbe）、NetLog、ServerSim、ServerSimCheck、
    NetTestScript、HitRig、NetCheckClient、NetLaunch（TickTerminalLobby を除く）、
    Chat/NetChat、MapPick、PlayerEntityNetAim/NetHud の網関連分。
  - 付随: BeamProjectile / ItemSpawn / ItemInstance / PlayerEntity・Process・
    Collision・Draw の網関連差分、NativeRuntime に Guid・BinaryPrimitives・
    CharIsControl・StringSplit・StringReplaceOrdinalIgnoreCase・
    Console.KeyAvailable/ReadKeyInfo・EndPointEquals。
  - 済: DedicatedServer＋LobbyCommands（全面書き直し）、HostPool（新規）、
    NetMaster（HostCandidate・FindHosts・所有者トークン・CanHost フラグ）、
    NetHostSession、ModEntry の -server 部（-hostports・-affinityweapons）。
  - 残り: LocalServer、NetCombatCheck、NetLobbyTest、
    HealthSimulationTest、NetHealthSyncTest、MapAuditTeams、SpireAltPoseCheck。
- 保留（依存先の移植待ち）:
  - PlayerEntity::TakeDamage の AimAssistTelemetry::Hit と ModControllerFeedback、
    PlayerSound の着地フィードバック、PlayerEntityNetAim::ApplyGamepadAim の
    照準補助・スコープ感度 → 6（入力）の後。
  - NetLaunch::TickTerminalLobby → Renderer の HasScene/EndScene と
    MatchStart::Begin(window, …)（1 ウィンドウ化）の後。

## 1. Platform helpers — 2 ファイル (新規 2), C# +73 行

| S | +/- | C# | C++ |
|---|---|---|---|
| A | +40/-0 | `Mods/Platform/AppPaths.cs` | — 新規 |
| A | +33/-0 | `Mods/Platform/WebLink.cs` | — 新規 |

## 2. Diagnostics — 5 ファイル (新規 5), C# +497 行

| S | +/- | C# | C++ |
|---|---|---|---|
| A | +149/-0 | `Mods/Diagnostics/CompatibilityCheck.cs` | — 新規 |
| A | +103/-0 | `Mods/Diagnostics/LauncherWindowCheck.cs` | — 新規 |
| A | +101/-0 | `Mods/Diagnostics/PlatformDiagnostics.cs` | — 新規 |
| A | +83/-0 | `Mods/Diagnostics/ThumbnailWindowCheck.cs` | — 新規 |
| A | +61/-0 | `Mods/Diagnostics/GlfwPathCheck.cs` | — 新規 |

## 3. Mods leaves — 13 ファイル (新規 3), C# +1597 行

| S | +/- | C# | C++ |
|---|---|---|---|
| A | +516/-0 | `Mods/MapPick.cs` | — 新規 |
| A | +339/-0 | `Mods/WindowGeometry.cs` | — 新規 |
| M | +164/-89 | `Mods/ThumbnailBatch.cs` | .cpp,.hpp |
| A | +150/-0 | `Mods/CrashReport.cs` | — 新規 |
| M | +135/-28 | `Mods/EndScreen.cs` | .cpp,.hpp |
| M | +76/-5 | `Mods/WindowMode.cs` | .cpp,.hpp |
| M | +63/-6 | `Mods/ScreenCapture.cs` | .cpp,.hpp |
| M | +47/-15 | `Mods/InputSettings.cs` | .cpp,.hpp |
| M | +43/-0 | `Mods/RenderOptions.cs` | .cpp,.hpp |
| M | +40/-0 | `Mods/SpectatorMode.cs` | .cpp,.hpp |
| M | +10/-18 | `Mods/ThumbnailCapture.cs` | .cpp,.hpp |
| M | +8/-7 | `Mods/GameSettings.cs` | .cpp,.hpp |
| M | +6/-1 | `Mods/ThumbnailLog.cs` | .cpp,.hpp |

## 4. Update — 4 ファイル (新規 0), C# +152 行

| S | +/- | C# | C++ |
|---|---|---|---|
| M | +139/-3 | `Mods/Update/UpdateCheck.cs` | .cpp,.hpp |
| M | +7/-0 | `Mods/Update/Updater.cs` | .cpp,.hpp |
| M | +4/-1 | `Mods/Update/DesktopUpdate.cs` | .cpp,.hpp |
| M | +2/-10 | `Mods/Update/BuildVersion.cs` | .cpp,.hpp |

## 5. Chat — 2 ファイル (新規 1), C# +36 行

| S | +/- | C# | C++ |
|---|---|---|---|
| A | +32/-0 | `Mods/Chat/NetChat.cs` | — 新規 |
| M | +4/-1 | `Mods/Chat/ChatBox.cs` | .cpp,.hpp |

## 6. Input and gamepad — 52 ファイル (新規 44), C# +4665 行

| S | +/- | C# | C++ |
|---|---|---|---|
| A | +350/-0 | `Mods/Input/PointerCheck.cs` | — 新規 |
| A | +309/-0 | `Mods/Input/PadBindingState.cs` | — 新規 |
| A | +301/-0 | `Mods/Input/GamepadChecks.cs` | — 新規 |
| A | +264/-0 | `Mods/Input/WindowsPenInput.cs` | — 新規 |
| M | +261/-34 | `Mods/Input/StylusZone.cs` | .cpp,.hpp |
| A | +258/-0 | `Mods/Input/MouseFlick.cs` | — 新規 |
| A | +210/-0 | `Mods/Input/GamepadManager.cs` | — 新規 |
| A | +199/-0 | `Mods/Input/GamepadProfiles.cs` | — 新規 |
| A | +158/-0 | `Mods/Input/GamepadEnhancementChecks.cs` | — 新規 |
| A | +141/-0 | `Mods/Input/WeaponWheel.cs` | — 新規 |
| A | +117/-0 | `Mods/Input/GamepadOptionState.cs` | — 新規 |
| A | +116/-0 | `Mods/Input/GamepadUiRouter.cs` | — 新規 |
| A | +115/-0 | `Mods/Input/AimAssist/AimAssistWorld.cs` | — 新規 |
| A | +111/-0 | `Mods/Input/PointerDevice.cs` | — 新規 |
| A | +101/-0 | `Mods/Input/ControllerRuntimeChecks.cs` | — 新規 |
| M | +99/-11 | `Mods/Input/GamepadMappings.cs` | .cpp,.hpp |
| A | +87/-0 | `Mods/Input/AimAssist/AimAssistTelemetry.cs` | — 新規 |
| A | +85/-0 | `Mods/Input/GamepadMappingWizard.cs` | — 新規 |
| M | +82/-117 | `Mods/Input/GamepadInput.cs` | .cpp,.hpp |
| M | +79/-274 | `Mods/Input/GamepadDesktop.cs` | .cpp,.hpp |
| A | +77/-0 | `Mods/Input/WindowsGamepadHaptics.cs` | — 新規 |
| A | +76/-0 | `Mods/Input/AimAssist/AimAssistChecks.cs` | — 新規 |
| A | +76/-0 | `Mods/Input/GamepadPlatformChecks.cs` | — 新規 |
| A | +72/-0 | `Mods/Input/AimAssist/AimAssist.cs` | — 新規 |
| M | +71/-47 | `Mods/Input/GamepadLayout.cs` | .cpp,.hpp |
| A | +69/-0 | `Mods/Input/GamepadAnalog.cs` | — 新規 |
| A | +62/-0 | `Mods/Input/GamepadCalibration.cs` | — 新規 |
| A | +56/-0 | `Mods/Input/GamepadGlyphs.cs` | — 新規 |
| A | +55/-0 | `Mods/Input/AimAssist/AimAssistDebug.cs` | — 新規 |
| A | +54/-0 | `Mods/Input/PadAction.cs` | — 新規 |
| A | +54/-0 | `Mods/Input/PlayerEntityMouseFlick.cs` | — 新規 |
| A | +51/-0 | `Mods/Input/GamepadHaptics.cs` | — 新規 |
| A | +40/-0 | `Mods/Input/GamepadOptions.cs` | — 新規 |
| A | +39/-0 | `Mods/Input/GamepadActions.cs` | — 新規 |
| A | +32/-0 | `Mods/Input/WeaponSelectionDirection.cs` | — 新規 |
| A | +30/-0 | `Mods/Input/PlayerEntityHaptics.cs` | — 新規 |
| A | +27/-0 | `Mods/Input/AimInputSourceTracker.cs` | — 新規 |
| A | +25/-0 | `Mods/Input/AimAssist/AimAssistTuning.cs` | — 新規 |
| M | +25/-18 | `Mods/Input/GamepadProbe.cs` | .cpp,.hpp |
| M | +25/-213 | `Mods/Input/PadBindings.cs` | .cpp,.hpp |
| A | +24/-0 | `Mods/Input/GamepadRuntimeConfig.cs` | — 新規 |
| A | +24/-0 | `Mods/Input/SpectatorInput.cs` | — 新規 |
| A | +23/-0 | `Mods/Input/GamepadDeviceSnapshot.cs` | — 新規 |
| A | +20/-0 | `Mods/Input/HapticScheduler.cs` | — 新規 |
| A | +19/-0 | `Mods/Input/InputPrompt.cs` | — 新規 |
| A | +19/-0 | `Mods/Input/InputSourceTracker.cs` | — 新規 |
| A | +17/-0 | `Mods/Input/AimAssist/AimAssistMath.cs` | — 新規 |
| A | +15/-0 | `Mods/Input/ControllerLayoutState.cs` | — 新規 |
| A | +13/-0 | `Mods/Input/StickCalibration.cs` | — 新規 |
| A | +12/-0 | `Mods/Input/AimAssist/AimAssistState.cs` | — 新規 |
| A | +10/-0 | `Mods/Input/AimAssist/AimAssistTarget.cs` | — 新規 |
| M | +10/-77 | `Mods/Input/PointerInput.cs` | .cpp,.hpp |

## 7. Render — 22 ファイル (新規 14), C# +2658 行

| S | +/- | C# | C++ |
|---|---|---|---|
| A | +416/-0 | `Mods/Render/LauncherPhoto.cs` | — 新規 |
| A | +277/-0 | `Mods/Render/PlayerEntityMapPick.cs` | — 新規 |
| A | +256/-0 | `Mods/Render/MapThumbnail.cs` | — 新規 |
| A | +244/-0 | `Mods/Render/UiOverlay.cs` | — 新規 |
| A | +226/-0 | `Mods/Render/NoiseField.cs` | — 新規 |
| A | +174/-0 | `Mods/Render/LauncherHunter.cs` | — 新規 |
| A | +160/-0 | `Mods/Render/LauncherNoise.cs` | — 新規 |
| M | +138/-3 | `Mods/Render/PreviewPass.cs` | .cpp,.hpp |
| A | +132/-0 | `Mods/Render/AppIcon.cs` | — 新規 |
| A | +116/-0 | `Mods/Render/Radar.cs` | — 新規 |
| A | +97/-0 | `Mods/Render/HunterShot.cs` | — 新規 |
| M | +77/-0 | `Mods/Render/FrameTimingCheck.cs` | .cpp,.hpp |
| A | +70/-0 | `Mods/Render/PlayerEntityTeamScoreboard.cs` | — 新規 |
| A | +62/-0 | `Mods/Render/LockjawTrailProbe.cs` | — 新規 |
| M | +51/-0 | `Mods/Render/PlayerEntityStylusHud.cs` | .cpp,.hpp |
| A | +43/-0 | `Mods/Render/DesktopGlContext.cs` | — 新規 |
| M | +31/-0 | `Mods/Render/GlEs.cs` | .cpp,.hpp |
| A | +28/-0 | `Mods/Render/LockjawTrailNoise.cs` | — 新規 |
| M | +26/-0 | `Mods/Render/HunterPreview.cs` | .cpp,.hpp |
| M | +17/-1 | `Mods/Render/PlayerEntityEndScreen.cs` | .cpp,.hpp |
| M | +12/-9 | `Mods/Render/PlayerEntityProHud.cs` | .cpp,.hpp |
| M | +5/-0 | `Mods/Render/PlayerEntityVoteHud.cs` | .cpp,.hpp |

## 8. Multiplayer and teams — 7 ファイル (新規 7), C# +503 行

| S | +/- | C# | C++ |
|---|---|---|---|
| A | +118/-0 | `Mods/Multiplayer/ResourceAudit.cs` | — 新規 |
| A | +99/-0 | `Mods/Multiplayer/MapResourceRules.cs` | — 新規 |
| A | +95/-0 | `Mods/Multiplayer/TeamGameplayTest.cs` | — 新規 |
| A | +89/-0 | `Mods/Multiplayer/GameStateTeams.cs` | — 新規 |
| A | +46/-0 | `Mods/Multiplayer/TeamLayout.cs` | — 新規 |
| A | +37/-0 | `Mods/Multiplayer/TeamVisuals.cs` | — 新規 |
| A | +19/-0 | `Mods/Multiplayer/MatchWorldProfile.cs` | — 新規 |

## 9. Network — 56 ファイル (新規 26), C# +12807 行

| S | +/- | C# | C++ |
|---|---|---|---|
| A | +1952/-0 | `Mods/Network/NetHitClaims.cs` | — 新規 |
| M | +934/-33 | `Mods/Network/NetProtocol.cs` | .cpp,.hpp |
| M | +883/-78 | `Mods/Network/NetHitPrediction.cs` | .cpp,.hpp |
| M | +704/-105 | `Mods/Network/DedicatedServer.cs` | .cpp,.hpp |
| A | +609/-0 | `Mods/Network/HitRig.cs` | — 新規 |
| A | +558/-0 | `Mods/Network/NetSmoothing.cs` | — 新規 |
| A | +536/-0 | `Mods/Network/LocalServer.cs` | — 新規 |
| A | +536/-0 | `Mods/Network/NetLobbyTest.cs` | — 新規 |
| M | +489/-116 | `Mods/Network/NetSession.cs` | .cpp,.hpp |
| M | +481/-22 | `Mods/Network/NetUnlagged.cs` | .cpp,.hpp |
| M | +384/-12 | `Mods/Network/NetMaster.cs` | .cpp,.hpp |
| M | +366/-389 | `Mods/Network/NetPlayerBridge.cs` | .cpp,.hpp |
| M | +307/-47 | `Mods/Network/NetDamage.cs` | .cpp,.hpp |
| A | +296/-0 | `Mods/Network/LobbyCommands.cs` | — 新規 |
| A | +284/-0 | `Mods/Network/NetCombatCheck.cs` | — 新規 |
| A | +264/-0 | `Mods/Network/HostPool.cs` | — 新規 |
| A | +200/-0 | `Mods/Network/MapAuditTeams.cs` | — 新規 |
| M | +198/-11 | `Mods/Network/PlayerEntityNetAim.cs` | .cpp,.hpp |
| A | +194/-0 | `Mods/Network/NetSessionLobby.cs` | — 新規 |
| A | +180/-0 | `Mods/Network/NetPlayerLifecycle.cs` | — 新規 |
| A | +180/-0 | `Mods/Network/SessionProtocol.cs` | — 新規 |
| M | +160/-15 | `Mods/Network/NetHooks.cs` | .cpp,.hpp |
| M | +150/-2 | `Mods/Network/NetCheckClient.cs` | .cpp,.hpp |
| A | +138/-0 | `Mods/Network/SpireAltPoseCheck.cs` | — 新規 |
| M | +122/-2 | `Mods/Network/NetTestScript.cs` | .cpp,.hpp |
| A | +100/-0 | `Mods/Network/ContinuousWeaponPhase.cs` | — 新規 |
| A | +98/-0 | `Mods/Network/NetHealthSync.cs` | — 新規 |
| A | +95/-0 | `Mods/Network/FormReconciliation.cs` | — 新規 |
| A | +91/-0 | `Mods/Network/NetShotDiagnostics.cs` | — 新規 |
| A | +90/-0 | `Mods/Network/HealthSimulationTest.cs` | — 新規 |
| M | +90/-35 | `Mods/Network/NetLaunch.cs` | .cpp,.hpp |
| A | +82/-0 | `Mods/Network/NetLifecycleTracker.cs` | — 新規 |
| M | +82/-12 | `Mods/Network/PlayerEntityNetHud.cs` | .cpp,.hpp |
| M | +80/-2 | `Mods/Network/ServerSimCheck.cs` | .cpp,.hpp |
| M | +77/-1 | `Mods/Network/NetLog.cs` | .cpp,.hpp |
| A | +76/-0 | `Mods/Network/LobbyRules.cs` | — 新規 |
| M | +75/-0 | `Mods/Network/MapRotation.cs` | .cpp,.hpp |
| A | +70/-0 | `Mods/Network/NetFaultQueue.cs` | — 新規 |
| A | +69/-0 | `Mods/Network/NetTimingDiagnostics.cs` | — 新規 |
| M | +66/-2 | `Mods/Network/ServerSim.cs` | .cpp,.hpp |
| A | +61/-0 | `Mods/Network/MatchDefinition.cs` | — 新規 |
| A | +58/-0 | `Mods/Network/NetHealthSyncTest.cs` | — 新規 |
| M | +41/-5 | `Mods/Network/MapAudit.cs` | .cpp,.hpp |
| M | +39/-30 | `Mods/Network/NetLag.cs` | .cpp,.hpp |
| M | +37/-3 | `Mods/Network/NetMatchSync.cs` | .cpp,.hpp |
| A | +37/-0 | `Mods/Network/NetMatchTimeSync.cs` | — 新規 |
| A | +31/-0 | `Mods/Network/NetHudHealth.cs` | — 新規 |
| M | +27/-0 | `Mods/Network/NetFeatureCheck.cs` | .cpp,.hpp |
| M | +23/-40 | `Mods/Network/NetTransport.cs` | .cpp,.hpp |
| M | +21/-17 | `Mods/Network/NetRoomChange.cs` | .cpp,.hpp |
| M | +21/-29 | `Mods/Network/NetSlotManager.cs` | .cpp,.hpp |
| M | +21/-4 | `Mods/Network/NetStatus.cs` | .cpp,.hpp |
| M | +20/-0 | `Mods/Network/NetDiagnostics.cs` | .cpp,.hpp |
| M | +16/-1 | `Mods/Network/NetHostSession.cs` | .cpp,.hpp |
| M | +5/-1 | `Mods/Network/DemoPlayback.cs` | .cpp,.hpp |
| M | +3/-2 | `Mods/Network/MechanicsDump.cs` | .cpp,.hpp |

## 10. MapGen — 11 ファイル (新規 3), C# +2034 行

| S | +/- | C# | C++ |
|---|---|---|---|
| A | +609/-0 | `Mods/MapGen/MapCheck.cs` | — 新規 |
| A | +430/-0 | `Mods/MapGen/CollisionObj.cs` | — 新規 |
| A | +369/-0 | `Mods/MapGen/AltFormProbe.cs` | — 新規 |
| M | +213/-20 | `Mods/MapGen/Q3Import.cs` | .cpp,.hpp |
| M | +120/-0 | `Mods/MapGen/MapDefinition.cs` | .cpp,.hpp |
| M | +113/-2 | `Mods/MapGen/MapReport.cs` | .cpp,.hpp |
| M | +91/-8 | `Mods/MapGen/Q3Convert.cs` | .cpp,.hpp |
| M | +48/-1 | `Mods/MapGen/MapPacker.cs` | .cpp,.hpp |
| M | +19/-0 | `Mods/MapGen/BuiltMap.cs` | .cpp,.hpp |
| M | +19/-0 | `Mods/MapGen/MapBundle.cs` | .cpp,.hpp |
| M | +3/-2 | `Mods/MapGen/CustomRooms.cs` | .cpp,.hpp |

## 11. Launcher portable — 7 ファイル (新規 2), C# +615 行

| S | +/- | C# | C++ |
|---|---|---|---|
| A | +320/-0 | `Mods/Launcher/Portable/NativeFilePicker.cs` | — 新規 |
| M | +98/-42 | `Mods/Launcher/Portable/MatchStart.cs` | .cpp,.hpp |
| M | +87/-8 | `Mods/Launcher/Portable/LauncherPrefs.cs` | .cpp,.hpp |
| A | +67/-0 | `Mods/Launcher/Portable/RomWhitelist.cs` | — 新規 |
| M | +24/-7 | `Mods/Launcher/Portable/TextLauncher.cs` | .cpp,.hpp |
| M | +15/-3 | `Mods/Launcher/Portable/GameFiles.cs` | .cpp,.hpp |
| M | +4/-0 | `Mods/Launcher/Portable/LaunchPlan.cs` | .cpp,.hpp |

## 12. Launcher GUI — 70 ファイル (新規 50), C# +22132 行

| S | +/- | C# | C++ |
|---|---|---|---|
| A | +1941/-0 | `Mods/Launcher/Gui/PlayScreen.cs` | — 新規 |
| A | +1529/-0 | `Mods/Launcher/Gui/UiDesigns.cs` | — 新規 |
| A | +1126/-0 | `Mods/Launcher/Gui/CreateServerScreen.cs` | — 新規 |
| A | +1077/-0 | `Mods/Launcher/Gui/Shell.cs` | — 新規 |
| A | +1031/-0 | `Mods/Launcher/Gui/UiSurface.cs` | — 新規 |
| A | +988/-0 | `Mods/Launcher/Gui/StartScreen.cs` | — 新規 |
| A | +921/-0 | `Mods/Launcher/Gui/LobbyScreen.cs` | — 新規 |
| A | +889/-0 | `Mods/Launcher/Gui/DeckTile.cs` | — 新規 |
| A | +838/-0 | `Mods/Launcher/Gui/DeckButton.cs` | — 新規 |
| A | +819/-0 | `Mods/Launcher/Gui/UiLayout.cs` | — 新規 |
| A | +786/-0 | `Mods/Launcher/Gui/HunterStand.cs` | — 新規 |
| A | +665/-0 | `Mods/Launcher/Gui/UiBench.cs` | — 新規 |
| A | +574/-0 | `Mods/Launcher/Gui/UiList.cs` | — 新規 |
| A | +535/-0 | `Mods/Launcher/Gui/UiTopLevel.cs` | — 新規 |
| M | +493/-387 | `Mods/Launcher/Gui/SettingsView.cs` | .cpp,.hpp |
| M | +491/-142 | `Mods/Launcher/Gui/ServerRow.cs` | .cpp,.hpp |
| A | +466/-0 | `Mods/Launcher/Gui/DeckChip.cs` | — 新規 |
| A | +409/-0 | `Mods/Launcher/Gui/Deck.cs` | — 新規 |
| A | +339/-0 | `Mods/Launcher/Gui/SetupScreen.cs` | — 新規 |
| A | +309/-0 | `Mods/Launcher/Gui/EndPanelView.cs` | — 新規 |
| A | +270/-0 | `Mods/Launcher/Gui/MovingBackdrop.cs` | — 新規 |
| M | +268/-64 | `Mods/Launcher/Gui/UiCapture.cs` | .cpp,.hpp |
| A | +239/-0 | `Mods/Launcher/Gui/GamepadUiChecks.cs` | — 新規 |
| A | +227/-0 | `Mods/Launcher/Gui/Flags.cs` | — 新規 |
| A | +222/-0 | `Mods/Launcher/Gui/TapCheck.cs` | — 新規 |
| M | +220/-26 | `Mods/Launcher/Gui/Rows.cs` | .cpp,.hpp |
| A | +202/-0 | `Mods/Launcher/Gui/InGameMenu.cs` | — 新規 |
| A | +197/-0 | `Mods/Launcher/Gui/UiWord.cs` | — 新規 |
| A | +196/-0 | `Mods/Launcher/Gui/BakedBackdrop.cs` | — 新規 |
| A | +195/-0 | `Mods/Launcher/Gui/DeckText.cs` | — 新規 |
| A | +193/-0 | `Mods/Launcher/Gui/GamepadSettingsPanel.cs` | — 新規 |
| A | +181/-0 | `Mods/Launcher/Gui/DeckSide.cs` | — 新規 |
| M | +181/-41 | `Mods/Launcher/Gui/PadRow.cs` | .cpp,.hpp |
| M | +178/-123 | `Mods/Launcher/Gui/PauseMenuView.cs` | .cpp,.hpp |
| A | +166/-0 | `Mods/Launcher/Gui/Tap.cs` | — 新規 |
| A | +165/-0 | `Mods/Launcher/Gui/UiMark.cs` | — 新規 |
| A | +162/-0 | `Mods/Launcher/Gui/UiScaleHost.cs` | — 新規 |
| A | +159/-0 | `Mods/Launcher/Gui/ServerBadge.cs` | — 新規 |
| A | +158/-0 | `Mods/Launcher/Gui/DeckCard.cs` | — 新規 |
| A | +155/-0 | `Mods/Launcher/Gui/DeckField.cs` | — 新規 |
| M | +149/-21 | `Mods/Launcher/Gui/GuiTheme.cs` | .cpp,.hpp |
| A | +147/-0 | `Mods/Launcher/Gui/DeckSheet.cs` | — 新規 |
| A | +146/-0 | `Mods/Launcher/Gui/UiTabs.cs` | — 新規 |
| A | +143/-0 | `Mods/Launcher/Gui/DeckWordmark.cs` | — 新規 |
| A | +143/-0 | `Mods/Launcher/Gui/GeoCountry.cs` | — 新規 |
| A | +142/-0 | `Mods/Launcher/Gui/MapCardPicker.cs` | — 新規 |
| A | +126/-0 | `Mods/Launcher/Gui/GamepadSetupPanel.cs` | — 新規 |
| M | +116/-10 | `Mods/Launcher/Gui/KeyRow.cs` | .cpp,.hpp |
| A | +93/-0 | `Mods/Launcher/Gui/FocusNavigator.cs` | — 新規 |
| A | +91/-0 | `Mods/Launcher/Gui/GamepadNavigation.cs` | — 新規 |
| A | +89/-0 | `Mods/Launcher/Gui/ControllerKeyboard.cs` | — 新規 |
| A | +88/-0 | `Mods/Launcher/Gui/MapShot.cs` | — 新規 |
| A | +80/-0 | `Mods/Launcher/Gui/GamepadMonitor.cs` | — 新規 |
| A | +78/-0 | `Mods/Launcher/Gui/ConfirmScreen.cs` | — 新規 |
| M | +70/-157 | `Mods/Launcher/Gui/GuiLauncher.cs` | .cpp,.hpp |
| M | +61/-10 | `Mods/Launcher/Gui/SliderRow.cs` | .cpp,.hpp |
| A | +58/-0 | `Mods/Launcher/Gui/LobbyPlayerRow.cs` | — 新規 |
| A | +51/-0 | `Mods/Launcher/Gui/GamepadProfilePanel.cs` | — 新規 |
| A | +44/-0 | `Mods/Launcher/Gui/ControllerNav.cs` | — 新規 |
| A | +37/-0 | `Mods/Launcher/Gui/GamepadGlyph.cs` | — 新規 |
| D | +0/-151 | `Mods/Launcher/Gui/DemoPickerView.cs` | .cpp,.hpp |
| D | +0/-2235 | `Mods/Launcher/Gui/HomeView.cs` | .cpp,.hpp |
| D | +0/-41 | `Mods/Launcher/Gui/HomeWindow.cs` | .cpp,.hpp |
| D | +0/-280 | `Mods/Launcher/Gui/MapPickerView.cs` | .cpp,.hpp |
| D | +0/-317 | `Mods/Launcher/Gui/MenuEntry.cs` | .cpp,.hpp |
| D | +0/-372 | `Mods/Launcher/Gui/PauseMenuWindow.cs` | .cpp,.hpp |
| D | +0/-78 | `Mods/Launcher/Gui/SettingsWindow.cs` | .cpp,.hpp |
| D | +0/-239 | `Mods/Launcher/Gui/SplashView.cs` | .cpp,.hpp |
| D | +0/-157 | `Mods/Launcher/Gui/UpdateBadge.cs` | .cpp,.hpp |
| M | +20/-62 | `Mods/PauseMenu.cs` | .cpp,.hpp |

## 13. Engine and entities — 34 ファイル (新規 0), C# +3279 行

| S | +/- | C# | C++ |
|---|---|---|---|
| M | +1050/-99 | `Renderer.cs` | .cpp,.hpp |
| M | +872/-49 | `Mods/ModEntry.cs` | .cpp,.hpp |
| M | +376/-94 | `Entities/Players/PlayerHud.cs` | .cpp,.hpp |
| M | +185/-40 | `Entities/Players/PlayerInput.cs` | .cpp,.hpp |
| M | +102/-126 | `GameState.cs` | .cpp,.hpp |
| M | +95/-11 | `Entities/Players/PlayerAi.cs` | .cpp,.hpp |
| M | +71/-3 | `Formats/Formats.cs` | .cpp,.hpp |
| M | +66/-33 | `Entities/BeamProjectileEntity.cs` | .cpp,.hpp |
| M | +54/-0 | `Shaders.cs` | .cpp,.hpp |
| M | +48/-6 | `Entities/Players/PlayerEntity.cs` | .cpp,.hpp |
| M | +45/-12 | `Read.cs` | .cpp,.hpp |
| M | +40/-6 | `Entities/Players/PlayerProcess.cs` | .cpp,.hpp |
| M | +39/-1 | `Entities/ItemSpawnEntity.cs` | .cpp,.hpp |
| M | +37/-8 | `Entities/BombEntity.cs` | .cpp,.hpp |
| M | +36/-2 | `Program.cs` | .cpp,.hpp |
| M | +36/-2 | `Utility/Console.cs` | .cpp,.hpp |
| M | +30/-1 | `Entities/Players/PlayerCollision.cs` | .cpp,.hpp |
| M | +26/-20 | `Entities/NodeDefenseEntity.cs` | .cpp,.hpp |
| M | +19/-1 | `Features.cs` | .cpp,.hpp |
| M | +11/-3 | `SceneSetup.cs` | .cpp,.hpp |
| M | +6/-2 | `Mods/Credits.cs` | .cpp,.hpp |
| M | +6/-1 | `Mods/DebugLog.cs` | .cpp,.hpp |
| M | +6/-2 | `Utility/Archive.cs` | .cpp,.hpp |
| M | +5/-3 | `Metadata/Metadata.cs` | .cpp,.hpp |
| M | +4/-3 | `Utility/Extract.cs` | .cpp,.hpp |
| M | +3/-1 | `Sound/Sfx.cs` | .cpp,.hpp |
| M | +2/-2 | `Entities/Enemies/18_AlimbicTurret.cs` | .cpp,.hpp |
| M | +2/-2 | `Entities/ItemInstanceEntity.cs` | .cpp,.hpp |
| M | +2/-1 | `Entities/Players/HalfturretEntity.cs` | .cpp,.hpp |
| M | +1/-9 | `Entities/Players/PlayerDraw.cs` | .cpp,.hpp |
| M | +1/-0 | `Entities/Players/PlayerSound.cs` | .cpp,.hpp |
| M | +1/-0 | `Menu.cs` | .cpp,.hpp |
| M | +1/-1 | `Scene.cs` | .cpp,.hpp |
| M | +1/-0 | `Sound/Music.cs` | .cpp,.hpp |

## 14. Android head — 17 ファイル (新規 7), C# +1735 行

| S | +/- | C# | C++ |
|---|---|---|---|
| A | +299/-0 | `(android) AndroidHunterShot.cs` | — 新規 |
| A | +271/-0 | `(android) AndroidUiSurface.cs` | — 新規 |
| A | +257/-0 | `(android) AndroidUiOverlay.cs` | — 新規 |
| M | +246/-131 | `(android) MainActivity.cs` | .cpp,.hpp |
| A | +167/-0 | `(android) MainApplication.cs` | — 新規 |
| M | +112/-2 | `(android) GameView.cs` | .cpp,.hpp |
| M | +98/-127 | `(android) GamepadBridge.cs` | .cpp,.hpp |
| M | +55/-22 | `(android) AndroidThumbnails.cs` | .cpp,.hpp |
| M | +54/-0 | `(android) TouchOverlayView.cs` | .cpp,.hpp |
| A | +52/-0 | `(android) AndroidWebLink.cs` | — 新規 |
| M | +47/-9 | `(android) AndroidApp.cs` | .cpp,.hpp |
| A | +39/-0 | `(android) AndroidGamepadHaptics.cs` | — 新規 |
| A | +22/-0 | `(android) AndroidGamepadProfile.cs` | — 新規 |
| M | +11/-14 | `(android) AndroidUpdateInstaller.cs` | .cpp,.hpp |
| M | +2/-3 | `(android) AndroidLogShare.cs` | .cpp,.hpp |
| M | +2/-1 | `(android) AndroidMatch.cs` | .cpp,.hpp |
| M | +1/-0 | `(android) TouchControls.cs` | .cpp,.hpp |


