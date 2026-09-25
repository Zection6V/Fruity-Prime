#include "SettingsView.hpp"
#include "NativeRuntime/System/Charconv.hpp"

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
#include "../../../NativeRuntime/System/Encoding.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"
#include "NativeRuntime/System/Globalization.hpp"

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

using ::MphRead::NativeRuntime::CharIsWhiteSpace;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase;
using ::MphRead::NativeRuntime::Utf8ToUtf16;

namespace
{
    using namespace MphRead;
    using namespace MphRead::Mods;
    using namespace MphRead::Mods::Input;
    using namespace MphRead::Mods::Launcher;
    using namespace MphRead::Mods::Launcher::Gui;
    using namespace MphRead::Mods::Network;
    using namespace MphRead::Mods::Render;

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

    [[nodiscard]] std::u16string Trim(std::u16string_view text)
    {
        std::size_t first = 0;
        while (first < text.size() && CharIsWhiteSpace(text[first]))
        {
            ++first;
        }
        std::size_t last = text.size();
        while (last > first && CharIsWhiteSpace(text[last - 1]))
        {
            --last;
        }
        return std::u16string(text.substr(first, last - first));
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
            return Utf8ToUtf16(_values[static_cast<std::size_t>(index)]);
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
        return value.has_value() ? Utf8ToUtf16(*value) : std::u16string{};
    }

