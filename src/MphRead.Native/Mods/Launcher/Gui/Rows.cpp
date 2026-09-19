#include "Rows.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <utility>

namespace
{
    using namespace MphRead::Mods::Launcher::Gui;

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

    [[nodiscard]] std::int32_t ClampInt32(
        std::int32_t value, std::int32_t min, std::int32_t max)
    {
        if (min > max)
        {
            throw RowsArgumentException();
        }
        if (value < min)
        {
            return min;
        }
        if (value > max)
        {
            return max;
        }
        return value;
    }

    [[nodiscard]] std::int32_t AddUnchecked(std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t sum = static_cast<std::uint32_t>(left)
            + static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(sum);
    }

    [[nodiscard]] bool Contains(GuiRect rect, RowsPoint point) noexcept
    {
        return point.X >= rect.X && point.X <= rect.X + rect.Width
            && point.Y >= rect.Y && point.Y <= rect.Y + rect.Height;
    }
}

namespace MphRead::Mods::Launcher::Gui
{
    RowsNullReferenceException::RowsNullReferenceException()
        : std::runtime_error("Object reference not set to an instance of an object.")
    {
    }

    RowsArgumentException::RowsArgumentException()
        : std::invalid_argument("'min' cannot be greater than max.")
    {
    }

    RowsDivideByZeroException::RowsDivideByZeroException()
        : std::runtime_error("Attempted to divide by zero.")
    {
    }

    const RowsEventArgs RowsEventArgs::Empty{};

    void RowsEvent::Add(RowsEventHandler handler)
    {
        if (handler.Function == nullptr)
        {
            return;
        }
        std::lock_guard lock(_mutex);
        _handlers.push_back(handler);
    }

    void RowsEvent::Remove(RowsEventHandler handler)
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

    void RowsEvent::Invoke(void* sender, const RowsEventArgs& args) const
    {
        std::vector<RowsEventHandler> handlers;
        {
            std::lock_guard lock(_mutex);
            handlers = _handlers;
        }
        for (const RowsEventHandler& handler : handlers)
        {
            handler.Function(handler.Context, sender, args);
        }
    }

    RowsControl::RowsControl(RowsControlAdapter& control) noexcept
        : _control(control)
    {
    }

    double RowsControl::Height() const
    {
        return _control.GetHeight();
    }

    void RowsControl::Height(double value)
    {
        _control.SetHeight(value);
    }

    bool RowsControl::IsVisible() const
    {
        return _control.GetIsVisible();
    }

    void RowsControl::IsVisible(bool value)
    {
        _control.SetIsVisible(value);
    }

    void RowsControl::Margin(RowsThickness value)
    {
        _control.SetMargin(value);
    }

    void RowsControl::InvalidateVisual()
    {
        _control.InvalidateVisual();
    }

    Caption::Caption(RowsTextControlAdapter& control, std::optional<std::u16string> text)
        : RowsControl(control), _textControl(control), _text(std::move(text))
    {
        _control.SetHeight(26.0);
    }

    RowsFormattedText Caption::Label()
    {
        if (!_text.has_value())
        {
            throw RowsNullReferenceException();
        }
        const std::u16string upper = _textControl.ToUpperInvariant(*_text);
        return _textControl.CreateFormattedText(upper, RowsCulture::Invariant,
            RowsFlowDirection::LeftToRight, GuiTheme::Face(true), 11.0,
            RowsBrush::TextDim());
    }

    RowsSize Caption::MeasureOverride(RowsSize availableSize)
    {
        const RowsSize size = _textControl.BaseMeasureOverride(availableSize);
        RowsFormattedText label = Label();
        return RowsSize{MathMin(label.Width + 8.0, availableSize.Width), size.Height};
    }

    void Caption::Render(RowsDrawingContext& context)
    {
        RowsFormattedText text = Label();
        context.DrawText(text, RowsPoint{0.0,
            _textControl.Bounds().Height - text.Height - 4.0});
        const double y = _textControl.Bounds().Height - 2.0;
        context.DrawLine(RowsPen{RowsBrush::Edge(), 1.0},
            RowsPoint{0.0, y}, RowsPoint{_textControl.Bounds().Width, y});
    }

