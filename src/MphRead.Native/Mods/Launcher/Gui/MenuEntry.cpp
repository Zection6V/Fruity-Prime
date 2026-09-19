#include "MenuEntry.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

namespace
{
    [[nodiscard]] double MathMax(double val1, double val2) noexcept
    {
        if (val1 != val2)
        {
            if (!std::isnan(val1))
            {
                return val2 < val1 ? val1 : val2;
            }
            return val1;
        }
        return std::signbit(val2) ? val1 : val2;
    }

    [[nodiscard]] double MathMin(double val1, double val2) noexcept
    {
        if (val1 != val2)
        {
            if (!std::isnan(val1))
            {
                return val1 < val2 ? val1 : val2;
            }
            return val1;
        }
        return std::signbit(val1) ? val1 : val2;
    }

    class MenuEntryMetricsAdapter final
        : public MphRead::Mods::Launcher::Gui::TrackedTextAdapter
    {
    public:
        explicit MenuEntryMetricsAdapter(
            MphRead::Mods::Launcher::Gui::MenuEntryControlAdapter& control) noexcept
            : TrackedTextAdapter(
                MphRead::Mods::Launcher::Gui::TrackedTextBrush{
                    &MphRead::Mods::Launcher::Gui::GuiTheme::TextBrush}),
              _control(control)
        {
        }

        MphRead::Mods::Launcher::Gui::TrackedTextFormattedText CreateFormattedText(
            std::u16string_view text,
            MphRead::Mods::Launcher::Gui::TrackedTextCulture culture,
            MphRead::Mods::Launcher::Gui::TrackedTextFlowDirection flowDirection,
            MphRead::Mods::Launcher::Gui::TrackedTextFace face,
            double fontSize,
            MphRead::Mods::Launcher::Gui::TrackedTextBrush brush) override
        {
            return _control.CreateFormattedText(
                text, culture, flowDirection, face, fontSize, brush);
        }

        void DrawText(
            const MphRead::Mods::Launcher::Gui::TrackedTextFormattedText&,
            MphRead::Mods::Launcher::Gui::TrackedTextPoint) override
        {
        }

    private:
        MphRead::Mods::Launcher::Gui::MenuEntryControlAdapter& _control;
    };
}

namespace MphRead::Mods::Launcher::Gui
{
    const MenuEntryStyledProperty<std::optional<std::u16string>> MenuEntry::TitleProperty{
        "Title", std::u16string{}, true
    };

    const MenuEntryStyledProperty<std::optional<std::u16string>> MenuEntry::SubtitleProperty{
        "Subtitle", std::u16string{}, true
    };

    const MenuEntryStyledProperty<GuiColor> MenuEntry::AccentProperty{
        "Accent", GuiTheme::Accent, true
    };

    const MenuEntryStyledProperty<GuiColor> MenuEntry::SubtitleColorProperty{
        "SubtitleColor", GuiTheme::TextDim, true
    };

    const MenuEntryStyledProperty<bool> MenuEntry::PrimaryProperty{
        "Primary", false, true
    };

    const MenuEntryStyledProperty<bool> MenuEntry::SelectedProperty{
        "Selected", false, true
    };

    const MenuEntryEventArgs MenuEntryEventArgs::Empty{};

    MenuEntryNullReferenceException::MenuEntryNullReferenceException()
        : std::runtime_error("Object reference not set to an instance of an object.")
    {
    }

    void MenuEntryEvent::Add(MenuEntryEventHandler handler)
    {
        if (handler.Function == nullptr)
        {
            return;
        }
        std::lock_guard lock(_mutex);
        _handlers.push_back(handler);
    }

    void MenuEntryEvent::Remove(MenuEntryEventHandler handler)
    {
        if (handler.Function == nullptr)
        {
            return;
        }
        std::lock_guard lock(_mutex);
        for (auto it = _handlers.rbegin(); it != _handlers.rend(); ++it)
        {
            if (*it == handler)
            {
                _handlers.erase(std::next(it).base());
                return;
            }
        }
    }

