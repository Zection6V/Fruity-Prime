#include "WindowMode.hpp"

#include "../Selection.hpp"

#include <bit>
#include <cstddef>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <locale.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace MphRead::Mods::Detail
{
    // Narrow platform boundary for the NativeWindow members observed by WindowMode.cs.
    // The renderer/window provider owns these operations; this migration owns only the
    // C# policy and ordering around them.
    struct WindowModeVector2i final
    {
        std::int32_t X = 0;
        std::int32_t Y = 0;
    };

    struct WindowModeMonitorArea final
    {
        WindowModeVector2i Min{};
        WindowModeVector2i Size{};
    };

    [[nodiscard]] std::int32_t WindowModeWindowBorder(MphRead::RenderWindow& window);
    [[nodiscard]] WindowModeVector2i WindowModeLocation(MphRead::RenderWindow& window);
    [[nodiscard]] WindowModeVector2i WindowModeClientSize(MphRead::RenderWindow& window);
    [[nodiscard]] WindowModeMonitorArea WindowModeMonitorClientArea(
        MphRead::RenderWindow& window);

    void WindowModeSetWindowStateNormal(MphRead::RenderWindow& window);
    void WindowModeSetWindowBorder(MphRead::RenderWindow& window, std::int32_t border);
    void WindowModePollEvents();
    void WindowModeSetLocation(
        MphRead::RenderWindow& window, WindowModeVector2i location);
    void WindowModeSetClientSize(
        MphRead::RenderWindow& window, WindowModeVector2i size);
    void WindowModeSetFloating(MphRead::RenderWindow& window, bool floating);
    [[nodiscard]] bool WindowModeIsFocused(MphRead::RenderWindow& window);

    // PauseMenu.cs is later in the dependency-first migration order.
    [[nodiscard]] bool WindowModePauseMenuOpen();
}

namespace
{
    using MphRead::Mods::WindowStartMode;
    using MphRead::Mods::Detail::WindowModeVector2i;

    constexpr std::int32_t ResizableWindowBorder = 0;
    constexpr std::int32_t HiddenWindowBorder = 2;
    constexpr std::int32_t EnterKey = 257;
    constexpr std::int32_t F11Key = 300;

    WindowStartMode startupState = WindowStartMode::Windowed;
    bool fullscreenState = false;
    std::int32_t savedBorderState = ResizableWindowBorder;
    WindowModeVector2i savedLocationState{};
    WindowModeVector2i savedSizeState{};
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

    enum class Utf8DecodeStatus
    {
        Done,
        NeedMoreData,
        InvalidData
    };

    [[nodiscard]] Utf8DecodeStatus DecodeUtf8Scalar(
        const std::uint8_t* data,
        std::size_t size,
        std::size_t& index,
        std::uint32_t& scalar) noexcept
    {
        const std::size_t start = index;
        if (start >= size)
        {
            scalar = 0xFFFDU;
            return Utf8DecodeStatus::NeedMoreData;
        }

        const std::uint32_t first = data[start];
        if (first <= 0x7FU)
        {
            scalar = first;
            index = start + 1;
            return Utf8DecodeStatus::Done;
        }
        if (first < 0xC2U || first > 0xF4U)
        {
            scalar = 0xFFFDU;
            index = start + 1;
            return Utf8DecodeStatus::InvalidData;
        }
        if (start + 1 >= size)
        {
            scalar = 0xFFFDU;
            index = size;
            return Utf8DecodeStatus::NeedMoreData;
        }

        const std::uint32_t second = data[start + 1];
        if ((second & 0xC0U) != 0x80U)
        {
            scalar = 0xFFFDU;
            index = start + 1;
            return Utf8DecodeStatus::InvalidData;
        }
        if (first <= 0xDFU)
        {
            scalar = ((first & 0x1FU) << 6) | (second & 0x3FU);
            index = start + 2;
            return Utf8DecodeStatus::Done;
        }
        if ((first == 0xE0U && second < 0xA0U)
            || (first == 0xEDU && second >= 0xA0U)
            || (first == 0xF0U && second < 0x90U)
            || (first == 0xF4U && second >= 0x90U))
        {
            scalar = 0xFFFDU;
            index = start + 1;
            return Utf8DecodeStatus::InvalidData;
        }
        if (start + 2 >= size)
        {
            scalar = 0xFFFDU;
            index = size;
            return Utf8DecodeStatus::NeedMoreData;
        }

        const std::uint32_t third = data[start + 2];
        if ((third & 0xC0U) != 0x80U)
        {
            scalar = 0xFFFDU;
            index = start + 2;
            return Utf8DecodeStatus::InvalidData;
        }
        if (first <= 0xEFU)
        {
            scalar = ((first & 0x0FU) << 12)
                | ((second & 0x3FU) << 6)
                | (third & 0x3FU);
            index = start + 3;
            return Utf8DecodeStatus::Done;
        }
        if (start + 3 >= size)
        {
            scalar = 0xFFFDU;
            index = size;
            return Utf8DecodeStatus::NeedMoreData;
        }

        const std::uint32_t fourth = data[start + 3];
        if ((fourth & 0xC0U) != 0x80U)
        {
            scalar = 0xFFFDU;
            index = start + 3;
            return Utf8DecodeStatus::InvalidData;
        }

        scalar = ((first & 0x07U) << 18)
            | ((second & 0x3FU) << 12)
            | ((third & 0x3FU) << 6)
            | (fourth & 0x3FU);
        index = start + 4;
        return Utf8DecodeStatus::Done;
    }

