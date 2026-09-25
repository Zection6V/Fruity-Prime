#include "KeyRow.hpp"

#include "../../InputSettings.hpp"
#include "../../../NativeRuntime/System/Encoding.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <utility>

using ::MphRead::NativeRuntime::MathMax;
using ::MphRead::NativeRuntime::Utf8ToUtf16;

namespace
{
    using namespace MphRead::Mods;
    using namespace MphRead::Mods::Launcher::Gui;

    constexpr InputButtonType ButtonTypeKey = static_cast<InputButtonType>(0);
    constexpr InputButtonType ButtonTypeMouse = static_cast<InputButtonType>(1);
    constexpr InputButtonType ButtonTypeScrollUp = static_cast<InputButtonType>(2);
    constexpr InputButtonType ButtonTypeScrollDown = static_cast<InputButtonType>(3);

    constexpr KeyRowGlfwKey KeyUnknown = static_cast<KeyRowGlfwKey>(-1);
    constexpr InputMouseButton MouseLeft = static_cast<InputMouseButton>(0);
    constexpr InputMouseButton MouseRight = static_cast<InputMouseButton>(1);
    constexpr InputMouseButton MouseMiddle = static_cast<InputMouseButton>(2);
    constexpr InputMouseButton MouseButton4 = static_cast<InputMouseButton>(3);
    constexpr InputMouseButton MouseButton5 = static_cast<InputMouseButton>(4);

    [[nodiscard]] bool Contains(GuiRect rect, KeyRowPoint point) noexcept
    {
        return point.X >= rect.X
            && point.X <= rect.X + rect.Width
            && point.Y >= rect.Y
            && point.Y <= rect.Y + rect.Height;
    }

    [[nodiscard]] constexpr KeyRowGlfwKey Glfw(std::int32_t value) noexcept
    {
        return static_cast<KeyRowGlfwKey>(value);
    }
}

namespace MphRead::Mods::Launcher::Gui
{
    KeyRowNullReferenceException::KeyRowNullReferenceException()
        : std::runtime_error("Object reference not set to an instance of an object.")
    {
    }

    KeyRowBrush::KeyRowBrush(KeyRowBrushKind kind, GuiBrush* shared,
        std::shared_ptr<GuiBrush> owned) noexcept
        : _kind(kind), _shared(shared), _owned(std::move(owned))
    {
    }

    KeyRowBrush KeyRowBrush::Transparent() noexcept
    {
        return KeyRowBrush(KeyRowBrushKind::Transparent, nullptr, {});
    }

    KeyRowBrush KeyRowBrush::Reference(GuiBrush& brush) noexcept
    {
        return KeyRowBrush(KeyRowBrushKind::Shared, &brush, {});
    }

    KeyRowBrush KeyRowBrush::Solid(GuiColor color)
    {
        auto brush = std::make_shared<GuiBrush>(color);
        return KeyRowBrush(KeyRowBrushKind::SolidColor, nullptr, std::move(brush));
    }

    KeyRowBrushKind KeyRowBrush::Kind() const noexcept
    {
        return _kind;
    }

    const GuiBrush* KeyRowBrush::Brush() const noexcept
    {
        return _owned ? _owned.get() : _shared;
    }

    const void* KeyRowBrush::Identity() const noexcept
    {
        return static_cast<const void*>(Brush());
    }

    KeyRowPen::KeyRowPen(KeyRowBrush brush, double thickness) noexcept
        : _brush(std::move(brush)), _thickness(thickness)
    {
    }

    const KeyRowBrush& KeyRowPen::Brush() const noexcept
    {
        return _brush;
    }

    double KeyRowPen::Thickness() const noexcept
    {
        return _thickness;
    }

    const KeyRowEventArgs KeyRowEventArgs::Empty{};

    KeyRowEventHandler::KeyRowEventHandler(
        std::shared_ptr<void> target, Callback function)
    {
        if (function != nullptr)
        {
            auto list = std::make_shared<std::vector<Invocation>>();
            list->push_back(Invocation{std::move(target), function});
            _invocations = std::move(list);
        }
    }

