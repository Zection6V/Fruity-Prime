#include "WindowGeometry.hpp"

#include "DebugLog.hpp"
#include "WindowMode.hpp"
#include "Launcher/Portable/LauncherPrefs.hpp"
#include "../NativeRuntime/System/DateTime.hpp"
#include "../NativeRuntime/System/ExceptionText.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <vector>

namespace MphRead::Mods
{
    namespace
    {
        using OpenTK::Mathematics::Vector2i;
        using MphRead::RendererPlatform::MonitorArea;
        using MphRead::RendererPlatform::WindowStateValue;

        // How much of the window has to be on a display for the saved position
        // to be used as it is.
        //
        // A margin rather than "any overlap at all": a window one pixel onto
        // the screen is a window that cannot be grabbed by its title bar,
        // which is the same problem as being off it entirely.
        constexpr std::int32_t VisibleMargin = 80;

        // How long the window has to hold still before the file is written.
        // DateTime ticks: 1.5 seconds.
        constexpr std::int64_t Settle = 15000000;

        bool EnabledState = true;
        bool OwnedState = false;
        bool DirtyState = false;
        std::int64_t ChangedAtTicks = 0;

        // The work area of the display this rectangle is usably on, or nothing
        // when it is on none of them.
        [[nodiscard]] std::optional<MonitorArea> Fit(
            MphRead::RenderWindow& window, Vector2i position, Vector2i size)
        {
            for (const MonitorArea& monitor : window.MonitorClientAreas())
            {
                const MonitorArea& area = monitor;
                const std::int32_t maxX = area.Min.X + area.Size.X;
                const std::int32_t maxY = area.Min.Y + area.Size.Y;
                const bool acrossX = position.X + size.X > area.Min.X + VisibleMargin
                    && position.X < maxX - VisibleMargin;
                // The *top* edge, not the whole height: a title bar below the
                // bottom of the screen is a window that cannot be moved, and
                // that is the only part of being off the edge that traps
                // anybody.
                const bool downY = position.Y >= area.Min.Y - VisibleMargin
                    && position.Y < maxY - VisibleMargin;
                if (acrossX && downY)
                {
                    return area;
                }
            }
            // Not an error: a headless run has no monitors and neither has a
            // display that went away between calls.
            return std::nullopt;
        }

        // Put it in the preference. Returns false for a shape not worth
        // keeping, and for one that is already what is held -- which is most
        // calls, since a move callback fires for a resize as well.
        [[nodiscard]] bool Store(Vector2i size, Vector2i position, bool maximized)
        {
            if (size.X <= 0 || size.Y <= 0)
            {
                return false;
            }
            if (Launcher::LauncherPrefs::WindowWidth() == size.X
                && Launcher::LauncherPrefs::WindowHeight() == size.Y
                && Launcher::LauncherPrefs::WindowX() == position.X
                && Launcher::LauncherPrefs::WindowY() == position.Y
                && Launcher::LauncherPrefs::WindowMaximized() == maximized)
            {
                return false;
            }
            Launcher::LauncherPrefs::WindowWidth(size.X);
            Launcher::LauncherPrefs::WindowHeight(size.Y);
            Launcher::LauncherPrefs::WindowX(position.X);
            Launcher::LauncherPrefs::WindowY(position.Y);
            Launcher::LauncherPrefs::WindowMaximized(maximized);
            return true;
        }

        // Read the window's shape into the preference, in memory. Returns
        // whether there was anything worth keeping.
        //
        // **Fullscreen is read through WindowMode, not off the window.** A
        // window in borderless fullscreen reports the size of the monitor and
        // a corner of 0,0, and saving that gives a player who quit from
        // fullscreen a windowed session the size of their screen with a title
        // bar pushing it off the bottom. What belongs here is the geometry
        // fullscreen was entered *from*, which is exactly what WindowMode
        // already keeps in order to put the window back.
        [[nodiscard]] bool Capture(MphRead::RenderWindow& window)
        {
            try
            {
                if (WindowMode::IsFullscreen())
                {
                    const Vector2i size = WindowMode::WindowedSize();
                    return size.X > 0 && size.Y > 0
                        && Store(size, WindowMode::WindowedLocation(), false);
                }
                if (window.WindowState() == WindowStateValue::Minimized)
                {
                    // A minimized window has no useful rectangle and its state
                    // is not one to reopen in. Whatever was kept last is a
                    // better answer than this.
                    return false;
                }
                return Store(window.ClientSize(), window.Location(),
                    window.WindowState() == WindowStateValue::Maximized);
            }
            catch (const std::exception& ex)
            {
                DebugLog::Line("window",
                    std::string("could not read the window geometry: ") + ex.what());
                return false;
            }
        }
    }

