#include "ModEntry.hpp"

#include "../Features.hpp"
#include "../GameState.hpp"
#include "../Program.hpp"
#include "../Utility/Console.hpp"
#include "ConsoleWindow.hpp"
#include "Credits.hpp"
#include "DebugLog.hpp"
#include "Diagnostics/PlatformDiagnostics.hpp"
#include "Entry/CommandLine.hpp"
#include "Entry/GameChecks.hpp"
#include "Entry/LaunchPath.hpp"
#include "Entry/MapCommands.hpp"
#include "Entry/NetworkCommands.hpp"
#include "Entry/NetworkOptions.hpp"
#include "Entry/OptionValues.hpp"
#include "Entry/PreSetupDiagnostics.hpp"
#include "Entry/RenderOverrides.hpp"
#include "Entry/ServerCommands.hpp"
#include "Entry/UiCommands.hpp"
#include "Input/AimAssist/AimAssistDebug.hpp"
#include "Input/AimAssist/AimAssistTelemetry.hpp"
#include "Input/GamepadChecks.hpp"
#include "Input/GamepadProbe.hpp"
#include "Input/PointerCheck.hpp"
#include "InputSettings.hpp"
#if defined(MPHREAD_SHELL)
#include "Launcher/GuiLauncher.hpp"
#include "Render/GoldenCapture.hpp"
#endif
#include "Launcher/Portable/LauncherPrefs.hpp"
#include "Launcher/Portable/TextLauncher.hpp"
#include "MapGen/CustomRooms.hpp"
#include "Render/InstagibRails.hpp"
#include "Mods/Network/Diagnostics/NetDiagnostics.hpp"
#include "Mods/Network/Testing/Maps/MapAudit.hpp"
#include "Render/FrameTimingCheck.hpp"
#include "ThumbnailGenerator.hpp"
#include "Update/DesktopUpdate.hpp"
#include "Update/UpdateCheck.hpp"
#include "Update/UpdateInstall.hpp"
#include "Update/Updater.hpp"
#include "WindowMode.hpp"
#include "../NativeRuntime/System/IO.hpp"
#include "../NativeRuntime/System/Number.hpp"
#include "../NativeRuntime/System/Runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::Int32TryParseCurrentCulture;

// Environment.ExitCode is process state, not an immediate exit. The executable
// wrapper is the Native equivalent of the CLR host and owns the eventual return
// from main; this two-file port only assigns the state through that runtime
// boundary. A focused probe provides the boundary while peer Native entry files
// are still being ported.

// ModEntry is the order things happen in; what each command does lives in
// Entry/ (and, below that, in the subsystem that owns it). Both dispatchers
// read the raw command line, and the first branch that takes it wins -- the
// order of the blocks below is the command line's precedence, whatever order
// the flags were typed in.
namespace
{
    // Every `::HasFlag` call site below keeps reading the way it always did.
    using MphRead::Mods::Entry::ApplyRenderOverrides;
    using MphRead::Mods::Entry::FullPathCombine;
    using MphRead::Mods::Entry::HasFlag;
    using MphRead::Mods::Entry::IndexOfFlag;
    using MphRead::Mods::Entry::TryParseDoubleCurrent;
    using MphRead::Mods::Entry::ValueAfter;
    using MphRead::Mods::Entry::ValuesAfter;

    void WriteLine(std::string_view value)
    {
        std::cout << value << '\n';
    }

    void SetExitCode(std::int32_t value) noexcept
    {
        MphRead::NativeRuntime::SetEnvironmentExitCode(value);
    }

    [[nodiscard]] std::pair<int, int> ParseSize(const std::vector<std::string>& args)
    {
        std::optional<std::string> value = ValueAfter(args, "size");
        if (value.has_value())
        {
            std::vector<std::string_view> parts;
            std::size_t start = 0;
            for (std::size_t i = 0; i <= value->size(); ++i)
            {
                if (i == value->size() || (*value)[i] == 'x' || (*value)[i] == 'X')
                {
                    parts.emplace_back(value->data() + start, i - start);
                    start = i + 1;
                }
            }
            std::int32_t width = 0;
            std::int32_t height = 0;
            if (parts.size() == 2 && Int32TryParseCurrentCulture(parts[0], width)
                && Int32TryParseCurrentCulture(parts[1], height) && width > 0 && height > 0)
            {
                return {width, height};
            }
            WriteLine("[thumbnails] ignoring -size " + *value
                + " (expected e.g. 1920x1440)");
        }
        return {MphRead::Mods::ThumbnailGenerator::ThumbnailWidth,
            MphRead::Mods::ThumbnailGenerator::ThumbnailHeight};
    }
}

