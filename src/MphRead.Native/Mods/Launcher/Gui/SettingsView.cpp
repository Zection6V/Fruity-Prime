#include "SettingsView.hpp"

#include "CrosshairPreview.hpp"
#include "../../../Features.hpp"
#include "../../../Formats/Enums.hpp"
#include "../../../GameState.hpp"
#include "../../../Menu.hpp"
#include "../../Branding.hpp"
#include "../../Credits.hpp"
#include "../../GameSettings.hpp"
#include "../../Input/PadBindings.hpp"
#include "../../Input/PointerInput.hpp"
#include "../../Input/StylusZone.hpp"
#include "../../Input/TouchSettings.hpp"
#include "../../InputSettings.hpp"
#include "../../Network/DemoClip.hpp"
#include "../../Network/PlayerColors.hpp"
#include "../../Render/Crosshair.hpp"
#include "../../Render/FrameTiming.hpp"
#include "../../RenderOptions.hpp"
#include "../../RespawnChoice.hpp"
#include "../../Update/Updater.hpp"
#include "../../WindowMode.hpp"
#include "../Portable/GameFiles.hpp"
#include "../Portable/LauncherPrefs.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    using namespace MphRead;
    using namespace MphRead::Mods;
    using namespace MphRead::Mods::Input;
    using namespace MphRead::Mods::Launcher;
    using namespace MphRead::Mods::Launcher::Gui;
    using namespace MphRead::Mods::Network;
    using namespace MphRead::Mods::Render;

    [[nodiscard]] std::u16string ToUtf16(std::string_view text)
    {
        std::u16string result;
        result.reserve(text.size());
        std::size_t index = 0;
        while (index < text.size())
        {
            const auto first = static_cast<unsigned char>(text[index]);
            char32_t value = 0;
            std::size_t length = 1;
            if (first <= 0x7FU)
            {
                value = first;
            }
            else if ((first & 0xE0U) == 0xC0U && index + 1 < text.size())
            {
                value = first & 0x1FU;
                length = 2;
            }
            else if ((first & 0xF0U) == 0xE0U && index + 2 < text.size())
            {
                value = first & 0x0FU;
                length = 3;
            }
            else if ((first & 0xF8U) == 0xF0U && index + 3 < text.size())
            {
                value = first & 0x07U;
                length = 4;
            }
            else
            {
                value = 0xFFFDU;
                length = 1;
            }

            bool valid = value != 0xFFFDU || first == 0xEFU;
            if (length > 1)
            {
                for (std::size_t i = 1; i < length; ++i)
                {
                    const auto continuation = static_cast<unsigned char>(text[index + i]);
                    if ((continuation & 0xC0U) != 0x80U)
                    {
                        valid = false;
                        break;
                    }
                    value = (value << 6) | (continuation & 0x3FU);
                }
                const char32_t minimum = length == 2 ? 0x80U : length == 3 ? 0x800U : 0x10000U;
                if (value < minimum || value > 0x10FFFFU
                    || (value >= 0xD800U && value <= 0xDFFFU))
                {
                    valid = false;
                }
            }
            if (!valid)
            {
                value = 0xFFFDU;
                length = 1;
            }

            if (value <= 0xFFFFU)
            {
                result.push_back(static_cast<char16_t>(value));
            }
            else
            {
                value -= 0x10000U;
                result.push_back(static_cast<char16_t>(0xD800U + (value >> 10)));
                result.push_back(static_cast<char16_t>(0xDC00U + (value & 0x3FFU)));
            }
            index += length;
        }
        return result;
    }

    [[nodiscard]] std::string ToUtf8(std::u16string_view text)
    {
        std::string result;
        result.reserve(text.size());
        for (std::size_t index = 0; index < text.size(); ++index)
        {
            char32_t value = text[index];
            if (value >= 0xD800U && value <= 0xDBFFU
                && index + 1 < text.size()
                && text[index + 1] >= 0xDC00U && text[index + 1] <= 0xDFFFU)
            {
                value = 0x10000U
                    + ((value - 0xD800U) << 10)
                    + (text[++index] - 0xDC00U);
            }
            else if (value >= 0xD800U && value <= 0xDFFFU)
            {
                value = 0xFFFDU;
            }

            if (value <= 0x7FU)
            {
                result.push_back(static_cast<char>(value));
            }
            else if (value <= 0x7FFU)
            {
                result.push_back(static_cast<char>(0xC0U | (value >> 6)));
                result.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
            }
            else if (value <= 0xFFFFU)
            {
                result.push_back(static_cast<char>(0xE0U | (value >> 12)));
                result.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
                result.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
            }
            else
            {
                result.push_back(static_cast<char>(0xF0U | (value >> 18)));
                result.push_back(static_cast<char>(0x80U | ((value >> 12) & 0x3FU)));
                result.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
                result.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
            }
        }
        return result;
    }

    [[nodiscard]] constexpr bool DotNetWhiteSpace(char16_t value) noexcept
    {
        return (value >= u'\u0009' && value <= u'\u000D')
            || value == u'\u0020'
            || value == u'\u0085'
            || value == u'\u00A0'
            || value == u'\u1680'
            || (value >= u'\u2000' && value <= u'\u200A')
            || value == u'\u2028'
            || value == u'\u2029'
            || value == u'\u202F'
            || value == u'\u205F'
            || value == u'\u3000';
    }

    [[nodiscard]] std::u16string Trim(std::u16string_view text)
    {
        std::size_t first = 0;
        while (first < text.size() && DotNetWhiteSpace(text[first]))
        {
            ++first;
        }
        std::size_t last = text.size();
        while (last > first && DotNetWhiteSpace(text[last - 1]))
        {
            --last;
        }
        return std::u16string(text.substr(first, last - first));
    }

    [[nodiscard]] std::string_view TrimNumberWhiteSpace(std::string_view text) noexcept
    {
        auto isWhite = [](unsigned char c) noexcept
        {
            return c == 0x20U || (c >= 0x09U && c <= 0x0DU);
        };
        std::size_t first = 0;
        while (first < text.size() && isWhite(static_cast<unsigned char>(text[first])))
        {
            ++first;
        }
        std::size_t last = text.size();
        while (last > first && text[last - 1] == '\0')
        {
            --last;
        }
        while (last > first && isWhite(static_cast<unsigned char>(text[last - 1])))
        {
            --last;
        }
        return text.substr(first, last - first);
    }

    [[nodiscard]] bool EqualsIgnoreCaseAscii(
        std::string_view left, std::string_view right) noexcept
    {
        if (left.size() != right.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            unsigned char a = static_cast<unsigned char>(left[i]);
            unsigned char b = static_cast<unsigned char>(right[i]);
            if (a >= 'A' && a <= 'Z') a = static_cast<unsigned char>(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = static_cast<unsigned char>(b + ('a' - 'A'));
            if (a != b) return false;
        }
        return true;
    }

    [[nodiscard]] bool TryParseSingleInvariant(std::string_view text, float& result)
    {
        text = TrimNumberWhiteSpace(text);
        if (text.empty())
        {
            result = 0.0F;
            return false;
        }
        if (EqualsIgnoreCaseAscii(text, "NaN")
            || EqualsIgnoreCaseAscii(text, "+NaN")
            || EqualsIgnoreCaseAscii(text, "-NaN"))
        {
            result = std::numeric_limits<float>::quiet_NaN();
            return true;
        }
        if (EqualsIgnoreCaseAscii(text, "Infinity")
            || EqualsIgnoreCaseAscii(text, "+Infinity"))
        {
            result = std::numeric_limits<float>::infinity();
            return true;
        }
        if (EqualsIgnoreCaseAscii(text, "-Infinity"))
        {
            result = -std::numeric_limits<float>::infinity();
            return true;
        }

        bool positiveSign = false;
        if (!text.empty() && text.front() == '+')
        {
            positiveSign = true;
            text.remove_prefix(1);
            if (text.empty())
            {
                result = 0.0F;
                return false;
            }
        }

        float value = 0.0F;
        const char* first = text.data();
        const char* last = text.data() + text.size();
        const auto parsed = std::from_chars(first, last, value, std::chars_format::general);
        if (parsed.ec == std::errc{} && parsed.ptr == last)
        {
            result = value;
            return true;
        }
        if (parsed.ec == std::errc::result_out_of_range && parsed.ptr == last)
        {
            const bool negative = !positiveSign && !text.empty() && text.front() == '-';
            std::size_t exponentPosition = text.find_first_of("eE");
            bool overflow = false;
            if (exponentPosition != std::string_view::npos)
            {
                std::string_view exponent = text.substr(exponentPosition + 1);
                bool exponentNegative = false;
                if (!exponent.empty() && (exponent.front() == '+' || exponent.front() == '-'))
                {
                    exponentNegative = exponent.front() == '-';
                    exponent.remove_prefix(1);
                }
                overflow = !exponentNegative;
            }
            else
            {
                const std::size_t dot = text.find('.');
                const std::size_t digitCount = dot == std::string_view::npos
                    ? text.size() - (negative ? 1U : 0U)
                    : dot - (negative ? 1U : 0U);
                overflow = digitCount > 1;
            }
            result = overflow
                ? (negative ? -std::numeric_limits<float>::infinity()
                            : std::numeric_limits<float>::infinity())
                : (negative ? -0.0F : 0.0F);
            return true;
        }
        result = 0.0F;
        return false;
    }

    [[nodiscard]] std::int32_t RoundToInt32(double value) noexcept
    {
        if (std::isnan(value) || value > static_cast<double>(std::numeric_limits<std::int32_t>::max())
            || value < static_cast<double>(std::numeric_limits<std::int32_t>::min()))
        {
            return std::numeric_limits<std::int32_t>::min();
        }
        const double floorValue = std::floor(value);
        const double fraction = value - floorValue;
        double rounded = floorValue;
        if (fraction > 0.5)
        {
            rounded = floorValue + 1.0;
        }
        else if (fraction == 0.5)
        {
            const auto integer = static_cast<std::int64_t>(floorValue);
            rounded = (integer & 1LL) == 0 ? floorValue : floorValue + 1.0;
        }
        return static_cast<std::int32_t>(rounded);
    }

    [[nodiscard]] std::string FloatInvariant(float value)
    {
        std::array<char, 64> buffer{};
        const auto converted = std::to_chars(
            buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::general);
        if (converted.ec != std::errc{})
        {
            throw std::runtime_error("Floating-point formatting failed.");
        }
        return std::string(buffer.data(), converted.ptr);
    }

    [[nodiscard]] std::u16string Fixed2(float value)
    {
        std::array<char, 64> buffer{};
        const auto converted = std::to_chars(
            buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::fixed, 2);
        if (converted.ec != std::errc{})
        {
            throw std::runtime_error("Floating-point formatting failed.");
        }
        return ToUtf16(std::string_view(buffer.data(),
            static_cast<std::size_t>(converted.ptr - buffer.data())));
    }

    [[nodiscard]] bool TryParseInt32Invariant(std::u16string_view source, std::int32_t& result) noexcept
    {
        while (!source.empty() && source.back() == u'\0')
        {
            source.remove_suffix(1);
        }
        const std::u16string trimmed = Trim(source);
        if (trimmed.empty())
        {
            result = 0;
            return false;
        }
        std::string ascii;
        ascii.reserve(trimmed.size());
        for (char16_t c : trimmed)
        {
            if (c > 0x7FU)
            {
                result = 0;
                return false;
            }
            ascii.push_back(static_cast<char>(c));
        }
        const char* first = ascii.data();
        const char* last = ascii.data() + ascii.size();
        if (first != last && *first == '+')
        {
            ++first;
            if (first == last)
            {
                result = 0;
                return false;
            }
        }
        std::int32_t parsed = 0;
        const auto conversion = std::from_chars(first, last, parsed, 10);
        if (conversion.ec != std::errc{} || conversion.ptr != last)
        {
            result = 0;
            return false;
        }
        result = parsed;
        return true;
    }

    class VectorStringList final : public RowsStringList
    {
    public:
        explicit VectorStringList(std::vector<std::u16string> values)
            : _values(std::move(values))
        {
        }

        [[nodiscard]] std::int32_t Count() const override
        {
            return static_cast<std::int32_t>(_values.size());
        }

        [[nodiscard]] std::optional<std::u16string> At(std::int32_t index) const override
        {
            if (index < 0 || static_cast<std::size_t>(index) >= _values.size())
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
            return _values[static_cast<std::size_t>(index)];
        }

    private:
        std::vector<std::u16string> _values;
    };

    template <std::size_t N>
    class StringArrayList final : public RowsStringList
    {
    public:
        explicit StringArrayList(std::array<std::string, N>& values) noexcept
            : _values(values)
        {
        }

        [[nodiscard]] std::int32_t Count() const override
        {
            return static_cast<std::int32_t>(N);
        }

        [[nodiscard]] std::optional<std::u16string> At(std::int32_t index) const override
        {
            if (index < 0 || static_cast<std::size_t>(index) >= N)
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
            return ToUtf16(_values[static_cast<std::size_t>(index)]);
        }

    private:
        std::array<std::string, N>& _values;
    };

    [[nodiscard]] RowsStringListRef Strings(std::initializer_list<std::u16string_view> values)
    {
        std::vector<std::u16string> result;
        result.reserve(values.size());
        for (const std::u16string_view value : values)
        {
            result.emplace_back(value);
        }
        return std::make_shared<VectorStringList>(std::move(result));
    }

    [[nodiscard]] RowsStringListRef Strings(std::vector<std::u16string> values)
    {
        return std::make_shared<VectorStringList>(std::move(values));
    }

    [[nodiscard]] std::u16string OptionalToUtf16(const std::optional<std::string>& value)
    {
        return value.has_value() ? ToUtf16(*value) : std::u16string{};
    }

    [[nodiscard]] MenuSettings& RequireSettings(const std::shared_ptr<MenuSettings>& settings)
    {
        if (!settings)
        {
            throw SettingsViewNullReferenceException();
        }
        return *settings;
    }

    template <typename T>
    [[nodiscard]] T& Require(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw SettingsViewNullReferenceException();
        }
        return *value;
    }

    [[nodiscard]] std::string HunterName(Hunter hunter)
    {
        switch (hunter)
        {
        case Hunter::Samus: return "Samus";
        case Hunter::Kanden: return "Kanden";
        case Hunter::Trace: return "Trace";
        case Hunter::Sylux: return "Sylux";
        case Hunter::Noxus: return "Noxus";
        case Hunter::Spire: return "Spire";
        case Hunter::Weavel: return "Weavel";
        case Hunter::Guardian: return "Guardian";
        case Hunter::Random: return "Random";
        }
        return std::to_string(static_cast<std::int32_t>(hunter));
    }

    [[nodiscard]] Hunter ParseHunter(std::u16string_view value)
    {
        if (value == u"Samus") return Hunter::Samus;
        if (value == u"Kanden") return Hunter::Kanden;
        if (value == u"Trace") return Hunter::Trace;
        if (value == u"Sylux") return Hunter::Sylux;
        if (value == u"Noxus") return Hunter::Noxus;
        if (value == u"Spire") return Hunter::Spire;
        if (value == u"Weavel") return Hunter::Weavel;
        if (value == u"Guardian") return Hunter::Guardian;
        if (value == u"Random") return Hunter::Random;
        std::int32_t numeric = 0;
        if (TryParseInt32Invariant(value, numeric))
        {
            return static_cast<Hunter>(numeric);
        }
        throw std::invalid_argument("Requested value was not found.");
    }

    [[nodiscard]] std::vector<std::u16string> LanguageNames()
    {
        return {u"English", u"Japanese", u"French", u"Spanish", u"German", u"Italian"};
    }

    struct FpsStop final
    {
        std::u16string_view Label;
        std::int32_t Cap;
    };

    constexpr std::array<FpsStop, 13> FpsStops{{
        {u"Display (VSync)", FrameTiming::DisplayRate},
        {u"30 fps", 30},
        {u"60 fps", 60},
        {u"75 fps", 75},
        {u"90 fps", 90},
        {u"100 fps", 100},
        {u"120 fps", 120},
        {u"144 fps", 144},
        {u"165 fps", 165},
        {u"180 fps", 180},
        {u"200 fps", 200},
        {u"240 fps", 240},
        {u"Unlimited", FrameTiming::MaxCap}
    }};

    [[nodiscard]] std::int32_t FpsLimitStopIndex(std::int32_t cap) noexcept
    {
        for (std::size_t i = 0; i < FpsStops.size(); ++i)
        {
            if (FpsStops[i].Cap == cap)
            {
                return static_cast<std::int32_t>(i);
            }
        }
        std::int32_t best = 0;
        for (std::size_t i = 1; i < FpsStops.size(); ++i)
        {
            if (FpsStops[i].Cap <= cap)
            {
                best = static_cast<std::int32_t>(i);
            }
        }
        return best;
    }

    [[nodiscard]] std::int32_t IndexOf(
        const std::vector<std::u16string>& values, std::u16string_view value) noexcept
    {
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            if (values[i] == value)
            {
                return static_cast<std::int32_t>(i);
            }
        }
        return -1;
    }

    class PreviewContext final : public CrosshairPreviewDrawingContext
    {
    public:
        explicit PreviewContext(RowsDrawingContext& context) noexcept
            : _context(context)
        {
        }

        void DrawRectangle(CrosshairPreviewBrush brush, CrosshairPreviewPen pen,
            CrosshairPreviewRoundedRect rect) override
        {
            _context.DrawRectangle(ToBrush(brush),
                RowsPen(ToBrush(pen.Brush), pen.Thickness),
                RowsRoundedRect{ToRect(rect.Rect), rect.Radius});
        }

        void FillRectangle(CrosshairPreviewBrush brush, CrosshairPreviewRect rect) override
        {
            _context.FillRectangle(ToBrush(brush), ToRect(rect));
        }

        void DrawEllipse(std::optional<CrosshairPreviewBrush> brush,
            CrosshairPreviewPen pen, CrosshairPreviewPoint center,
            double radiusX, double radiusY) override
        {
            _context.DrawEllipse(brush.has_value() ? ToBrush(*brush) : RowsBrush::Transparent(),
                RowsPen(ToBrush(pen.Brush), pen.Thickness),
                RowsPoint{center.X, center.Y}, radiusX, radiusY);
        }

    private:
        [[nodiscard]] static RowsBrush ToBrush(CrosshairPreviewBrush brush)
        {
            switch (brush)
            {
            case CrosshairPreviewBrush::Panel:
                return RowsBrush::Reference(GuiTheme::PanelBrush);
            case CrosshairPreviewBrush::Edge:
                return RowsBrush::Edge();
            case CrosshairPreviewBrush::Text:
                return RowsBrush::Text();
            }
            return RowsBrush::Transparent();
        }

        [[nodiscard]] static GuiRect ToRect(CrosshairPreviewRect rect) noexcept
        {
            return GuiRect{rect.X, rect.Y, rect.Width, rect.Height};
        }

        RowsDrawingContext& _context;
    };

    [[nodiscard]] KeyRowGlfwKey ChatKeyGet(void*)
    {
        return static_cast<KeyRowGlfwKey>(InputSettings::ChatKey());
    }

    void ChatKeySet(void*, KeyRowGlfwKey key)
    {
        InputSettings::ChatKey(static_cast<InputKey>(key));
    }

    [[nodiscard]] KeyRowGlfwKey ClipKeyGet(void*)
    {
        return static_cast<KeyRowGlfwKey>(InputSettings::ClipKey());
    }

    void ClipKeySet(void*, KeyRowGlfwKey key)
    {
        InputSettings::ClipKey(static_cast<InputKey>(key));
    }
}