    KeyRowEventHandler::KeyRowEventHandler(
        std::shared_ptr<const std::vector<Invocation>> invocations) noexcept
        : _invocations(std::move(invocations))
    {
    }

    KeyRowEventHandler KeyRowEventHandler::Combine(
        const KeyRowEventHandler& left, const KeyRowEventHandler& right)
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
        return KeyRowEventHandler(std::move(list));
    }

    bool KeyRowEventHandler::IsNull() const noexcept
    {
        return !_invocations || _invocations->empty();
    }

    bool operator==(
        const KeyRowEventHandler& left, const KeyRowEventHandler& right) noexcept
    {
        if (left.IsNull() || right.IsNull())
        {
            return left.IsNull() == right.IsNull();
        }
        const auto& a = *left._invocations;
        const auto& b = *right._invocations;
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
    }

    void KeyRowEvent::Add(const KeyRowEventHandler& handler)
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
                next->insert(next->end(), handler._invocations->begin(),
                    handler._invocations->end());
                desired = std::move(next);
            }
            if (_handlers.compare_exchange_weak(current, desired))
            {
                return;
            }
        }
    }

    void KeyRowEvent::Remove(const KeyRowEventHandler& handler)
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

    void KeyRowEvent::Invoke(void* sender, const KeyRowEventArgs& args) const
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

    KeyRowGetHandler::KeyRowGetHandler(
        std::shared_ptr<void> target, Callback function)
    {
        if (function != nullptr)
        {
            auto list = std::make_shared<std::vector<Invocation>>();
            list->push_back(Invocation{std::move(target), function});
            _invocations = std::move(list);
        }
    }

    KeyRowGetHandler::KeyRowGetHandler(
        std::shared_ptr<const std::vector<Invocation>> invocations) noexcept
        : _invocations(std::move(invocations))
    {
    }

    KeyRowGetHandler KeyRowGetHandler::Combine(
        const KeyRowGetHandler& left, const KeyRowGetHandler& right)
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
        return KeyRowGetHandler(std::move(list));
    }

    bool KeyRowGetHandler::IsNull() const noexcept
    {
        return !_invocations || _invocations->empty();
    }

    KeyRowGlfwKey KeyRowGetHandler::Invoke() const
    {
        if (IsNull())
        {
            throw KeyRowNullReferenceException();
        }

        KeyRowGlfwKey result = KeyUnknown;
        for (const Invocation& invocation : *_invocations)
        {
            result = invocation.Function(invocation.Target.get());
        }
        return result;
    }

    KeyRowSetHandler::KeyRowSetHandler(
        std::shared_ptr<void> target, Callback function)
    {
        if (function != nullptr)
        {
            auto list = std::make_shared<std::vector<Invocation>>();
            list->push_back(Invocation{std::move(target), function});
            _invocations = std::move(list);
        }
    }

    KeyRowSetHandler::KeyRowSetHandler(
        std::shared_ptr<const std::vector<Invocation>> invocations) noexcept
        : _invocations(std::move(invocations))
    {
    }

    KeyRowSetHandler KeyRowSetHandler::Combine(
        const KeyRowSetHandler& left, const KeyRowSetHandler& right)
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
        return KeyRowSetHandler(std::move(list));
    }

    bool KeyRowSetHandler::IsNull() const noexcept
    {
        return !_invocations || _invocations->empty();
    }

    void KeyRowSetHandler::Invoke(KeyRowGlfwKey key) const
    {
        if (IsNull())
        {
            throw KeyRowNullReferenceException();
        }

        for (const Invocation& invocation : *_invocations)
        {
            invocation.Function(invocation.Target.get(), key);
        }
    }

    KeyRowDrawingContext::KeyRowDrawingContext() noexcept
        : TrackedTextAdapter(TrackedTextBrush{&GuiTheme::TextBrush})
    {
    }

    KeyRow::KeyRow(KeyRowControlAdapter& control,
        const ::MphRead::Mods::InputBindingProperty* property,
        double labelWidth)
        : _control(control)
    {
        if (property != nullptr)
        {
            _property = std::make_shared<::MphRead::Mods::InputBindingProperty>(*property);
        }
        _labelWidth = labelWidth;
        Height(32.0);
        _control.SetFocusable(true);
        _control.SetHandCursor();
    }

    KeyRow::KeyRow(KeyRowControlAdapter& control,
        std::optional<std::u16string> label,
        KeyRowGetHandler get, KeyRowSetHandler set,
        double labelWidth)
        : _control(control)
    {
        _label = std::move(label);
        _get = std::move(get);
        _set = std::move(set);
        _labelWidth = labelWidth;
        Height(32.0);
        _control.SetFocusable(true);
        _control.SetHandCursor();
    }

    double KeyRow::Height() const
    {
        return _control.GetHeight();
    }

    void KeyRow::Height(double value)
    {
        _control.SetHeight(value);
    }

    void KeyRow::InvalidateVisual()
    {
        _control.InvalidateVisual();
    }

    void KeyRow::AddRebound(const KeyRowEventHandler& handler)
    {
        _rebound.Add(handler);
    }

    void KeyRow::RemoveRebound(const KeyRowEventHandler& handler)
    {
        _rebound.Remove(handler);
    }

    GuiRect KeyRow::Box() const
    {
        const double width = MathMax(60.0,
            _control.Bounds().Width - _labelWidth - 4.0);
        const double height = _control.Bounds().Height - 4.0;
        return GuiRect{_labelWidth, 2.0, width, height};
    }

    const ::MphRead::Mods::InputBindingProperty& KeyRow::RequireProperty() const
    {
        if (!_property)
        {
            throw KeyRowNullReferenceException();
        }
        return *_property;
    }

    void KeyRow::OnPointerPressed(KeyRowPointerPressedEventArgs& e)
    {
        _control.Focus();
        const KeyRowPointerUpdateKind updateKind = _control.GetPointerUpdateKind(e);
        if (!_listening)
        {
            const GuiRect box = Box();
            const KeyRowPoint position = _control.GetPosition(e);
            if (Contains(box, position))
            {
                _listening = true;
                _control.InvalidateVisual();
            }
            e.Handled = true;
            _control.BaseOnPointerPressed(e);
            return;
        }

        std::optional<InputMouseButton> button;
        switch (updateKind)
        {
        case KeyRowPointerUpdateKind::LeftButtonPressed:
            button = MouseLeft;
            break;
        case KeyRowPointerUpdateKind::RightButtonPressed:
            button = MouseRight;
            break;
        case KeyRowPointerUpdateKind::MiddleButtonPressed:
            button = MouseMiddle;
            break;
        case KeyRowPointerUpdateKind::XButton1Pressed:
            button = MouseButton4;
            break;
        case KeyRowPointerUpdateKind::XButton2Pressed:
            button = MouseButton5;
            break;
        default:
            break;
        }

        if (button.has_value() && _property)
        {
            InputSettings::Rebind(*_property, ButtonTypeMouse,
                static_cast<InputKey>(KeyUnknown), *button);
            Done();
        }
        e.Handled = true;
        _control.BaseOnPointerPressed(e);
    }

    void KeyRow::OnPointerWheelChanged(KeyRowPointerWheelEventArgs& e)
    {
        if (_listening && e.DeltaY != 0.0 && _property)
        {
            InputSettings::Rebind(*_property,
                e.DeltaY > 0.0 ? ButtonTypeScrollUp : ButtonTypeScrollDown,
                static_cast<InputKey>(KeyUnknown), MouseLeft);
            Done();
            e.Handled = true;
        }
        _control.BaseOnPointerWheelChanged(e);
    }

    void KeyRow::OnPointerEntered(KeyRowPointerEventArgs& e)
    {
        _hot = true;
        _control.InvalidateVisual();
        _control.BaseOnPointerEntered(e);
    }

    void KeyRow::OnPointerExited(KeyRowPointerEventArgs& e)
    {
        _hot = false;
        _control.InvalidateVisual();
        _control.BaseOnPointerExited(e);
    }

    void KeyRow::OnKeyDown(KeyRowKeyEventArgs& e)
    {
        if (!_listening)
        {
            if (e.Key == KeyRowKey::Enter || e.Key == KeyRowKey::Space)
            {
                _listening = true;
                _control.InvalidateVisual();
                e.Handled = true;
            }
            _control.BaseOnKeyDown(e);
            return;
        }

        e.Handled = true;
        if (e.Key == KeyRowKey::Escape)
        {
            Done();
            return;
        }
        if (e.Key == KeyRowKey::Back || e.Key == KeyRowKey::Delete)
        {
            Assign(KeyUnknown);
            Done();
            return;
        }
        const std::optional<KeyRowGlfwKey> key = Translate(e.Key);
        if (key.has_value())
        {
            Assign(*key);
            Done();
        }
    }

    void KeyRow::Assign(KeyRowGlfwKey key)
    {
        if (!_set.IsNull())
        {
            _set.Invoke(key);
            return;
        }
        InputSettings::Rebind(RequireProperty(), ButtonTypeKey,
            static_cast<InputKey>(key), MouseLeft);
    }

    void KeyRow::Done()
    {
        _listening = false;
        _control.InvalidateVisual();
        _rebound.Invoke(this, KeyRowEventArgs::Empty);
    }

    void KeyRow::OnLostFocus(KeyRowRoutedEventArgs& e)
    {
        _listening = false;
        _control.InvalidateVisual();
        _control.BaseOnLostFocus(e);
    }

    void KeyRow::OnGotFocus(KeyRowGotFocusEventArgs& e)
    {
        _control.InvalidateVisual();
        _control.BaseOnGotFocus(e);
    }

    std::optional<KeyRowGlfwKey> KeyRow::Translate(KeyRowKey key) noexcept
    {
        if (key >= KeyRowKey::A && key <= KeyRowKey::Z)
        {
            return Glfw(65 + static_cast<std::int32_t>(key)
                - static_cast<std::int32_t>(KeyRowKey::A));
        }
        if (key >= KeyRowKey::D0 && key <= KeyRowKey::D9)
        {
            return Glfw(48 + static_cast<std::int32_t>(key)
                - static_cast<std::int32_t>(KeyRowKey::D0));
        }
        if (key >= KeyRowKey::NumPad0 && key <= KeyRowKey::NumPad9)
        {
            return Glfw(320 + static_cast<std::int32_t>(key)
                - static_cast<std::int32_t>(KeyRowKey::NumPad0));
        }
        if (key >= KeyRowKey::F1 && key <= KeyRowKey::F12)
        {
            return Glfw(290 + static_cast<std::int32_t>(key)
                - static_cast<std::int32_t>(KeyRowKey::F1));
        }

        switch (key)
        {
        case KeyRowKey::Space: return Glfw(32);
        case KeyRowKey::Tab: return Glfw(258);
        case KeyRowKey::Enter: return Glfw(257);
        case KeyRowKey::LeftShift: return Glfw(340);
        case KeyRowKey::RightShift: return Glfw(344);
        case KeyRowKey::LeftCtrl: return Glfw(341);
        case KeyRowKey::RightCtrl: return Glfw(345);
        case KeyRowKey::LeftAlt: return Glfw(342);
        case KeyRowKey::RightAlt: return Glfw(346);
        case KeyRowKey::Left: return Glfw(263);
        case KeyRowKey::Right: return Glfw(262);
        case KeyRowKey::Up: return Glfw(265);
        case KeyRowKey::Down: return Glfw(264);
        case KeyRowKey::Insert: return Glfw(260);
        case KeyRowKey::Home: return Glfw(268);
        case KeyRowKey::End: return Glfw(269);
        case KeyRowKey::PageUp: return Glfw(266);
        case KeyRowKey::PageDown: return Glfw(267);
        case KeyRowKey::CapsLock: return Glfw(280);
        case KeyRowKey::OemMinus: return Glfw(45);
        case KeyRowKey::OemPlus: return Glfw(61);
        case KeyRowKey::OemOpenBrackets: return Glfw(91);
        case KeyRowKey::OemCloseBrackets: return Glfw(93);
        case KeyRowKey::OemSemicolon: return Glfw(59);
        case KeyRowKey::OemQuotes: return Glfw(39);
        case KeyRowKey::OemComma: return Glfw(44);
        case KeyRowKey::OemPeriod: return Glfw(46);
        case KeyRowKey::OemQuestion: return Glfw(47);
        case KeyRowKey::OemBackslash:
        case KeyRowKey::OemPipe:
            return Glfw(92);
        case KeyRowKey::OemTilde: return Glfw(96);
        case KeyRowKey::Add: return Glfw(334);
        case KeyRowKey::Subtract: return Glfw(333);
        case KeyRowKey::Multiply: return Glfw(332);
        case KeyRowKey::Divide: return Glfw(331);
        default:
            return std::nullopt;
        }
    }

    void KeyRow::Render(KeyRowDrawingContext& context)
    {
        const double fillWidth = _control.Bounds().Width;
        const double fillHeight = _control.Bounds().Height;
        context.FillRectangle(KeyRowBrush::Transparent(),
            GuiRect{0.0, 0.0, fillWidth, fillHeight});

        const std::u16string labelText = _label.has_value()
            ? *_label
            : Utf8ToUtf16(InputSettings::ActionName(RequireProperty()));
        const TrackedTextFormattedText label = TrackedText::Make(
            context, labelText, 12.0, true,
            TrackedTextBrush{&GuiTheme::TextBrush});
        context.DrawText(label, TrackedTextPoint{
            4.0, (_control.Bounds().Height - label.Height) / 2.0
        });

        const GuiRect box = Box();
        GuiColor edgeColor;
        if (_listening)
        {
            edgeColor = GuiTheme::Warm;
        }
        else if (_control.IsFocused() || _hot)
        {
            edgeColor = GuiTheme::Accent;
        }
        else
        {
            edgeColor = GuiTheme::Edge;
        }
        context.DrawRectangle(KeyRowBrush::Reference(GuiTheme::PanelLightBrush),
            KeyRowPen(KeyRowBrush::Solid(edgeColor), 1.0),
            KeyRowRoundedRect{box, 4.0});

        std::u16string text;
        if (_listening)
        {
            text = !_get.IsNull()
                ? std::u16string(u"press a key")
                : std::u16string(u"press a key, a mouse button or the wheel");
        }
        else if (!_get.IsNull())
        {
            if (_get.Invoke() == KeyUnknown)
            {
                text = u"none";
            }
            else
            {
                text = Utf8ToUtf16(InputSettings::KeyName(
                    static_cast<InputKey>(_get.Invoke())));
            }
        }
        else
        {
            text = Utf8ToUtf16(InputSettings::Describe(
                InputSettings::Bind(RequireProperty())));
        }

        const GuiColor valueColor = _listening ? GuiTheme::Warm : GuiTheme::Text;
        const KeyRowBrush valueBrush = KeyRowBrush::Solid(valueColor);
        TrackedTextFormattedText value = TrackedText::Make(
            context, text, 12.0, true,
            TrackedTextBrush{valueBrush.Brush()});
        context.SetFormattedTextMaxTextWidth(value,
            MathMax(20.0, box.Width - 12.0));
        context.SetFormattedTextMaxTextHeight(value, box.Height);
        context.SetFormattedTextTrimming(
            value, KeyRowTextTrimming::CharacterEllipsis);
        context.DrawText(value, TrackedTextPoint{
            box.X + (box.Width - value.Width) / 2.0,
            box.Y + (box.Height - value.Height) / 2.0
        });
    }
}
