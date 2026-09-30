#include "Shell.hpp"

#include "DeckButton.hpp"
#include "DeckTile.hpp"
#include "EndPanelView.hpp"
#include "InGameMenu.hpp"
#include "StartScreen.hpp"
#include "UiMark.hpp"
#include "UiSurface.hpp"
#include "../../Diagnostics/LauncherWindowCheck.hpp"
#include "../../Diagnostics/RhiReadbackCheck.hpp"
#include "../../../GameState.hpp"
#include "../../../Menu.hpp"
#include "../../../Renderer.hpp"
#include "../../../NativeRuntime/OpenTK/GLFW.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/IO.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"
#include "../../../NativeRuntime/System/Runtime.hpp"
#include "../../Branding.hpp"
#include "../../DebugLog.hpp"
#include "../../EndScreen.hpp"
#include "../../GameSettings.hpp"
#include "../../MapPick.hpp"
#include "../../Network/NetHostSession.hpp"
#include "../../Network/NetSession.hpp"
#include "../../PauseMenu.hpp"
#include "../../Render/LauncherHunter.hpp"
#include "../../Render/LauncherPhoto.hpp"
#include "../../Render/UiOverlay.hpp"
#include "../../ScreenCapture.hpp"
#include "../../ThumbnailGenerator.hpp"
#include "../../WindowGeometry.hpp"
#include "../../WindowMode.hpp"
#include "../Portable/GameFiles.hpp"
#include "../Portable/LauncherPrefs.hpp"
#include "../Portable/MatchStart.hpp"
#include "../Portable/NativeFilePicker.hpp"
#include "../../../NativeRuntime/System/ExceptionText.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <locale>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

namespace MphRead::Mods::Launcher::Gui
{
    bool Shell::_active = false;
    MphRead::RenderWindow* Shell::_window = nullptr;
    std::shared_ptr<StartScreen> Shell::_front{};
    std::shared_ptr<InGameMenu> Shell::_menu{};
    std::shared_ptr<MphRead::MenuSettings> Shell::_settings
        = std::make_shared<MphRead::MenuSettings>();
    std::vector<std::string> Shell::_rooms{};
    std::optional<MphRead::Mods::Launcher::LaunchPlan> Shell::_pending{};
    bool Shell::_endMatch = false;
    bool Shell::_quit = false;
    std::int32_t Shell::_basisWidth = 0;
    std::int32_t Shell::_basisHeight = 0;
    std::shared_ptr<EndPanelView> Shell::_endPanel{};
    std::optional<MphRead::Mods::Launcher::LaunchPlan> Shell::_played{};
    std::optional<std::string> Shell::_shotDirectory{};
    std::int32_t Shell::_shotStep = 0;
    std::int32_t Shell::_shotWait = 0;
    std::int32_t Shell::_shotMisses = 0;
    std::uint64_t Shell::_shotPreviewGeneration = 0;

    namespace
    {
        using Keys = OpenTK::Windowing::GraphicsLibraryFramework::Keys;
        using MouseButton = OpenTK::Windowing::GraphicsLibraryFramework::MouseButton;
        using MphRead::Mods::Launcher::LaunchKind;
        using MphRead::Mods::Launcher::LaunchPlan;
        namespace Av = ::MphRead::NativeRuntime::Avalonia;

        [[nodiscard]] std::string ExceptionMessage(std::exception_ptr exception)
        {
            return MphRead::NativeRuntime::ExceptionMessage(exception);
        }

        [[nodiscard]] std::optional<std::string> ExceptionStackTrace(
            std::exception_ptr exception)
        {
            (void)exception;
            return std::nullopt;
        }

        [[nodiscard]] std::string Number(double value)
        {
            std::ostringstream text;
            text.imbue(std::locale::classic());
            text << std::fixed << std::setprecision(4) << value;
            std::string result = text.str();
            while (!result.empty() && result.back() == '0')
            {
                result.pop_back();
            }
            if (!result.empty() && result.back() == '.')
            {
                result.pop_back();
            }
            return result;
        }

        [[nodiscard]] std::string SizeText(OpenTK::Mathematics::Vector2i size)
        {
            return std::to_string(size.X) + "x" + std::to_string(size.Y);
        }