namespace MphRead::Mods::Launcher::Gui
{
    struct SettingsViewSection final
    {
        SettingsViewControlRef<MenuEntry> Button;
        SettingsViewControlHandle Page;
    };

    struct SettingsViewTouchRow final
    {
        Mods::Input::TouchControl Control;
        SettingsViewControlRef<ToggleRow> Row;
    };

    struct SettingsViewState final
    {
        SettingsViewAdapter* Adapter = nullptr;
        SettingsView* Sender = nullptr;
        std::shared_ptr<MenuSettings> Settings;
        bool InGame = false;

        SettingsViewControlHandle Rail;
        SettingsViewControlHandle RailWrap;
        SettingsViewControlHandle Pages;
        SettingsViewControlHandle Grid;
        SettingsViewControlHandle RailPanel;
        SettingsViewControlHandle FooterPanel;
        SettingsViewControlHandle RailScroll;
        SettingsViewControlRef<Caption> Heading;
        std::vector<SettingsViewSection> Sections;

        bool Narrow = false;
        bool LaidOut = false;
        bool Saved = false;

        SettingsViewControlRef<ChoiceRow> WindowRow;
        SettingsViewControlRef<ChoiceRow> ClipSecondsRow;
        SettingsViewControlRef<SliderRow> ResolutionScale;
        SettingsViewControlRef<ToggleRow> LightingRow;
        SettingsViewControlRef<ToggleRow> FogRow;
        SettingsViewControlRef<ToggleRow> FilteringRow;
        SettingsViewControlRef<ToggleRow> CelRow;
        SettingsViewControlRef<ToggleRow> FpsRow;
        SettingsViewControlRef<SliderRow> FpsLimitRow;
        SettingsViewControlRef<ToggleRow> ProHud;
        SettingsViewControlRef<ChoiceRow> CrosshairSizeRow;
        SettingsViewControlRef<ChoiceRow> CrosshairStyleRow;
        SettingsViewControlRef<ChoiceRow> WeaponStyleRow;
        SettingsViewControlRef<SliderRow> SfxVolume;
        SettingsViewControlRef<SliderRow> MusicVolume;
        SettingsViewControlRef<ChoiceRow> LanguageRow;
        SettingsViewControlRef<SliderRow> Sensitivity;
        SettingsViewControlRef<ToggleRow> InvertY;
        SettingsViewControlRef<ToggleRow> InvertX;
        SettingsViewControlRef<ToggleRow> PenTablet;
        SettingsViewControlRef<ToggleRow> ScrollAllWeapons;
        SettingsViewControlRef<SliderRow> GamepadLook;
        SettingsViewControlRef<SliderRow> GamepadDeadZone;
        SettingsViewControlRef<ToggleRow> GamepadInvertY;
        SettingsViewControlRef<FieldRow> PointGoal;
        SettingsViewControlRef<FieldRow> TimeLimit;
        SettingsViewControlRef<ChoiceRow> DamageRow;
        SettingsViewControlRef<ToggleRow> TeamPlay;
        SettingsViewControlRef<ToggleRow> FriendlyFire;
        SettingsViewControlRef<ToggleRow> Radar;
        SettingsViewControlRef<ToggleRow> Affinity;
        SettingsViewControlRef<ToggleRow> ShadowFreeze;
        SettingsViewControlRef<FieldRow> PlayerName;
        SettingsViewControlRef<ChoiceRow> HunterRow;
        SettingsViewControlRef<ChoiceRow> ColorRow;
        SettingsViewControlRef<FieldRow> ServerRow;
        SettingsViewControlRef<FieldRow> MasterRow;
        SettingsViewControlRef<ToggleRow> AutoUpdate;
        SettingsViewControlRef<Note> SaveError;
        SettingsViewControlRef<ToggleRow> TouchButtonsRow;
        std::vector<SettingsViewTouchRow> TouchRows;
        SettingsViewControlRef<ToggleRow> StylusZone;
        SettingsViewControlRef<SliderRow> StylusOpacity;
        std::vector<SettingsViewControlRef<PadRow>> PadRows;
        std::vector<SettingsViewControlRef<KeyRow>> KeyRows;

