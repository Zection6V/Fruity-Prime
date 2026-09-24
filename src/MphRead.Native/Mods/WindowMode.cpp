#include "WindowMode.hpp"

#include "WindowGeometry.hpp"

#include "../Renderer.hpp"

#include "Chat/ChatBox.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace MphRead::Mods::Detail
{
    // PauseMenu owns its Open state and publishes only this narrow observation.
    [[nodiscard]] bool WindowModePauseMenuOpen();
}

namespace
{
    using MphRead::Mods::WindowStartMode;
    using OpenTK::Mathematics::Vector2i;

    constexpr std::int32_t ResizableWindowBorder = 0;
    constexpr std::int32_t HiddenWindowBorder = 2;
    constexpr std::int32_t EnterKey = 257;
    constexpr std::int32_t F11Key = 300;

    WindowStartMode startupState = WindowStartMode::Windowed;
    bool startupForcedState = false;
    bool fullscreenState = false;
    std::int32_t savedBorderState = ResizableWindowBorder;
    OpenTK::Mathematics::Vector2i savedLocationState{};
    OpenTK::Mathematics::Vector2i savedSizeState{};
    bool savedState = false;
    bool topmostState = false;

    struct Utf8CodePoint final
    {
        std::uint32_t Value;
        std::size_t Length;
    };

    [[nodiscard]] std::optional<Utf8CodePoint> DecodeUtf8Forward(
        std::string_view text, std::size_t position) noexcept
    {
        if (position >= text.size())
        {
            return std::nullopt;
        }

        const auto first = static_cast<unsigned char>(text[position]);
        if (first <= 0x7FU)
        {
            return Utf8CodePoint{first, 1};
        }

        std::uint32_t value = 0;
        std::size_t length = 0;
        std::uint32_t minimum = 0;
        if ((first & 0xE0U) == 0xC0U)
        {
            value = first & 0x1FU;
            length = 2;
            minimum = 0x80U;
        }
        else if ((first & 0xF0U) == 0xE0U)
        {
            value = first & 0x0FU;
            length = 3;
            minimum = 0x800U;
        }
        else if ((first & 0xF8U) == 0xF0U)
        {
            value = first & 0x07U;
            length = 4;
            minimum = 0x10000U;
        }
        else
        {
            return std::nullopt;
        }

        if (position + length > text.size())
        {
            return std::nullopt;
        }
        for (std::size_t index = 1; index < length; ++index)
        {
            const auto next = static_cast<unsigned char>(text[position + index]);
            if ((next & 0xC0U) != 0x80U)
            {
                return std::nullopt;
            }
            value = (value << 6) | (next & 0x3FU);
        }

        if (value < minimum || value > 0x10FFFFU
            || (value >= 0xD800U && value <= 0xDFFFU))
        {
            return std::nullopt;
        }
        return Utf8CodePoint{value, length};
    }