        [[nodiscard]] LaunchPlan WithRoomKey(const LaunchPlan& source, std::string roomKey)
        {
            LaunchPlan::Init init;
            init.Kind = source.Kind();
            init.Lobby = source.Lobby();
            init.Hunter = source.Hunter();
            init.RoomKey = std::move(roomKey);
            init.Mode = source.Mode();
            init.Bots = source.Bots();
            init.BotLevel = source.BotLevel();
            init.Port = source.Port();
            init.PlayerName = source.PlayerName();
            init.SaveSlot = source.SaveSlot();
            init.NewGame = source.NewGame();
            init.DemoPath = source.DemoPath();
            return LaunchPlan(init);
        }
    }

    bool Shell::Active() noexcept
    {
        return _active;
    }

    bool Shell::UiVisible()
    {
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        return surface != nullptr && surface->Visible();
    }

    MphRead::RenderWindow* Shell::Window() noexcept
    {
        return _window;
    }

    bool Shell::EndPanelUp() noexcept
    {
        return _endPanel != nullptr;
    }

    bool Shell::CanPlayAnother()
    {
        return _active && !_pending.has_value() && _played.has_value()
            && _played->Kind() == LaunchKind::Offline;
    }

    std::int32_t Shell::ShotMisses() noexcept
    {
        return _shotMisses;
    }

    void Shell::PublishNativeHandle(MphRead::RenderWindow& window)
    {
        if (!MphRead::NativeRuntime::IsWindows())
        {
            return;
        }
        try
        {
#if defined(_WIN32)
            NativeFilePicker::Owner(::glfwGetWin32Window(
                static_cast<GLFWwindow*>(window.WindowPtr())));
#endif
        }
        catch (...)
        {
            // An owner is an improvement, not a requirement.
        }
    }

    bool Shell::Run()
    {
        if (UiSurface::Ensure() == nullptr)
        {
            return false;
        }
        LauncherPrefs::Load();
        MphRead::Mods::Render::LauncherPhoto::Enabled(true);
        if (GameFiles::Ready())
        {
            GameFiles::ApplyPaths();
            MphRead::Mods::ThumbnailGenerator::EnsureCustomPreviews();
        }
        if (!MphRead::Mods::WindowMode::StartupForced())
        {
            MphRead::Mods::WindowMode::Startup(LauncherPrefs::WindowMode());
        }

        MphRead::RenderWindow::LogCreatingWindow();
        std::unique_ptr<MphRead::RenderWindow> window;
        bool ran = false;
        try
        {
            window = std::make_unique<MphRead::RenderWindow>(true);
            PublishNativeHandle(*window);
            _window = window.get();
            _active = true;
            ShowFrontScreen();
            window->Run();
            ran = true;
        }
        catch (const std::exception&)
        {
            const std::exception_ptr exception = std::current_exception();
            std::cout << "The window could not be opened: " << ExceptionMessage(exception) << '\n';
            MphRead::Mods::DebugLog::Exception("launcher", exception);
        }

        _active = false;
        _window = nullptr;
        _pending.reset();
        _endMatch = false;
        _quit = false;
        MphRead::Mods::Network::NetSession::Stop();
        MphRead::Mods::Network::NetHostSession::Stop();
        if (window != nullptr)
        {
            // UiOverlay owns Phase 4 VBO/IBO objects. Release them while the
            // launcher context is still alive, before RenderWindow destruction.
            MphRead::Mods::Render::UiOverlay::Release();
        }
        window.reset();
        return ran;
    }

    void Shell::BeforeFrame(MphRead::RenderWindow& window)
    {
        if (!_active)
        {
            return;
        }
        if (_quit)
        {
            _quit = false;
            window.Close();
            return;
        }
        if (window.HasScene() && MphRead::Mods::Network::NetSession::PersistentLobby()
            && MphRead::Mods::Network::NetSession::IsInLobby() && !_endMatch)
        {
            EndNetworkMatchToLobby(window);
        }
        if (window.HasScene() && (MphRead::Mods::Network::NetSession::Refused()
            || MphRead::Mods::Network::NetSession::SessionTimedOut()))
        {
            _endMatch = true;
        }
        if (_endMatch)
        {
            _endMatch = false;
            EndMatch(window);
        }
        if (_pending.has_value())
        {
            LaunchPlan plan = *_pending;
            _pending.reset();
            StartMatch(window, std::move(plan));
        }
    }