        SettingsViewEvent Closed;
        SettingsViewEvent GameFilesRequested;
        SettingsViewEvent StylusPlacementRequested;
    };

    struct SettingsViewSectionClickTarget final
    {
        SettingsViewState* View = nullptr;
        SettingsViewControlHandle Page;
    };

    struct SettingsViewSupportClickTarget final
    {
        std::shared_ptr<MenuEntry> Entry;
    };

    const SettingsViewEventArgs SettingsViewEventArgs::Empty{};

    SettingsViewNullReferenceException::SettingsViewNullReferenceException()
        : std::runtime_error("Object reference not set to an instance of an object.")
    {
    }

    SettingsViewEventHandler::SettingsViewEventHandler(
        std::shared_ptr<void> target, Callback function)
    {
        if (function != nullptr)
        {
            auto list = std::make_shared<std::vector<Invocation>>();
            list->push_back(Invocation{std::move(target), function});
            _invocations = std::move(list);
        }
    }

    SettingsViewEventHandler::SettingsViewEventHandler(
        std::shared_ptr<const std::vector<Invocation>> invocations) noexcept
        : _invocations(std::move(invocations))
    {
    }

    SettingsViewEventHandler SettingsViewEventHandler::Combine(
        const SettingsViewEventHandler& left,
        const SettingsViewEventHandler& right)
    {
        if (left.IsNull()) return right;
        if (right.IsNull()) return left;
        auto list = std::make_shared<std::vector<Invocation>>();
        list->reserve(left._invocations->size() + right._invocations->size());
        list->insert(list->end(), left._invocations->begin(), left._invocations->end());
        list->insert(list->end(), right._invocations->begin(), right._invocations->end());
        return SettingsViewEventHandler(std::move(list));
    }

    bool SettingsViewEventHandler::IsNull() const noexcept
    {
        return !_invocations || _invocations->empty();
    }

    bool operator==(
        const SettingsViewEventHandler& left,
        const SettingsViewEventHandler& right) noexcept
    {
        if (left.IsNull() || right.IsNull())
        {
            return left.IsNull() == right.IsNull();
        }
        const auto& a = *left._invocations;
        const auto& b = *right._invocations;
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
    }

    void SettingsViewEvent::Add(const SettingsViewEventHandler& handler)
    {
        if (handler.IsNull()) return;
        std::shared_ptr<const InvocationList> current = _handlers.load();
        for (;;)
        {
            std::shared_ptr<const InvocationList> desired;
            if (!current)
            {
                desired = handler._invocations;
            }
            else
            {
                auto next = std::make_shared<InvocationList>();
                next->reserve(current->size() + handler._invocations->size());
                next->insert(next->end(), current->begin(), current->end());
                next->insert(next->end(), handler._invocations->begin(), handler._invocations->end());
                desired = std::move(next);
            }
            if (_handlers.compare_exchange_weak(current, desired)) return;
        }
    }

    void SettingsViewEvent::Remove(const SettingsViewEventHandler& handler)
    {
        if (handler.IsNull()) return;
        std::shared_ptr<const InvocationList> current = _handlers.load();
        for (;;)
        {
            if (!current || current->size() < handler._invocations->size()) return;
            const std::size_t removeCount = handler._invocations->size();
            std::optional<std::size_t> match;
            for (std::size_t start = current->size() - removeCount + 1; start-- > 0;)
            {
                if (std::equal(handler._invocations->begin(), handler._invocations->end(),
                    current->begin() + static_cast<std::ptrdiff_t>(start)))
                {
                    match = start;
                    break;
                }
            }
            if (!match.has_value()) return;

            std::shared_ptr<const InvocationList> desired;
            if (removeCount != current->size())
            {
                auto next = std::make_shared<InvocationList>();
                next->reserve(current->size() - removeCount);
                next->insert(next->end(), current->begin(),
                    current->begin() + static_cast<std::ptrdiff_t>(*match));
                next->insert(next->end(),
                    current->begin() + static_cast<std::ptrdiff_t>(*match + removeCount),
                    current->end());
                desired = std::move(next);
            }
            if (_handlers.compare_exchange_weak(current, desired)) return;
        }
    }

    void SettingsViewEvent::Invoke(void* sender, const SettingsViewEventArgs& args) const
    {
        const std::shared_ptr<const InvocationList> handlers = _handlers.load();
        if (!handlers) return;
        for (const Invocation& handler : *handlers)
        {
            handler.Function(handler.Target.get(), sender, args);
        }
    }

    SettingsView::SettingsView(SettingsViewAdapter& adapter,
        std::shared_ptr<MenuSettings> settings, bool inGame)
        : _adapter(adapter), _state(std::make_shared<SettingsViewState>())
    {
        _state->Adapter = &_adapter;
        _state->Sender = this;

        // Field initializers execute before the constructor body in C#.
        _state->Rail = _adapter.ConstructStackPanel();
        _adapter.SetStackPanelSpacing(_state->Rail, 2.0);
        _state->RailWrap = _adapter.ConstructWrapPanel();
        _state->Pages = _adapter.ConstructPanel();
        _state->Grid = _adapter.ConstructGrid();

        _state->Settings = std::move(settings);
        _state->InGame = inGame;

        _adapter.SetBackground(GuiTheme::InkBrush);
        _adapter.SetFocusable(true);

        _state->RailScroll = _adapter.ConstructScrollViewer();
        _adapter.SetScrollViewerContent(_state->RailScroll, _state->Rail);
        _adapter.SetScrollViewerHorizontalScrollBarVisibility(
            _state->RailScroll, SettingsViewScrollBarVisibility::Disabled);

        const SettingsViewControlHandle footer = BuildFooter();

        _state->RailPanel = _adapter.ConstructBorder();
        _adapter.SetBorderBackground(_state->RailPanel, GuiTheme::PanelBrush);
        _adapter.SetBorderChild(_state->RailPanel, _state->RailScroll);

        _state->FooterPanel = _adapter.ConstructBorder();
        _adapter.SetBorderBackground(_state->FooterPanel, GuiTheme::PanelBrush);
        _adapter.SetBorderChild(_state->FooterPanel, footer);

        _adapter.AddPanelChild(_state->Grid, _state->RailPanel);
        _adapter.AddPanelChild(_state->Grid, _state->Pages);
        _adapter.AddPanelChild(_state->Grid, _state->FooterPanel);
        ApplyLayout(false);
        _adapter.SetContent(_state->Grid);
        _adapter.AddSizeChanged(SettingsViewSizeChangedHandler{
            _state.get(), &SettingsView::OnSizeChanged});

        _state->Heading = _adapter.ConstructCaption(std::u16string(u"Settings"));
        Require(_state->Heading.Value).Height(34.0);
        _adapter.AddPanelChild(_state->Rail, _state->Heading.Control);
        BuildPages();

        _state->LaidOut = false;
        ApplyLayout(_state->Narrow);
        ShowPage(_state->Sections.at(0).Page);
    }

