#include "GuiLauncher.hpp"

#include "PauseMenuWindow.hpp"
#include "../Portable/GameFiles.hpp"
#include "../Portable/LauncherPrefs.hpp"
#include "../Portable/LaunchPlan.hpp"
#include "../Portable/MatchStart.hpp"
#include "../../DebugLog.hpp"
#include "../../GameSettings.hpp"
#include "../../PauseMenu.hpp"
#include "../../ThumbnailGenerator.hpp"
#include "../../WindowMode.hpp"
#include "../../Network/NetHostSession.hpp"
#include "../../Network/NetSession.hpp"
#include "../../../GameState.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#if defined(__APPLE__) && defined(__MACH__)
#include <TargetConditionals.h>
#endif

namespace MphRead::Mods::Launcher::Gui
{
    std::atomic_bool GuiLauncher::_setUp{false};
    std::atomic_bool GuiLauncher::_failed{false};

    GuiLauncherNullReferenceException::GuiLauncherNullReferenceException()
        : std::runtime_error("Object reference not set to an instance of an object.")
    {
    }

    GuiLauncherDispatcherFrame::GuiLauncherDispatcherFrame(
        bool exitWhenRequested) noexcept
        : _exitWhenRequested(exitWhenRequested)
    {
    }

    bool GuiLauncherDispatcherFrame::Continue() const noexcept
    {
        return _continue;
    }

    void GuiLauncherDispatcherFrame::Continue(bool value) noexcept
    {
        _continue = value;
    }

    bool GuiLauncherDispatcherFrame::ExitWhenRequested() const noexcept
    {
        return _exitWhenRequested;
    }

    GuiLauncherCallback::GuiLauncherCallback(
        void* context, Function function, std::shared_ptr<void> keepAlive)
        : _context(context), _function(function), _keepAlive(std::move(keepAlive))
    {
    }

    void GuiLauncherCallback::Invoke() const
    {
        if (_function != nullptr)
        {
            _function(_context);
        }
    }

    GuiLauncherCallback::operator bool() const noexcept
    {
        return _function != nullptr;
    }

    namespace
    {
        [[nodiscard]] constexpr bool IsWindows() noexcept
        {
#if defined(_WIN32)
            return true;
#else
            return false;
#endif
        }

        [[nodiscard]] constexpr bool IsMacOS() noexcept
        {
#if defined(__APPLE__) && defined(__MACH__)
#if defined(TARGET_OS_OSX)
            return TARGET_OS_OSX != 0;
#else
            return TARGET_OS_MAC != 0 && TARGET_OS_IPHONE == 0;
#endif
#else
            return false;
#endif
        }

        [[nodiscard]] constexpr bool IsAndroid() noexcept
        {
#if defined(ANDROID) || defined(__ANDROID__)
            return true;
#else
            return false;
#endif
        }

        [[nodiscard]] bool IsNullOrEmptyEnvironmentVariable(const char* name) noexcept
        {
            const char* value = std::getenv(name);
            return value == nullptr || value[0] == '\0';
        }

        void StopFrame(void* context)
        {
            static_cast<GuiLauncherDispatcherFrame*>(context)->Continue(false);
        }

        [[nodiscard]] std::string ExceptionMessage(std::exception_ptr exception)
        {
            return Detail::GuiLauncherPlatformInstance().ExceptionMessage(
                std::move(exception));
        }

        [[nodiscard]] std::optional<std::string> ExceptionStackTrace(
            std::exception_ptr exception)
        {
            return Detail::GuiLauncherPlatformInstance().ExceptionStackTrace(
                std::move(exception));
        }

        void WriteLauncherOpenFailure(std::exception_ptr exception)
        {
            std::cout << "[launcher] the window could not be opened: "
                << ExceptionMessage(exception) << '\n';
            std::cout << "[launcher] falling back to the text launcher\n";
            MphRead::Mods::DebugLog::Exception("launcher", std::move(exception));
        }

        void WriteToolkitFailure(std::exception_ptr exception)
        {
            std::cout << "[launcher] the window toolkit could not start: "
                << ExceptionMessage(exception) << '\n';
            MphRead::Mods::DebugLog::Exception("launcher", std::move(exception));
        }

        void WriteMatchFailure(std::exception_ptr exception)
        {
            std::cout << '\n';
            std::cout << "The game could not start: "
                << ExceptionMessage(exception) << '\n';
            const std::optional<std::string> stack = ExceptionStackTrace(exception);
            std::cout << (stack ? *stack : std::string{}) << '\n';
            MphRead::Mods::DebugLog::Line("crash", "the match could not start");
            MphRead::Mods::DebugLog::Exception("crash", std::move(exception));
        }
    }

    bool GuiLauncher::TryRun()
    {
        if (!EnsureSetup())
        {
            return false;
        }
        try
        {
            Run();
            return true;
        }
        catch (const std::exception&)
        {
            WriteLauncherOpenFailure(std::current_exception());
            return false;
        }
    }

    bool GuiLauncher::EnsureSetup()
    {
        if (_setUp.load(std::memory_order_relaxed))
        {
            return true;
        }
        if (_failed.load(std::memory_order_relaxed) || !Probe())
        {
            return false;
        }
        try
        {
#if defined(ANDROID) || defined(__ANDROID__)
            _setUp.store(false, std::memory_order_relaxed);
            return false;
#else
            std::shared_ptr<GuiLauncherAppBuilder> builder
                = Detail::GuiLauncherPlatformInstance().ConfigureLauncherApp();
            if (!builder)
            {
                throw GuiLauncherNullReferenceException();
            }
            builder->UsePlatformDetect().WithInterFont().SetupWithoutStarting();
            _setUp.store(true, std::memory_order_relaxed);
            return true;
#endif
        }
        catch (const std::exception&)
        {
            _failed.store(true, std::memory_order_relaxed);
            WriteToolkitFailure(std::current_exception());
            SayWhyOnLinux();
            return false;
        }
    }