namespace MphRead::Mods
{
    bool ModEntry::TryHandleHeadless(const std::vector<std::string>& args)
    {
        // UI diagnostics return from this dispatch before normal launcher
        // startup. Apply their rendering/window overrides before creating Qt.
        ApplyRenderOverrides(args);
        if (::HasFlag(args, "exclusivefullscreen"))
            WindowMode::Startup(WindowStartMode::ExclusiveFullscreen);
        else if (::HasFlag(args, "fullscreen") || ::HasFlag(args, "borderless"))
            WindowMode::Startup(WindowStartMode::BorderlessFullscreen);
        else if (::HasFlag(args, "windowed"))
            WindowMode::Startup(WindowStartMode::Windowed);
        if (Entry::TryRunPreSetupDiagnostics(args))
        {
            return true;
        }

        InputSettings::Load();
        if (const auto nativeAim = ValueAfter(args, "nativeaim"); nativeAim.has_value())
        {
            const bool touch = *nativeAim == "touch" || *nativeAim == "nonexact-touch";
            const bool dual = *nativeAim == "dual" || *nativeAim == "nonexact-dual";
            if (!touch && !dual) throw ProgramException("-nativeaim requires touch, dual, nonexact-touch or nonexact-dual");
            auto& controls = InputSettings::Current();
            controls.SetNativeAim(true);
            controls.NativeControl().RomCadence = true;
            controls.NativeControl().Flags = static_cast<std::uint16_t>((touch ? 2 : 8)
                | ((*nativeAim == "touch" || *nativeAim == "dual") ? 0x20 : 0));
        }
        Launcher::LauncherPrefs::Load();
        if (::HasFlag(args, "debuglog"))
        {
            DebugLog::Force();
        }
        DebugLog::Attach();
        if (NativeRuntime::IsMacOS())
        {
            Diagnostics::PlatformDiagnostics::Start();
        }

        Update::Updater::Disabled(::HasFlag(args, "noupdate"));
        ApplyRenderOverrides(args);

        if (::HasFlag(args, "pointercheck"))
        {
            SetExitCode(Input::PointerCheck::Run());
            return true;
        }

        Input::AimAssist::AimAssistDebug::Enabled = ::HasFlag(args, "gamepadassistdebug");
        Input::AimAssist::AimAssistDebug::UnassistedArm = ::HasFlag(args, "gamepadassistbaseline");
        Input::AimAssist::AimAssistTelemetry::Configure(ValueAfter(args, "gamepadassisttelemetry"));

        if (::HasFlag(args, "gamepadcheck"))
        {
            SetExitCode(Input::GamepadChecks::Run(ValueAfter(args, "shots")));
            return true;
        }

        if (::HasFlag(args, "gamepad"))
        {
            double seconds = 15;
            const std::optional<std::string> given = ValueAfter(args, "seconds");
            double parsed = 0;
            if (given.has_value() && TryParseDoubleCurrent(*given, parsed) && parsed > 0)
            {
                seconds = parsed;
            }
            SetExitCode(Input::GamepadProbe::Run(seconds, ::HasFlag(args, "verbose")));
            return true;
        }

        if (::HasFlag(args, "frametimingcheck"))
        {
            SetExitCode(Render::FrameTimingCheck::Run());
            return true;
        }

        const int applyAt = IndexOfFlag(args, Update::DesktopUpdate::ApplyFlag);
        if (applyAt >= 0 && static_cast<std::size_t>(applyAt + 2) < args.size())
        {
            std::vector<std::string> relaunch;
            int separator = -1;
            for (std::size_t i = static_cast<std::size_t>(applyAt + 3); i < args.size(); ++i)
            {
                if (args[i] == Update::DesktopUpdate::RelaunchSeparator)
                {
                    separator = static_cast<int>(i);
                    break;
                }
            }
            for (int i = separator + 1; separator >= 0 && static_cast<std::size_t>(i) < args.size(); ++i)
            {
                relaunch.push_back(args[static_cast<std::size_t>(i)]);
            }
            std::int32_t waitFor = -1;
            if (!Int32TryParseCurrentCulture(args[static_cast<std::size_t>(applyAt + 2)], waitFor))
            {
                waitFor = -1;
            }
            SetExitCode(Update::DesktopUpdate::Apply(
                args[static_cast<std::size_t>(applyAt + 1)], waitFor, relaunch));
            return true;
        }

        if (!::HasFlag(args, "spireposecheck") && !::HasFlag(args, "dialanchecheck")
            && !::HasFlag(args, "dialanchepeercheck") && !::HasFlag(args, "formcheck"))
        {
            Update::DesktopUpdate::Clean();
        }
        Update::UpdateInstall::UseDesktopIfPossible();

        if (!Entry::ConfigureLineSimulation(args))
        {
            return true;
        }
        Entry::ApplyLagCompensationOptions(args);
        Entry::ApplyHitRigOptions(args);
        Entry::ApplyHitResolutionOptions(args);
        if (::HasFlag(args, "credits"))
        {
            Credits::Print();
            return true;
        }

        const std::optional<std::string> mapDir = ValueAfter(args, "mapdir");
        if (mapDir.has_value())
        {
            MapGen::CustomRooms::MapDirectory(
                FullPathCombine(ConsoleSetup::LaunchDirectory(), *mapDir));
        }

#if defined(MPHREAD_SHELL)
        if (const std::optional<std::string> golden = ValueAfter(args, "goldencapture");
            golden.has_value())
        {
            const std::string directory = ValueAfter(args, "goldendir").value_or(
                NativeRuntime::PathCombine(
                    NativeRuntime::EnvironmentCurrentDirectory(), "golden-rhi"));
            SetExitCode(Render::GoldenCapture::Run(*golden, directory));
            return true;
        }
#endif

        if (Entry::TryRunMapBundle(args))
        {
            return true;
        }

        if (::HasFlag(args, "update"))
        {
            Update::Updater::Disabled(false);
            const std::optional<Update::UpdateInfo> update = Update::Updater::Check();
            if (!update.has_value())
            {
                WriteLine("[update] " + Update::UpdateCheck::LastReason().value_or(""));
                return true;
            }
            WriteLine("[update] " + Update::Updater::Describe(*update));
            WriteLine("[update] " + update->PageUrl.Get().value_or(""));
            (void)Update::Updater::OpenPage(*update);
            return true;
        }

        if (Entry::TryRunUiCommands(args))
        {
            return true;
        }

        if (::HasFlag(args, "exclusivefullscreen"))
        {
            WindowMode::Startup(WindowStartMode::ExclusiveFullscreen);
        }
        else if (::HasFlag(args, "fullscreen") || ::HasFlag(args, "borderless"))
        {
            WindowMode::Startup(WindowStartMode::BorderlessFullscreen);
        }
        else if (::HasFlag(args, "windowed"))
        {
            WindowMode::Startup(WindowStartMode::Windowed);
        }

        bool doubleClicked = args.empty();
#if !defined(_WIN32) && !defined(__APPLE__)
        doubleClicked = false;
#endif
#if defined(MPHREAD_SERVER)
        doubleClicked = false;
#endif
        if ((::HasFlag(args, "launcher") || doubleClicked) && !::HasFlag(args, "menu"))
        {
#if defined(MPHREAD_SHELL)
            if (!::HasFlag(args, "text") && Launcher::Gui::GuiLauncher::TryRun())
            {
                return true;
            }
#if defined(_WIN32)
            ConsoleWindow::Show();
#endif
#endif
            Launcher::TextLauncher::Run();
            return true;
        }

        return Entry::TryRunServerCommands(args);
    }