    bool SettingsView::Saved() const noexcept
    {
        return _state->Saved;
    }

    std::string SettingsView::WindowTitle() const
    {
        std::string result(Mods::Branding::Name);
        result.append(" settings");
        return result;
    }

    bool SettingsView::InGame() const noexcept
    {
        return _state->InGame;
    }

    void SettingsView::AddClosed(const SettingsViewEventHandler& handler)
    {
        _state->Closed.Add(handler);
    }

    void SettingsView::RemoveClosed(const SettingsViewEventHandler& handler)
    {
        _state->Closed.Remove(handler);
    }

    void SettingsView::AddGameFilesRequested(const SettingsViewEventHandler& handler)
    {
        _state->GameFilesRequested.Add(handler);
    }

    void SettingsView::RemoveGameFilesRequested(const SettingsViewEventHandler& handler)
    {
        _state->GameFilesRequested.Remove(handler);
    }

    void SettingsView::AddStylusPlacementRequested(const SettingsViewEventHandler& handler)
    {
        _state->StylusPlacementRequested.Add(handler);
    }

    void SettingsView::RemoveStylusPlacementRequested(const SettingsViewEventHandler& handler)
    {
        _state->StylusPlacementRequested.Remove(handler);
    }

    void SettingsView::ApplyLayout(bool narrow)
    {
        if (_state->LaidOut && narrow == _state->Narrow)
        {
            return;
        }
        _state->Narrow = narrow;
        _state->LaidOut = true;
        if (narrow)
        {
            _adapter.SetGridColumnDefinitions(_state->Grid,
                _adapter.ConstructColumnDefinitions("*"));
            _adapter.SetGridRowDefinitions(_state->Grid,
                _adapter.ConstructRowDefinitions("Auto,*,Auto"));
            MoveSections(_state->RailWrap);
            _adapter.SetScrollViewerContent(_state->RailScroll, _state->RailWrap);
            _adapter.SetScrollViewerHorizontalScrollBarVisibility(
                _state->RailScroll, SettingsViewScrollBarVisibility::Disabled);
            _adapter.SetScrollViewerVerticalScrollBarVisibility(
                _state->RailScroll, SettingsViewScrollBarVisibility::Disabled);
            _adapter.SetControlWidth(_state->RailPanel,
                std::numeric_limits<double>::quiet_NaN());
            _adapter.SetBorderPadding(_state->RailPanel,
                SettingsViewThickness{12.0, 8.0, 12.0, 6.0});
            _adapter.SetControlWidth(_state->FooterPanel,
                std::numeric_limits<double>::quiet_NaN());
            _adapter.SetBorderPadding(_state->FooterPanel,
                SettingsViewThickness{12.0, 6.0, 12.0, 10.0});
            Place(_state->RailPanel, 0, 0, 1);
            Place(_state->Pages, 1, 0, 1);
            Place(_state->FooterPanel, 2, 0, 1);
            return;
        }

        _adapter.SetGridColumnDefinitions(_state->Grid,
            _adapter.ConstructColumnDefinitions("Auto,*"));
        _adapter.SetGridRowDefinitions(_state->Grid,
            _adapter.ConstructRowDefinitions("*,Auto"));
        MoveSections(_state->Rail);
        _adapter.SetScrollViewerContent(_state->RailScroll, _state->Rail);
        _adapter.SetScrollViewerHorizontalScrollBarVisibility(
            _state->RailScroll, SettingsViewScrollBarVisibility::Disabled);
        _adapter.SetScrollViewerVerticalScrollBarVisibility(
            _state->RailScroll, SettingsViewScrollBarVisibility::Auto);
        _adapter.SetControlWidth(_state->RailPanel, RailWidth);
        _adapter.SetBorderPadding(_state->RailPanel,
            SettingsViewThickness{18.0, 20.0, 14.0, 4.0});
        _adapter.SetControlWidth(_state->FooterPanel, RailWidth);
        _adapter.SetBorderPadding(_state->FooterPanel,
            SettingsViewThickness{18.0, 4.0, 14.0, 14.0});
        Place(_state->RailPanel, 0, 0, 1);
        Place(_state->FooterPanel, 1, 0, 1);
        Place(_state->Pages, 0, 1, 2);
    }

    void SettingsView::MoveSections(const SettingsViewControlHandle& target)
    {
        if (_state->Sections.empty()
            || _adapter.GetParent(_state->Sections[0].Button.Control) == target)
        {
            return;
        }
        _adapter.ClearPanelChildren(_state->Rail);
        _adapter.ClearPanelChildren(_state->RailWrap);
        if (target == _state->Rail && _state->Heading.Value)
        {
            _adapter.AddPanelChild(_state->Rail, _state->Heading.Control);
        }
        for (const SettingsViewSection& section : _state->Sections)
        {
            _adapter.AddPanelChild(target, section.Button.Control);
        }
    }

    void SettingsView::Place(const SettingsViewControlHandle& control,
        std::int32_t row, std::int32_t column, std::int32_t rowSpan)
    {
        _adapter.SetGridRow(control, row);
        _adapter.SetGridColumn(control, column);
        _adapter.SetGridRowSpan(control, rowSpan);
    }

    void SettingsView::OnAttachedToVisualTree(SettingsViewVisualTreeAttachmentEventArgs& e)
    {
        _adapter.BaseOnAttachedToVisualTree(e);
        _adapter.PostUiThread(SettingsViewAction{
            _state.get(), &SettingsView::OnFocusPosted, _state},
            SettingsViewDispatcherPriority::Background);
    }

    void SettingsView::OnKeyDown(SettingsViewKeyEventArgs& e)
    {
        if (e.Key == SettingsViewKey::Escape)
        {
            Close();
            e.Handled = true;
            return;
        }
        _adapter.BaseOnKeyDown(e);
    }

    void SettingsView::Close()
    {
        _state->Closed.Invoke(this, SettingsViewEventArgs::Empty);
    }

    SettingsViewControlHandle SettingsView::AddSection(std::u16string name)
    {
        const SettingsViewControlHandle page = _adapter.ConstructStackPanel();
        _adapter.SetStackPanelSpacing(page, 2.0);
        _adapter.SetControlMargin(page,
            SettingsViewThickness{26.0, 22.0, 26.0, 22.0});

        const SettingsViewControlHandle scroll = _adapter.ConstructScrollViewer();
        _adapter.SetScrollViewerContent(scroll, page);
        _adapter.SetControlIsVisible(scroll, false);
        _adapter.SetScrollViewerHorizontalScrollBarVisibility(
            scroll, SettingsViewScrollBarVisibility::Disabled);

        SettingsViewControlRef<MenuEntry> button =
            _adapter.ConstructMenuEntry(std::move(name), std::u16string{}, 15.0);
        Require(button.Value).Height(32.0);
        auto target = std::make_shared<SettingsViewSectionClickTarget>(
            SettingsViewSectionClickTarget{_state.get(), scroll});
        _adapter.AddMenuEntryClick(button.Control,
            SettingsViewAction{target.get(), &SettingsView::OnSectionClick, std::move(target)});

        _adapter.AddPanelChild(_state->Rail, button.Control);
        _adapter.AddPanelChild(_state->Pages, scroll);
        _state->Sections.push_back(SettingsViewSection{std::move(button), scroll});
        return page;
    }

    void SettingsView::ShowSection(std::optional<std::u16string_view> name)
    {
        for (const SettingsViewSection& section : _state->Sections)
        {
            const std::optional<std::u16string> title = Require(section.Button.Value).Title();
            const std::optional<std::u16string_view> titleView = title.has_value()
                ? std::optional<std::u16string_view>(*title) : std::nullopt;
            if (_adapter.OrdinalIgnoreCaseEquals(titleView, name))
            {
                ShowPage(section.Page);
                return;
            }
        }
    }

    void SettingsView::ShowPage(const SettingsViewControlHandle& page)
    {
        for (const SettingsViewSection& section : _state->Sections)
        {
            const bool selected = section.Page == page;
            _adapter.SetControlIsVisible(section.Page, selected);
            Require(section.Button.Value).Selected(selected);
        }
    }

    SettingsViewControlRef<Caption> SettingsView::Heading(
        const SettingsViewControlHandle& page, std::u16string text)
    {
        SettingsViewControlRef<Caption> caption =
            _adapter.ConstructCaption(std::move(text));
        Require(caption.Value).Height(30.0);
        Require(caption.Value).Margin(RowsThickness{0.0, 8.0, 0.0, 4.0});
        _adapter.AddPanelChild(page, caption.Control);
        return caption;
    }

