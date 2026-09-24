#include "PadRow.hpp"

#include "../../Input/GamepadDesktop.hpp"
#include "../../Input/GamepadInput.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

using ::MphRead::NativeRuntime::MathMax;

namespace
{
    using MphRead::Mods::Input::GamepadButtons;

    [[nodiscard]] GamepadButtons BitAnd(
        GamepadButtons left, GamepadButtons right) noexcept
    {
        return static_cast<GamepadButtons>(
            static_cast<std::int32_t>(left) & static_cast<std::int32_t>(right));
    }

    [[nodiscard]] GamepadButtons AndNot(
        GamepadButtons left, GamepadButtons right) noexcept
    {
        return static_cast<GamepadButtons>(
            static_cast<std::int32_t>(left) & ~static_cast<std::int32_t>(right));
    }

    [[nodiscard]] std::u16string ToUtf16(std::string_view text)
    {
        std::u16string result;
        result.reserve(text.size());
        for (unsigned char byte : text)
        {
            result.push_back(static_cast<char16_t>(byte));
        }
        return result;
    }

    constexpr std::array<GamepadButtons, 17> GamepadButtonValues{
        GamepadButtons::None,
        GamepadButtons::A,
        GamepadButtons::B,
        GamepadButtons::X,
        GamepadButtons::Y,
        GamepadButtons::LeftBumper,
        GamepadButtons::RightBumper,
        GamepadButtons::Back,
        GamepadButtons::Start,
        GamepadButtons::LeftThumb,
        GamepadButtons::RightThumb,
        GamepadButtons::DpadUp,
        GamepadButtons::DpadRight,
        GamepadButtons::DpadDown,
        GamepadButtons::DpadLeft,
        GamepadButtons::LeftTrigger,
        GamepadButtons::RightTrigger
    };
}

namespace MphRead::Mods::Launcher::Gui
{
    using Mods::Input::GamepadButtons;
    using Mods::Input::GamepadDesktop;
    using Mods::Input::GamepadInput;
    using Mods::Input::PadBindings;

    PadRowBrush::PadRowBrush(PadRowBrushKind kind, const GuiBrush* shared,
        std::shared_ptr<GuiBrush> owned) noexcept
        : _kind(kind), _shared(shared), _owned(std::move(owned))
    {
    }

    PadRowBrush PadRowBrush::Transparent() noexcept
    {
        return PadRowBrush(PadRowBrushKind::Transparent, nullptr, {});
    }

    PadRowBrush PadRowBrush::Reference(const GuiBrush& brush) noexcept
    {
        return PadRowBrush(PadRowBrushKind::Reference, &brush, {});
    }

    PadRowBrush PadRowBrush::Solid(GuiColor color)
    {
        auto brush = std::make_shared<GuiBrush>(color);
        const GuiBrush* pointer = brush.get();
        return PadRowBrush(PadRowBrushKind::SolidColor, pointer, std::move(brush));
    }

    PadRowBrushKind PadRowBrush::Kind() const noexcept
    {
        return _kind;
    }

    const GuiBrush* PadRowBrush::Brush() const noexcept
    {
        return _shared;
    }

    const void* PadRowBrush::Identity() const noexcept
    {
        return _shared;
    }

    PadRowPen::PadRowPen(PadRowBrush brush, double thickness)
        : _brush(std::move(brush)), _thickness(thickness)
    {
    }

    const PadRowBrush& PadRowPen::Brush() const noexcept
    {
        return _brush;
    }

    double PadRowPen::Thickness() const noexcept
    {
        return _thickness;
    }

    const PadRowEventArgs PadRowEventArgs::Empty{};

    PadRowEventHandler::PadRowEventHandler(
        std::shared_ptr<void> target, Callback function)
    {
        if (function != nullptr)
        {
            auto list = std::make_shared<std::vector<Invocation>>();
            list->push_back(Invocation{std::move(target), function});
            _invocations = std::move(list);
        }
    }

    PadRowEventHandler::PadRowEventHandler(
        std::shared_ptr<const std::vector<Invocation>> invocations) noexcept
        : _invocations(std::move(invocations))
    {
    }