    void AppendUtf8(std::string& output, std::uint32_t scalar)
    {
        if (scalar <= 0x7FU)
        {
            output.push_back(static_cast<char>(scalar));
        }
        else if (scalar <= 0x7FFU)
        {
            output.push_back(static_cast<char>(0xC0U | (scalar >> 6)));
            output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
        }
        else if (scalar <= 0xFFFFU)
        {
            output.push_back(static_cast<char>(0xE0U | (scalar >> 12)));
            output.push_back(static_cast<char>(0x80U | ((scalar >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
        }
        else
        {
            output.push_back(static_cast<char>(0xF0U | (scalar >> 18)));
            output.push_back(static_cast<char>(0x80U | ((scalar >> 12) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | ((scalar >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
        }
    }

#if !defined(_WIN32)
    [[nodiscard]] void* FindVersionedIcuSymbol(
        void* library, const char* base) noexcept
    {
        if (library == nullptr)
        {
            return nullptr;
        }
        if (void* symbol = dlsym(library, base); symbol != nullptr)
        {
            return symbol;
        }
        char name[96]{};
        for (int version = 99; version >= 50; --version)
        {
            const int count = std::snprintf(
                name, sizeof(name), "%s_%d", base, version);
            if (count <= 0
                || static_cast<std::size_t>(count) >= sizeof(name))
            {
                continue;
            }
            if (void* symbol = dlsym(library, name); symbol != nullptr)
            {
                return symbol;
            }
        }
        return nullptr;
    }

    [[nodiscard]] std::uint32_t IcuLower(
        std::uint32_t scalar) noexcept
    {
        using CaseFunction = std::int32_t (*)(std::int32_t);
        static const CaseFunction function = []() noexcept
        {
            void* library = dlopen("libicuuc.so", RTLD_LAZY | RTLD_LOCAL);
#if defined(__APPLE__)
            if (library == nullptr)
            {
                library = dlopen(
                    "/usr/lib/libicucore.A.dylib",
                    RTLD_LAZY | RTLD_LOCAL);
            }
#endif
            return reinterpret_cast<CaseFunction>(
                FindVersionedIcuSymbol(library, "u_tolower"));
        }();

        if (function == nullptr || scalar > 0x10FFFFU)
        {
            return scalar;
        }
        const std::int32_t mapped = function(
            static_cast<std::int32_t>(scalar));
        return mapped < 0
            ? scalar
            : static_cast<std::uint32_t>(mapped);
    }
#endif

    [[nodiscard]] std::uint32_t InvariantLowerScalar(
        std::uint32_t scalar) noexcept
    {
        if (scalar >= 'A' && scalar <= 'Z')
        {
            return scalar + ('a' - 'A');
        }

        // Match the invariant casing behavior used by .NET on all platforms.
        if (scalar == 0x0130U)
        {
            return scalar;
        }

#if defined(_WIN32)
        wchar_t source[2]{};
        int sourceLength = 0;
        if (scalar <= 0xFFFFU)
        {
            source[0] = static_cast<wchar_t>(scalar);
            sourceLength = 1;
        }
        else if (scalar <= 0x10FFFFU)
        {
            const std::uint32_t value = scalar - 0x10000U;
            source[0] = static_cast<wchar_t>(
                0xD800U + (value >> 10));
            source[1] = static_cast<wchar_t>(
                0xDC00U + (value & 0x3FFU));
            sourceLength = 2;
        }
        if (sourceLength != 0)
        {
            wchar_t target[2]{};
            const int mapped = LCMapStringEx(
                LOCALE_NAME_INVARIANT,
                LCMAP_LOWERCASE,
                source,
                sourceLength,
                target,
                2,
                nullptr,
                nullptr,
                0);
            if (mapped == 1)
            {
                return static_cast<std::uint32_t>(target[0]);
            }
            if (mapped == 2
                && target[0] >= 0xD800
                && target[0] <= 0xDBFF
                && target[1] >= 0xDC00
                && target[1] <= 0xDFFF)
            {
                return 0x10000U
                    + ((static_cast<std::uint32_t>(target[0]) - 0xD800U) << 10)
                    + (static_cast<std::uint32_t>(target[1]) - 0xDC00U);
            }
        }
#else
        const std::uint32_t mapped = IcuLower(scalar);
        if (mapped != scalar)
        {
            return mapped;
        }

        static locale_t locale = []() noexcept
        {
            locale_t value = newlocale(
                LC_CTYPE_MASK, "C.UTF-8", nullptr);
            if (value == nullptr)
            {
                value = newlocale(
                    LC_CTYPE_MASK, "en_US.UTF-8", nullptr);
            }
            return value;
        }();
        if (locale != nullptr
            && scalar <= static_cast<std::uint32_t>(WCHAR_MAX))
        {
            const wint_t mappedWide
                = towlower_l(static_cast<wint_t>(scalar), locale);
            if (mappedWide != WEOF)
            {
                return static_cast<std::uint32_t>(mappedWide);
            }
        }
#endif

        if (scalar >= 0x00C0U && scalar <= 0x00D6U)
        {
            return scalar + 0x20U;
        }
        if (scalar >= 0x00D8U && scalar <= 0x00DEU)
        {
            return scalar + 0x20U;
        }
        if (scalar == 0x0178U)
        {
            return 0x00FFU;
        }
        if (scalar >= 0x0391U && scalar <= 0x03A1U)
        {
            return scalar + 0x20U;
        }
        if (scalar >= 0x03A3U && scalar <= 0x03ABU)
        {
            return scalar + 0x20U;
        }
        if (scalar >= 0x0410U && scalar <= 0x042FU)
        {
            return scalar + 0x20U;
        }
        return scalar;
    }

    [[nodiscard]] std::string InvariantLower(
        std::string_view value)
    {
        std::string result;
        result.reserve(value.size());
        std::size_t index = 0;
        while (index < value.size())
        {
            std::uint32_t scalar = 0;
            (void)DecodeUtf8Scalar(
                reinterpret_cast<const std::uint8_t*>(value.data()),
                value.size(),
                index,
                scalar);
            AppendUtf8(result, InvariantLowerScalar(scalar));
        }
        return result;
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

    bool WindowMode::IsFullscreen() noexcept
    {
        return fullscreenState;
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
            savedBorderState = Detail::WindowModeWindowBorder(window);
            savedLocationState = Detail::WindowModeLocation(window);
            savedSizeState = Detail::WindowModeClientSize(window);
            savedState = true;
        }

        const Detail::WindowModeMonitorArea monitor
            = Detail::WindowModeMonitorClientArea(window);

        Detail::WindowModeSetWindowStateNormal(window);
        Detail::WindowModeSetWindowBorder(window, HiddenWindowBorder);
        Detail::WindowModePollEvents();
        Detail::WindowModeSetLocation(window, monitor.Min);
        Detail::WindowModeSetClientSize(
            window,
            Detail::WindowModeVector2i{
                monitor.Size.X,
                SubtractOneUnchecked(monitor.Size.Y)});

        fullscreenState = true;
        SetTopmost(window, true);
    }

    void WindowMode::Leave(MphRead::RenderWindow& window)
    {
        if (!fullscreenState)
        {
            return;
        }

        Detail::WindowModeSetWindowStateNormal(window);
        Detail::WindowModeSetWindowBorder(
            window,
            savedState ? savedBorderState : ResizableWindowBorder);
        Detail::WindowModePollEvents();

        if (savedState)
        {
            Detail::WindowModeSetClientSize(window, savedSizeState);
            Detail::WindowModeSetLocation(window, savedLocationState);
        }

        fullscreenState = false;
        SetTopmost(window, false);
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
            Detail::WindowModeSetFloating(window, topmost);
        }
        catch (...)
        {
        }
    }

    void WindowMode::SyncTopmost(MphRead::RenderWindow& window)
    {
        const bool topmost
            = fullscreenState
            && !Detail::WindowModePauseMenuOpen()
            && Detail::WindowModeIsFocused(window);
        SetTopmost(window, topmost);
    }

    WindowStartMode WindowMode::Parse(
        std::optional<std::string_view> value,
        WindowStartMode fallback)
    {
        if (!value.has_value())
        {
            return fallback;
        }

        const std::string text
            = InvariantLower(TrimDotNetWhitespace(*value));

        if (text == "borderless"
            || text == "fullscreen"
            || text == "borderless fullscreen"
            || text == "1"
            || text == "true")
        {
            return WindowStartMode::BorderlessFullscreen;
        }
        if (text == "windowed"
            || text == "window"
            || text == "0"
            || text == "false")
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

    MphRead::Mods::WindowStartMode LauncherPrefsWindowModeParse(
        std::string_view value,
        MphRead::Mods::WindowStartMode fallback)
    {
        return MphRead::Mods::WindowMode::Parse(
            std::optional<std::string_view>{value},
            fallback);
    }
}
