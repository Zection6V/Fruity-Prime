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

    enum class GuiRelativeUnit : std::int32_t
    {
        Relative,
        Absolute
    };

    struct GuiPoint final
    {
        double X;
        double Y;

        friend constexpr bool operator==(const GuiPoint&, const GuiPoint&) noexcept = default;
    };

    struct GuiRelativePoint final
    {
        GuiPoint Point;
        GuiRelativeUnit Unit;

        friend constexpr bool operator==(
            const GuiRelativePoint&, const GuiRelativePoint&) noexcept = default;
    };

    class GuiBrush final
    {
    public:
        explicit GuiBrush(GuiColor color) noexcept
            : Color(color)
        {
        }

        GuiBrush(const GuiBrush&) = delete;
        GuiBrush& operator=(const GuiBrush&) = delete;
        GuiBrush(GuiBrush&&) = delete;
        GuiBrush& operator=(GuiBrush&&) = delete;

        GuiColor Color;
        double Opacity = 1.0;
        std::shared_ptr<void> Transform;
        GuiRelativePoint TransformOrigin{{0.0, 0.0}, GuiRelativeUnit::Relative};
    };

    struct GuiFontFamily final
    {
        std::string_view Source;
        std::string_view Name;
        std::string_view KeySource;

        friend constexpr bool operator==(
            const GuiFontFamily&, const GuiFontFamily&) noexcept = default;
    };

    enum class GuiFontStyle : std::int32_t
    {
        Normal,
        Italic,
        Oblique
    };

    enum class GuiFontWeight : std::int32_t
    {
        Thin = 100,
        ExtraLight = 200,
        UltraLight = ExtraLight,
        Light = 300,
        SemiLight = 350,
        Normal = 400,
        Regular = Normal,
        Medium = 500,
        SemiBold = 600,
        DemiBold = SemiBold,
        Bold = 700,
        ExtraBold = 800,
        UltraBold = ExtraBold,
        Black = 900,
        Heavy = Black,
        Solid = Black,
        ExtraBlack = 950,
        UltraBlack = ExtraBlack
    };

    enum class GuiFontStretch : std::int32_t
    {
        UltraCondensed = 1,
        ExtraCondensed = 2,
        Condensed = 3,
        SemiCondensed = 4,
        Normal = 5,
        SemiExpanded = 6,
        Expanded = 7,
        ExtraExpanded = 8,
        UltraExpanded = 9
    };

    struct GuiTypeface final
    {
        const GuiFontFamily* FontFamily;
        GuiFontStyle Style;
        GuiFontWeight Weight;
        GuiFontStretch Stretch;

        friend constexpr bool operator==(
            const GuiTypeface& left, const GuiTypeface& right) noexcept
        {
            const bool familiesEqual = left.FontFamily == right.FontFamily
                || (left.FontFamily != nullptr && right.FontFamily != nullptr
                    && *left.FontFamily == *right.FontFamily);
            return familiesEqual
                && left.Style == right.Style
                && left.Weight == right.Weight
                && left.Stretch == right.Stretch;
        }
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

    struct GuiVector final
    {
        double X;
        double Y;

        friend constexpr bool operator==(const GuiVector&, const GuiVector&) noexcept = default;
    };

    // Avalonia.Size.
    struct GuiSize final
    {
        double Width;
        double Height;

        friend constexpr bool operator==(const GuiSize&, const GuiSize&) noexcept = default;
    };

    [[nodiscard]] constexpr GuiVector operator-(GuiPoint left, GuiPoint right) noexcept
    {
        return GuiVector{left.X - right.X, left.Y - right.Y};
    }

    struct GuiRoundedRect final
    {
        GuiRect Rect;
        GuiVector RadiiTopLeft;
        GuiVector RadiiTopRight;
        GuiVector RadiiBottomLeft;
        GuiVector RadiiBottomRight;

        friend constexpr bool operator==(
            const GuiRoundedRect&, const GuiRoundedRect&) noexcept = default;
    };

    namespace Detail
    {
        // Platform boundary for Avalonia's AssetLoader.Open + WindowIcon construction.
        // The implementation must open this exact URI, synchronously construct an owning
        // icon from the stream, release the stream before returning, and either return
        // that icon or signal failure by throwing/returning nullopt.
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

        [[nodiscard]] bool IsValueCreated() const;
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

        static GuiBrush InkBrush;
        static GuiBrush PanelBrush;
        static GuiBrush PanelLightBrush;
        static GuiBrush EdgeBrush;
        static GuiBrush TextBrush;
        static GuiBrush TextDimBrush;
        static GuiBrush AccentBrush;
        static GuiBrush WarmBrush;
        static GuiBrush GoodBrush;
        static GuiBrush BadBrush;

        static GuiBrush ScrimBrush;
        static const GuiFontFamily Display;
        static const GuiLazyWindowIcon AppIcon;

        [[nodiscard]] static GuiTypeface Face(bool bold) noexcept;
        [[nodiscard]] static GuiColor Shade(GuiColor color, double amount) noexcept;
        [[nodiscard]] static GuiRoundedRect Round(GuiRect rect, double radius) noexcept;
    };
}
