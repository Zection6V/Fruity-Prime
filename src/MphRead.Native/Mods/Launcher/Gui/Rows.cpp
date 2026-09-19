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

    const int TransparentBrushIdentity = 0;

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

    [[nodiscard]] std::int32_t RemainderInt32(std::int32_t left, std::int32_t right)
    {
        if (right == 0)
        {
            throw RowsDivideByZeroException();
        }
        if (left == std::numeric_limits<std::int32_t>::min() && right == -1)
        {
            throw RowsOverflowException();
        }
        return left % right;
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

    RowsOverflowException::RowsOverflowException()
        : std::overflow_error("Arithmetic operation resulted in an overflow.")
    {
    }

    RowsBrush::RowsBrush(
        RowsBrushKind kind, GuiBrush* shared, std::shared_ptr<GuiBrush> owned) noexcept
        : _kind(kind), _shared(shared), _owned(std::move(owned))
    {
    }

    RowsBrush RowsBrush::Transparent() noexcept
    {
        return RowsBrush(RowsBrushKind::Transparent, nullptr, {});
    }

    RowsBrush RowsBrush::PanelLight() noexcept
    {
        return RowsBrush(RowsBrushKind::PanelLightBrush, &GuiTheme::PanelLightBrush, {});
    }

    RowsBrush RowsBrush::Edge() noexcept
    {
        return RowsBrush(RowsBrushKind::EdgeBrush, &GuiTheme::EdgeBrush, {});
    }

    RowsBrush RowsBrush::Text() noexcept
    {
        return RowsBrush(RowsBrushKind::TextBrush, &GuiTheme::TextBrush, {});
    }

    RowsBrush RowsBrush::TextDim() noexcept
    {
        return RowsBrush(RowsBrushKind::TextDimBrush, &GuiTheme::TextDimBrush, {});
    }

    RowsBrush RowsBrush::Accent() noexcept
    {
        return RowsBrush(RowsBrushKind::AccentBrush, &GuiTheme::AccentBrush, {});
    }

    RowsBrush RowsBrush::Warm() noexcept
    {
        return RowsBrush(RowsBrushKind::WarmBrush, &GuiTheme::WarmBrush, {});
    }

    RowsBrush RowsBrush::Good() noexcept
    {
        return RowsBrush(RowsBrushKind::GoodBrush, &GuiTheme::GoodBrush, {});
    }

    RowsBrush RowsBrush::Bad() noexcept
    {
        return RowsBrush(RowsBrushKind::BadBrush, &GuiTheme::BadBrush, {});
    }

    RowsBrush RowsBrush::Reference(GuiBrush& brush) noexcept
    {
        return RowsBrush(RowsBrushKind::MutableBrush, &brush, {});
    }

    RowsBrush RowsBrush::Solid(GuiColor color)
    {
        return RowsBrush(RowsBrushKind::SolidColor, nullptr,
            std::make_shared<GuiBrush>(color));
    }

    RowsBrushKind RowsBrush::Kind() const noexcept
    {
        return _kind;
    }

    GuiBrush* RowsBrush::MutableBrush() const noexcept
    {
        return _owned ? _owned.get() : _shared;
    }

    const void* RowsBrush::Identity() const noexcept
    {
        if (_kind == RowsBrushKind::Transparent)
        {
            return &TransparentBrushIdentity;
        }
        if (_owned)
        {
            return _owned.get();
        }
        return _shared;
    }

    std::optional<GuiColor> RowsBrush::CurrentColor() const noexcept
    {
        if (GuiBrush* brush = MutableBrush())
        {
            return brush->Color;
        }
        if (_kind == RowsBrushKind::Transparent)
        {
            return GuiColor::FromArgb(0, 255, 255, 255);
        }
        return std::nullopt;
    }

    RowsPen::RowsPen(std::optional<RowsBrush> brush, double thickness)
        : _state(std::make_shared<State>(State{std::move(brush), thickness, {}, RowsPenLineCap::Flat, RowsPenLineJoin::Miter, 10.0}))
    {
    }

    RowsPen::RowsPen(RowsBrush brush, double thickness)
        : RowsPen(std::optional<RowsBrush>(std::move(brush)), thickness)
    {
    }

    std::optional<RowsBrush> RowsPen::Brush() const
    {
        return _state->Brush;
    }

    void RowsPen::Brush(std::optional<RowsBrush> value)
    {
        _state->Brush = std::move(value);
    }

    double RowsPen::Thickness() const noexcept
    {
        return _state->Thickness;
    }

    void RowsPen::Thickness(double value) noexcept
    {
        _state->Thickness = value;
    }

    std::shared_ptr<void> RowsPen::DashStyle() const noexcept
    {
        return _state->DashStyle;
    }

    void RowsPen::DashStyle(std::shared_ptr<void> value) noexcept
    {
        _state->DashStyle = std::move(value);
    }

    RowsPenLineCap RowsPen::LineCap() const noexcept
    {
        return _state->LineCap;
    }

    void RowsPen::LineCap(RowsPenLineCap value) noexcept
    {
        _state->LineCap = value;
    }

    RowsPenLineJoin RowsPen::LineJoin() const noexcept
    {
        return _state->LineJoin;
    }

    void RowsPen::LineJoin(RowsPenLineJoin value) noexcept
    {
        _state->LineJoin = value;
    }

    double RowsPen::MiterLimit() const noexcept
    {
        return _state->MiterLimit;
    }

    void RowsPen::MiterLimit(double value) noexcept
    {
        _state->MiterLimit = value;
    }

    const void* RowsPen::Identity() const noexcept
    {
        return _state.get();
    }

    RowsTriangleGeometry::RowsTriangleGeometry(
        std::array<RowsPoint, 3> points, bool isFilled, bool isClosed)
        : _state(std::make_shared<State>(State{std::move(points), isFilled, isClosed}))
    {
    }

    const std::array<RowsPoint, 3>& RowsTriangleGeometry::Points() const noexcept
    {
        return _state->Points;
    }

    bool RowsTriangleGeometry::IsFilled() const noexcept
    {
        return _state->IsFilled;
    }

    bool RowsTriangleGeometry::IsClosed() const noexcept
    {
        return _state->IsClosed;
    }

    const void* RowsTriangleGeometry::Identity() const noexcept
    {
        return _state.get();
    }

    const RowsEventArgs RowsEventArgs::Empty{};

    RowsEventHandler::RowsEventHandler(
        void* context, Callback function, std::shared_ptr<void> keepAlive)
    {
        if (function != nullptr)
        {
            auto list = std::make_shared<std::vector<Invocation>>();
            list->push_back(Invocation{context, function, std::move(keepAlive)});
            _invocations = std::move(list);
        }
    }

    RowsEventHandler::RowsEventHandler(
        std::shared_ptr<const std::vector<Invocation>> invocations) noexcept
        : _invocations(std::move(invocations))
    {
    }

    RowsEventHandler RowsEventHandler::Static(Callback function)
    {
        return RowsEventHandler(nullptr, function, {});
    }

    RowsEventHandler RowsEventHandler::Instance(
        std::shared_ptr<void> target, Callback function)
    {
        void* context = target.get();
        return RowsEventHandler(context, function, std::move(target));
    }

    RowsEventHandler RowsEventHandler::Combine(
        const RowsEventHandler& left, const RowsEventHandler& right)
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
        return RowsEventHandler(std::move(list));
    }

    bool RowsEventHandler::IsNull() const noexcept
    {
        return !_invocations || _invocations->empty();
    }

    bool operator==(const RowsEventHandler& left, const RowsEventHandler& right) noexcept
    {
        if (left.IsNull() || right.IsNull())
        {
            return left.IsNull() == right.IsNull();
        }
        const auto& a = *left._invocations;
        const auto& b = *right._invocations;
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
    }

    void RowsEvent::Add(const RowsEventHandler& handler)
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

    void RowsEvent::Remove(const RowsEventHandler& handler)
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

    void RowsEvent::Invoke(void* sender, const RowsEventArgs& args) const
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

    RowsControl::RowsControl(RowsControlAdapter& control) noexcept : _control(control) {}
    double RowsControl::Height() const { return _control.GetHeight(); }
    void RowsControl::Height(double value) { _control.SetHeight(value); }
    bool RowsControl::IsVisible() const { return _control.GetIsVisible(); }
    void RowsControl::IsVisible(bool value) { _control.SetIsVisible(value); }
    void RowsControl::Margin(RowsThickness value) { _control.SetMargin(value); }
    void RowsControl::InvalidateVisual() { _control.InvalidateVisual(); }

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
            RowsFlowDirection::LeftToRight, GuiTheme::Face(true), 11.0, RowsBrush::TextDim());
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
        context.DrawText(text, RowsPoint{0.0, _textControl.Bounds().Height - text.Height - 4.0});
        const double y = _textControl.Bounds().Height - 2.0;
        RowsPen pen(RowsBrush::Edge(), 1.0);
        const RowsPoint start{0.0, y};
        const RowsPoint end{_textControl.Bounds().Width, y};
        context.DrawLine(std::move(pen), start, end);
    }

    ChoiceRow::ChoiceRow(RowsInteractiveControlAdapter& control,
        std::optional<std::u16string> label, RowsStringListRef options, std::int32_t index)
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

    std::int32_t ChoiceRow::Index() const noexcept { return _index; }

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

    const ChoiceRow::PreviewHandler& ChoiceRow::Preview() const noexcept { return _preview; }

    void ChoiceRow::Preview(PreviewHandler value)
    {
        _preview = std::move(value);
        _control.SetHeight(_preview ? 48.0 : 34.0);
        _control.InvalidateVisual();
    }

    RowsEvent& ChoiceRow::Changed() noexcept { return _changed; }
    const RowsEvent& ChoiceRow::Changed() const noexcept { return _changed; }
    void ChoiceRow::AddChanged(const RowsEventHandler& handler) { _changed.Add(handler); }
    void ChoiceRow::RemoveChanged(const RowsEventHandler& handler) { _changed.Remove(handler); }
    double ChoiceRow::PreviewRoom() const noexcept { return _preview ? PreviewWidth : 0.0; }

    GuiRect ChoiceRow::LeftArrow() const
    {
        const double x = _interactive.Bounds().Width - PreviewRoom()
            - ArrowWidth - ValueColumn - ArrowWidth;
        return GuiRect{MathMax(110.0, x), 0.0, ArrowWidth, _interactive.Bounds().Height};
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
            Step(-1);
        else
            Step(1);
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
            return;
        const std::int32_t partial = AddUnchecked(_index, direction);
        const std::int32_t numerator = AddUnchecked(partial, Options().Count());
        const std::int32_t divisor = Options().Count();
        _index = RemainderInt32(numerator, divisor);
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
        context.DrawText(label, RowsPoint{4.0, (_interactive.Bounds().Height - label.Height) / 2.0});

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

    void ChoiceRow::Arrow(RowsDrawingContext& context, GuiRect area, bool pointsLeft, bool hot)
    {
        const double cx = area.X + area.Width / 2.0;
        const double cy = area.Y + area.Height / 2.0;
        constexpr double w = 4.5;
        constexpr double h = 6.0;
        std::array<RowsPoint, 3> points;
        if (pointsLeft)
        {
            points = {RowsPoint{cx + w, cy - h}, RowsPoint{cx - w, cy}, RowsPoint{cx + w, cy + h}};
        }
        else
        {
            points = {RowsPoint{cx - w, cy - h}, RowsPoint{cx + w, cy}, RowsPoint{cx - w, cy + h}};
        }
        RowsTriangleGeometry geometry(points, true, true);
        context.DrawGeometry(hot ? RowsBrush::Accent() : RowsBrush::TextDim(), std::nullopt, geometry);
    }

    ToggleRow::ToggleRow(RowsInteractiveControlAdapter& control,
        std::optional<std::u16string> label, bool on)
        : RowsControl(control), _interactive(control), _label(std::move(label)), _on(on)
    {
        _control.SetHeight(34.0);
        _interactive.SetFocusable(true);
        _interactive.SetHandCursor();
    }

    bool ToggleRow::On() const noexcept { return _on; }

    void ToggleRow::On(bool value)
    {
        if (_on != value)
        {
            _on = value;
            _control.InvalidateVisual();
            _changed.Invoke(this, RowsEventArgs::Empty);
        }
    }

    RowsEvent& ToggleRow::Changed() noexcept { return _changed; }
    const RowsEvent& ToggleRow::Changed() const noexcept { return _changed; }
    void ToggleRow::AddChanged(const RowsEventHandler& handler) { _changed.Add(handler); }
    void ToggleRow::RemoveChanged(const RowsEventHandler& handler) { _changed.Remove(handler); }

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
        context.DrawText(label, RowsPoint{4.0, (_interactive.Bounds().Height - label.Height) / 2.0});

        constexpr double w = 40.0;
        constexpr double h = 20.0;
        const GuiRect track{_interactive.Bounds().Width - w - 4.0,
            (_interactive.Bounds().Height - h) / 2.0, w, h};
        RowsBrush trackBrush = RowsBrush::Solid(_on ? GuiTheme::Accent : GuiTheme::Edge);
        context.DrawRectangle(std::move(trackBrush), std::nullopt, RowsRoundedRect{track, h / 2.0});
        const double knob = _on ? track.X + track.Width - h / 2.0 : track.X + h / 2.0;
        RowsBrush knobBrush = RowsBrush::Solid(_on ? GuiTheme::Ink : GuiTheme::TextDim);
        context.DrawEllipse(std::move(knobBrush), std::nullopt,
            RowsPoint{knob, track.Y + h / 2.0}, h / 2.0 - 3.0, h / 2.0 - 3.0);
    }

    FieldRowTextBox::FieldRowTextBox(FieldRowAdapter& adapter, FieldRowAdapter::ElementHandle handle) noexcept
        : _adapter(adapter), _handle(handle) {}
    FieldRowAdapter::ElementHandle FieldRowTextBox::Native() const noexcept { return _handle; }
    std::optional<std::u16string> FieldRowTextBox::Text() const { return _adapter.GetTextBoxText(_handle); }
    void FieldRowTextBox::Text(std::optional<std::u16string_view> value) { _adapter.SetTextBoxText(_handle, value); }
    void FieldRowTextBox::Watermark(std::optional<std::u16string_view> value) { _adapter.SetTextBoxWatermark(_handle, value); }
    void FieldRowTextBox::AddLostFocus(const RowsEventHandler& handler) { _adapter.AddTextBoxLostFocus(_handle, handler); }
    void FieldRowTextBox::RemoveLostFocus(const RowsEventHandler& handler) { _adapter.RemoveTextBoxLostFocus(_handle, handler); }

    FieldRow::FieldRow(FieldRowAdapter& adapter, std::optional<std::u16string> label,
        std::optional<std::u16string> value, double boxWidth)
        : RowsControl(adapter), _fieldAdapter(adapter)
    {
        _control.SetHeight(36.0);
        const FieldRowAdapter::ElementHandle caption = _fieldAdapter.CreateTextBlock();
        _fieldAdapter.SetTextBlockText(caption, label ? std::optional<std::u16string_view>(*label) : std::nullopt);
        _fieldAdapter.SetTextBlockFontFamily(caption, GuiTheme::Display);
        _fieldAdapter.SetTextBlockFontSize(caption, 13.0);
        _fieldAdapter.SetTextBlockForeground(caption, RowsBrush::TextDim());
        _fieldAdapter.SetTextBlockVerticalAlignment(caption, RowsVerticalAlignment::Center);
        _fieldAdapter.SetTextBlockHorizontalAlignment(caption, RowsHorizontalAlignment::Left);
        _fieldAdapter.SetTextBlockMargin(caption, RowsThickness{4.0, 0.0, 0.0, 0.0});

        const FieldRowAdapter::ElementHandle box = _fieldAdapter.CreateTextBox();
        _fieldAdapter.SetTextBoxText(box, value ? std::optional<std::u16string_view>(*value) : std::nullopt);
        _fieldAdapter.SetTextBoxWidth(box, boxWidth);
        _fieldAdapter.SetTextBoxFontFamily(box, GuiTheme::Display);
        _fieldAdapter.SetTextBoxFontSize(box, 13.0);
        _fieldAdapter.SetTextBoxCornerRadius(box, 4.0);
        _fieldAdapter.SetTextBoxPadding(box, RowsThickness{8.0, 4.0, 8.0, 4.0});
        _fieldAdapter.SetTextBoxVerticalAlignment(box, RowsVerticalAlignment::Center);
        _fieldAdapter.SetTextBoxHorizontalAlignment(box, RowsHorizontalAlignment::Right);
        _box.emplace(_fieldAdapter, box);

        _fieldAdapter.AddChild(caption);
        _fieldAdapter.AddChild(box);
    }

    FieldRowTextBox& FieldRow::Box() noexcept { return *_box; }
    const FieldRowTextBox& FieldRow::Box() const noexcept { return *_box; }
    std::u16string FieldRow::Value() const
    {
        std::optional<std::u16string> text = _box->Text();
        return text.has_value() ? std::move(*text) : std::u16string{};
    }
    void FieldRow::Value(std::optional<std::u16string_view> value) { _box->Text(value); }

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

    std::optional<std::u16string> Note::Text() const { return _noteAdapter.GetText(); }
    void Note::Text(std::optional<std::u16string_view> value) { _noteAdapter.SetText(value); }
    std::optional<RowsBrush> Note::Foreground() const { return _noteAdapter.GetForeground(); }
    void Note::Foreground(std::optional<RowsBrush> value) { _noteAdapter.SetForeground(std::move(value)); }
    void Note::Foreground(RowsBrush value) { Foreground(std::optional<RowsBrush>(std::move(value))); }
}