    void MenuEntryEvent::Invoke(void* sender, const MenuEntryEventArgs& args) const
    {
        std::vector<MenuEntryEventHandler> handlers;
        {
            std::lock_guard lock(_mutex);
            handlers = _handlers;
        }
        for (const MenuEntryEventHandler& handler : handlers)
        {
            handler.Function(handler.Context, sender, args);
        }
    }

    MenuEntryDrawingContext::MenuEntryDrawingContext() noexcept
        : TrackedTextAdapter(TrackedTextBrush{&GuiTheme::TextBrush})
    {
    }

    MenuEntry::MenuEntry(MenuEntryControlAdapter& control,
        std::optional<std::u16string> title,
        std::optional<std::u16string> subtitle,
        double titleSize)
        : _control(control),
          _title(TitleProperty.DefaultValue),
          _subtitle(SubtitleProperty.DefaultValue),
          _accent(AccentProperty.DefaultValue),
          _subtitleColor(SubtitleColorProperty.DefaultValue),
          _primary(PrimaryProperty.DefaultValue),
          _selected(SelectedProperty.DefaultValue)
    {
        Title(title);
        Subtitle(subtitle);
        _titleSize = titleSize;
        _control.SetFocusable(true);
        _control.SetHandCursor();
        Height(Require(subtitle).length() > 0 ? SubtitledHeight : PlainHeight);
    }

    const std::u16string& MenuEntry::Require(
        const std::optional<std::u16string>& value) const
    {
        if (!value.has_value())
        {
            throw MenuEntryNullReferenceException();
        }
        return *value;
    }

    void MenuEntry::InvalidateForProperty(MenuEntryProperty property)
    {
        switch (property)
        {
        case MenuEntryProperty::Title:
        case MenuEntryProperty::Subtitle:
        case MenuEntryProperty::Accent:
        case MenuEntryProperty::SubtitleColor:
        case MenuEntryProperty::Primary:
        case MenuEntryProperty::Selected:
        case MenuEntryProperty::IsEnabled:
            _control.InvalidateVisual();
            break;
        }
    }

    void MenuEntry::OnPropertyChanged(MenuEntryProperty property)
    {
        _control.BaseOnPropertyChanged(property);
        if (property == MenuEntryProperty::Subtitle)
        {
            Height(Require(_subtitle).length() > 0 ? SubtitledHeight : PlainHeight);
        }
    }

    std::optional<std::u16string> MenuEntry::Title() const
    {
        return _title;
    }

    void MenuEntry::Title(std::optional<std::u16string> value)
    {
        if (_title == value)
        {
            return;
        }
        _title = std::move(value);
        OnPropertyChanged(MenuEntryProperty::Title);
        InvalidateForProperty(MenuEntryProperty::Title);
    }

    std::optional<std::u16string> MenuEntry::Subtitle() const
    {
        return _subtitle;
    }

    void MenuEntry::Subtitle(std::optional<std::u16string> value)
    {
        if (_subtitle == value)
        {
            return;
        }
        _subtitle = std::move(value);
        OnPropertyChanged(MenuEntryProperty::Subtitle);
        InvalidateForProperty(MenuEntryProperty::Subtitle);
    }

    GuiColor MenuEntry::Accent() const noexcept
    {
        return _accent;
    }

    void MenuEntry::Accent(GuiColor value)
    {
        if (_accent == value)
        {
            return;
        }
        _accent = value;
        OnPropertyChanged(MenuEntryProperty::Accent);
        InvalidateForProperty(MenuEntryProperty::Accent);
    }

    GuiColor MenuEntry::SubtitleColor() const noexcept
    {
        return _subtitleColor;
    }

    void MenuEntry::SubtitleColor(GuiColor value)
    {
        if (_subtitleColor == value)
        {
            return;
        }
        _subtitleColor = value;
        OnPropertyChanged(MenuEntryProperty::SubtitleColor);
        InvalidateForProperty(MenuEntryProperty::SubtitleColor);
    }

    bool MenuEntry::Primary() const noexcept
    {
        return _primary;
    }

    void MenuEntry::Primary(bool value)
    {
        if (_primary == value)
        {
            return;
        }
        _primary = value;
        OnPropertyChanged(MenuEntryProperty::Primary);
        InvalidateForProperty(MenuEntryProperty::Primary);
    }

    bool MenuEntry::Selected() const noexcept
    {
        return _selected;
    }

