#pragma once

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>

namespace MphRead::Mods::Launcher::Gui
{
    struct GuiColor final
    {
        std::uint8_t A;
        std::uint8_t R;
        std::uint8_t G;
        std::uint8_t B;

        [[nodiscard]] static constexpr GuiColor FromRgb(
            std::uint8_t red, std::uint8_t green, std::uint8_t blue) noexcept
        {
            return GuiColor{255, red, green, blue};
        }

        [[nodiscard]] static constexpr GuiColor FromArgb(
            std::uint8_t alpha, std::uint8_t red,
            std::uint8_t green, std::uint8_t blue) noexcept
        {
            return GuiColor{alpha, red, green, blue};
        }

        friend constexpr bool operator==(const GuiColor&, const GuiColor&) noexcept = default;
    };

    struct GuiBrush final
    {
        GuiColor Color;

        friend constexpr bool operator==(const GuiBrush&, const GuiBrush&) noexcept = default;
    };

    struct GuiFontFamily final
    {
        std::string_view Source;

        friend constexpr bool operator==(
            const GuiFontFamily&, const GuiFontFamily&) noexcept = default;
    };

    enum class GuiFontStyle : std::uint8_t
    {
        Normal
    };

    enum class GuiFontWeight : std::uint16_t
    {
        Normal = 400,
        SemiBold = 600
    };

    enum class GuiFontStretch : std::uint8_t
    {
        Normal
    };

    struct GuiTypeface final
    {
        GuiFontFamily FontFamily;
        GuiFontStyle Style;
        GuiFontWeight Weight;
        GuiFontStretch Stretch;

        friend constexpr bool operator==(
            const GuiTypeface&, const GuiTypeface&) noexcept = default;
    };

    struct GuiWindowIcon final
    {
        std::shared_ptr<const void> Native;
    };

    struct GuiRect final
    {
        double X;
        double Y;
        double Width;
        double Height;

        friend constexpr bool operator==(const GuiRect&, const GuiRect&) noexcept = default;
    };

    struct GuiRoundedRect final
    {
        GuiRect Rect;
        double Radius;

        friend constexpr bool operator==(
            const GuiRoundedRect&, const GuiRoundedRect&) noexcept = default;
    };

    namespace Detail
    {
        // Platform boundary for Avalonia's AssetLoader.Open + WindowIcon construction.
        // Return nullopt when opening or decoding the resource cannot produce an icon.
        [[nodiscard]] std::optional<GuiWindowIcon> GuiThemeLoadWindowIcon(
            std::string_view uri);
    }

    class GuiLazyWindowIcon final
    {
    public:
        GuiLazyWindowIcon() = default;

        GuiLazyWindowIcon(const GuiLazyWindowIcon&) = delete;
        GuiLazyWindowIcon& operator=(const GuiLazyWindowIcon&) = delete;
        GuiLazyWindowIcon(GuiLazyWindowIcon&&) = delete;
        GuiLazyWindowIcon& operator=(GuiLazyWindowIcon&&) = delete;

        [[nodiscard]] const std::optional<GuiWindowIcon>& Value() const;

    private:
        enum class State : std::uint8_t
        {
            NotStarted,
            Running,
            Completed
        };

        mutable std::mutex _mutex;
        mutable std::condition_variable _condition;
        mutable State _state = State::NotStarted;
        mutable std::thread::id _owner;
        mutable std::optional<GuiWindowIcon> _value;
    };

    class GuiTheme final
    {
    public:
        GuiTheme() = delete;

        static const GuiColor Ink;
        static const GuiColor Panel;
        static const GuiColor PanelLight;
        static const GuiColor Edge;
        static const GuiColor Text;
        static const GuiColor TextDim;
        static const GuiColor Accent;
        static const GuiColor Warm;
        static const GuiColor Good;
        static const GuiColor Bad;

        static const GuiBrush InkBrush;
        static const GuiBrush PanelBrush;
        static const GuiBrush PanelLightBrush;
        static const GuiBrush EdgeBrush;
        static const GuiBrush TextBrush;
        static const GuiBrush TextDimBrush;
        static const GuiBrush AccentBrush;
        static const GuiBrush WarmBrush;
        static const GuiBrush GoodBrush;
        static const GuiBrush BadBrush;

        static const GuiBrush ScrimBrush;
        static const GuiFontFamily Display;
        static const GuiLazyWindowIcon AppIcon;

        [[nodiscard]] static GuiTypeface Face(bool bold) noexcept;
        [[nodiscard]] static GuiColor Shade(GuiColor color, double amount) noexcept;
        [[nodiscard]] static GuiRoundedRect Round(GuiRect rect, double radius) noexcept;
    };
}