    SettingsViewControlRef<Note> SettingsView::Explain(
        const SettingsViewControlHandle& page, std::u16string text,
        std::optional<GuiColor> color)
    {
        SettingsViewControlRef<Note> note =
            _adapter.ConstructNote(std::move(text), color);
        _adapter.AddPanelChild(page, note.Control);
        return note;
    }

    void SettingsView::BuildPages()
    {
        BuildDisplay();
        BuildAudio();
        BuildControls();
        BuildMatch();
        BuildLauncher();
        BuildCredits();
    }

    void SettingsView::BuildCredits()
    {
        const SettingsViewControlHandle page = AddSection(u"Credits");
        (void)Heading(page, u"Credits");
        (void)Explain(page, ToUtf16(Mods::Credits::Summary()));

        SettingsViewControlRef<Caption> author =
            _adapter.ConstructCaption(ToUtf16(Mods::Credits::Author));
        _adapter.AddPanelChild(page, author.Control);
        SettingsViewControlRef<Note> forkWork =
            _adapter.ConstructNote(ToUtf16(Mods::Credits::ForkWork));
        _adapter.AddPanelChild(page, forkWork.Control);

        SettingsViewControlRef<MenuEntry> support =
            _adapter.ConstructMenuEntry(std::u16string(u"\u2615 Support this project"),
                std::u16string{}, 15.0);
        auto target = std::make_shared<SettingsViewSupportClickTarget>(
            SettingsViewSupportClickTarget{support.Value});
        _adapter.AddMenuEntryClick(support.Control,
            SettingsViewAction{target.get(), &SettingsView::OnSupportClick, std::move(target)});
        _adapter.AddPanelChild(page, support.Control);

        (void)Heading(page, u"Built on");
        for (const Mods::Credits::Entry& entry : Mods::Credits::Entries())
        {
            SettingsViewControlRef<Caption> who =
                _adapter.ConstructCaption(OptionalToUtf16(entry.Who()));
            _adapter.AddPanelChild(page, who.Control);

            std::string what = entry.What().value_or(std::string{});
            const std::string where = entry.Where().value_or(std::string{});
            if (!where.empty())
            {
                what.append("\n");
                what.append(where);
            }
            SettingsViewControlRef<Note> note =
                _adapter.ConstructNote(ToUtf16(what));
            _adapter.AddPanelChild(page, note.Control);
        }
    }

    void SettingsView::BuildDisplay()
    {
        const SettingsViewControlHandle page = AddSection(u"Display");
        if (!_adapter.IsAndroid())
        {
            (void)Heading(page, u"Window");
            _state->WindowRow = _adapter.ConstructChoiceRow(
                std::u16string(u"Mode"),
                Strings({u"Windowed", u"Fullscreen (borderless)"}),
                LauncherPrefs::WindowMode() == WindowStartMode::BorderlessFullscreen ? 1 : 0);
            _adapter.AddPanelChild(page, _state->WindowRow.Control);
        }

        (void)Heading(page, u"Performance");
        _state->ResolutionScale = _adapter.ConstructSliderRow(
            std::u16string(u"Render scale"), RenderOptions::ResolutionScale(),
            [](std::int32_t value) -> std::optional<std::u16string>
            {
                const std::int32_t scale = std::max(RenderOptions::MinScale, value);
                return ToUtf16(std::to_string(scale) + "%");
            });
        _adapter.AddPanelChild(page, _state->ResolutionScale.Control);

        _state->FpsLimitRow = _adapter.ConstructSliderRow(
            std::u16string(u"FPS limit"), FpsLimitStopIndex(FrameTiming::FrameRateCap()),
            [](std::int32_t value) -> std::optional<std::u16string>
            {
                const std::int32_t index = std::clamp(value, 0,
                    static_cast<std::int32_t>(FpsStops.size()) - 1);
                return std::u16string(FpsStops[static_cast<std::size_t>(index)].Label);
            }, 120.0, 0, static_cast<std::int32_t>(FpsStops.size()) - 1, 1);
        _adapter.AddPanelChild(page, _state->FpsLimitRow.Control);

        _state->LightingRow = _adapter.ConstructToggleRow(
            std::u16string(u"Lighting"), RenderOptions::Lighting());
        _adapter.AddPanelChild(page, _state->LightingRow.Control);
        _state->FogRow = _adapter.ConstructToggleRow(
            std::u16string(u"Fog"), RenderOptions::Fog());
        _adapter.AddPanelChild(page, _state->FogRow.Control);
        _state->FilteringRow = _adapter.ConstructToggleRow(
            std::u16string(u"Texture filtering"), RenderOptions::TextureFiltering());
        _adapter.AddPanelChild(page, _state->FilteringRow.Control);
        _state->FpsRow = _adapter.ConstructToggleRow(
            std::u16string(u"FPS counter"), RenderOptions::ShowFps());
        _adapter.AddPanelChild(page, _state->FpsRow.Control);

        (void)Heading(page, u"Cel shading");
        _state->CelRow = _adapter.ConstructToggleRow(
            std::u16string(u"Cel shading"), RenderOptions::CelShading());
        _adapter.AddPanelChild(page, _state->CelRow.Control);

        (void)Heading(page, u"HUD");
        _state->ProHud = _adapter.ConstructToggleRow(
            std::u16string(u"Pro mode HUD"), Features::ProHud());
        _adapter.AddPanelChild(page, _state->ProHud.Control);

        _state->CrosshairSizeRow = _adapter.ConstructChoiceRow(
            std::u16string(u"Crosshair size"),
            std::make_shared<StringArrayList<3>>(Crosshair::SizeNames),
            static_cast<std::int32_t>(Crosshair::Size));
        _adapter.AddPanelChild(page, _state->CrosshairSizeRow.Control);

        _state->CrosshairStyleRow = _adapter.ConstructChoiceRow(
            std::u16string(u"Crosshair type"),
            std::make_shared<StringArrayList<5>>(Crosshair::StyleNames),
            static_cast<std::int32_t>(Crosshair::Style));
        _adapter.AddPanelChild(page, _state->CrosshairStyleRow.Control);

        const std::weak_ptr<SettingsViewState> weak = _state;
        Require(_state->CrosshairStyleRow.Value).Preview(
            std::make_shared<const ChoiceRow::PreviewHandler>(
                [weak](RowsDrawingContext& context, GuiRect area)
                {
                    const std::shared_ptr<SettingsViewState> state = weak.lock();
                    if (!state) return;
                    PreviewContext preview(context);
                    CrosshairPreview::Draw(preview,
                        CrosshairPreviewRect{area.X, area.Y, area.Width, area.Height},
                        static_cast<CrosshairStyle>(Require(state->CrosshairStyleRow.Value).Index()),
                        static_cast<CrosshairSize>(Require(state->CrosshairSizeRow.Value).Index()));
                }));
        Require(_state->CrosshairSizeRow.Value).AddChanged(
            RowsEventHandler(_state.get(), &SettingsView::OnCrosshairSizeChanged));

        _state->WeaponStyleRow = _adapter.ConstructChoiceRow(
            std::u16string(u"Weapon"),
            Strings({u"Static (Quake)", u"Dynamic (Metroid)"}),
            Features::ProHudFixedWeapon() ? 0 : 1);
        _adapter.AddPanelChild(page, _state->WeaponStyleRow.Control);

        Require(_state->ProHud.Value).AddChanged(
            RowsEventHandler(_state.get(), &SettingsView::OnProHudChanged));
        ShowCrosshairRows();
    }

    void SettingsView::ShowCrosshairRows()
    {
        const bool visible = Require(_state->ProHud.Value).On();
        Require(_state->CrosshairSizeRow.Value).IsVisible(visible);
        Require(_state->CrosshairStyleRow.Value).IsVisible(visible);
        Require(_state->WeaponStyleRow.Value).IsVisible(visible);
    }

    void SettingsView::BuildAudio()
    {
        const SettingsViewControlHandle page = AddSection(u"Audio");
        (void)Heading(page, u"Volume");
        MenuSettings& settings = RequireSettings(_state->Settings);

        auto percent = [](const std::string& stored, std::int32_t fallback)
        {
            float parsed = 0.0F;
            if (!TryParseSingleInvariant(stored, parsed)) return fallback;
            const std::int32_t rounded = RoundToInt32(static_cast<double>(parsed * 100.0F));
            return std::clamp(rounded, 0, 100);
        };

        _state->SfxVolume = _adapter.ConstructSliderRow(
            std::u16string(u"Sound effects"), percent(settings.SfxVolume, 35));
        _adapter.AddPanelChild(page, _state->SfxVolume.Control);
        _state->MusicVolume = _adapter.ConstructSliderRow(
            std::u16string(u"Music"), percent(settings.MusicVolume, 50));
        _adapter.AddPanelChild(page, _state->MusicVolume.Control);

        (void)Heading(page, u"Language");
        std::vector<std::u16string> languages = LanguageNames();
        const std::int32_t languageIndex = std::max<std::int32_t>(
            0, IndexOf(languages, ToUtf16(settings.Language)));
        _state->LanguageRow = _adapter.ConstructChoiceRow(
            std::u16string(u"Text"), Strings(languages), languageIndex);
        _adapter.AddPanelChild(page, _state->LanguageRow.Control);
    }