    void GuiLauncher::SayWhyOnLinux()
    {
        if (IsWindows() || IsMacOS() || IsAndroid())
        {
            return;
        }
        std::cout << "[launcher] the game itself is unaffected -- the text launcher "
            << "below starts the same matches.\n";
        std::cout << "[launcher] the window needs libICE, libSM and fontconfig, which "
            << "a minimal install often lacks:\n";
        std::cout << "[launcher]   Debian/Ubuntu: sudo apt install libice6 libsm6 "
            << "libfontconfig1\n";
        std::cout << "[launcher]   Fedora: sudo dnf install libICE libSM fontconfig\n";
        std::cout << "[launcher]   NixOS/Guix: run it inside an FHS environment, "
            << "e.g. steam-run ./FruityPrime -launcher\n";
    }

    bool GuiLauncher::Probe()
    {
        if (IsWindows() || IsMacOS())
        {
            return true;
        }
        if (IsNullOrEmptyEnvironmentVariable("DISPLAY")
            && IsNullOrEmptyEnvironmentVariable("WAYLAND_DISPLAY"))
        {
            std::cout << "[launcher] no DISPLAY or WAYLAND_DISPLAY; "
                << "using the text launcher\n";
            return false;
        }
        return true;
    }

    void GuiLauncher::Run()
    {
        LauncherPrefs::Load();
        if (GameFiles::Ready())
        {
            GameFiles::ApplyPaths();
            MphRead::Mods::ThumbnailGenerator::EnsureCustomPreviews();
        }
        std::vector<std::string> rooms;

        while (true)
        {
            MphRead::Mods::PauseMenu::Reset();
            std::shared_ptr<MphRead::MenuSettings> settings
                = MphRead::GameState::LoadSettings();
            MphRead::Mods::GameSettings::Apply(settings);
            LauncherPrefs::Load();
            MphRead::Mods::WindowMode::Startup(LauncherPrefs::WindowMode());
            if (rooms.empty() && GameFiles::Ready())
            {
                rooms = MphRead::Mods::ThumbnailGenerator::MultiplayerRooms();
            }

            Hunters::Reroll();
            LaunchPlan plan = Ask(settings, rooms);
            if (plan.Kind() == LaunchKind::None)
            {
                return;
            }

            bool launchFailed = false;
            std::exception_ptr pending;
            try
            {
                MatchStart::Launch(settings, plan);
            }
            catch (const std::exception&)
            {
                launchFailed = true;
                try
                {
                    WriteMatchFailure(std::current_exception());
                }
                catch (...)
                {
                    pending = std::current_exception();
                }
            }
            catch (...)
            {
                pending = std::current_exception();
            }

            try
            {
                MphRead::Mods::Network::NetSession::Stop();
                MphRead::Mods::Network::NetHostSession::Stop();
                PauseMenuWindow::CloseIfOpen();
            }
            catch (...)
            {
                pending = std::current_exception();
            }

            if (pending)
            {
                std::rethrow_exception(pending);
            }
            if (launchFailed)
            {
                return;
            }
            if (MphRead::Mods::PauseMenu::QuitProgram())
            {
                return;
            }
        }
    }

    LaunchPlan GuiLauncher::Ask(
        const std::shared_ptr<MphRead::MenuSettings>& settings,
        const std::vector<std::string>& rooms)
    {
        GuiLauncherPlatform& platform = Detail::GuiLauncherPlatformInstance();
        std::shared_ptr<HomeWindowAdapter> adapter = platform.CreateHomeWindowAdapter();
        if (!adapter)
        {
            throw GuiLauncherNullReferenceException();
        }

        HomeWindow window(*adapter,
            HomeWindowMenuSettingsRef{settings.get()},
            HomeWindowRoomsRef{const_cast<std::vector<std::string>*>(&rooms)});
        auto frame = std::make_shared<GuiLauncherDispatcherFrame>();
        platform.AddHomeWindowClosed(*adapter,
            GuiLauncherCallback(frame.get(), &StopFrame, frame));
        platform.ShowHomeWindow(*adapter);
        platform.PushFrame(*frame);
        Pump();
        return window.Plan();
    }

    void GuiLauncher::Pump()
    {
        if (!_setUp.load(std::memory_order_relaxed))
        {
            return;
        }
        auto frame = std::make_shared<GuiLauncherDispatcherFrame>(false);
        GuiLauncherPlatform& platform = Detail::GuiLauncherPlatformInstance();
        platform.Post(GuiLauncherCallback(frame.get(), &StopFrame, frame),
            GuiLauncherDispatcherPriority::Background);
        platform.PushFrame(*frame);
    }

    void LauncherApp::Initialize(GuiLauncherPlatform& platform)
    {
        platform.AddFluentTheme(*this);
        platform.SetRequestedThemeVariantDark(*this);
        platform.BaseInitialize(*this);
    }
}

namespace MphRead::Mods::Detail
{
    bool PauseMenuGuiEnsureSetup()
    {
        return MphRead::Mods::Launcher::Gui::GuiLauncher::EnsureSetup();
    }

    void PauseMenuGuiPump()
    {
        MphRead::Mods::Launcher::Gui::GuiLauncher::Pump();
    }
}