    ChoiceRow::ChoiceRow(RowsInteractiveControlAdapter& control,
        std::optional<std::u16string> label, RowsStringListRef options,
        std::int32_t index)
        : RowsControl(control), _interactive(control), _label(std::move(label)),
          _options(std::move(options)), _index(0)
    {
        _index = Options().Count() == 0
            ? 0
            : ClampInt32(index, 0, AddUnchecked(Options().Count(), -1));
        _control.SetHeight(34.0);
        _interactive.SetFocusable(true);
        _interactive.SetHandCursor();
    }

    RowsStringList& ChoiceRow::Options() const
    {
        if (!_options)
        {
            throw RowsNullReferenceException();
        }
        return *_options;
    }

    std::int32_t ChoiceRow::Index() const noexcept
    {
        return _index;
    }

    void ChoiceRow::Index(std::int32_t value)
    {
        const std::int32_t clamped = Options().Count() == 0
            ? 0
            : ClampInt32(value, 0, AddUnchecked(Options().Count(), -1));
        if (clamped != _index)
        {
            _index = clamped;
            _control.InvalidateVisual();
            _changed.Invoke(this, RowsEventArgs::Empty);
        }
    }

    std::optional<std::u16string> ChoiceRow::Value() const
    {
        if (Options().Count() == 0)
        {
            return std::u16string{};
        }
        return Options().At(_index);
    }

    void ChoiceRow::SetItems(RowsStringListRef options, std::int32_t index)
    {
        _options = std::move(options);
        const std::int32_t replacement = Options().Count() == 0
            ? 0
            : ClampInt32(index, 0, AddUnchecked(Options().Count(), -1));
        _index = replacement;
        _control.InvalidateVisual();
    }

    const ChoiceRow::PreviewHandler& ChoiceRow::Preview() const noexcept
    {
        return _preview;
    }

    void ChoiceRow::Preview(PreviewHandler value)
    {
        _preview = std::move(value);
        _control.SetHeight(_preview ? 48.0 : 34.0);
        _control.InvalidateVisual();
    }

    RowsEvent& ChoiceRow::Changed() noexcept
    {
        return _changed;
    }

    const RowsEvent& ChoiceRow::Changed() const noexcept
    {
        return _changed;
    }

    void ChoiceRow::AddChanged(RowsEventHandler handler)
    {
        _changed.Add(handler);
    }

    void ChoiceRow::RemoveChanged(RowsEventHandler handler)
    {
        _changed.Remove(handler);
    }

    double ChoiceRow::PreviewRoom() const noexcept
    {
        return _preview ? PreviewWidth : 0.0;
    }

    GuiRect ChoiceRow::LeftArrow() const
    {
        const double x = _interactive.Bounds().Width - PreviewRoom()
            - ArrowWidth - ValueColumn - ArrowWidth;
        return GuiRect{MathMax(110.0, x), 0.0, ArrowWidth,
            _interactive.Bounds().Height};
    }

    GuiRect ChoiceRow::RightArrow() const
    {
        return GuiRect{_interactive.Bounds().Width - PreviewRoom() - ArrowWidth,
            0.0, ArrowWidth, _interactive.Bounds().Height};
    }

    void ChoiceRow::OnPointerMoved(RowsPointerEventArgs& e)
    {
        const RowsPoint p = _interactive.GetPosition(e);
        const bool left = Contains(LeftArrow(), p);
        const bool right = Contains(RightArrow(), p);
        if (left != _leftHot || right != _rightHot)
        {
            _leftHot = left;
            _rightHot = right;
            _control.InvalidateVisual();
        }
        _interactive.BaseOnPointerMoved(e);
    }

    void ChoiceRow::OnPointerExited(RowsPointerEventArgs& e)
    {
        _rightHot = false;
        _leftHot = false;
        _control.InvalidateVisual();
        _interactive.BaseOnPointerExited(e);
    }

    void ChoiceRow::OnPointerPressed(RowsPointerEventArgs& e)
    {
        _interactive.Focus();
        const RowsPoint p = _interactive.GetPosition(e);
        if (Contains(LeftArrow(), p))
        {
            Step(-1);
        }
        else
        {
            Step(1);
        }
        _interactive.BaseOnPointerPressed(e);
    }