    void SettingsView::BuildControls()
    {
        const SettingsViewControlHandle page = AddSection(u"Controls");
        (void)Heading(page, u"Mouse");

        const auto sensitivityToSlider = [](float sensitivity)
        {
            return std::clamp(RoundToInt32(
                static_cast<double>((sensitivity - 0.1F) / 2.9F * 100.0F)), 0, 100);
        };
        const auto sliderToSensitivity = [](std::int32_t value)
        {
            return 0.1F + static_cast<float>(value) / 100.0F * 2.9F;
        };
        const auto lookToSlider = [](float look)
        {
            return std::clamp(RoundToInt32(
                static_cast<double>((look - 0.25F) / 2.75F * 100.0F)), 0, 100);
        };
        const auto sliderToLook = [](std::int32_t value)
        {
            return 0.25F + static_cast<float>(value) / 100.0F * 2.75F;
        };
        const auto deadZoneToSlider = [](float dead)
        {
            return std::clamp(RoundToInt32(
                static_cast<double>(dead / 0.5F * 100.0F)), 0, 100);
        };
        const auto sliderToDeadZone = [](std::int32_t value)
        {
            return static_cast<float>(value) / 100.0F * 0.5F;
        };

        _state->Sensitivity = _adapter.ConstructSliderRow(
            std::u16string(u"Sensitivity"),
            sensitivityToSlider(InputSettings::MouseSensitivity()),
            [sliderToSensitivity](std::int32_t value) -> std::optional<std::u16string>
            {
                std::u16string result = Fixed2(sliderToSensitivity(value));
                result.push_back(u'x');
                return result;
            });
        _adapter.AddPanelChild(page, _state->Sensitivity.Control);

        _state->InvertY = _adapter.ConstructToggleRow(
            std::u16string(u"Invert vertical aim"), InputSettings::InvertMouseY());
        _adapter.AddPanelChild(page, _state->InvertY.Control);
        _state->InvertX = _adapter.ConstructToggleRow(
            std::u16string(u"Invert horizontal aim"), InputSettings::InvertMouseX());
        _adapter.AddPanelChild(page, _state->InvertX.Control);
        _state->ScrollAllWeapons = _adapter.ConstructToggleRow(
            std::u16string(u"Wheel cycles every weapon"), InputSettings::ScrollAllWeapons());
        _adapter.AddPanelChild(page, _state->ScrollAllWeapons.Control);
        _state->PenTablet = _adapter.ConstructToggleRow(
            std::u16string(u"Pen tablet: ignore pointer jumps"), PointerInput::GuardJumps());
        _adapter.AddPanelChild(page, _state->PenTablet.Control);

        BuildStylusZone(page);
        BuildTouchControls(page);

        (void)Heading(page, u"Gamepad");
        _state->GamepadLook = _adapter.ConstructSliderRow(
            std::u16string(u"Look sensitivity"),
            lookToSlider(InputSettings::GamepadLookSensitivity()),
            [sliderToLook](std::int32_t value) -> std::optional<std::u16string>
            {
                std::u16string result = Fixed2(sliderToLook(value));
                result.push_back(u'x');
                return result;
            });
        _adapter.AddPanelChild(page, _state->GamepadLook.Control);

        _state->GamepadDeadZone = _adapter.ConstructSliderRow(
            std::u16string(u"Stick dead zone"),
            deadZoneToSlider(InputSettings::GamepadDeadZone()),
            [sliderToDeadZone](std::int32_t value) -> std::optional<std::u16string>
            {
                return Fixed2(sliderToDeadZone(value));
            });
        _adapter.AddPanelChild(page, _state->GamepadDeadZone.Control);

        _state->GamepadInvertY = _adapter.ConstructToggleRow(
            std::u16string(u"Invert vertical aim (stick)"), InputSettings::GamepadInvertY());
        _adapter.AddPanelChild(page, _state->GamepadInvertY.Control);

        (void)Heading(page, u"Gamepad buttons");
        for (const PadAction action : PadBindings::Actions())
        {
            SettingsViewControlRef<PadRow> row = _adapter.ConstructPadRow(action);
            _adapter.AddPanelChild(page, row.Control);
            _state->PadRows.push_back(std::move(row));
        }

        (void)Heading(page, u"Keys");
        SettingsViewControlRef<KeyRow> chat = _adapter.ConstructKeyRow(
            std::u16string(u"Chat"),
            KeyRowGetHandler({}, &ChatKeyGet), KeyRowSetHandler({}, &ChatKeySet));
        _adapter.AddPanelChild(page, chat.Control);
        _state->KeyRows.push_back(std::move(chat));

        SettingsViewControlRef<KeyRow> clip = _adapter.ConstructKeyRow(
            std::u16string(u"Save clip"),
            KeyRowGetHandler({}, &ClipKeyGet), KeyRowSetHandler({}, &ClipKeySet));
        _adapter.AddPanelChild(page, clip.Control);
        _state->KeyRows.push_back(std::move(clip));

        std::vector<std::u16string> clipLengths;
        clipLengths.reserve(3);
        for (const std::int32_t length : DemoClip::Lengths)
        {
            clipLengths.push_back(ToUtf16(std::to_string(length) + " seconds"));
        }
        const std::int32_t clipSeconds = DemoClip::Seconds();
        std::int32_t clipIndex = -1;
        for (std::size_t i = 0; i < 3; ++i)
        {
            if (DemoClip::Lengths[i] == clipSeconds)
            {
                clipIndex = static_cast<std::int32_t>(i);
                break;
            }
        }
        _state->ClipSecondsRow = _adapter.ConstructChoiceRow(
            std::u16string(u"Clip length"), Strings(std::move(clipLengths)),
            std::max<std::int32_t>(0, clipIndex));
        _adapter.AddPanelChild(page, _state->ClipSecondsRow.Control);

        for (const Mods::InputBindingProperty& property : InputSettings::Bindings())
        {
            SettingsViewControlRef<KeyRow> row = _adapter.ConstructKeyRow(&property);
            _adapter.AddPanelChild(page, row.Control);
            _state->KeyRows.push_back(std::move(row));
        }

        SettingsViewControlRef<MenuEntry> reset = _adapter.ConstructMenuEntry(
            std::u16string(u"Reset to defaults"), std::u16string{}, 13.0);
        Require(reset.Value).Height(30.0);
        Require(reset.Value).Accent(GuiTheme::Warm);
        _adapter.SetControlMargin(reset.Control,
            SettingsViewThickness{0.0, 8.0, 0.0, 0.0});
        _adapter.AddMenuEntryClick(reset.Control,
            SettingsViewAction{_state.get(), &SettingsView::OnResetClick});
        _adapter.AddPanelChild(page, reset.Control);
    }

    void SettingsView::BuildStylusZone(const SettingsViewControlHandle& page)
    {
        if (_adapter.IsAndroid())
        {
            return;
        }

        _state->StylusZone = _adapter.ConstructToggleRow(
            std::u16string(u"DS bottom screen for a pen tablet"), StylusZone::Enabled());
        _adapter.AddPanelChild(page, _state->StylusZone.Control);

        _state->StylusOpacity = _adapter.ConstructSliderRow(
            std::u16string(u"Bottom screen opacity"),
            RoundToInt32(static_cast<double>(StylusZone::Opacity() * 100.0F)),
            [](std::int32_t value) -> std::optional<std::u16string>
            {
                return ToUtf16(std::to_string(value) + "%");
            }, 120.0, 4, 60, 2);
        _adapter.AddPanelChild(page, _state->StylusOpacity.Control);

        SettingsViewControlRef<MenuEntry> place = _adapter.ConstructMenuEntry(
            std::u16string(u"Place the bottom screen"), std::u16string{}, 13.0);
        Require(place.Value).Height(30.0);
        _adapter.SetControlMargin(place.Control,
            SettingsViewThickness{0.0, 6.0, 0.0, 0.0});
        _adapter.AddMenuEntryClick(place.Control,
            SettingsViewAction{_state.get(), &SettingsView::OnPlaceStylusClick});
        _adapter.AddPanelChild(page, place.Control);

        SettingsViewControlRef<Note> note = _adapter.ConstructNote(std::u16string(
            u"Drag a rectangle where the DS's touch screen should be, then map "
            u"your tablet to it. Escape leaves it as it was."));
        _adapter.AddPanelChild(page, note.Control);
    }

    void SettingsView::BuildTouchControls(const SettingsViewControlHandle& page)
    {
        if (!_adapter.IsAndroid())
        {
            return;
        }

        (void)Heading(page, u"On-screen buttons");
        _state->TouchButtonsRow = _adapter.ConstructToggleRow(
            std::u16string(u"Show on-screen buttons"), TouchSettings::ButtonsVisible);
        _adapter.AddPanelChild(page, _state->TouchButtonsRow.Control);

        SettingsViewControlRef<Note> note = _adapter.ConstructNote(std::u16string(
            u"The stick, aiming, the double tap that jumps and the flick "
            u"that boosts are not buttons, so they keep working with every one of these off."));
        _adapter.AddPanelChild(page, note.Control);

        for (const TouchControlOrderEntry& item : TouchSettings::Order)
        {
            SettingsViewControlRef<ToggleRow> row = _adapter.ConstructToggleRow(
                ToUtf16(item.Label), TouchSettings::IsEnabled(item.Control));
            _adapter.AddPanelChild(page, row.Control);
            _state->TouchRows.push_back(SettingsViewTouchRow{item.Control, std::move(row)});
        }

        Require(_state->TouchButtonsRow.Value).AddChanged(
            RowsEventHandler(_state.get(), &SettingsView::OnTouchButtonsChanged));
        OnTouchButtonsChanged(_state.get(), nullptr, RowsEventArgs::Empty);
    }