    PadRowEventHandler PadRowEventHandler::Combine(
        const PadRowEventHandler& left, const PadRowEventHandler& right)
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
        return PadRowEventHandler(std::move(list));
    }

    bool PadRowEventHandler::IsNull() const noexcept
    {
        return !_invocations || _invocations->empty();
    }

    bool operator==(
        const PadRowEventHandler& left, const PadRowEventHandler& right) noexcept
    {
        if (left.IsNull() || right.IsNull())
        {
            return left.IsNull() == right.IsNull();
        }
        const auto& a = *left._invocations;
        const auto& b = *right._invocations;
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
    }

    void PadRowEvent::Add(const PadRowEventHandler& handler)
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

    void PadRowEvent::Remove(const PadRowEventHandler& handler)
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

    void PadRowEvent::Invoke(void* sender, const PadRowEventArgs& args) const
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

    PadRowDrawingContext::PadRowDrawingContext() noexcept
        : TrackedTextAdapter(TrackedTextBrush{&GuiTheme::TextBrush})
    {
    }

    PadRow::PadRow(PadRowControlAdapter& control,
        Mods::Input::PadAction action, double labelWidth)
        : _control(control), _action(action), _labelWidth(labelWidth)
    {
        Height(32.0);
        _control.SetFocusable(true);
        _control.SetHandCursor();
    }

    double PadRow::Height() const
    {
        return _control.GetHeight();
    }

    void PadRow::Height(double value)
    {
        _control.SetHeight(value);
    }

    void PadRow::InvalidateVisual()
    {
        _control.InvalidateVisual();
    }

    void PadRow::AddRebound(const PadRowEventHandler& handler)
    {
        _rebound.Add(handler);
    }

    void PadRow::RemoveRebound(const PadRowEventHandler& handler)
    {
        _rebound.Remove(handler);
    }

    GuiRect PadRow::Box() const
    {
        const double width = _control.Bounds().Width;
        const double height = _control.Bounds().Height;
        return GuiRect{
            _labelWidth,
            2.0,
            MathMax(60.0, width - _labelWidth - 4.0),
            height - 4.0
        };
    }

    void PadRow::OnPointerPressed(PadRowPointerPressedEventArgs& e)
    {
        _control.Focus();
        if (!_listening)
        {
            const GuiRect box = Box();
            const PadRowPoint position = _control.GetPosition(e);
            if (_control.RectContains(box, position))
            {
                Listen();
            }
        }
        e.Handled = true;
        _control.BaseOnPointerPressed(e);
    }

    void PadRow::OnKeyDown(PadRowKeyEventArgs& e)
    {
        if (!_listening)
        {
            if (e.Key == PadRowKey::Enter || e.Key == PadRowKey::Space)
            {
                Listen();
                e.Handled = true;
            }
            _control.BaseOnKeyDown(e);
            return;
        }

        e.Handled = true;
        if (e.Key == PadRowKey::Escape)
        {
            Done();
            return;
        }
        if (e.Key == PadRowKey::Back || e.Key == PadRowKey::Delete)
        {
            PadBindings::Set(_action, GamepadButtons::None);
            Done();
        }
    }

    void PadRow::Listen()
    {
        _listening = true;
        GamepadDesktop::PollForMenu();
        _baseline = GamepadInput::State.Buttons;
        if (_watch)
        {
            _watch->Stop();
        }
        std::shared_ptr<PadRowDispatcherTimer> watch =
            _control.CreateDispatcherTimer(std::chrono::milliseconds(30),
                PadRowDispatcherPriority::Input, [this]() { Check(); });
        // Avalonia's interval/priority/callback constructor starts the timer
        // before it returns, so preserve that start before assigning _watch.
        watch->Start();
        _watch = std::move(watch);
        // The C# source then calls Start() again; Avalonia makes it idempotent.
        _watch->Start();
        _control.InvalidateVisual();
    }

    void PadRow::Check()
    {
        if (!_listening)
        {
            return;
        }

        GamepadDesktop::PollForMenu();
        const GamepadButtons pressed = AndNot(
            GamepadInput::State.Buttons, _baseline);
        _baseline = BitAnd(_baseline, GamepadInput::State.Buttons);
        if (pressed == GamepadButtons::None)
        {
            return;
        }

        for (const GamepadButtons button : GamepadButtonValues)
        {
            if (button != GamepadButtons::None
                && BitAnd(pressed, button) == button)
            {
                PadBindings::Set(_action, button);
                Done();
                return;
            }
        }
    }

    void PadRow::Done()
    {
        _listening = false;
        if (_watch)
        {
            _watch->Stop();
        }
        _watch.reset();
        _control.InvalidateVisual();
        _rebound.Invoke(this, PadRowEventArgs::Empty);
    }

    void PadRow::OnPointerEntered(PadRowPointerEventArgs& e)
    {
        _hot = true;
        _control.InvalidateVisual();
        _control.BaseOnPointerEntered(e);
    }

    void PadRow::OnPointerExited(PadRowPointerEventArgs& e)
    {
        _hot = false;
        _control.InvalidateVisual();
        _control.BaseOnPointerExited(e);
    }

    void PadRow::OnLostFocus(PadRowRoutedEventArgs& e)
    {
        if (_listening)
        {
            Done();
        }
        _control.BaseOnLostFocus(e);
    }

    void PadRow::OnGotFocus(PadRowGotFocusEventArgs& e)
    {
        _control.InvalidateVisual();
        _control.BaseOnGotFocus(e);
    }

    void PadRow::OnDetachedFromVisualTree(PadRowVisualTreeAttachmentEventArgs& e)
    {
        if (_watch)
        {
            _watch->Stop();
        }
        _watch.reset();
        _listening = false;
        _control.BaseOnDetachedFromVisualTree(e);
    }

    void PadRow::Render(PadRowDrawingContext& context)
    {
        const double fillWidth = _control.Bounds().Width;
        const double fillHeight = _control.Bounds().Height;
        context.FillRectangle(PadRowBrush::Transparent(),
            GuiRect{0.0, 0.0, fillWidth, fillHeight});

        const std::u16string labelText = ToUtf16(PadBindings::Name(_action));
        const TrackedTextFormattedText label = TrackedText::Make(
            context, labelText, 12.0, true,
            TrackedTextBrush{&GuiTheme::TextBrush});
        const double labelBoundsHeight = _control.Bounds().Height;
        context.DrawText(label, TrackedTextPoint{
            4.0,
            (labelBoundsHeight - label.Height) / 2.0
        });

        const GuiRect box = Box();
        GuiColor borderColor;
        if (_listening)
        {
            borderColor = GuiTheme::Warm;
        }
        else if (_control.IsFocused() || _hot)
        {
            borderColor = GuiTheme::Accent;
        }
        else
        {
            borderColor = GuiTheme::Edge;
        }
        context.DrawRectangle(
            PadRowBrush::Reference(GuiTheme::PanelLightBrush),
            PadRowPen(PadRowBrush::Solid(borderColor), 1.0),
            PadRowRoundedRect{box, 4.0});

        const std::u16string text = _listening
            ? std::u16string(u"press a button on the pad")
            : ToUtf16(PadBindings::Describe(PadBindings::Get(_action)));
        const GuiColor valueColor = _listening ? GuiTheme::Warm : GuiTheme::Text;
        const PadRowBrush valueBrush = PadRowBrush::Solid(valueColor);
        TrackedTextFormattedText value = TrackedText::Make(
            context, text, 12.0, true,
            TrackedTextBrush{valueBrush.Brush()});
        context.SetFormattedTextMaxTextWidth(
            value, MathMax(20.0, box.Width - 12.0));
        context.SetFormattedTextMaxTextHeight(value, box.Height);
        context.SetFormattedTextTrimming(
            value, PadRowTextTrimming::CharacterEllipsis);
        context.DrawText(value, TrackedTextPoint{
            box.X + (box.Width - value.Width) / 2.0,
            box.Y + (box.Height - value.Height) / 2.0
        });
    }
}