    bool WindowGeometry::Enabled() noexcept
    {
        return EnabledState;
    }

    void WindowGeometry::Enabled(bool value) noexcept
    {
        EnabledState = value;
    }

    bool WindowGeometry::Owned() noexcept
    {
        return OwnedState;
    }

    void WindowGeometry::Owned(bool value) noexcept
    {
        OwnedState = value;
    }

    void WindowGeometry::Restore(MphRead::RenderWindow& window, Vector2i minimum)
    {
        const std::int32_t width = Launcher::LauncherPrefs::WindowWidth();
        const std::int32_t height = Launcher::LauncherPrefs::WindowHeight();
        if (!EnabledState || width <= 0 || height <= 0)
        {
            return;
        }
        try
        {
            Vector2i size(std::max(width, minimum.X), std::max(height, minimum.Y));
            const Vector2i position(
                Launcher::LauncherPrefs::WindowX(), Launcher::LauncherPrefs::WindowY());
            const std::optional<MonitorArea> screen = Fit(window, position, size);
            if (!screen.has_value())
            {
                // Nothing this rectangle touches any more. The size is still
                // what the player chose, so it is kept and only the corner is
                // given up -- GLFW puts an unplaced window somewhere sensible
                // on the primary display.
                window.ClientSize(size);
                DebugLog::Line("window", "restored " + std::to_string(size.X) + "x"
                    + std::to_string(size.Y) + "; the saved corner ("
                    + std::to_string(position.X) + "," + std::to_string(position.Y)
                    + ") is on no display now");
                Launcher::LauncherPrefs::WindowX(0);
                Launcher::LauncherPrefs::WindowY(0);
            }
            else
            {
                const MonitorArea area = *screen;
                // Clamped rather than refused: a window saved on a bigger
                // screen than this one should come back as large as this one
                // allows, not at the default.
                size = Vector2i(
                    std::clamp(size.X, minimum.X, std::max(minimum.X, area.Size.X)),
                    std::clamp(size.Y, minimum.Y, std::max(minimum.Y, area.Size.Y)));
                window.ClientSize(size);
                window.Location(position);
                DebugLog::Line("window", "restored " + std::to_string(size.X) + "x"
                    + std::to_string(size.Y) + " at " + std::to_string(position.X)
                    + "," + std::to_string(position.Y));
            }
            if (Launcher::LauncherPrefs::WindowMaximized())
            {
                window.WindowStateMaximized();
            }
        }
        catch (const std::exception& ex)
        {
            DebugLog::Line("window",
                std::string("could not restore the window geometry: ") + ex.what());
        }
    }

    void WindowGeometry::Note(MphRead::RenderWindow& window)
    {
        if (!EnabledState)
        {
            return;
        }
        if (!Capture(window))
        {
            return;
        }
        DirtyState = true;
        ChangedAtTicks = ::MphRead::NativeRuntime::DateTimeUtcNowTicks();
    }

    void WindowGeometry::Flush(bool force)
    {
        if (!DirtyState)
        {
            return;
        }
        if (!force
            && ::MphRead::NativeRuntime::DateTimeUtcNowTicks() - ChangedAtTicks < Settle)
        {
            return;
        }
        DirtyState = false;
        Launcher::LauncherPrefs::Save();
        DebugLog::Line("window", "kept "
            + std::to_string(Launcher::LauncherPrefs::WindowWidth()) + "x"
            + std::to_string(Launcher::LauncherPrefs::WindowHeight()) + " at "
            + std::to_string(Launcher::LauncherPrefs::WindowX()) + ","
            + std::to_string(Launcher::LauncherPrefs::WindowY())
            + (Launcher::LauncherPrefs::WindowMaximized() ? ", maximized" : "")
            + (Launcher::LauncherPrefs::WindowMode() == WindowStartMode::BorderlessFullscreen
                ? ", fullscreen" : ""));
    }

    void WindowGeometry::NoteMode()
    {
        if (!EnabledState || !OwnedState)
        {
            return;
        }
        const WindowStartMode mode = WindowMode::IsFullscreen()
            ? WindowStartMode::BorderlessFullscreen
            : WindowStartMode::Windowed;
        if (Launcher::LauncherPrefs::WindowMode() == mode)
        {
            return;
        }
        Launcher::LauncherPrefs::WindowMode(mode);
        DirtyState = true;
        ChangedAtTicks = ::MphRead::NativeRuntime::DateTimeUtcNowTicks();
    }

    void WindowGeometry::Remember(MphRead::RenderWindow& window)
    {
        if (!EnabledState)
        {
            return;
        }
        if (Capture(window))
        {
            DirtyState = true;
        }
        Flush(true);
    }
}