    void SettingsView::BuildMatch()
    {
        const SettingsViewControlHandle page = AddSection(u"Match rules");
        (void)Heading(page, u"Match rules");
        MenuSettings& settings = RequireSettings(_state->Settings);

        _state->PointGoal = _adapter.ConstructFieldRow(
            std::u16string(u"Point goal"), ToUtf16(settings.PointGoal), 120.0);
        _adapter.AddPanelChild(page, _state->PointGoal.Control);
        _state->TimeLimit = _adapter.ConstructFieldRow(
            std::u16string(u"Time limit"), ToUtf16(settings.TimeLimit), 120.0);
        _adapter.AddPanelChild(page, _state->TimeLimit.Control);
        Require(_state->TimeLimit.Value).Box()->Watermark(u"m:ss");

        std::vector<std::u16string> damage{u"low", u"medium", u"high"};
        _state->DamageRow = _adapter.ConstructChoiceRow(
            std::u16string(u"Damage"), Strings(damage),
            std::max<std::int32_t>(0, IndexOf(damage, ToUtf16(settings.DamageLevel))));
        _adapter.AddPanelChild(page, _state->DamageRow.Control);

        _state->TeamPlay = _adapter.ConstructToggleRow(
            std::u16string(u"Team play"), settings.TeamPlay == "on");
        _adapter.AddPanelChild(page, _state->TeamPlay.Control);
        _state->FriendlyFire = _adapter.ConstructToggleRow(
            std::u16string(u"Friendly fire"), settings.FriendlyFire == "on");
        _adapter.AddPanelChild(page, _state->FriendlyFire.Control);
        _state->Radar = _adapter.ConstructToggleRow(
            std::u16string(u"Hunter radar"), settings.HunterRadar == "on");
        _adapter.AddPanelChild(page, _state->Radar.Control);
        _state->Affinity = _adapter.ConstructToggleRow(
            std::u16string(u"Affinity weapons"), settings.AffinityWeapons == "on");
        _adapter.AddPanelChild(page, _state->Affinity.Control);
        _state->ShadowFreeze = _adapter.ConstructToggleRow(
            std::u16string(u"Shadow freeze"), settings.ShadowFreeze != "off");
        _adapter.AddPanelChild(page, _state->ShadowFreeze.Control);
    }

    void SettingsView::BuildLauncher()
    {
        const SettingsViewControlHandle page = AddSection(u"Profile");
        (void)Heading(page, u"You");

        _state->PlayerName = _adapter.ConstructFieldRow(
            std::u16string(u"Your name"), ToUtf16(LauncherPrefs::PlayerName()), 200.0);
        _adapter.AddPanelChild(page, _state->PlayerName.Control);

        std::vector<std::u16string> hunters;
        hunters.reserve(8);
        for (std::int32_t i = 0; i < 7; ++i)
        {
            hunters.push_back(ToUtf16(HunterName(static_cast<Hunter>(i))));
        }
        hunters.push_back(ToUtf16(HunterName(Hunter::Random)));
        const std::u16string lastHunter = ToUtf16(HunterName(LauncherPrefs::LastHunter()));
        _state->HunterRow = _adapter.ConstructChoiceRow(
            std::u16string(u"Hunter"), Strings(hunters),
            std::max<std::int32_t>(0, IndexOf(hunters, lastHunter)));
        _adapter.AddPanelChild(page, _state->HunterRow.Control);

        std::vector<std::u16string> colors;
        colors.reserve(PlayerColors::Count);
        for (std::int32_t i = 1; i <= PlayerColors::Count; ++i)
        {
            colors.push_back(ToUtf16(std::to_string(i)));
        }
        _state->ColorRow = _adapter.ConstructChoiceRow(
            std::u16string(u"Suit colour"), Strings(std::move(colors)),
            PlayerColors::Clamp(LauncherPrefs::LastColor()));
        _adapter.AddPanelChild(page, _state->ColorRow.Control);

        (void)Heading(page, u"Servers");
        _state->ServerRow = _adapter.ConstructFieldRow(
            std::u16string(u"Default server"),
            ToUtf16(LauncherPrefs::ServerAddress() + ":"
                + std::to_string(LauncherPrefs::ServerPort())), 220.0);
        _adapter.AddPanelChild(page, _state->ServerRow.Control);
        _state->MasterRow = _adapter.ConstructFieldRow(
            std::u16string(u"Server directory"),
            ToUtf16(LauncherPrefs::MasterHost() + ":"
                + std::to_string(LauncherPrefs::MasterPort())), 220.0);
        _adapter.AddPanelChild(page, _state->MasterRow.Control);
        _state->AutoUpdate = _adapter.ConstructToggleRow(
            std::u16string(u"Check for updates on startup"), LauncherPrefs::AutoUpdate());
        _adapter.AddPanelChild(page, _state->AutoUpdate.Control);

        (void)Heading(page, u"Game files");
        SettingsViewControlRef<MenuEntry> files = _adapter.ConstructMenuEntry(
            std::u16string(u"Game files"), ToUtf16(GameFiles::Describe()), 15.0);
        Require(files.Value).SubtitleColor(GameFiles::Ready() ? GuiTheme::Good : GuiTheme::Warm);
        _adapter.AddMenuEntryClick(files.Control,
            SettingsViewAction{_state.get(), &SettingsView::OnGameFilesClick});
        _adapter.AddPanelChild(page, files.Control);
    }

    SettingsViewControlHandle SettingsView::BuildFooter()
    {
        SettingsViewControlRef<MenuEntry> save = _adapter.ConstructMenuEntry(
            std::u16string(_state->InGame ? u"Apply" : u"Save and close"),
            std::u16string{}, 15.0);
        Require(save.Value).Primary(true);
        Require(save.Value).Height(40.0);
        _adapter.AddMenuEntryClick(save.Control,
            SettingsViewAction{_state.get(), &SettingsView::OnSaveClick});

        SettingsViewControlRef<MenuEntry> cancel = _adapter.ConstructMenuEntry(
            std::u16string(u"Cancel"), std::u16string{}, 13.0);
        Require(cancel.Value).Height(26.0);
        Require(cancel.Value).Accent(GuiTheme::TextDim);
        _adapter.AddMenuEntryClick(cancel.Control,
            SettingsViewAction{_state.get(), &SettingsView::OnCancelClick});

        _state->SaveError = _adapter.ConstructNote(std::u16string{}, GuiTheme::Warm);
        Require(_state->SaveError.Value).IsVisible(false);

        const SettingsViewControlHandle footer = _adapter.ConstructStackPanel();
        _adapter.SetStackPanelSpacing(footer, 6.0);
        _adapter.SetControlMargin(footer,
            SettingsViewThickness{0.0, 12.0, 0.0, 0.0});
        _adapter.SetControlVerticalAlignment(
            footer, SettingsViewVerticalAlignment::Bottom);
        _adapter.AddPanelChild(footer, save.Control);
        _adapter.AddPanelChild(footer, cancel.Control);
        _adapter.AddPanelChild(footer, _state->SaveError.Control);
        return footer;
    }

    void SettingsView::TryCommit()
    {
        try
        {
            Commit();
        }
        catch (const std::exception& ex)
        {
            std::u16string text = u"Could not save: ";
            text.append(ToUtf16(ex.what()));
            Require(_state->SaveError.Value).Text(text);
            Require(_state->SaveError.Value).IsVisible(true);
        }
    }