    void ChoiceRow::OnKeyDown(RowsKeyEventArgs& e)
    {
        if (e.Key == RowsKey::Left)
        {
            Step(-1);
            e.Handled = true;
            return;
        }
        if (e.Key == RowsKey::Right || e.Key == RowsKey::Enter || e.Key == RowsKey::Space)
        {
            Step(1);
            e.Handled = true;
            return;
        }
        _interactive.BaseOnKeyDown(e);
    }

    void ChoiceRow::Step(std::int32_t direction)
    {
        if (Options().Count() == 0)
        {
            return;
        }
        const std::int32_t numerator = AddUnchecked(
            AddUnchecked(_index, direction), Options().Count());
        const std::int32_t divisor = Options().Count();
        if (divisor == 0)
        {
            throw RowsDivideByZeroException();
        }
        _index = numerator % divisor;
        _control.InvalidateVisual();
        _changed.Invoke(this, RowsEventArgs::Empty);
    }

    void ChoiceRow::Render(RowsDrawingContext& context)
    {
        context.FillRectangle(RowsBrush::Transparent(),
            GuiRect{0.0, 0.0, _interactive.Bounds().Width, _interactive.Bounds().Height});
        if (_interactive.IsFocused())
        {
            context.FillRectangle(RowsBrush::PanelLight(),
                GuiRect{0.0, 0.0, _interactive.Bounds().Width, _interactive.Bounds().Height}, 4.0);
        }

        RowsFormattedText label = _interactive.CreateFormattedText(_label,
            RowsCulture::Invariant, RowsFlowDirection::LeftToRight,
            GuiTheme::Face(false), 13.0, RowsBrush::TextDim());
        context.DrawText(label, RowsPoint{4.0,
            (_interactive.Bounds().Height - label.Height) / 2.0});

        const std::optional<std::u16string> current = Value();
        RowsFormattedText value = _interactive.CreateFormattedText(
            current ? std::optional<std::u16string_view>(*current) : std::nullopt,
            RowsCulture::Invariant, RowsFlowDirection::LeftToRight,
            GuiTheme::Face(true), 13.0, RowsBrush::Text());
        const GuiRect left = LeftArrow();
        const double room = RightArrow().X - (left.X + left.Width) - 8.0;
        if (value.Width > room)
        {
            _interactive.SetFormattedTextMaxTextWidth(value, MathMax(20.0, room));
            _interactive.SetFormattedTextTrimming(value, RowsTextTrimming::CharacterEllipsis);
        }
        const double centre = (left.X + left.Width + RightArrow().X) / 2.0;
        context.DrawText(value, RowsPoint{centre - value.Width / 2.0,
            (_interactive.Bounds().Height - value.Height) / 2.0});

        Arrow(context, left, true, _leftHot);
        Arrow(context, RightArrow(), false, _rightHot);
        if (_preview)
        {
            constexpr double inset = 3.0;
            _preview(context, GuiRect{
                _interactive.Bounds().Width - PreviewWidth + inset,
                inset,
                PreviewWidth - inset * 2.0,
                _interactive.Bounds().Height - inset * 2.0
            });
        }
    }

    void ChoiceRow::Arrow(RowsDrawingContext& context, GuiRect area,
        bool pointsLeft, bool hot)
    {
        const double cx = area.X + area.Width / 2.0;
        const double cy = area.Y + area.Height / 2.0;
        constexpr double w = 4.5;
        constexpr double h = 6.0;

        RowsTriangleGeometry geometry{};
        geometry.IsFilled = true;
        geometry.IsClosed = true;
        if (pointsLeft)
        {
            geometry.Points = {
                RowsPoint{cx + w, cy - h},
                RowsPoint{cx - w, cy},
                RowsPoint{cx + w, cy + h}
            };
        }
        else
        {
            geometry.Points = {
                RowsPoint{cx - w, cy - h},
                RowsPoint{cx + w, cy},
                RowsPoint{cx - w, cy + h}
            };
        }
        context.DrawGeometry(hot ? RowsBrush::Accent() : RowsBrush::TextDim(),
            std::nullopt, geometry);
    }

