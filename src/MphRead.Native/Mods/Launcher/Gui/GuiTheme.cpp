#include "GuiTheme.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr std::string_view AppIconUri
        = "avares://FruityPrime/Assets/fruity-prime-mark.png";

    [[nodiscard]] std::uint8_t DoubleToByteUnchecked(double value) noexcept
    {
        if (std::isnan(value) || value <= 0.0)
        {
            return 0;
        }
        if (value >= 255.0)
        {
            return 255;
        }
        return static_cast<std::uint8_t>(value);
    }
}

namespace MphRead::Mods::Launcher::Gui
{
    const GuiColor GuiTheme::Ink = GuiColor::FromRgb(10, 12, 16);
    const GuiColor GuiTheme::Panel = GuiColor::FromRgb(18, 21, 28);
    const GuiColor GuiTheme::PanelLight = GuiColor::FromRgb(26, 31, 41);
    const GuiColor GuiTheme::Edge = GuiColor::FromRgb(38, 46, 60);
    const GuiColor GuiTheme::Text = GuiColor::FromRgb(230, 234, 242);
    const GuiColor GuiTheme::TextDim = GuiColor::FromRgb(138, 147, 166);
    const GuiColor GuiTheme::Accent = GuiColor::FromRgb(41, 197, 255);
    const GuiColor GuiTheme::Warm = GuiColor::FromRgb(255, 179, 71);
    const GuiColor GuiTheme::Good = GuiColor::FromRgb(110, 231, 135);
    const GuiColor GuiTheme::Bad = GuiColor::FromRgb(255, 107, 107);

    GuiBrush GuiTheme::InkBrush{Ink};
    GuiBrush GuiTheme::PanelBrush{Panel};
    GuiBrush GuiTheme::PanelLightBrush{PanelLight};
    GuiBrush GuiTheme::EdgeBrush{Edge};
    GuiBrush GuiTheme::TextBrush{Text};
    GuiBrush GuiTheme::TextDimBrush{TextDim};
    GuiBrush GuiTheme::AccentBrush{Accent};
    GuiBrush GuiTheme::WarmBrush{Warm};
    GuiBrush GuiTheme::GoodBrush{Good};
    GuiBrush GuiTheme::BadBrush{Bad};

    GuiBrush GuiTheme::ScrimBrush{
        GuiColor::FromArgb(196, Ink.R, Ink.G, Ink.B)};

    const GuiFontFamily GuiTheme::Display{
        "avares://Avalonia.Fonts.Inter/Assets#Inter",
        "Inter",
        "avares://Avalonia.Fonts.Inter/Assets"};

    const GuiLazyWindowIcon GuiTheme::AppIcon{};

    bool GuiLazyWindowIcon::IsValueCreated() const
    {
        std::lock_guard lock(_mutex);
        return _state == State::Completed;
    }

    const std::optional<GuiWindowIcon>& GuiLazyWindowIcon::Value() const
    {
        {
            std::unique_lock lock(_mutex);
            for (;;)
            {
                if (_state == State::Completed)
                {
                    return _value;
                }
                if (_state == State::Running)
                {
                    if (_owner == std::this_thread::get_id())
                    {
                        throw std::logic_error(
                            "ValueFactory attempted to access the Value property of this instance.");
                    }
                    _condition.wait(lock, [this]
                    {
                        return _state != State::Running;
                    });
                    continue;
                }

                _state = State::Running;
                _owner = std::this_thread::get_id();
                break;
            }
        }

        std::optional<GuiWindowIcon> value;
        try
        {
            value = Detail::GuiThemeLoadWindowIcon(AppIconUri);
        }
        catch (...)
        {
            value.reset();
        }

        {
            std::lock_guard lock(_mutex);
            _value = std::move(value);
            _owner = std::thread::id{};
            _state = State::Completed;
        }
        _condition.notify_all();
        return _value;
    }

    GuiTypeface GuiTheme::Face(bool bold) noexcept
    {
        return GuiTypeface{
            &Display,
            GuiFontStyle::Normal,
            bold ? GuiFontWeight::SemiBold : GuiFontWeight::Normal,
            GuiFontStretch::Normal
        };
    }

    GuiColor GuiTheme::Shade(GuiColor color, double amount) noexcept
    {
        const double t = amount < 0.0 ? -amount : amount;
        const int target = amount >= 0.0 ? 255 : 0;
        return GuiColor::FromArgb(
            color.A,
            DoubleToByteUnchecked(color.R + (target - color.R) * t),
            DoubleToByteUnchecked(color.G + (target - color.G) * t),
            DoubleToByteUnchecked(color.B + (target - color.B) * t));
    }

    GuiRoundedRect GuiTheme::Round(GuiRect rect, double radius) noexcept
    {
        const GuiVector radii{radius, radius};
        return GuiRoundedRect{rect, radii, radii, radii, radii};
    }
}