    void Shell::TickUi(MphRead::RenderWindow& window)
    {
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        if (surface == nullptr)
        {
            MphRead::Mods::Render::UiOverlay::Visible(false);
            return;
        }
        const OpenTK::Mathematics::Vector2i framebuffer = window.FramebufferSize();
        surface->Resize(framebuffer.X, framebuffer.Y);
        NotePointerBasis(window);
        if (!surface->Visible())
        {
            MphRead::Mods::Render::UiOverlay::Visible(false);
            return;
        }
        surface->Tick();
    }

    void Shell::NotePointerBasis(MphRead::RenderWindow& window)
    {
        const OpenTK::Mathematics::Vector2i framebuffer = window.FramebufferSize();
        const std::int32_t fw = framebuffer.X;
        const std::int32_t fh = framebuffer.Y;
        if (fw == _basisWidth && fh == _basisHeight)
        {
            return;
        }
        _basisWidth = fw;
        _basisHeight = fh;
        const OpenTK::Mathematics::Vector2i client = window.ClientSize();
        const double sx = client.X > 0 ? fw / static_cast<double>(client.X) : 1;
        const double sy = client.Y > 0 ? fh / static_cast<double>(client.Y) : 1;
        std::string message = "pointer basis: framebuffer " + SizeText(framebuffer)
            + ", client " + SizeText(client) + ", pointer scaled by "
            + Number(sx) + "x" + Number(sy)
            + (std::abs(sx - 1) > 0.001 || std::abs(sy - 1) > 0.001
                ? " -- not 1, so clicks land off by that fraction" : "");
        MphRead::Mods::DebugLog::Line("ui", message);
    }

    void Shell::TickEndPanel()
    {
        // EndScreen::Available() is backed by process-global game state, which
        // outlives RenderWindow::EndScene(). A results panel cannot own the UI
        // once the match scene itself is gone.
        const bool want = _window != nullptr && _window->HasScene()
            && MphRead::Mods::EndScreen::Available() && _menu == nullptr;
        if (want && !EndPanelUp())
        {
            const std::shared_ptr<UiSurface> surface = UiSurface::Ensure();
            if (surface == nullptr)
            {
                return;
            }
            std::shared_ptr<EndPanelView> panel = std::make_shared<EndPanelView>();
            _endPanel = panel;
            MphRead::Mods::EndScreen::PanelUp(true);
            surface->Show(panel);
            return;
        }
        if (!want && EndPanelUp())
        {
            CloseEndPanel();
            return;
        }
        if (_endPanel != nullptr)
        {
            _endPanel->Refresh();
        }
    }

    void Shell::CloseEndPanel()
    {
        // UiSurface is shared by the results panel, pause/settings screens and
        // launcher. Only hide it when the results panel still owns the current
        // view. A delayed results teardown must never hide a replacement view
        // that has already been shown during the match -> launcher transition.
        const std::shared_ptr<EndPanelView> panel = std::move(_endPanel);
        MphRead::Mods::EndScreen::PanelUp(false);
        if (panel == nullptr)
        {
            return;
        }
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        if (surface != nullptr && surface->View() == panel)
        {
            surface->Hide();
        }
    }

    void Shell::ShowFrontScreen()
    {
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        if (surface == nullptr)
        {
            return;
        }
        MphRead::Mods::PauseMenu::Reset();
        _settings = MphRead::GameState::LoadSettings();
        MphRead::Mods::GameSettings::Apply(_settings);
        LauncherPrefs::Load();
        if (_rooms.empty() && GameFiles::Ready())
        {
            _rooms = MphRead::Mods::ThumbnailGenerator::MultiplayerRooms();
        }
        if (_window != nullptr)
        {
            _window->Title(std::string(MphRead::Mods::Branding::Name));
        }
        if (_front == nullptr)
        {
            Hunters::Reroll();
            _front = StartScreen::Create(_settings, _rooms);
            _front->Done += [](StartScreen&, LaunchPlan plan) { Decided(std::move(plan)); };
            _front->MatchRequested += [](StartScreen&, LaunchPlan plan) { Decided(std::move(plan)); };
        }
        else
        {
            _front->Reset();
        }
        surface->Show(_front);
    }

    void Shell::Decided(LaunchPlan plan)
    {
        if (plan.Kind() == LaunchKind::None)
        {
            RequestQuit();
            return;
        }
        _pending = std::move(plan);
    }

    void Shell::PlayAnother(std::string roomKey)
    {
        if (!CanPlayAnother() || MphRead::NativeRuntime::StringIsNullOrWhiteSpace(roomKey)
            || !_played.has_value())
        {
            return;
        }
        _endMatch = true;
        _pending = WithRoomKey(*_played, std::move(roomKey));
    }