    [[nodiscard]] std::optional<std::pair<Utf8CodePoint, std::size_t>>
        DecodeUtf8Backward(std::string_view text, std::size_t end) noexcept
    {
        if (end == 0 || end > text.size())
        {
            return std::nullopt;
        }

        std::size_t start = end - 1;
        while (start > 0
            && (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U)
        {
            --start;
        }

        const std::optional<Utf8CodePoint> decoded = DecodeUtf8Forward(text, start);
        if (!decoded.has_value() || start + decoded->Length != end)
        {
            return std::nullopt;
        }
        return std::make_pair(*decoded, start);
    }

    [[nodiscard]] bool IsDotNetWhitespace(std::uint32_t value) noexcept
    {
        if (value >= 0x0009U && value <= 0x000DU)
        {
            return true;
        }

        switch (value)
        {
        case 0x0020U:
        case 0x0085U:
        case 0x00A0U:
        case 0x1680U:
        case 0x2000U:
        case 0x2001U:
        case 0x2002U:
        case 0x2003U:
        case 0x2004U:
        case 0x2005U:
        case 0x2006U:
        case 0x2007U:
        case 0x2008U:
        case 0x2009U:
        case 0x200AU:
        case 0x2028U:
        case 0x2029U:
        case 0x202FU:
        case 0x205FU:
        case 0x3000U:
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] std::string_view TrimDotNetWhitespace(
        std::string_view value) noexcept
    {
        std::size_t first = 0;
        std::size_t last = value.size();

        while (first < last)
        {
            const std::optional<Utf8CodePoint> decoded
                = DecodeUtf8Forward(value.substr(0, last), first);
            if (!decoded.has_value() || !IsDotNetWhitespace(decoded->Value))
            {
                break;
            }
            first += decoded->Length;
        }

        while (last > first)
        {
            const auto decoded = DecodeUtf8Backward(value, last);
            if (!decoded.has_value()
                || !IsDotNetWhitespace(decoded->first.Value))
            {
                break;
            }
            last = decoded->second;
        }

        return value.substr(first, last - first);
    }

    [[nodiscard]] bool EqualsAsciiIgnoreCase(
        std::string_view value, std::string_view expected) noexcept
    {
        if (value.size() != expected.size())
        {
            return false;
        }

        for (std::size_t index = 0; index < value.size(); ++index)
        {
            auto current = static_cast<unsigned char>(value[index]);
            if (current >= static_cast<unsigned char>('A')
                && current <= static_cast<unsigned char>('Z'))
            {
                current = static_cast<unsigned char>(
                    current + ('a' - 'A'));
            }
            if (current != static_cast<unsigned char>(expected[index]))
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::int32_t SubtractOneUnchecked(
        std::int32_t value) noexcept
    {
        std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
        bits -= 1U;
        return std::bit_cast<std::int32_t>(bits);
    }
}

namespace MphRead::Mods
{
    WindowStartMode WindowMode::Startup() noexcept
    {
        return startupState;
    }

    void WindowMode::Startup(WindowStartMode value) noexcept
    {
        startupState = value;
    }

    bool WindowMode::StartupForced() noexcept
    {
        return startupForcedState;
    }

    void WindowMode::ForceStartup(WindowStartMode mode) noexcept
    {
        startupState = mode;
        startupForcedState = true;
    }

    bool WindowMode::IsFullscreen() noexcept
    {
        return fullscreenState;
    }

    OpenTK::Mathematics::Vector2i WindowMode::WindowedSize() noexcept
    {
        return savedState ? savedSizeState : OpenTK::Mathematics::Vector2i();
    }

    OpenTK::Mathematics::Vector2i WindowMode::WindowedLocation() noexcept
    {
        return savedLocationState;
    }

    void WindowMode::ApplyStartup(MphRead::RenderWindow& window)
    {
        if (startupState == WindowStartMode::BorderlessFullscreen
            && !fullscreenState)
        {
            Enter(window);
        }
    }

    bool WindowMode::HandleKey(
        MphRead::RenderWindow& window,
        const OpenTK::Windowing::Common::KeyboardKeyEventArgs& e)
    {
        const std::int32_t key = static_cast<std::int32_t>(e.Key);
        if (key == F11Key || (key == EnterKey && e.Alt))
        {
            Toggle(window);
            return true;
        }
        return false;
    }

    void WindowMode::Toggle(MphRead::RenderWindow& window)
    {
        if (fullscreenState)
        {
            Leave(window);
        }
        else
        {
            Enter(window);
        }
    }

    void WindowMode::Enter(MphRead::RenderWindow& window)
    {
        if (fullscreenState)
        {
            return;
        }
        if (!savedState)
        {
            savedBorderState = window.WindowBorder();
            savedLocationState = window.Location();
            savedSizeState = window.ClientSize();
            savedState = true;
        }

        // Before the window is touched, not after the geometry is set --
        // and after the monitor lookup, which is the one line above that
        // can fail and leave this method without a fullscreen window to
        // describe.
        //
        // Windows dispatches WM_SIZE from inside SetWindowPos, so the
        // resize callback for the lines below runs *during* them, and
        // WindowGeometry::Capture reads this flag to decide whether the
        // rectangle it is being handed is the player's window or the
        // monitor. Set at the end instead, the one callback that matters
        // arrived while it still said "windowed", and the monitor's
        // rectangle went into the remembered window size.
        const RendererPlatform::MonitorArea monitor
            = window.CurrentMonitorClientArea();
        fullscreenState = true;

        window.WindowStateNormal();
        window.WindowBorder(HiddenWindowBorder);
        RendererPlatform::ProcessEvents();
        window.Location(monitor.Min);
        
            window.ClientSize(OpenTK::Mathematics::Vector2i{
                monitor.Size.X,
                SubtractOneUnchecked(monitor.Size.Y)});

        SetTopmost(window, true);
        // And written down, so the next session opens this way. See
        // WindowGeometry::NoteMode: F11 used to be a decision the program
        // forgot on exit.
        WindowGeometry::NoteMode();
    }

    void WindowMode::Leave(MphRead::RenderWindow& window)
    {
        if (!fullscreenState)
        {
            return;
        }

        window.WindowStateNormal();
        
            window.WindowBorder(savedState ? savedBorderState : ResizableWindowBorder);
        RendererPlatform::ProcessEvents();

        if (savedState)
        {
            window.ClientSize(savedSizeState);
            window.Location(savedLocationState);
        }

        fullscreenState = false;
        // Forget it, so the *next* Enter captures where the window is then.
        // Without this the saved rectangle is whatever the window was the
        // first time fullscreen was ever used: go fullscreen, come back, drag
        // the window somewhere else, go fullscreen again, and leaving put it
        // back at the first size rather than the one it was just at.
        savedState = false;
        SetTopmost(window, false);
        WindowGeometry::NoteMode();
    }

    bool WindowMode::IsTopmost() noexcept
    {
        return topmostState;
    }

    void WindowMode::SetTopmost(
        MphRead::RenderWindow& window, bool topmost)
    {
        if (topmostState == topmost)
        {
            return;
        }

        topmostState = topmost;
        try
        {
            window.Floating(topmost);
        }
        catch (...)
        {
        }
    }

    void WindowMode::SyncTopmost(MphRead::RenderWindow& window)
    {
        // Not "unless the pause menu is up" any more. That exception was for
        // a menu that was its *own* window and had to be allowed above this
        // one; the menu is drawn inside this window now, so dropping out of
        // the band while it is open only lets the taskbar cover the bottom of
        // our own screen -- which is where Save and Cancel are, and they
        // became unclickable the moment Escape was pressed in fullscreen.
        SetTopmost(window, fullscreenState && window.IsFocused());
    }

    WindowStartMode WindowMode::Parse(
        std::optional<std::string_view> value,
        WindowStartMode fallback)
    {
        if (!value.has_value())
        {
            return fallback;
        }

        const std::string_view text = TrimDotNetWhitespace(*value);
        if (EqualsAsciiIgnoreCase(text, "borderless")
            || EqualsAsciiIgnoreCase(text, "fullscreen")
            || EqualsAsciiIgnoreCase(text, "borderless fullscreen")
            || text == "1"
            || EqualsAsciiIgnoreCase(text, "true"))
        {
            return WindowStartMode::BorderlessFullscreen;
        }
        if (EqualsAsciiIgnoreCase(text, "windowed")
            || EqualsAsciiIgnoreCase(text, "window")
            || text == "0"
            || EqualsAsciiIgnoreCase(text, "false"))
        {
            return WindowStartMode::Windowed;
        }
        return fallback;
    }
}

namespace MphRead::Mods::Launcher::Detail
{
    void TextLauncherSetWindowStartup(MphRead::Mods::WindowStartMode value)
    {
        MphRead::Mods::WindowMode::Startup(value);
    }
}