    bool ModEntry::TryHandle(const std::vector<std::string>& args)
    {
        const auto [width, height] = ParseSize(args);

        // In the background: the launcher shows how far it is, and a room
        // loaded before its map is ready waits for it (Scene::AddRoom). Not
        // in a preview worker: the launcher that started it builds the maps,
        // and ten workers building them all at once wrote over each other.
        if (!::HasFlag(args, "mapgen") && ValuesAfter(args, "thumbnail").empty())
        {
            MapGen::CustomRooms::StartGenerating();
        }

        // Local diagnostics use the rule directly; network clients receive it from the server.
        if (::HasFlag(args, "instagibdemo"))
        {
            Render::InstagibRails::DemoMode(true);
            GameState::Instagib(true);
        }
        if (::HasFlag(args, "instagib"))
        {
            GameState::Instagib(true);
        }

        if (::HasFlag(args, "nohelmet"))
        {
            Features::HelmetOpacity(0);
            Features::VisorOpacity(0);
        }
        // Qt Quick always draws at the native window resolution; -uinativeres remains accepted.
        if (::HasFlag(args, "netdebug"))
        {
            Network::NetDiagnostics::SetEnabled(true);
            Network::MapAudit::Diagnostic(true);
        }

        if (Entry::TryRunCatalogCommands(args))
        {
            return true;
        }

        if (Entry::TryRunMapCommands(args))
        {
            return true;
        }

        if (Entry::TryRunGameChecks(args))
        {
            return true;
        }

        if (Entry::TryRunMapAudits(args))
        {
            return true;
        }

        if (Entry::TryRunNetworkCommands(args, width, height))
        {
            return true;
        }

        if (Entry::TryRunThumbnails(args, width, height))
        {
            return true;
        }

        return false;
    }
}
