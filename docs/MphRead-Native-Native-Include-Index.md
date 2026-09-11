# MphRead.Native hpp/cpp include索引

生成元: `src/MphRead.Native/**/*.{hpp,cpp}` / branch `develop2` / snapshot commit `2da573aa860507eab7150dfd3a69667528721063`

Native側48ペアについて、hppとcpp双方から文字列形式の `#include` を抽出し、重複排除しています。これはincludeの存在を示すだけで、C#との意味的完全再現やリンク成功を示しません。hppはcppと同じ機能ディレクトリに置く方針です。

| # | Native stem（拡張子なし） | include依存 |
|---:|---|---|
| 1 | `Entities/Enemies/43_SlenchNest` | ../../Formats/Culling.hpp<br>../EnemyInstanceEntity.hpp<br>../EnemySpawnEntity.hpp<br>43_SlenchNest.hpp<br>cassert |
| 2 | `Entities/Enemies/51_CarnivorousPlant` | ../../Formats/Culling.hpp<br>../EnemyInstanceEntity.hpp<br>../EnemySpawnEntity.hpp<br>51_CarnivorousPlant.hpp<br>cassert |
| 3 | `Entities/LightSourceEntity` | ../Formats/Entity.hpp<br>../Formats/Types.hpp<br>../Renderer.hpp<br>EntityBase.hpp<br>LightSourceEntity.hpp<br>optional |
| 4 | `Entities/PointModuleEntity` | ../Formats/Entity.hpp<br>cstdint<br>EntityBase.hpp<br>PointModuleEntity.hpp |
| 5 | `Formats/Culling` | any<br>array<br>bit<br>cstdint<br>Culling.hpp<br>memory<br>random<br>string<br>utility |
| 6 | `Formats/Enums` | cstdint<br>Enums.hpp |
| 7 | `Formats/Frontend` | bit<br>cassert<br>charconv<br>cstddef<br>cstdint<br>cstring<br>Frontend.hpp<br>fstream<br>iostream<br>span<br>stdexcept<br>string_view<br>type_traits<br>vector |
| 8 | `Mods/Branding` | Branding.hpp<br>cstdint<br>cstdio<br>cstdlib<br>cstring<br>limits<br>limits.h<br>mach-o/dyld.h<br>memory<br>optional<br>stdexcept<br>stdlib.h<br>string<br>string_view<br>sys/auxv.h<br>sys/param.h<br>sys/stat.h<br>sys/sysctl.h<br>sys/types.h<br>system_error<br>unistd.h<br>Update/BuildVersion.hpp<br>vector<br>windows.h |
| 9 | `Mods/ConsoleWindow` | algorithm<br>array<br>atomic<br>ConsoleWindow.hpp<br>cstddef<br>cstdint<br>ios<br>iostream<br>memory<br>mutex<br>optional<br>stdexcept<br>streambuf<br>string<br>string_view<br>utility<br>vector<br>windows.h |
| 10 | `Mods/Credits` | array<br>Branding.hpp<br>Credits.hpp<br>iostream<br>optional<br>string<br>string_view<br>utility |
| 11 | `Mods/Headless` | Headless.hpp |
| 12 | `Mods/Input/GamepadState` | cstdint<br>GamepadState.hpp<br>optional<br>string |
| 13 | `Mods/Input/PointerInput` | bit<br>cmath<br>cstdint<br>PointerInput.hpp<br>string_view |
| 14 | `Mods/Input/SyntheticInput` | ../../Program.hpp<br>exception<br>memory<br>string<br>string_view<br>SyntheticInput.hpp<br>type_traits<br>typeinfo<br>utility<br>variant |
| 15 | `Mods/Input/TouchSettings` | cstddef<br>cstdint<br>optional<br>string<br>string_view<br>TouchSettings.hpp<br>unordered_set<br>vector |
| 16 | `Mods/Launcher/Gui/CrosshairPreview` | ../../Render/Crosshair.hpp<br>CrosshairPreview.hpp<br>cstddef<br>cstdint<br>optional<br>tuple<br>vector |
| 17 | `Mods/Launcher/Gui/HomeWindow` | ../../Branding.hpp<br>../Portable/LaunchPlan.hpp<br>cstdint<br>HomeWindow.hpp<br>string_view |
| 18 | `Mods/Launcher/Gui/ProgressRow` | charconv<br>cmath<br>cstdint<br>limits<br>ProgressRow.hpp<br>string<br>string_view |
| 19 | `Mods/Launcher/Gui/TrackedText` | cmath<br>cstdint<br>string_view<br>TrackedText.hpp |
| 20 | `Mods/Launcher/Portable/LaunchPlan` | atomic<br>cstdint<br>LaunchPlan.hpp<br>optional<br>random<br>string |
| 21 | `Mods/Launcher/SetupProgress` | bit<br>charconv<br>cmath<br>cstdint<br>limits<br>SetupProgress.hpp<br>stdexcept<br>string<br>string_view<br>system_error<br>utility |
| 22 | `Mods/MapGen/BuiltMap` | ../../Formats/Enums.hpp<br>BuiltMap.hpp<br>cstdint<br>vector |
| 23 | `Mods/MapGen/MapTexturePack` | ../../Program.hpp<br>algorithm<br>cstddef<br>cstdint<br>filesystem<br>fstream<br>ios<br>iosfwd<br>istream<br>limits<br>MapTexturePack.hpp<br>stdexcept<br>streambuf<br>string<br>string_view<br>utility<br>vector |
| 24 | `Mods/MapGen/RepackAccess` | cstdint<br>RepackAccess.hpp<br>span<br>vector |
| 25 | `Mods/ModEntry` | ../Entities/Players/PlayerEntity.hpp<br>../Features.hpp<br>../Formats/Enums.hpp<br>../Utility/Console.hpp<br>algorithm<br>array<br>Branding.hpp<br>charconv<br>cmath<br>conio.h<br>ConsoleWindow.hpp<br>Credits.hpp<br>crt_externs.h<br>cstdint<br>cstdlib<br>DebugLog.hpp<br>filesystem<br>fstream<br>Input/GamepadProbe.hpp<br>InputSettings.hpp<br>iomanip<br>iostream<br>Launcher/Gui/GuiLauncher.hpp<br>Launcher/Gui/UiCapture.hpp<br>Launcher/Portable/LauncherPrefs.hpp<br>Launcher/Portable/TextLauncher.hpp<br>limits<br>limits.h<br>locale<br>mach-o/dyld.h<br>MapGen/CustomRooms.hpp<br>MapGen/MapBundle.hpp<br>MapGen/MapPacker.hpp<br>MapGen/MapReport.hpp<br>MapGen/MapTextureBake.hpp<br>MapGen/Q3Bsp.hpp<br>MapGen/Q3Convert.hpp<br>memory<br>ModEntry.hpp<br>Network/DedicatedServer.hpp<br>Network/DemoInfo.hpp<br>Network/MapAudit.hpp<br>Network/MapRotation.hpp<br>Network/MechanicsDump.hpp<br>Network/NetCheckClient.hpp<br>Network/NetConnectCommand.hpp<br>Network/NetDiagnostics.hpp<br>Network/NetHitPrediction.hpp<br>Network/NetLag.hpp<br>Network/NetMaster.hpp<br>Network/NetUnlagged.hpp<br>Network/ServerSimCheck.hpp<br>Network/WeaponDps.hpp<br>optional<br>Render/Crosshair.hpp<br>Render/FrameTiming.hpp<br>Render/FrameTimingCheck.hpp<br>RenderOptions.hpp<br>ShutdownSignals.hpp<br>sstream<br>stop_token<br>string<br>string_view<br>system_error<br>ThumbnailBatch.hpp<br>ThumbnailCapture.hpp<br>ThumbnailGenerator.hpp<br>unistd.h<br>Update/DesktopUpdate.hpp<br>Update/ServerUpdate.hpp<br>Update/UpdateCheck.hpp<br>Update/UpdateInstall.hpp<br>Update/Updater.hpp<br>utility<br>vector<br>WindowMode.hpp<br>windows.h |
| 26 | `Mods/Network/NetConnectCommand` | ../../Formats/Enums.hpp<br>cstdint<br>NetConnectCommand.hpp<br>optional<br>string<br>string_view |
| 27 | `Mods/Network/NetLag` | array<br>bcrypt.h<br>bit<br>cerrno<br>charconv<br>cmath<br>cstddef<br>cstdint<br>cstdlib<br>cstring<br>exception<br>fcntl.h<br>langinfo.h<br>limits<br>locale.h<br>NetLag.hpp<br>new<br>optional<br>stdexcept<br>string<br>string_view<br>system_error<br>unistd.h<br>windows.h |
| 28 | `Mods/Network/NetMatchSync` | cmath<br>cstdint<br>NetMatchSync.hpp<br>optional<br>string<br>string_view |
| 29 | `Mods/Network/NetPlayerSetup` | cstdint<br>NetPlayerSetup.hpp<br>string<br>string_view |
| 30 | `Mods/Network/NetProbe` | arpa/inet.h<br>array<br>cerrno<br>chrono<br>cstdint<br>cstring<br>ifaddrs.h<br>memory<br>net/if.h<br>netdb.h<br>NetProbe.hpp<br>stdexcept<br>string<br>string_view<br>strings.h<br>sys/socket.h<br>sys/time.h<br>unistd.h<br>utility<br>vector<br>windows.h<br>winsock2.h<br>ws2tcpip.h |
| 31 | `Mods/Network/NetScoreboard` | cstdint<br>NetScoreboard.hpp |
| 32 | `Mods/Render/Crosshair` | array<br>charconv<br>cmath<br>Crosshair.hpp<br>cstdint<br>limits<br>new<br>optional<br>string<br>string_view<br>tuple<br>type_traits<br>vector |
| 33 | `Mods/Render/FrameTiming` | algorithm<br>array<br>charconv<br>cmath<br>cstddef<br>cstdint<br>FrameTiming.hpp<br>limits<br>locale<br>optional<br>string<br>string_view |
| 34 | `Mods/Render/FrameTimingCheck` | algorithm<br>array<br>cmath<br>cstddef<br>cstdint<br>FrameTiming.hpp<br>FrameTimingCheck.hpp<br>functional<br>iomanip<br>iostream<br>limits<br>locale<br>memory<br>sstream<br>string |
| 35 | `Mods/Render/PlayerEntityIconBounds` | bit<br>concepts<br>cstddef<br>cstdint<br>limits<br>memory<br>PlayerEntityIconBounds.hpp<br>ranges<br>span<br>stdexcept<br>type_traits |
| 36 | `Mods/Render/PreviewCamera` | cstdint<br>PreviewCamera.hpp |
| 37 | `Mods/RenderOptions` | bit<br>cstddef<br>cstdint<br>limits<br>optional<br>RenderOptions.hpp<br>string_view |
| 38 | `Mods/ShutdownSignals` | algorithm<br>atomic<br>cerrno<br>csignal<br>cstdint<br>fcntl.h<br>functional<br>iterator<br>memory<br>mutex<br>ShutdownSignals.hpp<br>signal.h<br>stdexcept<br>system_error<br>thread<br>unistd.h<br>utility<br>vector<br>windows.h |
| 39 | `Mods/ThumbnailHost` | cstddef<br>exception<br>ThumbnailHost.hpp |
| 40 | `Mods/ThumbnailLog` | algorithm<br>atomic<br>Branding.hpp<br>cerrno<br>chrono<br>cstdint<br>cstdio<br>cstdlib<br>ctime<br>fcntl.h<br>filesystem<br>fstream<br>ios<br>limits<br>mach-o/dyld.h<br>memory<br>mutex<br>optional<br>stdexcept<br>stdlib.h<br>string<br>string_view<br>sys/auxv.h<br>sys/file.h<br>sys/mount.h<br>sys/param.h<br>sys/stat.h<br>sys/sysctl.h<br>sys/types.h<br>sys/vfs.h<br>system_error<br>thread<br>ThumbnailLog.hpp<br>unistd.h<br>Update/BuildVersion.hpp<br>vector<br>windows.h |
| 41 | `Mods/ThumbnailMode` | ../Sound/Music.hpp<br>../Sound/Sfx.hpp<br>ThumbnailMode.hpp |
| 42 | `Mods/Update/BuildVersion` | array<br>BuildVersion.hpp<br>compare<br>cstddef<br>cstdint<br>exception<br>limits<br>mutex<br>optional<br>stdexcept<br>string<br>string_view |
| 43 | `Mods/Update/SyncHttp` | cstdint<br>exception<br>memory<br>SyncHttp.hpp |
| 44 | `Mods/WorldEvents` | array<br>bit<br>cstddef<br>cstdint<br>WorldEvents.hpp |
| 45 | `Program` | array<br>cerrno<br>conio.h<br>cstdint<br>cstdio<br>cstdlib<br>deque<br>Export/Images.hpp<br>filesystem<br>Formats/Formats.hpp<br>Formats/Movie.hpp<br>Formats/Sound.hpp<br>fstream<br>io.h<br>iostream<br>iterator<br>limits<br>Menu.hpp<br>Metadata/Metadata.hpp<br>Metadata/Rooms.hpp<br>Mods/Branding.hpp<br>Mods/ConsoleWindow.hpp<br>Mods/ModEntry.hpp<br>optional<br>poll.h<br>Program.hpp<br>Read.hpp<br>Renderer.hpp<br>stdexcept<br>string<br>string_view<br>sys/stat.h<br>termios.h<br>unistd.h<br>utility<br>Utility/Console.hpp<br>Utility/Extract.hpp<br>vector<br>windows.h |
| 46 | `Utility/Console` | atomic<br>clocale<br>Console.hpp<br>cstdint<br>cstdlib<br>exception<br>filesystem<br>limits<br>locale<br>locale.h<br>mach-o/dyld.h<br>memory<br>optional<br>stdexcept<br>stdlib.h<br>string<br>string_view<br>sys/auxv.h<br>sys/param.h<br>sys/sysctl.h<br>sys/types.h<br>utility<br>vector<br>windows.h |
| 47 | `Utility/Output` | array<br>atomic<br>chrono<br>clocale<br>condition_variable<br>coroutine<br>cstdint<br>cstring<br>deque<br>exception<br>functional<br>iostream<br>locale<br>locale.h<br>memory<br>mutex<br>optional<br>Output.hpp<br>random<br>stdexcept<br>string<br>system_error<br>thread<br>unistd.h<br>utility<br>vector<br>windows.h |
| 48 | `Utility/Rng` | cstdint<br>iomanip<br>iostream<br>limits<br>Rng.hpp<br>sstream<br>string |

Native側の全48stemは、配置移動した `Mods/Launcher/Portable/SetupProgress.cs` を除き、C#と同じ相対パスのCSに対応します。Native専用のtop-level host adapter（main/WinMain等）は、CS一対一再現の対象外として薄い接続に限定します。