    void MenuEntry::Selected(bool value)
    {
        if (_selected == value)
        {
            return;
        }
        _selected = value;
        OnPropertyChanged(MenuEntryProperty::Selected);
        InvalidateForProperty(MenuEntryProperty::Selected);
    }

    double MenuEntry::Height() const
    {
        return _control.GetHeight();
    }

    void MenuEntry::Height(double value)
    {
        _control.SetHeight(value);
    }

    bool MenuEntry::IsEnabled() const
    {
        return _control.GetIsEnabled();
    }

    void MenuEntry::IsEnabled(bool value)
    {
        if (_control.GetIsEnabled() == value)
        {
            return;
        }
        _control.SetIsEnabled(value);
        OnPropertyChanged(MenuEntryProperty::IsEnabled);
        InvalidateForProperty(MenuEntryProperty::IsEnabled);
    }

    void MenuEntry::AddClick(MenuEntryEventHandler handler)
    {
        _click.Add(handler);
    }

    void MenuEntry::RemoveClick(MenuEntryEventHandler handler)
    {
        _click.Remove(handler);
    }

    MenuEntrySize MenuEntry::MeasureOverride(MenuEntrySize availableSize)
    {
        const MenuEntrySize size = _control.BaseMeasureOverride(availableSize);
        const double tracking = Primary() ? 2.0 : 1.0;
        MenuEntryMetricsAdapter metrics(_control);
        const std::u16string upper = _control.ToUpperInvariant(Require(_title));
        double width = TrackedText::Measure(metrics, upper, _titleSize, tracking);
        if (Require(_subtitle).length() > 0)
        {
            width = MathMax(width,
                TrackedText::Measure(metrics, Require(_subtitle), 12.0, 0.0));
        }
        constexpr double padding = 3.0 + 14.0 + 14.0;
        return MenuEntrySize{
            MathMin(width + padding, availableSize.Width),
            size.Height
        };
    }

    void MenuEntry::OnPointerEntered(MenuEntryPointerEventArgs& e)
    {
        _control.InvalidateVisual();
        _control.BaseOnPointerEntered(e);
    }

    void MenuEntry::OnPointerExited(MenuEntryPointerEventArgs& e)
    {
        _pressed = false;
        _control.InvalidateVisual();
        _control.BaseOnPointerExited(e);
    }

    void MenuEntry::OnPointerPressed(MenuEntryPointerEventArgs& e)
    {
        _pressed = true;
        _control.Focus();
        _control.Capture(e.Pointer, this);
        _control.InvalidateVisual();
        _control.BaseOnPointerPressed(e);
    }

    void MenuEntry::OnPointerReleased(MenuEntryPointerEventArgs& e)
    {
        const bool wasPressed = _pressed;
        _pressed = false;
        _control.InvalidateVisual();
        if (_control.Captured(e.Pointer) == this)
        {
            _control.Capture(e.Pointer, nullptr);
        }
        _control.BaseOnPointerReleased(e);
        const MenuEntryPoint p = _control.GetPosition(e);
        const bool inside = p.X >= 0.0 && p.Y >= 0.0
            && p.X <= _control.Bounds().Width
            && p.Y <= _control.Bounds().Height;
        if (wasPressed && _control.GetIsEnabled() && inside)
        {
            _click.Invoke(this, MenuEntryEventArgs::Empty);
        }
    }

    void MenuEntry::OnPointerCaptureLost(MenuEntryPointerCaptureLostEventArgs& e)
    {
        _pressed = false;
        _control.InvalidateVisual();
        _control.BaseOnPointerCaptureLost(e);
    }

    void MenuEntry::OnGotFocus(MenuEntryGotFocusEventArgs& e)
    {
        _control.InvalidateVisual();
        _control.BaseOnGotFocus(e);
    }

    void MenuEntry::OnLostFocus(MenuEntryRoutedEventArgs& e)
    {
        _control.InvalidateVisual();
        _control.BaseOnLostFocus(e);
    }