    ToggleRow::ToggleRow(RowsInteractiveControlAdapter& control,
        std::optional<std::u16string> label, bool on)
        : RowsControl(control), _interactive(control), _label(std::move(label)), _on(on)
    {
        _control.SetHeight(34.0);
        _interactive.SetFocusable(true);
        _interactive.SetHandCursor();
    }

    bool ToggleRow::On() const noexcept
    {
        return _on;
    }

    void ToggleRow::On(bool value)
    {
        if (_on != value)
        {
            _on = value;
            _control.InvalidateVisual();
            _changed.Invoke(this, RowsEventArgs::Empty);
        }
    }

    RowsEvent& ToggleRow::Changed() noexcept
    {
        return _changed;
    }

    const RowsEvent& ToggleRow::Changed() const noexcept
    {
        return _changed;
    }

    void ToggleRow::AddChanged(RowsEventHandler handler)
    {
        _changed.Add(handler);
    }

    void ToggleRow::RemoveChanged(RowsEventHandler handler)
    {
        _changed.Remove(handler);
    }

    void ToggleRow::OnPointerPressed(RowsPointerEventArgs& e)
    {
        _interactive.Focus();
        On(!On());
        _interactive.BaseOnPointerPressed(e);
    }

    void ToggleRow::OnKeyDown(RowsKeyEventArgs& e)
    {
        if (e.Key == RowsKey::Enter || e.Key == RowsKey::Space
            || e.Key == RowsKey::Left || e.Key == RowsKey::Right)
        {
            On(!On());
            e.Handled = true;
            return;
        }
        _interactive.BaseOnKeyDown(e);
    }

    void ToggleRow::Render(RowsDrawingContext& context)
    {
        context.FillRectangle(RowsBrush::Transparent(),
            GuiRect{0.0, 0.0, _interactive.Bounds().Width, _interactive.Bounds().Height});
        if (_interactive.IsFocused())
        {
            context.FillRectangle(RowsBrush::PanelLight(),
                GuiRect{0.0, 0.0, _interactive.Bounds().Width, _interactive.Bounds().Height}, 4.0);
        }

        RowsFormattedText label = _interactive.CreateFormattedText(_label,
            RowsCulture::Invariant, RowsFlowDirection::LeftToRight,
            GuiTheme::Face(false), 13.0, RowsBrush::TextDim());
        context.DrawText(label, RowsPoint{4.0,
            (_interactive.Bounds().Height - label.Height) / 2.0});

        constexpr double w = 40.0;
        constexpr double h = 20.0;
        const GuiRect track{
            _interactive.Bounds().Width - w - 4.0,
            (_interactive.Bounds().Height - h) / 2.0,
            w,
            h
        };
        context.DrawRectangle(RowsBrush::Solid(_on ? GuiTheme::Accent : GuiTheme::Edge),
            std::nullopt, RowsRoundedRect{track, h / 2.0});
        const double knob = _on ? track.X + track.Width - h / 2.0 : track.X + h / 2.0;
        context.DrawEllipse(RowsBrush::Solid(_on ? GuiTheme::Ink : GuiTheme::TextDim),
            std::nullopt, RowsPoint{knob, track.Y + h / 2.0}, h / 2.0 - 3.0, h / 2.0 - 3.0);
    }

    FieldRowTextBox::FieldRowTextBox(
        FieldRowAdapter& adapter, FieldRowAdapter::ElementHandle handle) noexcept
        : _adapter(adapter), _handle(handle)
    {
    }

    FieldRowAdapter::ElementHandle FieldRowTextBox::Native() const noexcept
    {
        return _handle;
    }

    std::optional<std::u16string> FieldRowTextBox::Text() const
    {
        return _adapter.GetTextBoxText(_handle);
    }

    void FieldRowTextBox::Text(std::optional<std::u16string_view> value)
    {
        _adapter.SetTextBoxText(_handle, value);
    }

    void FieldRowTextBox::Watermark(std::optional<std::u16string_view> value)
    {
        _adapter.SetTextBoxWatermark(_handle, value);
    }

