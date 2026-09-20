#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace OpenTK::Windowing::Common
{
    struct KeyboardKeyEventArgs;
}

namespace MphRead
{
    class RenderWindow;
}

namespace MphRead::Mods
{
    enum class WindowStartMode : std::int32_t
    {
        Windowed,
        BorderlessFullscreen
    };

    class WindowMode final
    {
    public:
        WindowMode() = delete;

        [[nodiscard]] static WindowStartMode Startup() noexcept;
        static void Startup(WindowStartMode value) noexcept;

        [[nodiscard]] static bool IsFullscreen() noexcept;

        static void ApplyStartup(MphRead::RenderWindow& window);
        [[nodiscard]] static bool HandleKey(
            MphRead::RenderWindow& window,
            const OpenTK::Windowing::Common::KeyboardKeyEventArgs& e);
        static void Toggle(MphRead::RenderWindow& window);
        static void Enter(MphRead::RenderWindow& window);
        static void Leave(MphRead::RenderWindow& window);

        [[nodiscard]] static bool IsTopmost() noexcept;
        static void SetTopmost(MphRead::RenderWindow& window, bool topmost);
        static void SyncTopmost(MphRead::RenderWindow& window);

        [[nodiscard]] static WindowStartMode Parse(
            std::optional<std::string_view> value,
            WindowStartMode fallback);
    };
}