    void Shell::StartMatch(MphRead::RenderWindow& window, LaunchPlan plan)
    {
        _played = plan;
        if (_front != nullptr)
        {
            _front->SuspendLobby();
        }
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        if (surface != nullptr)
        {
            surface->Hide();
        }
        try
        {
            if (!MphRead::Mods::Launcher::MatchStart::Begin(window, _settings, plan))
            {
                MphRead::Mods::Network::NetSession::ReportMatchLoadFailed(
                    "The map could not be loaded.");
                EndMatch(window);
            }
        }
        catch (const std::exception&)
        {
            const std::exception_ptr exception = std::current_exception();
            std::cout << '\n';
            std::cout << "The game could not start: " << ExceptionMessage(exception) << '\n';
            const std::optional<std::string> stack = ExceptionStackTrace(exception);
            std::cout << (stack.has_value() ? *stack : std::string{}) << '\n';
            MphRead::Mods::DebugLog::Line("crash", "the match could not start");
            MphRead::Mods::DebugLog::Exception("crash", exception);
            MphRead::Mods::Network::NetSession::ReportMatchLoadFailed(
                ExceptionMessage(exception));
            EndMatch(window);
        }
    }

    void Shell::EndNetworkMatchToLobby(MphRead::RenderWindow& window)
    {
        CloseMenu();
        CloseEndPanel();
        window.EndScene();
        MphRead::Mods::Launcher::MatchStart::AfterMatch();
        MphRead::Mods::Network::NetSession::ResetMatchState();
        MphRead::Mods::PauseMenu::Reset();
        if (_front != nullptr)
        {
            const std::shared_ptr<UiSurface> surface = UiSurface::Current();
            if (surface != nullptr)
            {
                surface->Show(_front);
            }
            _front->ResumeLobby();
        }
    }

    void Shell::EndMatch(MphRead::RenderWindow& window)
    {
        CloseMenu();
        // Tear the results view down while it is still the surface owner.
        // ShowFrontScreen() below must be the next owner, not something a
        // delayed TickEndPanel() can subsequently hide.
        CloseEndPanel();
        window.EndScene();
        MphRead::Mods::Network::NetSession::Stop();
        MphRead::Mods::Network::NetHostSession::Stop();
        MphRead::Mods::Launcher::MatchStart::AfterMatch();
        ShowFrontScreen();
    }

    void Shell::RequestEndMatch()
    {
        if (!_active)
        {
            return;
        }
        _endMatch = true;
    }

    void Shell::RequestQuit()
    {
        _quit = true;
    }

    void Shell::LeaveMatch(MphRead::RenderWindow& window)
    {
        if (_active)
        {
            RequestEndMatch();
            return;
        }
        window.Close();
    }

    void Shell::Quit(MphRead::RenderWindow& window)
    {
        if (_active)
        {
            RequestQuit();
            return;
        }
        window.Close();
    }

    bool Shell::OpenPauseMenu()
    {
        const std::shared_ptr<UiSurface> surface = UiSurface::Ensure();
        if (surface == nullptr)
        {
            return false;
        }
        if (_menu != nullptr)
        {
            return true;
        }
        std::shared_ptr<InGameMenu> menu
            = std::make_shared<InGameMenu>(MphRead::GameState::LoadSettings());
        menu->Emptied += [](InGameMenu&) { CloseMenu(); };
        _menu = menu;
        surface->Show(menu);
        return true;
    }

    void Shell::CloseMenu()
    {
        if (_menu == nullptr)
        {
            return;
        }
        _menu.reset();
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        if (surface != nullptr)
        {
            surface->Hide();
        }
        MphRead::Mods::PauseMenu::MarkClosed();
    }

    void Shell::RequestShots(std::string directory)
    {
        _shotDirectory = std::move(directory);
        _shotStep = 0;
        _shotWait = 0;
        _shotMisses = 0;
        _shotPreviewGeneration = 0;
        NativeFilePicker::Suppressed(true);
        MphRead::Mods::WindowGeometry::Enabled(false);
    }

    void Shell::AfterDraw(MphRead::RenderWindow& window)
    {
        Diagnostics::LauncherWindowCheck::AfterDraw(window);
        if (!_shotDirectory.has_value())
        {
            return;
        }
        if (_shotWait > 0)
        {
            --_shotWait;
            return;
        }
        std::vector<ShotAction> script = Script();
        if (_shotStep >= static_cast<std::int32_t>(script.size()))
        {
            _shotDirectory.reset();
            window.Close();
            return;
        }
        script[static_cast<std::size_t>(_shotStep++)](window);
    }