    void SettingsView::Commit()
    {
        MenuSettings& settings = RequireSettings(_state->Settings);

        if (_state->WindowRow.Value)
        {
            LauncherPrefs::WindowMode(Require(_state->WindowRow.Value).Index() == 1
                ? WindowStartMode::BorderlessFullscreen
                : WindowStartMode::Windowed);
            Mods::WindowMode::Startup(LauncherPrefs::WindowMode());
        }

        if (_state->ClipSecondsRow.Value)
        {
            const std::int32_t index = std::clamp(
                Require(_state->ClipSecondsRow.Value).Index(), 0,
                static_cast<std::int32_t>(std::size(DemoClip::Lengths)) - 1);
            DemoClip::Seconds(DemoClip::Lengths[static_cast<std::size_t>(index)]);
        }

        settings.ResolutionScale = std::to_string(std::max(RenderOptions::MinScale,
            Require(_state->ResolutionScale.Value).Value()));
        settings.Lighting = std::string(RenderOptions::OnOff(Require(_state->LightingRow.Value).On()));
        settings.Fog = std::string(RenderOptions::OnOff(Require(_state->FogRow.Value).On()));
        settings.TextureFiltering = std::string(RenderOptions::OnOff(Require(_state->FilteringRow.Value).On()));
        settings.ShowFps = std::string(RenderOptions::OnOff(Require(_state->FpsRow.Value).On()));
        const std::int32_t capIndex = std::clamp(
            Require(_state->FpsLimitRow.Value).Value(), 0,
            static_cast<std::int32_t>(FpsStops.size()) - 1);
        const std::int32_t cap = FpsStops[static_cast<std::size_t>(capIndex)].Cap;
        FrameTiming::SetFrameRateCap(cap);
        settings.FrameRateCap = FrameTiming::CapString(cap);
        settings.CelShading = std::string(RenderOptions::OnOff(Require(_state->CelRow.Value).On()));
        settings.CelBands = "8";
        settings.CelEdge = "50";
        Features::ProHud(Require(_state->ProHud.Value).On());
        Crosshair::Size = static_cast<CrosshairSize>(Require(_state->CrosshairSizeRow.Value).Index());
        Crosshair::Style = static_cast<CrosshairStyle>(Require(_state->CrosshairStyleRow.Value).Index());
        Features::ProHudFixedWeapon(Require(_state->WeaponStyleRow.Value).Index() == 0);

        settings.SfxVolume = FloatInvariant(
            static_cast<float>(Require(_state->SfxVolume.Value).Value()) / 100.0F);
        settings.MusicVolume = FloatInvariant(
            static_cast<float>(Require(_state->MusicVolume.Value).Value()) / 100.0F);
        settings.Language = ToUtf8(Require(_state->LanguageRow.Value).Value().value_or(std::u16string{}));

        const auto sliderToSensitivity = [](std::int32_t value)
        {
            return 0.1F + static_cast<float>(value) / 100.0F * 2.9F;
        };
        const auto sliderToLook = [](std::int32_t value)
        {
            return 0.25F + static_cast<float>(value) / 100.0F * 2.75F;
        };
        const auto sliderToDeadZone = [](std::int32_t value)
        {
            return static_cast<float>(value) / 100.0F * 0.5F;
        };

        InputSettings::MouseSensitivity(sliderToSensitivity(
            Require(_state->Sensitivity.Value).Value()));
        InputSettings::InvertMouseY(Require(_state->InvertY.Value).On());
        InputSettings::InvertMouseX(Require(_state->InvertX.Value).On());
        PointerInput::GuardJumps(Require(_state->PenTablet.Value).On());
        if (_state->StylusZone.Value && _state->StylusOpacity.Value)
        {
            StylusZone::Enabled(Require(_state->StylusZone.Value).On());
            StylusZone::Opacity(std::clamp(
                static_cast<float>(Require(_state->StylusOpacity.Value).Value()) / 100.0F,
                0.02F, 1.0F));
        }
        InputSettings::ScrollAllWeapons(Require(_state->ScrollAllWeapons.Value).On());
        InputSettings::GamepadLookSensitivity(sliderToLook(
            Require(_state->GamepadLook.Value).Value()));
        InputSettings::GamepadDeadZone(sliderToDeadZone(
            Require(_state->GamepadDeadZone.Value).Value()));
        InputSettings::GamepadInvertY(Require(_state->GamepadInvertY.Value).On());
        if (_state->TouchButtonsRow.Value)
        {
            TouchSettings::ButtonsVisible = Require(_state->TouchButtonsRow.Value).On();
            for (const SettingsViewTouchRow& item : _state->TouchRows)
            {
                TouchSettings::SetEnabled(item.Control, Require(item.Row.Value).On());
            }
        }
        InputSettings::Save();
        InputSettings::ApplyToPlayers();

        settings.PointGoal = ToUtf8(Require(_state->PointGoal.Value).Value());
        settings.TimeLimit = ToUtf8(Require(_state->TimeLimit.Value).Value());
        settings.DamageLevel = ToUtf8(Require(_state->DamageRow.Value).Value().value_or(std::u16string{}));
        settings.TeamPlay = Require(_state->TeamPlay.Value).On() ? "on" : "off";
        settings.FriendlyFire = Require(_state->FriendlyFire.Value).On() ? "on" : "off";
        settings.HunterRadar = Require(_state->Radar.Value).On() ? "on" : "off";
        settings.AffinityWeapons = Require(_state->Affinity.Value).On() ? "on" : "off";
        settings.ShadowFreeze = Require(_state->ShadowFreeze.Value).On() ? "on" : "off";

        const std::u16string playerValue = Require(_state->PlayerName.Value).Value();
        const std::u16string playerTrimmedForLength = Trim(playerValue);
        if (!playerTrimmedForLength.empty())
        {
            const std::u16string secondValue = Require(_state->PlayerName.Value).Value();
            LauncherPrefs::PlayerName(ToUtf8(Trim(secondValue)));
        }

        LauncherPrefs::LastHunter(ParseHunter(
            Require(_state->HunterRow.Value).Value().value_or(std::u16string{})));
        std::int32_t suit = 0;
        if (TryParseInt32Invariant(
            Require(_state->ColorRow.Value).Value().value_or(std::u16string{}), suit))
        {
            LauncherPrefs::LastColor(PlayerColors::Clamp(suit - 1));
        }
        RespawnChoice::Request(LauncherPrefs::LastHunter(), LauncherPrefs::LastColor());

        auto parseEndpoint = [](std::u16string text, std::string& host, std::int32_t& port)
        {
            text = Trim(text);
            if (text.empty()) return false;
            const std::size_t colon = text.find_last_of(u':');
            if (colon == std::u16string::npos || colon == 0)
            {
                host = ToUtf8(text);
                return true;
            }
            std::int32_t parsed = 0;
            if (!TryParseInt32Invariant(
                std::u16string_view(text).substr(colon + 1), parsed)
                || parsed < 1 || parsed > 65535)
            {
                return false;
            }
            host = ToUtf8(std::u16string_view(text).substr(0, colon));
            port = parsed;
            return true;
        };

        std::string host = LauncherPrefs::ServerAddress();
        std::int32_t port = LauncherPrefs::ServerPort();
        if (parseEndpoint(Require(_state->ServerRow.Value).Value(), host, port))
        {
            LauncherPrefs::ServerAddress(host);
            LauncherPrefs::ServerPort(port);
        }

        std::string masterHost = LauncherPrefs::MasterHost();
        std::int32_t masterPort = LauncherPrefs::MasterPort();
        if (parseEndpoint(Require(_state->MasterRow.Value).Value(), masterHost, masterPort))
        {
            LauncherPrefs::MasterHost(masterHost);
            LauncherPrefs::MasterPort(masterPort);
        }

        LauncherPrefs::AutoUpdate(Require(_state->AutoUpdate.Value).On());
        GameState::CommitSettings(_state->Settings);
        LauncherPrefs::Save();
        Mods::GameSettings::Apply(_state->Settings);
        _state->Saved = true;
        Close();
    }

    void SettingsView::OnSizeChanged(void* target, double newWidth)
    {
        auto& state = *static_cast<SettingsViewState*>(target);
        state.Sender->ApplyLayout(newWidth < NarrowWidth);
    }

    void SettingsView::OnSectionClick(void* target)
    {
        auto& click = *static_cast<SettingsViewSectionClickTarget*>(target);
        click.View->Sender->ShowPage(click.Page);
    }

    void SettingsView::OnSupportClick(void* target)
    {
        auto& click = *static_cast<SettingsViewSupportClickTarget*>(target);
        if (!Mods::Update::Updater::OpenLink(std::string(Mods::Credits::SupportUrl)))
        {
            Require(click.Entry).Subtitle(ToUtf16(Mods::Credits::SupportUrl));
        }
    }

    void SettingsView::OnResetClick(void* target)
    {
        auto& state = *static_cast<SettingsViewState*>(target);
        InputSettings::Reset();

        const auto sensitivityToSlider = [](float sensitivity)
        {
            return std::clamp(RoundToInt32(
                static_cast<double>((sensitivity - 0.1F) / 2.9F * 100.0F)), 0, 100);
        };
        const auto lookToSlider = [](float look)
        {
            return std::clamp(RoundToInt32(
                static_cast<double>((look - 0.25F) / 2.75F * 100.0F)), 0, 100);
        };
        const auto deadZoneToSlider = [](float dead)
        {
            return std::clamp(RoundToInt32(
                static_cast<double>(dead / 0.5F * 100.0F)), 0, 100);
        };

        Require(state.Sensitivity.Value).Value(
            sensitivityToSlider(InputSettings::MouseSensitivity()));
        Require(state.InvertY.Value).On(InputSettings::InvertMouseY());
        Require(state.InvertX.Value).On(InputSettings::InvertMouseX());
        Require(state.PenTablet.Value).On(PointerInput::GuardJumps());
        if (state.StylusZone.Value && state.StylusOpacity.Value)
        {
            Require(state.StylusZone.Value).On(StylusZone::Enabled());
            Require(state.StylusOpacity.Value).Value(
                RoundToInt32(static_cast<double>(StylusZone::Opacity() * 100.0F)));
        }
        Require(state.ScrollAllWeapons.Value).On(InputSettings::ScrollAllWeapons());
        Require(state.GamepadLook.Value).Value(
            lookToSlider(InputSettings::GamepadLookSensitivity()));
        Require(state.GamepadDeadZone.Value).Value(
            deadZoneToSlider(InputSettings::GamepadDeadZone()));
        Require(state.GamepadInvertY.Value).On(InputSettings::GamepadInvertY());
        for (const SettingsViewControlRef<PadRow>& row : state.PadRows)
        {
            Require(row.Value).InvalidateVisual();
        }
        for (const SettingsViewControlRef<KeyRow>& row : state.KeyRows)
        {
            Require(row.Value).InvalidateVisual();
        }
        if (state.TouchButtonsRow.Value)
        {
            Require(state.TouchButtonsRow.Value).On(TouchSettings::ButtonsVisible);
        }
        for (const SettingsViewTouchRow& item : state.TouchRows)
        {
            Require(item.Row.Value).On(TouchSettings::IsEnabled(item.Control));
        }
    }

    void SettingsView::OnPlaceStylusClick(void* target)
    {
        auto& state = *static_cast<SettingsViewState*>(target);
        StylusZone::BeginPlacement();
        state.StylusPlacementRequested.Invoke(state.Sender, SettingsViewEventArgs::Empty);
    }

    void SettingsView::OnGameFilesClick(void* target)
    {
        auto& state = *static_cast<SettingsViewState*>(target);
        state.GameFilesRequested.Invoke(state.Sender, SettingsViewEventArgs::Empty);
        state.Sender->Close();
    }

    void SettingsView::OnSaveClick(void* target)
    {
        auto& state = *static_cast<SettingsViewState*>(target);
        state.Sender->TryCommit();
    }

    void SettingsView::OnCancelClick(void* target)
    {
        auto& state = *static_cast<SettingsViewState*>(target);
        state.Sender->Close();
    }

    void SettingsView::OnFocusPosted(void* target)
    {
        auto& state = *static_cast<SettingsViewState*>(target);
        if (state.Sections.empty())
        {
            throw std::out_of_range("Index was outside the bounds of the array.");
        }
        state.Adapter->Focus(state.Sections[0].Button.Control);
    }

    void SettingsView::OnProHudChanged(
        void* target, void*, const RowsEventArgs&)
    {
        auto& state = *static_cast<SettingsViewState*>(target);
        state.Sender->ShowCrosshairRows();
    }

    void SettingsView::OnCrosshairSizeChanged(
        void* target, void*, const RowsEventArgs&)
    {
        auto& state = *static_cast<SettingsViewState*>(target);
        Require(state.CrosshairStyleRow.Value).InvalidateVisual();
    }

    void SettingsView::OnTouchButtonsChanged(
        void* target, void*, const RowsEventArgs&)
    {
        auto& state = *static_cast<SettingsViewState*>(target);
        const bool visible = Require(state.TouchButtonsRow.Value).On();
        for (const SettingsViewTouchRow& item : state.TouchRows)
        {
            Require(item.Row.Value).IsVisible(visible);
        }
    }
}
