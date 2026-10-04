#include "PauseMenu.hpp"

#include "../Renderer.hpp"

#include "WindowMode.hpp"
#if defined(MPHREAD_SHELL)
#include "Launcher/GuiLauncher.hpp"
#include "Launcher/Shell.hpp"
#endif

#include <atomic>
#include <exception>

namespace MphRead::Mods
{
    std::atomic_bool PauseMenu::_open{false};
    std::atomic_bool PauseMenu::_leaveRequested{false};
    std::atomic_bool PauseMenu::_quit{false};
    std::atomic_bool PauseMenu::_toggleFullscreen{false};
    std::atomic_bool PauseMenu::_refocus{false};
    bool PauseMenu::_leftMatch = false;
    bool PauseMenu::_quitProgram = false;

    bool PauseMenu::Open() noexcept
    {
        return _open.load(std::memory_order_acquire);
    }

    bool PauseMenu::LeftMatch() noexcept
    {
        return _leftMatch;
    }

    bool PauseMenu::QuitProgram() noexcept
    {
        return _quitProgram;
    }

    bool PauseMenu::HandleEscape(MphRead::RenderWindow& window)
    {
#if defined(MPHREAD_SHELL)
        if (!Launcher::Gui::GuiLauncher::EnsureSetup())
        {
            return false;
        }
        if (_open.load(std::memory_order_acquire))
        {
            Close();
            return true;
        }
        OpenMenu();
        return true;
#else
        return false;
#endif
    }

    void PauseMenu::Poll(MphRead::RenderWindow& window)
    {
        if (_refocus.load(std::memory_order_acquire))
        {
            _refocus.store(false, std::memory_order_release);
            try
            {
                window.Focus();
            }
            catch (const std::exception&)
            {
            }
        }
        WindowMode::SyncTopmost(window);
        if (_toggleFullscreen.load(std::memory_order_acquire))
        {
            _toggleFullscreen.store(false, std::memory_order_release);
            WindowMode::Toggle(window);
        }
        if (_quit.load(std::memory_order_acquire))
        {
            _quit.store(false, std::memory_order_release);
            _quitProgram = true;
            Close();
#if defined(MPHREAD_SHELL)
            Launcher::Gui::Shell::Quit(window);
#else
            window.Close();
#endif
        }
        else if (_leaveRequested.load(std::memory_order_acquire))
        {
            _leaveRequested.store(false, std::memory_order_release);
            _leftMatch = true;
            Close();
#if defined(MPHREAD_SHELL)
            Launcher::Gui::Shell::LeaveMatch(window);
#else
            window.Close();
#endif
        }
    }

    void PauseMenu::Reset() noexcept
    {
        _leftMatch = false;
        _quitProgram = false;
        _leaveRequested.store(false, std::memory_order_release);
        _quit.store(false, std::memory_order_release);
    }

    void PauseMenu::RequestLeave() noexcept
    {
        _leaveRequested.store(true, std::memory_order_release);
    }

    void PauseMenu::RequestQuit() noexcept
    {
        _quit.store(true, std::memory_order_release);
    }

    void PauseMenu::RequestFullscreenToggle() noexcept
    {
        _toggleFullscreen.store(true, std::memory_order_release);
    }

    void PauseMenu::MarkClosed() noexcept
    {
        _open.store(false, std::memory_order_release);
        _refocus.store(true, std::memory_order_release);
    }

    void PauseMenu::OpenMenu()
    {
#if defined(MPHREAD_SHELL)
        const bool opened = Launcher::Gui::Shell::OpenPauseMenu();
        _open.store(opened, std::memory_order_release);
#endif
    }

    void PauseMenu::Close()
    {
#if defined(MPHREAD_SHELL)
        Launcher::Gui::Shell::CloseMenu();
#endif
        _open.store(false, std::memory_order_release);
    }
}

namespace MphRead::Mods::Detail
{
    bool WindowModePauseMenuOpen()
    {
        return PauseMenu::Open();
    }
}