    void MenuEntry::OnKeyDown(MenuEntryKeyEventArgs& e)
    {
        if (e.Key == MenuEntryKey::Enter || e.Key == MenuEntryKey::Space)
        {
            e.Handled = true;
            if (_control.GetIsEnabled())
            {
                _click.Invoke(this, MenuEntryEventArgs::Empty);
            }
            return;
        }
        _control.BaseOnKeyDown(e);
    }

    void MenuEntry::Render(MenuEntryDrawingContext& context)
    {
        const bool pointerOver = _control.IsPointerOver();
        const bool focused = pointerOver ? false : _control.IsFocused();
        const bool lit = (pointerOver || focused)
            ? _control.GetIsEnabled()
            : false;

        bool marked = lit;
        if (!marked && Selected())
        {
            marked = _control.GetIsEnabled();
        }

        const GuiRect body{
            0.0,
            0.0,
            _control.Bounds().Width,
            _control.Bounds().Height
        };
        context.FillRectangle(MenuEntryBrush::Transparent(), body);
        if (Primary())
        {
            RenderPrimary(context, body, lit);
            return;
        }
        if (lit)
        {
            const GuiColor fill = _pressed
                ? GuiTheme::Shade(GuiTheme::PanelLight, -0.2)
                : GuiTheme::PanelLight;
            context.FillRectangle(MenuEntryBrush::Solid(fill), body);
        }

        constexpr double bar = 3.0;
        context.FillRectangle(
            MenuEntryBrush::Solid(marked
                ? Accent()
                : GuiColor::FromRgb(48, 56, 72)),
            GuiRect{0.0, 0.0, bar, body.Height});

        const double textLeft = bar + 14.0;
        GuiColor titleColor;
        if (!_control.GetIsEnabled())
        {
            titleColor = GuiTheme::TextDim;
        }
        else if (marked)
        {
            titleColor = Accent();
        }
        else
        {
            titleColor = GuiTheme::Text;
        }

        MenuEntryMetricsAdapter metrics(_control);
        const double lineHeight = TrackedText::LineHeight(metrics, _titleSize);
        const double top = Require(_subtitle).length() > 0
            ? 8.0
            : (body.Height - lineHeight) / 2.0;

        const std::u16string title = _control.ToUpperInvariant(Require(_title));
        GuiBrush titleBrush{titleColor};
        TrackedText::Draw(context, title, _titleSize,
            TrackedTextBrush{&titleBrush}, textLeft, top, 1.0);

        if (Require(_subtitle).length() > 0)
        {
            const GuiColor subColor = _control.GetIsEnabled()
                ? SubtitleColor()
                : GuiTheme::TextDim;
            GuiBrush subtitleBrush{subColor};
            const TrackedTextFormattedText sub = TrackedText::Make(
                context, Require(_subtitle), 12.0, false,
                TrackedTextBrush{&subtitleBrush});
            context.DrawText(sub, TrackedTextPoint{
                textLeft - 1.0,
                top + lineHeight + 1.0
            });
        }
    }

    void MenuEntry::RenderPrimary(
        MenuEntryDrawingContext& context, GuiRect body, bool lit)
    {
        GuiColor fill;
        if (!_control.GetIsEnabled())
        {
            fill = GuiTheme::PanelLight;
        }
        else if (_pressed)
        {
            fill = GuiTheme::Shade(Accent(), -0.25);
        }
        else if (lit)
        {
            fill = GuiTheme::Shade(Accent(), 0.18);
        }
        else
        {
            fill = Accent();
        }

        context.DrawRectangle(MenuEntryBrush::Solid(fill), std::nullopt,
            GuiTheme::Round(body, 5.0));

        const std::u16string text = _control.ToUpperInvariant(Require(_title));
        const GuiColor ink = _control.GetIsEnabled()
            ? GuiColor::FromRgb(8, 12, 18)
            : GuiTheme::TextDim;
        constexpr double tracking = 2.0;
        MenuEntryMetricsAdapter metrics(_control);
        const double width = TrackedText::Measure(
            metrics, text, _titleSize, tracking) - tracking;
        GuiBrush inkBrush{ink};
        TrackedText::Draw(context, text, _titleSize,
            TrackedTextBrush{&inkBrush},
            (body.Width - width) / 2.0,
            (body.Height - TrackedText::LineHeight(metrics, _titleSize)) / 2.0,
            tracking);
    }
}