    [[nodiscard]] MenuSettings& RequireSettings(const std::shared_ptr<MenuSettings>& settings)
    {
        if (!settings)
        {
            throw SettingsViewNullReferenceException();
        }
        return *settings;
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
        if (::MphRead::NativeRuntime::Int32TryParseInvariant(::MphRead::NativeRuntime::Utf16ToUtf8(value), numeric))
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
        RequireReference(_state->Heading.Value).Height(34.0);
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
        RequireReference(button.Value).Height(32.0);
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
            const std::optional<std::u16string> title = RequireReference(section.Button.Value).Title();
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
            RequireReference(section.Button.Value).Selected(selected);
        }
    }

    SettingsViewControlRef<Caption> SettingsView::Heading(
        const SettingsViewControlHandle& page, std::u16string text)
    {
        SettingsViewControlRef<Caption> caption =
            _adapter.ConstructCaption(std::move(text));
        RequireReference(caption.Value).Height(30.0);
        RequireReference(caption.Value).Margin(RowsThickness{0.0, 8.0, 0.0, 4.0});
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
        (void)Explain(page, Utf8ToUtf16(Mods::Credits::Summary()));

        SettingsViewControlRef<Caption> author =
            _adapter.ConstructCaption(Utf8ToUtf16(Mods::Credits::Author));
        _adapter.AddPanelChild(page, author.Control);
        SettingsViewControlRef<Note> forkWork =
            _adapter.ConstructNote(Utf8ToUtf16(Mods::Credits::ForkWork));
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
                _adapter.ConstructNote(Utf8ToUtf16(what));
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
                return Utf8ToUtf16(std::to_string(scale) + "%");
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
        RequireReference(_state->CrosshairStyleRow.Value).Preview(
            std::make_shared<const ChoiceRow::PreviewHandler>(
                [weak](RowsDrawingContext& context, GuiRect area)
                {
                    const std::shared_ptr<SettingsViewState> state = weak.lock();
                    if (!state) return;
                    PreviewContext preview(context);
                    CrosshairPreview::Draw(preview,
                        CrosshairPreviewRect{area.X, area.Y, area.Width, area.Height},
                        static_cast<CrosshairStyle>(RequireReference(state->CrosshairStyleRow.Value).Index()),
                        static_cast<CrosshairSize>(RequireReference(state->CrosshairSizeRow.Value).Index()));
                }));
        RequireReference(_state->CrosshairSizeRow.Value).AddChanged(
            RowsEventHandler(_state.get(), &SettingsView::OnCrosshairSizeChanged));

        _state->WeaponStyleRow = _adapter.ConstructChoiceRow(
            std::u16string(u"Weapon"),
            Strings({u"Static (Quake)", u"Dynamic (Metroid)"}),
            Features::ProHudFixedWeapon() ? 0 : 1);
        _adapter.AddPanelChild(page, _state->WeaponStyleRow.Control);

        RequireReference(_state->ProHud.Value).AddChanged(
            RowsEventHandler(_state.get(), &SettingsView::OnProHudChanged));
        ShowCrosshairRows();
    }

    void SettingsView::ShowCrosshairRows()
    {
        const bool visible = RequireReference(_state->ProHud.Value).On();
        RequireReference(_state->CrosshairSizeRow.Value).IsVisible(visible);
        RequireReference(_state->CrosshairStyleRow.Value).IsVisible(visible);
        RequireReference(_state->WeaponStyleRow.Value).IsVisible(visible);
    }

    void SettingsView::BuildAudio()
    {
        const SettingsViewControlHandle page = AddSection(u"Audio");
        (void)Heading(page, u"Volume");
        MenuSettings& settings = RequireSettings(_state->Settings);

        auto percent = [](const std::string& stored, std::int32_t fallback)
        {
            float parsed = 0.0F;
            if (!::MphRead::NativeRuntime::SingleTryParseInvariant(stored, parsed)) return fallback;
            const std::int32_t rounded = ::MphRead::NativeRuntime::MathRoundToInt32(static_cast<double>(parsed * 100.0F));
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
            0, IndexOf(languages, Utf8ToUtf16(settings.Language)));
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
            return std::clamp(::MphRead::NativeRuntime::MathRoundToInt32(static_cast<double>((sensitivity - 0.1F) / 2.9F * 100.0F)), 0, 100);
        };
        const auto sliderToSensitivity = [](std::int32_t value)
        {
            return 0.1F + static_cast<float>(value) / 100.0F * 2.9F;
        };
        const auto lookToSlider = [](float look)
        {
            return std::clamp(::MphRead::NativeRuntime::MathRoundToInt32(static_cast<double>((look - 0.25F) / 2.75F * 100.0F)), 0, 100);
        };
        const auto sliderToLook = [](std::int32_t value)
        {
            return 0.25F + static_cast<float>(value) / 100.0F * 2.75F;
        };
        const auto deadZoneToSlider = [](float dead)
        {
            return std::clamp(::MphRead::NativeRuntime::MathRoundToInt32(static_cast<double>(dead / 0.5F * 100.0F)), 0, 100);
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
                std::u16string result = ::MphRead::NativeRuntime::Utf8ToUtf16(::MphRead::NativeRuntime::ToStringInvariant(sliderToSensitivity(value), "0.00"));
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
                std::u16string result = ::MphRead::NativeRuntime::Utf8ToUtf16(::MphRead::NativeRuntime::ToStringInvariant(sliderToLook(value), "0.00"));
                result.push_back(u'x');
                return result;
            });
        _adapter.AddPanelChild(page, _state->GamepadLook.Control);

        _state->GamepadDeadZone = _adapter.ConstructSliderRow(
            std::u16string(u"Stick dead zone"),
            deadZoneToSlider(InputSettings::GamepadDeadZone()),
            [sliderToDeadZone](std::int32_t value) -> std::optional<std::u16string>
            {
                return ::MphRead::NativeRuntime::Utf8ToUtf16(::MphRead::NativeRuntime::ToStringInvariant(sliderToDeadZone(value), "0.00"));
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
            clipLengths.push_back(Utf8ToUtf16(std::to_string(length) + " seconds"));
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
        RequireReference(reset.Value).Height(30.0);
        RequireReference(reset.Value).Accent(GuiTheme::Warm);
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
            ::MphRead::NativeRuntime::MathRoundToInt32(static_cast<double>(StylusZone::Opacity() * 100.0F)),
            [](std::int32_t value) -> std::optional<std::u16string>
            {
                return Utf8ToUtf16(std::to_string(value) + "%");
            }, 120.0, 4, 60, 2);
        _adapter.AddPanelChild(page, _state->StylusOpacity.Control);

        SettingsViewControlRef<MenuEntry> place = _adapter.ConstructMenuEntry(
            std::u16string(u"Place the bottom screen"), std::u16string{}, 13.0);
        RequireReference(place.Value).Height(30.0);
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
                Utf8ToUtf16(item.Label), TouchSettings::IsEnabled(item.Control));
            _adapter.AddPanelChild(page, row.Control);
            _state->TouchRows.push_back(SettingsViewTouchRow{item.Control, std::move(row)});
        }

        RequireReference(_state->TouchButtonsRow.Value).AddChanged(
            RowsEventHandler(_state.get(), &SettingsView::OnTouchButtonsChanged));
        OnTouchButtonsChanged(_state.get(), nullptr, RowsEventArgs::Empty);
    }

    void SettingsView::BuildMatch()
    {
        const SettingsViewControlHandle page = AddSection(u"Match rules");
        (void)Heading(page, u"Match rules");
        MenuSettings& settings = RequireSettings(_state->Settings);

        _state->PointGoal = _adapter.ConstructFieldRow(
            std::u16string(u"Point goal"), Utf8ToUtf16(settings.PointGoal), 120.0);
        _adapter.AddPanelChild(page, _state->PointGoal.Control);
        _state->TimeLimit = _adapter.ConstructFieldRow(
            std::u16string(u"Time limit"), Utf8ToUtf16(settings.TimeLimit), 120.0);
        _adapter.AddPanelChild(page, _state->TimeLimit.Control);
        RequireReference(_state->TimeLimit.Value).Box()->Watermark(u"m:ss");

        std::vector<std::u16string> damage{u"low", u"medium", u"high"};
        _state->DamageRow = _adapter.ConstructChoiceRow(
            std::u16string(u"Damage"), Strings(damage),
            std::max<std::int32_t>(0, IndexOf(damage, Utf8ToUtf16(settings.DamageLevel))));
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
            std::u16string(u"Your name"), Utf8ToUtf16(LauncherPrefs::PlayerName()), 200.0);
        _adapter.AddPanelChild(page, _state->PlayerName.Control);

        std::vector<std::u16string> hunters;
        hunters.reserve(8);
        for (std::int32_t i = 0; i < 7; ++i)
        {
            hunters.push_back(Utf8ToUtf16(::MphRead::ToString(static_cast<Hunter>(i))));
        }
        hunters.push_back(Utf8ToUtf16(::MphRead::ToString(Hunter::Random)));
        const std::u16string lastHunter = Utf8ToUtf16(::MphRead::ToString(LauncherPrefs::LastHunter()));
        _state->HunterRow = _adapter.ConstructChoiceRow(
            std::u16string(u"Hunter"), Strings(hunters),
            std::max<std::int32_t>(0, IndexOf(hunters, lastHunter)));
        _adapter.AddPanelChild(page, _state->HunterRow.Control);

        std::vector<std::u16string> colors;
        colors.reserve(PlayerColors::Count);
        for (std::int32_t i = 1; i <= PlayerColors::Count; ++i)
        {
            colors.push_back(Utf8ToUtf16(std::to_string(i)));
        }
        _state->ColorRow = _adapter.ConstructChoiceRow(
            std::u16string(u"Suit colour"), Strings(std::move(colors)),
            PlayerColors::Clamp(LauncherPrefs::LastColor()));
        _adapter.AddPanelChild(page, _state->ColorRow.Control);

        (void)Heading(page, u"Servers");
        _state->ServerRow = _adapter.ConstructFieldRow(
            std::u16string(u"Default server"),
            Utf8ToUtf16(LauncherPrefs::ServerAddress() + ":"
                + std::to_string(LauncherPrefs::ServerPort())), 220.0);
        _adapter.AddPanelChild(page, _state->ServerRow.Control);
        _state->MasterRow = _adapter.ConstructFieldRow(
            std::u16string(u"Server directory"),
            Utf8ToUtf16(LauncherPrefs::MasterHost() + ":"
                + std::to_string(LauncherPrefs::MasterPort())), 220.0);
        _adapter.AddPanelChild(page, _state->MasterRow.Control);
        _state->AutoUpdate = _adapter.ConstructToggleRow(
            std::u16string(u"Check for updates on startup"), LauncherPrefs::AutoUpdate());
        _adapter.AddPanelChild(page, _state->AutoUpdate.Control);

        (void)Heading(page, u"Game files");
        SettingsViewControlRef<MenuEntry> files = _adapter.ConstructMenuEntry(
            std::u16string(u"Game files"), Utf8ToUtf16(GameFiles::Describe()), 15.0);
        RequireReference(files.Value).SubtitleColor(GameFiles::Ready() ? GuiTheme::Good : GuiTheme::Warm);
        _adapter.AddMenuEntryClick(files.Control,
            SettingsViewAction{_state.get(), &SettingsView::OnGameFilesClick});
        _adapter.AddPanelChild(page, files.Control);
    }

    SettingsViewControlHandle SettingsView::BuildFooter()
    {
        SettingsViewControlRef<MenuEntry> save = _adapter.ConstructMenuEntry(
            std::u16string(_state->InGame ? u"Apply" : u"Save and close"),
            std::u16string{}, 15.0);
        RequireReference(save.Value).Primary(true);
        RequireReference(save.Value).Height(40.0);
        _adapter.AddMenuEntryClick(save.Control,
            SettingsViewAction{_state.get(), &SettingsView::OnSaveClick});

        SettingsViewControlRef<MenuEntry> cancel = _adapter.ConstructMenuEntry(
            std::u16string(u"Cancel"), std::u16string{}, 13.0);
        RequireReference(cancel.Value).Height(26.0);
        RequireReference(cancel.Value).Accent(GuiTheme::TextDim);
        _adapter.AddMenuEntryClick(cancel.Control,
            SettingsViewAction{_state.get(), &SettingsView::OnCancelClick});

        _state->SaveError = _adapter.ConstructNote(std::u16string{}, GuiTheme::Warm);
        RequireReference(_state->SaveError.Value).IsVisible(false);

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
            text.append(Utf8ToUtf16(ex.what()));
            RequireReference(_state->SaveError.Value).Text(text);
            RequireReference(_state->SaveError.Value).IsVisible(true);
        }
    }

    void SettingsView::Commit()
    {
        MenuSettings& settings = RequireSettings(_state->Settings);

        if (_state->WindowRow.Value)
        {
            LauncherPrefs::WindowMode(RequireReference(_state->WindowRow.Value).Index() == 1
                ? WindowStartMode::BorderlessFullscreen
                : WindowStartMode::Windowed);
            Mods::WindowMode::Startup(LauncherPrefs::WindowMode());
        }

        if (_state->ClipSecondsRow.Value)
        {
            const std::int32_t index = std::clamp(
                RequireReference(_state->ClipSecondsRow.Value).Index(), 0,
                static_cast<std::int32_t>(std::size(DemoClip::Lengths)) - 1);
            DemoClip::Seconds(DemoClip::Lengths[static_cast<std::size_t>(index)]);
        }

        settings.ResolutionScale = std::to_string(std::max(RenderOptions::MinScale,
            RequireReference(_state->ResolutionScale.Value).Value()));
        settings.Lighting = std::string(RenderOptions::OnOff(RequireReference(_state->LightingRow.Value).On()));
        settings.Fog = std::string(RenderOptions::OnOff(RequireReference(_state->FogRow.Value).On()));
        settings.TextureFiltering = std::string(RenderOptions::OnOff(RequireReference(_state->FilteringRow.Value).On()));
        settings.ShowFps = std::string(RenderOptions::OnOff(RequireReference(_state->FpsRow.Value).On()));
        const std::int32_t capIndex = std::clamp(
            RequireReference(_state->FpsLimitRow.Value).Value(), 0,
            static_cast<std::int32_t>(FpsStops.size()) - 1);
        const std::int32_t cap = FpsStops[static_cast<std::size_t>(capIndex)].Cap;
        FrameTiming::SetFrameRateCap(cap);
        settings.FrameRateCap = FrameTiming::CapString(cap);
        settings.CelShading = std::string(RenderOptions::OnOff(RequireReference(_state->CelRow.Value).On()));
        settings.CelBands = "8";
        settings.CelEdge = "50";
        Features::ProHud(RequireReference(_state->ProHud.Value).On());
        Crosshair::Size = static_cast<CrosshairSize>(RequireReference(_state->CrosshairSizeRow.Value).Index());
        Crosshair::Style = static_cast<CrosshairStyle>(RequireReference(_state->CrosshairStyleRow.Value).Index());
        Features::ProHudFixedWeapon(RequireReference(_state->WeaponStyleRow.Value).Index() == 0);

        settings.SfxVolume = ::MphRead::NativeRuntime::ToStringInvariant(static_cast<float>(RequireReference(_state->SfxVolume.Value).Value()) / 100.0F);
        settings.MusicVolume = ::MphRead::NativeRuntime::ToStringInvariant(static_cast<float>(RequireReference(_state->MusicVolume.Value).Value()) / 100.0F);
        settings.Language = ToUtf8(RequireReference(_state->LanguageRow.Value).Value().value_or(std::u16string{}));

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
            RequireReference(_state->Sensitivity.Value).Value()));
        InputSettings::InvertMouseY(RequireReference(_state->InvertY.Value).On());
        InputSettings::InvertMouseX(RequireReference(_state->InvertX.Value).On());
        PointerInput::GuardJumps(RequireReference(_state->PenTablet.Value).On());
        if (_state->StylusZone.Value && _state->StylusOpacity.Value)
        {
            StylusZone::Enabled(RequireReference(_state->StylusZone.Value).On());
            StylusZone::Opacity(std::clamp(
                static_cast<float>(RequireReference(_state->StylusOpacity.Value).Value()) / 100.0F,
                0.02F, 1.0F));
        }
        InputSettings::ScrollAllWeapons(RequireReference(_state->ScrollAllWeapons.Value).On());
        InputSettings::GamepadLookSensitivity(sliderToLook(
            RequireReference(_state->GamepadLook.Value).Value()));
        InputSettings::GamepadDeadZone(sliderToDeadZone(
            RequireReference(_state->GamepadDeadZone.Value).Value()));
        InputSettings::GamepadInvertY(RequireReference(_state->GamepadInvertY.Value).On());
        if (_state->TouchButtonsRow.Value)
        {
            TouchSettings::ButtonsVisible = RequireReference(_state->TouchButtonsRow.Value).On();
            for (const SettingsViewTouchRow& item : _state->TouchRows)
            {
                TouchSettings::SetEnabled(item.Control, RequireReference(item.Row.Value).On());
            }
        }
        InputSettings::Save();
        InputSettings::ApplyToPlayers();

        settings.PointGoal = ToUtf8(RequireReference(_state->PointGoal.Value).Value());
        settings.TimeLimit = ToUtf8(RequireReference(_state->TimeLimit.Value).Value());
        settings.DamageLevel = ToUtf8(RequireReference(_state->DamageRow.Value).Value().value_or(std::u16string{}));
        settings.TeamPlay = RequireReference(_state->TeamPlay.Value).On() ? "on" : "off";
        settings.FriendlyFire = RequireReference(_state->FriendlyFire.Value).On() ? "on" : "off";
        settings.HunterRadar = RequireReference(_state->Radar.Value).On() ? "on" : "off";
        settings.AffinityWeapons = RequireReference(_state->Affinity.Value).On() ? "on" : "off";
        settings.ShadowFreeze = RequireReference(_state->ShadowFreeze.Value).On() ? "on" : "off";

        const std::u16string playerValue = RequireReference(_state->PlayerName.Value).Value();
        const std::u16string playerTrimmedForLength = Trim(playerValue);
        if (!playerTrimmedForLength.empty())
        {
            const std::u16string secondValue = RequireReference(_state->PlayerName.Value).Value();
            LauncherPrefs::PlayerName(ToUtf8(Trim(secondValue)));
        }

        LauncherPrefs::LastHunter(ParseHunter(
            RequireReference(_state->HunterRow.Value).Value().value_or(std::u16string{})));
        std::int32_t suit = 0;
        if (::MphRead::NativeRuntime::Int32TryParseInvariant(::MphRead::NativeRuntime::Utf16ToUtf8(RequireReference(_state->ColorRow.Value).Value().value_or(std::u16string{})), suit))
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
            if (!::MphRead::NativeRuntime::Int32TryParseInvariant(::MphRead::NativeRuntime::Utf16ToUtf8(std::u16string_view(text).substr(colon + 1)), parsed)
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
        if (parseEndpoint(RequireReference(_state->ServerRow.Value).Value(), host, port))
        {
            LauncherPrefs::ServerAddress(host);
            LauncherPrefs::ServerPort(port);
        }

        std::string masterHost = LauncherPrefs::MasterHost();
        std::int32_t masterPort = LauncherPrefs::MasterPort();
        if (parseEndpoint(RequireReference(_state->MasterRow.Value).Value(), masterHost, masterPort))
        {
            LauncherPrefs::MasterHost(masterHost);
            LauncherPrefs::MasterPort(masterPort);
        }

        LauncherPrefs::AutoUpdate(RequireReference(_state->AutoUpdate.Value).On());
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
            RequireReference(click.Entry).Subtitle(Utf8ToUtf16(Mods::Credits::SupportUrl));
        }
    }

    void SettingsView::OnResetClick(void* target)
    {
        auto& state = *static_cast<SettingsViewState*>(target);
        InputSettings::Reset();

        const auto sensitivityToSlider = [](float sensitivity)
        {
            return std::clamp(::MphRead::NativeRuntime::MathRoundToInt32(static_cast<double>((sensitivity - 0.1F) / 2.9F * 100.0F)), 0, 100);
        };
        const auto lookToSlider = [](float look)
        {
            return std::clamp(::MphRead::NativeRuntime::MathRoundToInt32(static_cast<double>((look - 0.25F) / 2.75F * 100.0F)), 0, 100);
        };
        const auto deadZoneToSlider = [](float dead)
        {
            return std::clamp(::MphRead::NativeRuntime::MathRoundToInt32(static_cast<double>(dead / 0.5F * 100.0F)), 0, 100);
        };

        RequireReference(state.Sensitivity.Value).Value(
            sensitivityToSlider(InputSettings::MouseSensitivity()));
        RequireReference(state.InvertY.Value).On(InputSettings::InvertMouseY());
        RequireReference(state.InvertX.Value).On(InputSettings::InvertMouseX());
        RequireReference(state.PenTablet.Value).On(PointerInput::GuardJumps());
        if (state.StylusZone.Value && state.StylusOpacity.Value)
        {
            RequireReference(state.StylusZone.Value).On(StylusZone::Enabled());
            RequireReference(state.StylusOpacity.Value).Value(
                ::MphRead::NativeRuntime::MathRoundToInt32(static_cast<double>(StylusZone::Opacity() * 100.0F)));
        }
        RequireReference(state.ScrollAllWeapons.Value).On(InputSettings::ScrollAllWeapons());
        RequireReference(state.GamepadLook.Value).Value(
            lookToSlider(InputSettings::GamepadLookSensitivity()));
        RequireReference(state.GamepadDeadZone.Value).Value(
            deadZoneToSlider(InputSettings::GamepadDeadZone()));
        RequireReference(state.GamepadInvertY.Value).On(InputSettings::GamepadInvertY());
        for (const SettingsViewControlRef<PadRow>& row : state.PadRows)
        {
            RequireReference(row.Value).InvalidateVisual();
        }
        for (const SettingsViewControlRef<KeyRow>& row : state.KeyRows)
        {
            RequireReference(row.Value).InvalidateVisual();
        }
        if (state.TouchButtonsRow.Value)
        {
            RequireReference(state.TouchButtonsRow.Value).On(TouchSettings::ButtonsVisible);
        }
        for (const SettingsViewTouchRow& item : state.TouchRows)
        {
            RequireReference(item.Row.Value).On(TouchSettings::IsEnabled(item.Control));
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
        RequireReference(state.CrosshairStyleRow.Value).InvalidateVisual();
    }

    void SettingsView::OnTouchButtonsChanged(
        void* target, void*, const RowsEventArgs&)
    {
        auto& state = *static_cast<SettingsViewState*>(target);
        const bool visible = RequireReference(state.TouchButtonsRow.Value).On();
        for (const SettingsViewTouchRow& item : state.TouchRows)
        {
            RequireReference(item.Row.Value).IsVisible(visible);
        }
    }
}