    void FieldRowTextBox::AddLostFocus(RowsEventHandler handler)
    {
        _adapter.AddTextBoxLostFocus(_handle, handler);
    }

    void FieldRowTextBox::RemoveLostFocus(RowsEventHandler handler)
    {
        _adapter.RemoveTextBoxLostFocus(_handle, handler);
    }

    FieldRow::FieldRow(FieldRowAdapter& adapter, std::optional<std::u16string> label,
        std::optional<std::u16string> value, double boxWidth)
        : RowsControl(adapter), _fieldAdapter(adapter), _box(nullptr)
    {
        _control.SetHeight(36.0);

        const FieldRowAdapter::ElementHandle caption = _fieldAdapter.CreateTextBlock();
        _fieldAdapter.SetTextBlockText(caption,
            label ? std::optional<std::u16string_view>(*label) : std::nullopt);
        _fieldAdapter.SetTextBlockFontFamily(caption, GuiTheme::Display);
        _fieldAdapter.SetTextBlockFontSize(caption, 13.0);
        _fieldAdapter.SetTextBlockForeground(caption, RowsBrush::TextDim());
        _fieldAdapter.SetTextBlockVerticalAlignment(caption, RowsVerticalAlignment::Center);
        _fieldAdapter.SetTextBlockHorizontalAlignment(caption, RowsHorizontalAlignment::Left);
        _fieldAdapter.SetTextBlockMargin(caption, RowsThickness{4.0, 0.0, 0.0, 0.0});

        const FieldRowAdapter::ElementHandle box = _fieldAdapter.CreateTextBox();
        _fieldAdapter.SetTextBoxText(box,
            value ? std::optional<std::u16string_view>(*value) : std::nullopt);
        _fieldAdapter.SetTextBoxWidth(box, boxWidth);
        _fieldAdapter.SetTextBoxFontFamily(box, GuiTheme::Display);
        _fieldAdapter.SetTextBoxFontSize(box, 13.0);
        _fieldAdapter.SetTextBoxCornerRadius(box, 4.0);
        _fieldAdapter.SetTextBoxPadding(box, RowsThickness{8.0, 4.0, 8.0, 4.0});
        _fieldAdapter.SetTextBoxVerticalAlignment(box, RowsVerticalAlignment::Center);
        _fieldAdapter.SetTextBoxHorizontalAlignment(box, RowsHorizontalAlignment::Right);
        _box = box;

        _fieldAdapter.AddChild(caption);
        _fieldAdapter.AddChild(_box);
    }

    FieldRowTextBox FieldRow::Box() noexcept
    {
        return FieldRowTextBox(_fieldAdapter, _box);
    }

    std::u16string FieldRow::Value() const
    {
        std::optional<std::u16string> text = _fieldAdapter.GetTextBoxText(_box);
        return text.has_value() ? std::move(*text) : std::u16string{};
    }

    void FieldRow::Value(std::optional<std::u16string_view> value)
    {
        _fieldAdapter.SetTextBoxText(_box, value);
    }

    Note::Note(NoteAdapter& adapter, std::optional<std::u16string> text,
        std::optional<GuiColor> color)
        : RowsControl(adapter), _noteAdapter(adapter)
    {
        _noteAdapter.SetText(text ? std::optional<std::u16string_view>(*text) : std::nullopt);
        _noteAdapter.SetFontFamily(GuiTheme::Display);
        _noteAdapter.SetFontSize(12.0);
        _noteAdapter.SetForeground(RowsBrush::Solid(color.value_or(GuiTheme::TextDim)));
        _noteAdapter.SetTextWrapping(RowsTextWrapping::Wrap);
        _control.SetMargin(RowsThickness{4.0, 4.0, 4.0, 4.0});
    }

    std::optional<std::u16string> Note::Text() const
    {
        return _noteAdapter.GetText();
    }

    void Note::Text(std::optional<std::u16string_view> value)
    {
        _noteAdapter.SetText(value);
    }

    RowsBrush Note::Foreground() const
    {
        return _noteAdapter.GetForeground();
    }

    void Note::Foreground(RowsBrush value)
    {
        _noteAdapter.SetForeground(value);
    }
}
