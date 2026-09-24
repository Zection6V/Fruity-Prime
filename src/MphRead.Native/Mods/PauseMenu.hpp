#pragma once

#include <atomic>
#include <cstdint>

namespace MphRead
{
    class RenderWindow;
}

namespace MphRead::Mods
{
    class PauseMenu final
    {
    public:
        PauseMenu() = delete;

        [[nodiscard]] static bool Open() noexcept;
        [[nodiscard]] static bool LeftMatch() noexcept;
        [[nodiscard]] static bool QuitProgram() noexcept;

        [[nodiscard]] static std::int32_t WindowX() noexcept;
        [[nodiscard]] static std::int32_t WindowY() noexcept;
        [[nodiscard]] static std::int32_t WindowWidth() noexcept;
        [[nodiscard]] static std::int32_t WindowHeight() noexcept;

        [[nodiscard]] static bool WindowMoved() noexcept;
        static void WindowMoved(bool value) noexcept;

        [[nodiscard]] static bool HandleEscape(MphRead::RenderWindow& window);
        static void Poll(MphRead::RenderWindow& window);
        static void Reset() noexcept;

        static void RequestLeave() noexcept;
        static void RequestQuit() noexcept;
        static void RequestFullscreenToggle() noexcept;
        static void MarkClosed() noexcept;

    private:
        static void TakeWindowRect(MphRead::RenderWindow& window);
        static void OpenMenu();
        static void Close();

        static std::atomic_bool _open;
        static std::atomic_bool _leaveRequested;
        static std::atomic_bool _quit;
        static std::atomic_bool _toggleFullscreen;
        static std::atomic_bool _refocus;
        static bool _leftMatch;
        static bool _quitProgram;
        static std::int32_t _windowX;
        static std::int32_t _windowY;
        static std::int32_t _windowWidth;
        static std::int32_t _windowHeight;
        static bool _windowMoved;
    };
}
