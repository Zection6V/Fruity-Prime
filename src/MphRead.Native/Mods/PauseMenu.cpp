#include "PauseMenu.hpp"

#include "WindowMode.hpp"

#include <atomic>
#include <cstdint>

namespace MphRead::Mods::Detail
{
    [[nodiscard]] std::int32_t PauseMenuClientLocationX(MphRead::RenderWindow& window);
    [[nodiscard]] std::int32_t PauseMenuClientLocationY(MphRead::RenderWindow& window);
    [[nodiscard]] std::int32_t PauseMenuClientSizeX(MphRead::RenderWindow& window);
    [[nodiscard]] std::int32_t PauseMenuClientSizeY(MphRead::RenderWindow& window);
    void PauseMenuFocus(MphRead::RenderWindow& window);
    void PauseMenuCloseGameWindow(MphRead::RenderWindow& window);

#if defined(MPHREAD_AVALONIA)
    [[nodiscard]] bool PauseMenuGuiEnsureSetup();
    void PauseMenuGuiFollowGameWindow();
    void PauseMenuGuiPump();
    [[nodiscard]] bool PauseMenuGuiOpenWindow();
    void PauseMenuGuiCloseWindowIfOpen();
#endif
}

namespace MphRead::Mods
{
    std::atomic_bool PauseMenu::_open{false};
    std::atomic_bool PauseMenu::_leave{false};
    std::atomic_bool PauseMenu::_quit{false};
    std::atomic_bool PauseMenu::_toggleFullscreen{false};
    std::atomic_bool PauseMenu::_refocus{false};
    bool PauseMenu::_leftMatch = false;
    bool PauseMenu::_quitProgram = false;
    std::int32_t PauseMenu::_windowX = 0;
    std::int32_t PauseMenu::_windowY = 0;
    std::int32_t PauseMenu::_windowWidth = 0;
    std::int32_t PauseMenu::_windowHeight = 0;
    bool PauseMenu::_windowMoved = false;

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

    std::int32_t PauseMenu::WindowX() noexcept
    {
        return _windowX;
    }

    std::int32_t PauseMenu::WindowY() noexcept
    {
        return _windowY;
    }

    std::int32_t PauseMenu::WindowWidth() noexcept
    {
        return _windowWidth;
    }

    std::int32_t PauseMenu::WindowHeight() noexcept
    {
        return _windowHeight;
    }

    bool PauseMenu::WindowMoved() noexcept
    {
        return _windowMoved;
    }

    void PauseMenu::WindowMoved(bool value) noexcept
    {
        _windowMoved = value;
    }

    void PauseMenu::TakeWindowRect(MphRead::RenderWindow& window)
    {
        const std::int32_t x = Detail::PauseMenuClientLocationX(window);
        const std::int32_t y = Detail::PauseMenuClientLocationY(window);
        const std::int32_t width = Detail::PauseMenuClientSizeX(window);
        const std::int32_t height = Detail::PauseMenuClientSizeY(window);
        if (x == _windowX && y == _windowY
            && width == _windowWidth && height == _windowHeight)
        {
            return;
        }
        _windowX = x;
        _windowY = y;
        _windowWidth = width;
        _windowHeight = height;
        _windowMoved = true;
    }

    bool PauseMenu::HandleEscape(MphRead::RenderWindow& window)
    {
        TakeWindowRect(window);
#if defined(MPHREAD_AVALONIA)
        if (!Detail::PauseMenuGuiEnsureSetup())
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
#if defined(MPHREAD_AVALONIA)
        if (_open.load(std::memory_order_acquire))
        {
            TakeWindowRect(window);
            if (_windowMoved)
            {
                _windowMoved = false;
                Detail::PauseMenuGuiFollowGameWindow();
            }
            Detail::PauseMenuGuiPump();
        }
#endif
        if (_refocus.load(std::memory_order_acquire))
        {
            _refocus.store(false, std::memory_order_release);
            try
            {
                Detail::PauseMenuFocus(window);
            }
            catch (...)
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
            Detail::PauseMenuCloseGameWindow(window);
        }
        else if (_leave.load(std::memory_order_acquire))
        {
            _leave.store(false, std::memory_order_release);
            _leftMatch = true;
            Close();
            Detail::PauseMenuCloseGameWindow(window);
        }
    }

    void PauseMenu::Reset() noexcept
    {
        _leftMatch = false;
        _quitProgram = false;
        _leave.store(false, std::memory_order_release);
        _quit.store(false, std::memory_order_release);
    }

    void PauseMenu::RequestLeave() noexcept
    {
        _leave.store(true, std::memory_order_release);
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
#if defined(MPHREAD_AVALONIA)
        const bool opened = Detail::PauseMenuGuiOpenWindow();
        _open.store(opened, std::memory_order_release);
#endif
    }

    void PauseMenu::Close()
    {
#if defined(MPHREAD_AVALONIA)
        Detail::PauseMenuGuiCloseWindowIfOpen();
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
