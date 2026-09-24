#include "SliderRow.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <system_error>
#include <utility>
#include "../../../NativeRuntime/System/IO.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

using ::MphRead::NativeRuntime::RoundToEven;

namespace
{
    using namespace MphRead::Mods::Launcher::Gui;

    [[nodiscard]] std::int32_t AddUnchecked(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t sum = static_cast<std::uint32_t>(left)
            + static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(sum);
    }

    [[nodiscard]] std::int32_t SubtractUnchecked(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t difference = static_cast<std::uint32_t>(left)
            - static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(difference);
    }

    [[nodiscard]] std::int32_t MathMax(std::int32_t left, std::int32_t right) noexcept
    {
        return left > right ? left : right;
    }

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

    [[nodiscard]] std::int32_t ClampInt32(
        std::int32_t value, std::int32_t min, std::int32_t max)
    {
        if (min > max)
        {
            throw SliderRowArgumentException(min, max);
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

    [[nodiscard]] double ClampDouble(double value, double min, double max)
    {
        if (min > max)
        {
            throw SliderRowArgumentException();
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

    [[nodiscard]] std::int32_t DoubleToInt32Unchecked(double value) noexcept
    {
        if (std::isnan(value))
        {
            return 0;
        }
        if (value < static_cast<double>(std::numeric_limits<std::int32_t>::min()))
        {
            return std::numeric_limits<std::int32_t>::min();
        }
        if (value > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
        {
            return std::numeric_limits<std::int32_t>::max();
        }
        return static_cast<std::int32_t>(value);
    }
}

namespace MphRead::Mods::Launcher::Gui
{
    SliderRowNullReferenceException::SliderRowNullReferenceException()
        : std::runtime_error("Object reference not set to an instance of an object.")
    {
    }

    SliderRowArgumentException::SliderRowArgumentException()
        : std::invalid_argument("'min' cannot be greater than max.")
    {
    }

    SliderRowArgumentException::SliderRowArgumentException(
        std::int32_t min, std::int32_t max)
        : std::invalid_argument("'" + std::to_string(min)
            + "' cannot be greater than " + std::to_string(max) + ".")
    {
    }

    const SliderRowEventArgs SliderRowEventArgs::Empty{};

    SliderRowEventHandler::SliderRowEventHandler(
        std::shared_ptr<void> target, Callback function)
    {
        if (function != nullptr)
        {
            auto list = std::make_shared<std::vector<Invocation>>();
            list->push_back(Invocation{std::move(target), function});
            _invocations = std::move(list);
        }
    }

    SliderRowEventHandler::SliderRowEventHandler(
        std::shared_ptr<const std::vector<Invocation>> invocations) noexcept
        : _invocations(std::move(invocations))
    {
    }

    SliderRowEventHandler SliderRowEventHandler::Combine(
        const SliderRowEventHandler& left, const SliderRowEventHandler& right)
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
        return SliderRowEventHandler(std::move(list));
    }

    bool SliderRowEventHandler::IsNull() const noexcept
    {
        return !_invocations || _invocations->empty();
    }

    bool operator==(
        const SliderRowEventHandler& left, const SliderRowEventHandler& right) noexcept
    {
        if (left.IsNull() || right.IsNull())
        {
            return left.IsNull() == right.IsNull();
        }
        const auto& a = *left._invocations;
        const auto& b = *right._invocations;
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
    }

    void SliderRowEvent::Add(const SliderRowEventHandler& handler)
    {
        if (handler.IsNull())
        {
            return;
        }

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
                next->insert(next->end(),
                    handler._invocations->begin(), handler._invocations->end());
                desired = std::move(next);
            }
            if (_handlers.compare_exchange_weak(current, desired))
            {
                return;
            }
        }
    }

    void SliderRowEvent::Remove(const SliderRowEventHandler& handler)
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

    void SliderRowEvent::Invoke(void* sender, const SliderRowEventArgs& args) const
    {
        const std::shared_ptr<const InvocationList> handlers = _handlers.load();
        if (!handlers)
        {
            return;
        }
        for (const Invocation& handler : *handlers)
        {
            handler.Function(handler.Target.get(), sender, args);
        }
    }

    SliderRowDrawingContext::SliderRowDrawingContext() noexcept
        : TrackedTextAdapter(TrackedTextBrush{&GuiTheme::TextBrush})
    {
    }

    SliderRow::SliderRow(SliderRowControlAdapter& control,
        std::optional<std::u16string> label, std::int32_t value,
        FormatHandler format, double labelWidth,
        std::int32_t min, std::int32_t max, std::int32_t keyStep)
        : _control(control)
    {
        _label = std::move(label);
        _labelWidth = labelWidth;
        _min = min;
        _max = MathMax(AddUnchecked(min, 1), max);
        _keyStep = MathMax(1, keyStep);
        _value = ClampInt32(value, _min, _max);
        _format = format ? std::move(format) : FormatHandler(DefaultFormat);
        Height(34.0);
        _control.SetFocusable(true);
        _control.SetHandCursor();
    }

    double SliderRow::Height() const
    {
        return _control.GetHeight();
    }

    void SliderRow::Height(double value)
    {
        _control.SetHeight(value);
    }

    bool SliderRow::IsEnabled() const
    {
        return _control.GetIsEnabled();
    }

    void SliderRow::IsEnabled(bool value)
    {
        _control.SetIsEnabled(value);
    }

    std::int32_t SliderRow::Value() const noexcept
    {
        return _value;
    }

    void SliderRow::Value(std::int32_t value)
    {
        const std::int32_t clamped = ClampInt32(value, _min, _max);
        if (clamped != _value)
        {
            _value = clamped;
            _control.InvalidateVisual();
            _valueChanged.Invoke(this, SliderRowEventArgs::Empty);
        }
    }

    void SliderRow::AddValueChanged(const SliderRowEventHandler& handler)
    {
        _valueChanged.Add(handler);
    }

    void SliderRow::RemoveValueChanged(const SliderRowEventHandler& handler)
    {
        _valueChanged.Remove(handler);
    }

    void SliderRow::InvalidateVisual()
    {
        _control.InvalidateVisual();
    }

    const std::u16string& SliderRow::RequireLabel() const
    {
        if (!_label.has_value())
        {
            throw SliderRowNullReferenceException();
        }
        return *_label;
    }

    const std::u16string& SliderRow::RequireFormatted(
        const std::optional<std::u16string>& value)
    {
        if (!value.has_value())
        {
            throw SliderRowNullReferenceException();
        }
        return *value;
    }

    std::optional<std::u16string> SliderRow::DefaultFormat(std::int32_t value)
    {
        std::array<char, 16> buffer{};
        const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        std::u16string text;
        text.reserve(static_cast<std::size_t>(result.ptr - buffer.data()) + 1);
        for (const char* it = buffer.data(); it != result.ptr; ++it)
        {
            text.push_back(static_cast<char16_t>(*it));
        }
        text.push_back(u'%');
        return text;
    }

    GuiRect SliderRow::Track() const
    {
        return GuiRect{
            _labelWidth,
            _control.Bounds().Height / 2.0 - 2.0,
            MathMax(40.0, _control.Bounds().Width - _labelWidth - ValueGutter),
            4.0
        };
    }

    void SliderRow::SetFromPointer(double x)
    {
        const GuiRect track = Track();
        const double fraction = (x - track.X) / MathMax(1.0, track.Width);
        const double scaled = ClampDouble(fraction, 0.0, 1.0)
            * static_cast<double>(SubtractUnchecked(_max, _min));
        Value(AddUnchecked(_min, DoubleToInt32Unchecked(RoundToEven(scaled))));
    }

    void SliderRow::OnPointerPressed(SliderRowPointerEventArgs& e)
    {
        _control.Focus();
        const SliderRowPoint p = _control.GetPosition(e);
        if (p.X >= _labelWidth && _control.GetIsEnabled())
        {
            _dragging = true;
            _control.Capture(e.Pointer, this);
            SetFromPointer(p.X);
        }
        _control.BaseOnPointerPressed(e);
    }

    void SliderRow::OnPointerMoved(SliderRowPointerEventArgs& e)
    {
        const SliderRowPoint p = _control.GetPosition(e);
        const bool hot = p.X >= _labelWidth;
        if (hot != _hot)
        {
            _hot = hot;
            _control.InvalidateVisual();
        }
        if (_dragging)
        {
            SetFromPointer(p.X);
        }
        _control.BaseOnPointerMoved(e);
    }

    void SliderRow::OnPointerReleased(SliderRowPointerEventArgs& e)
    {
        _dragging = false;
        _control.Capture(e.Pointer, nullptr);
        _control.BaseOnPointerReleased(e);
    }

    void SliderRow::OnPointerExited(SliderRowPointerEventArgs& e)
    {
        _hot = false;
        _control.InvalidateVisual();
        _control.BaseOnPointerExited(e);
    }

    void SliderRow::OnKeyDown(SliderRowKeyEventArgs& e)
    {
        if (!_control.GetIsEnabled())
        {
            _control.BaseOnKeyDown(e);
            return;
        }
        if (e.Key == SliderRowKey::Left)
        {
            Value(SubtractUnchecked(Value(), _keyStep));
            e.Handled = true;
            return;
        }
        if (e.Key == SliderRowKey::Right)
        {
            Value(AddUnchecked(Value(), _keyStep));
            e.Handled = true;
            return;
        }
        _control.BaseOnKeyDown(e);
    }

    void SliderRow::OnGotFocus(SliderRowGotFocusEventArgs& e)
    {
        _control.InvalidateVisual();
        _control.BaseOnGotFocus(e);
    }

    void SliderRow::OnLostFocus(SliderRowRoutedEventArgs& e)
    {
        _control.InvalidateVisual();
        _control.BaseOnLostFocus(e);
    }

    void SliderRow::Render(SliderRowDrawingContext& context)
    {
        context.FillRectangle(SliderRowBrush::Transparent(),
            GuiRect{0.0, 0.0, _control.Bounds().Width, _control.Bounds().Height});

        GuiBrush dim{GuiColor::FromRgb(70, 76, 90)};
        const std::u16string upper = _control.ToUpperInvariant(RequireLabel());
        const bool labelEnabled = _control.GetIsEnabled();
        const double labelBoundsHeight = _control.Bounds().Height;
        const double labelLineHeight = TrackedText::LineHeight(context, 11.0);
        TrackedText::Draw(context, upper, 11.0,
            TrackedTextBrush{labelEnabled
                ? static_cast<const void*>(&GuiTheme::TextDimBrush)
                : static_cast<const void*>(&dim)},
            4.0, (labelBoundsHeight - labelLineHeight) / 2.0, 1.0);

        const GuiRect track = Track();
        context.FillRectangle(SliderRowBrush::From(GuiTheme::PanelLightBrush), track);
        const double filled = track.Width
            * (static_cast<double>(SubtractUnchecked(_value, _min))
                / static_cast<double>(SubtractUnchecked(_max, _min)));

        const GuiBrush* accent = &dim;
        std::unique_ptr<GuiBrush> ownedAccent;
        if (_control.GetIsEnabled())
        {
            const GuiColor color = (_control.IsFocused() || _hot)
                ? GuiTheme::Shade(GuiTheme::Accent, 0.15)
                : GuiTheme::Accent;
            ownedAccent = std::make_unique<GuiBrush>(color);
            accent = ownedAccent.get();
        }

        context.FillRectangle(SliderRowBrush::From(*accent),
            GuiRect{track.X, track.Y, filled, track.Height});
        context.DrawEllipse(SliderRowBrush::From(*accent), std::nullopt,
            SliderRowPoint{track.X + filled, track.Y + track.Height / 2.0},
            5.0, 5.0);

        const std::optional<std::u16string> formatted = _format(_value);
        const bool valueEnabled = _control.GetIsEnabled();
        const TrackedTextFormattedText value = TrackedText::Make(
            context, RequireFormatted(formatted), 12.0, true,
            TrackedTextBrush{valueEnabled
                ? static_cast<const void*>(&GuiTheme::TextBrush)
                : static_cast<const void*>(&dim)});
        context.DrawText(value, TrackedTextPoint{
            _control.Bounds().Width - 4.0 - value.Width,
            (_control.Bounds().Height - value.Height) / 2.0
        });
    }
}
