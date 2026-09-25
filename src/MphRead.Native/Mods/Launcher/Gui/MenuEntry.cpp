#include "MenuEntry.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

using ::MphRead::NativeRuntime::MathMax;
using ::MphRead::NativeRuntime::MathMin;

namespace
{
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

    MenuEntryEventHandler::MenuEntryEventHandler(
        void* context, Callback function, std::shared_ptr<void> keepAlive)
    {
        if (function != nullptr)
        {
            auto list = std::make_shared<std::vector<Invocation>>();
            list->push_back(Invocation{context, function, std::move(keepAlive)});
            _invocations = std::move(list);
        }
    }

    MenuEntryEventHandler::MenuEntryEventHandler(
        std::shared_ptr<const std::vector<Invocation>> invocations) noexcept
        : _invocations(std::move(invocations))
    {
    }

    MenuEntryEventHandler MenuEntryEventHandler::Combine(
        const MenuEntryEventHandler& left, const MenuEntryEventHandler& right)
    {
        if (left.IsNull())
        {
            return right;
        }
        if (right.IsNull())
        {
            return left;
        }

        auto list = std::make_shared<std::vector<Invocation>>();
        list->reserve(left._invocations->size() + right._invocations->size());
        list->insert(list->end(), left._invocations->begin(), left._invocations->end());
        list->insert(list->end(), right._invocations->begin(), right._invocations->end());
        return MenuEntryEventHandler(std::move(list));
    }

    bool MenuEntryEventHandler::IsNull() const noexcept
    {
        return !_invocations || _invocations->empty();
    }

    bool operator==(
        const MenuEntryEventHandler& left,
        const MenuEntryEventHandler& right) noexcept
    {
        if (left.IsNull() || right.IsNull())
        {
            return left.IsNull() == right.IsNull();
        }
        const auto& a = *left._invocations;
        const auto& b = *right._invocations;
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
    }

    void MenuEntryEvent::Add(const MenuEntryEventHandler& handler)
    {
        if (handler.IsNull())
        {
            return;
        }

        std::shared_ptr<const InvocationList> current = _handlers.load();
        for (;;)
        {
            auto next = std::make_shared<InvocationList>();
            next->reserve((current ? current->size() : 0) + handler._invocations->size());
            if (current)
            {
                next->insert(next->end(), current->begin(), current->end());
            }
            next->insert(next->end(), handler._invocations->begin(), handler._invocations->end());
            std::shared_ptr<const InvocationList> desired = std::move(next);
            if (_handlers.compare_exchange_weak(current, desired))
            {
                return;
            }
        }
    }

    void MenuEntryEvent::Remove(const MenuEntryEventHandler& handler)
    {
        if (handler.IsNull())
        {
            return;
        }

        std::shared_ptr<const InvocationList> current = _handlers.load();
        for (;;)
        {
            if (!current || current->size() < handler._invocations->size())
            {
                return;
            }

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
            if (!match.has_value())
            {
                return;
            }

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

            if (_handlers.compare_exchange_weak(current, desired))
            {
                return;
            }
        }
    }

    void MenuEntryEvent::Invoke(void* sender, const MenuEntryEventArgs& args) const
    {
        const std::shared_ptr<const InvocationList> handlers = _handlers.load();
        if (!handlers)
        {
            return;
        }
        for (const Invocation& handler : *handlers)
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

    void MenuEntry::AddClick(const MenuEntryEventHandler& handler)
    {
        _click.Add(handler);
    }

    void MenuEntry::RemoveClick(const MenuEntryEventHandler& handler)
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
            const std::u16string& subtitle = Require(_subtitle);
            const GuiColor subColor = _control.GetIsEnabled()
                ? SubtitleColor()
                : GuiTheme::TextDim;
            GuiBrush subtitleBrush{subColor};
            const TrackedTextFormattedText sub = TrackedText::Make(
                context, subtitle, 12.0, false,
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