    std::vector<Shell::ShotAction> Shell::Script()
    {
        return {
            [](MphRead::RenderWindow&) { Wait(20); },
            [](MphRead::RenderWindow& window) { Shot(window, "shell-start"); Escape(); Wait(15); },
            [](MphRead::RenderWindow& window) { Shot(window, "shell-escape"); Escape(); Wait(10); },
            [](MphRead::RenderWindow&) { HoverFront(); Wait(15); },
            [](MphRead::RenderWindow& window) { Shot(window, "shell-hover"); Wait(2); },
            [](MphRead::RenderWindow&) { ClickSettings(); Wait(15); },
            [](MphRead::RenderWindow& window) { Shot(window, "shell-click"); Escape(); Wait(10); },
            [](MphRead::RenderWindow& window)
            {
                window.ClientSize(OpenTK::Mathematics::Vector2i{1000, 620});
                Wait(20);
            },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-resized");
                window.WindowStateMinimized();
                Wait(20);
            },
            [](MphRead::RenderWindow& window)
            {
                if (window.WindowState() != MphRead::RendererPlatform::WindowStateValue::Minimized)
                {
                    ++_shotMisses;
                    std::cout << "[shellshot] shell-resized minimize failed\n";
                }
                window.WindowStateNormal();
                Wait(20);
            },
            [](MphRead::RenderWindow& window)
            {
                if (window.WindowState() != MphRead::RendererPlatform::WindowStateValue::Normal)
                {
                    ++_shotMisses;
                    std::cout << "[shellshot] shell-resized restore failed\n";
                }
                Shot(window, "shell-resized-restored");
                window.WindowStateMaximized();
                Wait(20);
            },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-maximized");
                window.WindowStateNormal();
                Wait(20);
            },
            [](MphRead::RenderWindow& window)
            {
                // ShellShot may inherit a saved fullscreen launcher preference.
                // This screenshot has a named state, so establish it instead of
                // assuming Toggle() always means windowed -> fullscreen.
                MphRead::Mods::WindowMode::Enter(window);
                Wait(25);
            },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-fullscreen");
                ClickSettings();
                Wait(15);
            },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-fullscreen-click");
                Escape();
                // Establish the precondition for the F11 delivery gate below.
                // The gate itself still has to enter fullscreen through
                // RenderWindow::FeedKey(); no direct mode call satisfies it.
                MphRead::Mods::WindowMode::Leave(window);
                Wait(25);
            },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-windowed");
                if (MphRead::Mods::WindowMode::IsFullscreen())
                {
                    ++_shotMisses;
                    std::cout << "[shellshot] could not establish windowed state before F11\n";
                    _shotDirectory.reset();
                    window.Close();
                    return;
                }
                Wait(5);
            },
            [](MphRead::RenderWindow& window) { WindowKey(window, KeyValue(300)); Wait(20); },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-launcher-fullscreen");
                if (!MphRead::Mods::WindowMode::IsFullscreen())
                {
                    ++_shotMisses;
                    std::cout << "[shellshot] F11 did not reach the window on the front screen\n";
                }
                WindowKey(window, KeyValue(300));
                Wait(20);
            },
            [](MphRead::RenderWindow&) { ClickIfReady([](Av::Controls::Control& control)
            {
                const auto* button = dynamic_cast<DeckButton*>(&control);
                return button != nullptr && button->Text() == "PLAY";
            }); Wait(20); },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-play");
                // The Phase 4 gate must not depend on an external live server.
                // Move deterministically from Online to Offline; the required
                // DeckTile click below proves that this transition succeeded.
                Key(KeyValue(262));
                Wait(15);
            },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-play-offline");
                const std::shared_ptr<UiSurface> surface = UiSurface::Current();
                if (surface != nullptr)
                {
                    const OpenTK::Mathematics::Vector2i client = window.ClientSize();
                    surface->PointerMoved(client.X / 2.0, client.Y / 2.0);
                }
                Scroll(60);
                Scroll(60, 1);
                Wait(10);
            },
            [](MphRead::RenderWindow&) { ClickIfReady([](Av::Controls::Control& control)
            {
                return dynamic_cast<DeckTile*>(&control) != nullptr;
            }); Wait(25); },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-play-selected");
                if (window.HasScene() || !MphRead::Mods::Render::LauncherHunter::Drawn()
                    || MphRead::Mods::Render::LauncherHunter::SceneGeneration() == 0)
                {
                    ++_shotMisses;
                    std::cout << "[shellshot] pre-match production hunter preview did not draw\n";
                    _shotDirectory.reset();
                    window.Close();
                    return;
                }
                _shotPreviewGeneration = MphRead::Mods::Render::LauncherHunter::SceneGeneration();
                std::cout << "[shellshot] pre-match production hunter preview drawn; side-scene generation="
                    << _shotPreviewGeneration << '\n';
                ClickIfReady([](Av::Controls::Control& control)
                {
                    const auto* button = dynamic_cast<DeckButton*>(&control);
                    return button != nullptr && button->Text() == "START";
                });
                Wait(40);
            },
            [](MphRead::RenderWindow& window)
            {
                if (!window.HasScene())
                {
                    ++_shotMisses;
                    std::cout << "[shellshot] Play/START did not create a real match scene\n";
                    _shotDirectory.reset();
                    window.Close();
                    return;
                }
                std::cout << "[shellshot] real match scene started through Play/START\n";
                window.Scene().BeginModelReloadDrawProbe();
                std::cout << "[shellshot] production model unload/reload/draw probe armed\n";
                Wait(8);
            },
            [](MphRead::RenderWindow& window)
            {
                if (!window.Scene().ModelReloadDrawProbePassed())
                {
                    ++_shotMisses;
                    std::cout << "[shellshot] production model unload/reload/draw probe failed: "
                        << window.Scene().ModelReloadDrawProbeStatus() << '\n';
                    _shotDirectory.reset();
                    window.Close();
                    return;
                }
                std::cout << "[shellshot] production model unload/reload/draw probe passed: "
                    << window.Scene().ModelReloadDrawProbeStatus() << '\n';
                Shot(window, "shell-match");
                window.WindowStateMinimized();
                Wait(20);
            },
            [](MphRead::RenderWindow& window)
            {
                if (window.WindowState() != MphRead::RendererPlatform::WindowStateValue::Minimized)
                {
                    ++_shotMisses;
                    std::cout << "[shellshot] shell-match minimize failed\n";
                }
                window.WindowStateNormal();
                Wait(20);
            },
            [](MphRead::RenderWindow& window)
            {
                if (window.WindowState() != MphRead::RendererPlatform::WindowStateValue::Normal)
                {
                    ++_shotMisses;
                    std::cout << "[shellshot] shell-match restore failed\n";
                }
                Shot(window, "shell-match-restored");
                if (!MphRead::Mods::Diagnostics::CheckRhiImageExports(*_shotDirectory))
                    ++_shotMisses;
                window.WindowStateMaximized();
                Wait(30);
            },
            [](MphRead::RenderWindow& window)
            {
                MphRead::Mods::PauseMenu::HandleEscape(window);
                Wait(20);
            },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-pause-maximized");
                MphRead::Mods::PauseMenu::HandleEscape(window);
                window.WindowStateNormal();
                Wait(30);
            },
            [](MphRead::RenderWindow& window)
            {
                MphRead::Mods::WindowMode::Toggle(window);
                Wait(30);
            },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-match-fullscreen");
                MphRead::Mods::PauseMenu::HandleEscape(window);
                Wait(20);
            },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-pause-fullscreen");
                MphRead::Mods::WindowMode::Toggle(window);
                Wait(30);
            },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-pause");
                Click([](Av::Controls::Control& control)
                {
                    const auto* button = dynamic_cast<DeckButton*>(&control);
                    return button != nullptr && button->Text() == "Settings";
                });
                Wait(20);
            },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-settings-ingame");
                Escape();
                Wait(15);
            },
            [](MphRead::RenderWindow& window)
            {
                Escape();
                MphRead::Mods::WindowMode::Toggle(window);
                Wait(30);
            },
            [](MphRead::RenderWindow&) { HoldResults(); Wait(60); },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-endgame");
                Click([](Av::Controls::Control& control)
                {
                    const auto* button = dynamic_cast<DeckButton*>(&control);
                    return button != nullptr && button->Text() == "Change hunter";
                });
                Wait(30);
            },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-endgame-hunter");
                Click([](Av::Controls::Control& control)
                {
                    const auto* button = dynamic_cast<DeckButton*>(&control);
                    return button != nullptr && button->Text() == "READY";
                });
                Wait(15);
            },
            [](MphRead::RenderWindow& window)
            {
                if (!MphRead::Mods::EndScreen::Ready())
                {
                    ++_shotMisses;
                    std::cout << "[shellshot] results READY did not take effect\n";
                    _shotDirectory.reset();
                    window.Close();
                    return;
                }
                std::cout << "[shellshot] results READY accepted; leaving through the shell match path\n";
                LeaveMatch(window);
                Wait(60);
            },
            [](MphRead::RenderWindow& window)
            {
                if (window.HasScene())
                {
                    ++_shotMisses;
                    std::cout << "[shellshot] READY/leave did not return to the launcher\n";
                    _shotDirectory.reset();
                    window.Close();
                    return;
                }
                if (EndPanelUp())
                {
                    ++_shotMisses;
                    std::cout << "[shellshot] results panel still marked up after launcher return\n";
                    _shotDirectory.reset();
                    window.Close();
                    return;
                }
                Shot(window, "shell-back-fullscreen");
                MphRead::Mods::WindowMode::Toggle(window);
                Wait(25);
            },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-back");
                Click([](Av::Controls::Control& control)
                {
                    const auto* button = dynamic_cast<DeckButton*>(&control);
                    return button != nullptr && button->Text() == "PLAY";
                });
                Wait(20);
            },
            [](MphRead::RenderWindow&)
            {
                Key(KeyValue(262));
                Wait(20);
            },
            [](MphRead::RenderWindow&)
            {
                ClickIfReady([](Av::Controls::Control& control)
                {
                    return dynamic_cast<DeckTile*>(&control) != nullptr;
                });
                Wait(30);
            },
            [](MphRead::RenderWindow& window)
            {
                Shot(window, "shell-return-hunter");
                const std::uint64_t generation
                    = MphRead::Mods::Render::LauncherHunter::SceneGeneration();
                if (window.HasScene() || !MphRead::Mods::Render::LauncherHunter::Drawn()
                    || generation <= _shotPreviewGeneration)
                {
                    ++_shotMisses;
                    std::cout << "[shellshot] post-match production hunter preview did not reload/draw; before="
                        << _shotPreviewGeneration << " after=" << generation << '\n';
                    _shotDirectory.reset();
                    window.Close();
                    return;
                }
                std::cout << "[shellshot] post-match production hunter preview drawn after reload; before="
                    << _shotPreviewGeneration << " after=" << generation << '\n';
                Wait(5);
            }
        };
    }

    void Shell::Wait(std::int32_t frames)
    {
        _shotWait = frames;
    }

    void Shell::ClickSettings()
    {
        if (GameFiles::Ready())
        {
            Click([](Av::Controls::Control& control)
            {
                const auto* button = dynamic_cast<DeckButton*>(&control);
                return button != nullptr && button->Text() == "SETTINGS";
            });
            return;
        }
        Click([](Av::Controls::Control& control)
        {
            const auto* mark = dynamic_cast<UiMark*>(&control);
            return mark != nullptr && mark->Label() == "choose your .nds file";
        });
    }

    void Shell::HoverFront()
    {
        if (GameFiles::Ready())
        {
            Hover([](Av::Controls::Control& control)
            {
                const auto* button = dynamic_cast<DeckButton*>(&control);
                return button != nullptr && button->Text() == "SETTINGS";
            });
            return;
        }
        Hover([](Av::Controls::Control& control)
        {
            const auto* mark = dynamic_cast<UiMark*>(&control);
            return mark != nullptr && mark->Label() == "choose your .nds file";
        });
    }

    void Shell::ClickIfReady(const ControlPredicate& match)
    {
        if (GameFiles::Ready())
        {
            Click(match);
        }
    }

    void Shell::Hover(const ControlPredicate& match)
    {
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        if (surface == nullptr || !surface->HoverOn(match))
        {
            ++_shotMisses;
            std::cout << "[shellshot] nothing on screen matched the hover\n";
        }
    }

    void Shell::Click(const ControlPredicate& match)
    {
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        if (surface == nullptr || !surface->ClickOn(match))
        {
            ++_shotMisses;
            std::cout << "[shellshot] nothing on screen matched the click\n";
        }
    }

    void Shell::Key(Keys key)
    {
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        if (surface != nullptr)
        {
            surface->KeyDown(key, Av::Input::RawInputModifiers::None);
            surface->KeyUp(key, Av::Input::RawInputModifiers::None);
        }
    }

    void Shell::Scroll(std::int32_t frames, double notches)
    {
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        if (surface == nullptr)
        {
            return;
        }
        for (std::int32_t i = 0; i < frames; ++i)
        {
            surface->PointerWheel(0, notches);
            Wait(1);
        }
    }

    void Shell::WindowKey(MphRead::RenderWindow& window, Keys key)
    {
        OpenTK::Windowing::Common::KeyboardKeyEventArgs args{};
        args.Key = key;
        window.FeedKey(args);
    }

    void Shell::Escape()
    {
        Key(KeyValue(256));
    }

    void Shell::HoldResults()
    {
        MphRead::GameState::MatchState(MphRead::MatchState::Ending);
        MphRead::GameState::MatchTime(30);
        try
        {
            MphRead::Mods::MapPick::Begin(
                MphRead::NativeRuntime::RequireReference(_settings).RoomKey, true);
        }
        catch (const std::exception&)
        {
            std::cout << "[shellshot] no ballot to show: "
                << ExceptionMessage(std::current_exception()) << '\n';
        }
    }

    void Shell::Shot(MphRead::RenderWindow& window, const std::string& name)
    {
        const std::string directory = _shotDirectory.value();
        const std::string path = MphRead::NativeRuntime::PathCombine(
            directory, name + ".png");
        MphRead::NativeRuntime::DirectoryCreateDirectory(directory);
        const OpenTK::Mathematics::Vector2i framebuffer = window.FramebufferSize();
        std::string description = "[shellshot] " + name + ": pixels=" + SizeText(framebuffer)
            + " scene=" + (window.HasScene() ? SizeText(window.Scene().Size()) : "no match")
            + " ";
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        if (surface != nullptr)
        {
            description += surface->Describe();
        }
        std::cout << description << '\n';
        const bool saved = MphRead::Mods::ScreenCapture::SaveWindow(
            framebuffer.X, framebuffer.Y, path);
        std::cout << (saved ? "[shellshot] " + path
            : "[shellshot] " + name + " could not be read from the window") << '\n';
    }

    Keys Shell::KeyValue(std::int32_t value) noexcept
    {
        return static_cast<Keys>(value);
    }

    Av::Input::MouseButton Shell::Translate(MouseButton button) noexcept
    {
        switch (button)
        {
        case MouseButton::Button2: return Av::Input::MouseButton::Right;
        case MouseButton::Button3: return Av::Input::MouseButton::Middle;
        default: return Av::Input::MouseButton::Left;
        }
    }

    Av::Input::RawInputModifiers Shell::Modifiers(
        const OpenTK::Windowing::Common::KeyboardKeyEventArgs& e) noexcept
    {
        Av::Input::RawInputModifiers modifiers = Av::Input::RawInputModifiers::None;
        if (e.Shift)
        {
            modifiers |= Av::Input::RawInputModifiers::Shift;
        }
        if (e.Control)
        {
            modifiers |= Av::Input::RawInputModifiers::Control;
        }
        if (e.Alt)
        {
            modifiers |= Av::Input::RawInputModifiers::Alt;
        }
        return modifiers;
    }

    void Shell::PointerMoved(double x, double y)
    {
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        if (surface != nullptr)
        {
            surface->PointerMoved(x, y);
        }
    }

    void Shell::PointerButton(MouseButton button, double x, double y, bool down)
    {
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        if (surface == nullptr)
        {
            return;
        }
        surface->PointerMoved(x, y);
        surface->PointerButton(Translate(button), down);
    }

    void Shell::PointerWheel(double deltaX, double deltaY)
    {
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        if (surface != nullptr)
        {
            surface->PointerWheel(deltaX, deltaY);
        }
    }

    void Shell::KeyDown(const OpenTK::Windowing::Common::KeyboardKeyEventArgs& e)
    {
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        if (surface != nullptr)
        {
            surface->KeyDown(e.Key, Modifiers(e));
        }
    }

    void Shell::KeyUp(const OpenTK::Windowing::Common::KeyboardKeyEventArgs& e)
    {
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        if (surface != nullptr)
        {
            surface->KeyUp(e.Key, Modifiers(e));
        }
    }

    void Shell::TextInput(const std::string& text)
    {
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        if (surface != nullptr)
        {
            surface->TextInput(text);
        }
    }
}
