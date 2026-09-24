#include "HostInputRows.hpp"

#include "../Gui/Host.hpp"
#include "../Gui/Text.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <string>

namespace MphRead::NativeRuntime::Avalonia
{
    namespace
    {
        [[nodiscard]] std::u16string Upper(std::u16string_view text)
        {
            std::u16string result(text);
            for (char16_t& c : result)
            {
                if (c >= u'a' && c <= u'z')
                {
                    c = static_cast<char16_t>(c - u'a' + u'A');
                }
            }
            return result;
        }

        [[nodiscard]] Launcher::GuiColor ColorOfTracked(Launcher::TrackedTextBrush brush)
        {
            const auto* const value
                = static_cast<const Launcher::GuiBrush*>(brush.Native);
            return value != nullptr ? value->Color : Launcher::GuiTheme::Text;
        }

        [[nodiscard]] Launcher::GuiColor ColorOf(Launcher::SliderRowBrush brush)
        {
            return brush.Kind == Launcher::SliderRowBrushKind::Transparent
                    || brush.Brush == nullptr
                ? Launcher::GuiColor{0, 0, 0, 0}
                : brush.Brush->Color;
        }

        [[nodiscard]] Launcher::GuiColor ColorOf(const Launcher::KeyRowBrush& brush)
        {
            return brush.Kind() == Launcher::KeyRowBrushKind::Transparent
                    || brush.Brush() == nullptr
                ? Launcher::GuiColor{0, 0, 0, 0}
                : brush.Brush()->Color;
        }

        [[nodiscard]] Launcher::GuiColor ColorOf(const Launcher::PadRowBrush& brush)
        {
            return brush.Kind() == Launcher::PadRowBrushKind::Transparent
                    || brush.Brush() == nullptr
                ? Launcher::GuiColor{0, 0, 0, 0}
                : brush.Brush()->Color;
        }

        // GLFW's key codes, in the numbering Avalonia's Key uses, because the
        // key rows were written against that enum and translate it back.
        [[nodiscard]] Launcher::KeyRowKey KeyOf(std::int32_t code) noexcept
        {
            using Key = Launcher::KeyRowKey;
            if (code >= GLFW_KEY_A && code <= GLFW_KEY_Z)
            {
                return static_cast<Key>(static_cast<std::int32_t>(Key::A)
                    + (code - GLFW_KEY_A));
            }
            if (code >= GLFW_KEY_0 && code <= GLFW_KEY_9)
            {
                return static_cast<Key>(static_cast<std::int32_t>(Key::D0)
                    + (code - GLFW_KEY_0));
            }
            if (code >= GLFW_KEY_KP_0 && code <= GLFW_KEY_KP_9)
            {
                return static_cast<Key>(static_cast<std::int32_t>(Key::NumPad0)
                    + (code - GLFW_KEY_KP_0));
            }
            if (code >= GLFW_KEY_F1 && code <= GLFW_KEY_F12)
            {
                return static_cast<Key>(static_cast<std::int32_t>(Key::F1)
                    + (code - GLFW_KEY_F1));
            }
            switch (code)
            {
            case GLFW_KEY_SPACE: return Key::Space;
            case GLFW_KEY_TAB: return Key::Tab;
            case GLFW_KEY_ENTER:
            case GLFW_KEY_KP_ENTER: return Key::Enter;
            case GLFW_KEY_ESCAPE: return Key::Escape;
            case GLFW_KEY_BACKSPACE: return Key::Back;
            case GLFW_KEY_DELETE: return Key::Delete;
            case GLFW_KEY_LEFT_SHIFT: return Key::LeftShift;
            case GLFW_KEY_RIGHT_SHIFT: return Key::RightShift;
            case GLFW_KEY_LEFT_CONTROL: return Key::LeftCtrl;
            case GLFW_KEY_RIGHT_CONTROL: return Key::RightCtrl;
            case GLFW_KEY_LEFT_ALT: return Key::LeftAlt;
            case GLFW_KEY_RIGHT_ALT: return Key::RightAlt;
            case GLFW_KEY_LEFT: return Key::Left;
            case GLFW_KEY_RIGHT: return Key::Right;
            case GLFW_KEY_UP: return Key::Up;
            case GLFW_KEY_DOWN: return Key::Down;
            case GLFW_KEY_INSERT: return Key::Insert;
            case GLFW_KEY_HOME: return Key::Home;
            case GLFW_KEY_END: return Key::End;
            case GLFW_KEY_PAGE_UP: return Key::PageUp;
            case GLFW_KEY_PAGE_DOWN: return Key::PageDown;
            case GLFW_KEY_CAPS_LOCK: return Key::CapsLock;
            case GLFW_KEY_MINUS: return Key::OemMinus;
            case GLFW_KEY_EQUAL: return Key::OemPlus;
            case GLFW_KEY_LEFT_BRACKET: return Key::OemOpenBrackets;
            case GLFW_KEY_RIGHT_BRACKET: return Key::OemCloseBrackets;
            case GLFW_KEY_SEMICOLON: return Key::OemSemicolon;
            case GLFW_KEY_APOSTROPHE: return Key::OemQuotes;
            case GLFW_KEY_COMMA: return Key::OemComma;
            case GLFW_KEY_PERIOD: return Key::OemPeriod;
            case GLFW_KEY_SLASH: return Key::OemQuestion;
            case GLFW_KEY_WORLD_2: return Key::OemBackslash;
            case GLFW_KEY_BACKSLASH: return Key::OemPipe;
            case GLFW_KEY_GRAVE_ACCENT: return Key::OemTilde;
            case GLFW_KEY_KP_ADD: return Key::Add;
            case GLFW_KEY_KP_SUBTRACT: return Key::Subtract;
            case GLFW_KEY_KP_MULTIPLY: return Key::Multiply;
            case GLFW_KEY_KP_DIVIDE: return Key::Divide;
            default: return Key::Other;
            }
        }

        [[nodiscard]] Launcher::SliderRowKey SliderKeyOf(Toolkit::Key key) noexcept
        {
            switch (key)
            {
            case Toolkit::Key::Left: return Launcher::SliderRowKey::Left;
            case Toolkit::Key::Right: return Launcher::SliderRowKey::Right;
            default: return Launcher::SliderRowKey::Other;
            }
        }

        [[nodiscard]] Launcher::PadRowKey PadKeyOf(Toolkit::Key key) noexcept
        {
            switch (key)
            {
            case Toolkit::Key::Enter: return Launcher::PadRowKey::Enter;
            case Toolkit::Key::Space: return Launcher::PadRowKey::Space;
            case Toolkit::Key::Escape: return Launcher::PadRowKey::Escape;
            default: return Launcher::PadRowKey::Other;
            }
        }

        template <typename Context>
        class TextContext : public Context
        {
        public:
            explicit TextContext(const Surface& surface) noexcept
                : _surface(surface)
            {
            }

            Launcher::TrackedTextFormattedText CreateFormattedText(
                std::u16string_view text, Launcher::TrackedTextCulture culture,
                Launcher::TrackedTextFlowDirection flowDirection,
                Launcher::TrackedTextFace face, double fontSize,
                Launcher::TrackedTextBrush brush) override
            {
                (void)culture;
                (void)flowDirection;
                return MakeFormattedText(text,
                    face == Launcher::TrackedTextFace::FaceTrue, fontSize,
                    ColorOfTracked(brush));
            }

            void DrawText(const Launcher::TrackedTextFormattedText& text,
                Launcher::TrackedTextPoint point) override
            {
                FormattedRun* const run = RunOf(text.Native);
                if (run == nullptr)
                {
                    return;
                }
                _surface.Renderer().DrawTextAt(run->Text, _surface.Bounds().X + point.X,
                    _surface.Bounds().Y + point.Y, run->FontSize,
                    run->Bold ? Toolkit::FontWeight::Bold : Toolkit::FontWeight::Normal,
                    ToColor(run->Color));
            }

        protected:
            Surface _surface;
        };

        class SliderContext final
            : public TextContext<Launcher::SliderRowDrawingContext>
        {
        public:
            using TextContext::TextContext;

            void FillRectangle(
                Launcher::SliderRowBrush brush, Launcher::GuiRect rect) override
            {
                _surface.FillRect(rect, ColorOf(brush));
            }

            void DrawEllipse(Launcher::SliderRowBrush brush,
                std::optional<Launcher::SliderRowPen> pen,
                Launcher::SliderRowPoint center, double radiusX,
                double radiusY) override
            {
                (void)pen;
                _surface.DrawEllipse(
                    center.X, center.Y, radiusX, radiusY, ColorOf(brush));
            }
        };

        class KeyContext final : public TextContext<Launcher::KeyRowDrawingContext>
        {
        public:
            using TextContext::TextContext;

            void FillRectangle(
                Launcher::KeyRowBrush brush, Launcher::GuiRect rect) override
            {
                _surface.FillRect(rect, ColorOf(brush));
            }

            void DrawRectangle(Launcher::KeyRowBrush brush, Launcher::KeyRowPen pen,
                Launcher::KeyRowRoundedRect rect) override
            {
                _surface.FillRounded(
                    Launcher::GuiTheme::Round(rect.Rect, rect.Radius), ColorOf(brush));
                _surface.StrokeRounded(
                    Launcher::GuiTheme::Round(rect.Rect, rect.Radius),
                    ColorOf(pen.Brush()), pen.Thickness());
            }

            void SetFormattedTextMaxTextWidth(
                Launcher::TrackedTextFormattedText& text, double maxTextWidth) override
            {
                if (FormattedRun* const run = RunOf(text.Native))
                {
                    run->MaxTextWidth = maxTextWidth;
                    text.Width = std::min(text.Width, maxTextWidth);
                }
            }

            void SetFormattedTextMaxTextHeight(
                Launcher::TrackedTextFormattedText& text, double maxTextHeight) override
            {
                text.Height = std::min(text.Height, maxTextHeight);
            }

            void SetFormattedTextTrimming(Launcher::TrackedTextFormattedText& text,
                Launcher::KeyRowTextTrimming trimming) override
            {
                (void)trimming;
                if (FormattedRun* const run = RunOf(text.Native))
                {
                    run->Trim = true;
                }
            }
        };

        class PadContext final : public TextContext<Launcher::PadRowDrawingContext>
        {
        public:
            using TextContext::TextContext;

            void FillRectangle(
                Launcher::PadRowBrush brush, Launcher::GuiRect rect) override
            {
                _surface.FillRect(rect, ColorOf(brush));
            }

            void DrawRectangle(Launcher::PadRowBrush brush, Launcher::PadRowPen pen,
                Launcher::PadRowRoundedRect rect) override
            {
                _surface.FillRounded(
                    Launcher::GuiTheme::Round(rect.Rect, rect.Radius), ColorOf(brush));
                _surface.StrokeRounded(
                    Launcher::GuiTheme::Round(rect.Rect, rect.Radius),
                    ColorOf(pen.Brush()), pen.Thickness());
            }

            void SetFormattedTextMaxTextWidth(
                Launcher::TrackedTextFormattedText& text, double maxTextWidth) override
            {
                if (FormattedRun* const run = RunOf(text.Native))
                {
                    run->MaxTextWidth = maxTextWidth;
                    text.Width = std::min(text.Width, maxTextWidth);
                }
            }

            void SetFormattedTextMaxTextHeight(
                Launcher::TrackedTextFormattedText& text, double maxTextHeight) override
            {
                text.Height = std::min(text.Height, maxTextHeight);
            }

            void SetFormattedTextTrimming(Launcher::TrackedTextFormattedText& text,
                Launcher::PadRowTextTrimming trimming) override
            {
                (void)trimming;
                if (FormattedRun* const run = RunOf(text.Native))
                {
                    run->Trim = true;
                }
            }
        };

        // The pad row watches for a button with a timer of its own.
        class HostPadTimer final : public Launcher::PadRowDispatcherTimer
        {
        public:
            explicit HostPadTimer(std::shared_ptr<Toolkit::Timer> timer) noexcept
                : _timer(std::move(timer))
            {
            }

            void Start() override { _timer->Start(); }
            void Stop() override { _timer->Stop(); }

        private:
            std::shared_ptr<Toolkit::Timer> _timer;
        };
    }

    // --- SliderRowHost ----------------------------------------------------

    Toolkit::ElementPtr SliderRowHost::Create(std::optional<std::u16string> label,
        std::int32_t value, Launcher::SliderRow::FormatHandler format,
        double labelWidth, std::int32_t min, std::int32_t max, std::int32_t keyStep)
    {
        auto host = std::make_shared<SliderRowHost>();
        Toolkit::ElementPtr element = Bind(host);
        host->_row = std::make_unique<Launcher::SliderRow>(*host, std::move(label),
            value, std::move(format), labelWidth, min, max, keyStep);

        SliderRowHost* const raw = host.get();
        element->PointerPressed = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::SliderRowPointerEventArgs args{&e};
            raw->_row->OnPointerPressed(args);
            e.Handled = true;
        };
        element->PointerMoved = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::SliderRowPointerEventArgs args{&e};
            raw->_row->OnPointerMoved(args);
        };
        element->PointerReleased = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::SliderRowPointerEventArgs args{&e};
            raw->_row->OnPointerReleased(args);
        };
        element->PointerExited = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::SliderRowPointerEventArgs args{&e};
            raw->_row->OnPointerExited(args);
        };
        element->GotFocus = [raw]()
        {
            Launcher::SliderRowGotFocusEventArgs args{nullptr};
            raw->_row->OnGotFocus(args);
        };
        element->LostFocus = [raw]()
        {
            Launcher::SliderRowRoutedEventArgs args{nullptr};
            raw->_row->OnLostFocus(args);
        };
        element->KeyDown = [raw](Toolkit::KeyEvent& e)
        {
            Launcher::SliderRowKeyEventArgs args;
            args.Native = &e;
            args.Key = SliderKeyOf(e.Which);
            raw->_row->OnKeyDown(args);
            e.Handled = args.Handled;
        };
        return element;
    }

    Toolkit::Size SliderRowHost::Measure(
        Toolkit::Element& element, Toolkit::Size available)
    {
        (void)element;
        return Toolkit::Size{available.Width, 0.0};
    }

    void SliderRowHost::Render(Toolkit::Element& element, Toolkit::Renderer& renderer)
    {
        SliderContext context{Surface(renderer, element.Bounds())};
        _row->Render(context);
    }

    void SliderRowHost::SetHeight(double height)
    {
        _visual->Height = height;
    }

    double SliderRowHost::GetHeight() const
    {
        return _visual->Height.value_or(0.0);
    }

    void SliderRowHost::SetFocusable(bool focusable)
    {
        _visual->Focusable = focusable;
    }

    void SliderRowHost::SetHandCursor()
    {
        _visual->CursorKind = Toolkit::Cursor::Hand;
    }

    void SliderRowHost::Focus()
    {
        SetFocusHere();
    }

    bool SliderRowHost::GetIsEnabled() const
    {
        return _visual->IsEnabled;
    }

    void SliderRowHost::SetIsEnabled(bool isEnabled)
    {
        _visual->IsEnabled = isEnabled;
    }

    bool SliderRowHost::IsFocused() const
    {
        return _visual->IsFocused;
    }

    Launcher::GuiRect SliderRowHost::Bounds() const
    {
        return ControlBounds();
    }

    std::u16string SliderRowHost::ToUpperInvariant(std::u16string_view text)
    {
        return Upper(text);
    }

    Launcher::SliderRowPoint SliderRowHost::GetPosition(
        const Launcher::SliderRowPointerEventArgs& e) const
    {
        const auto* const event = static_cast<const Toolkit::PointerEvent*>(e.Native);
        if (event == nullptr)
        {
            return Launcher::SliderRowPoint{0.0, 0.0};
        }
        const Toolkit::Rect bounds = _visual->Bounds();
        return Launcher::SliderRowPoint{event->X - bounds.X, event->Y - bounds.Y};
    }

    void SliderRowHost::Capture(void* pointer, void* control)
    {
        (void)pointer;
        if (control != nullptr)
        {
            CaptureHere();
            return;
        }
        ReleaseCapture();
    }

    void SliderRowHost::InvalidateVisual()
    {
    }

    void SliderRowHost::BaseOnPointerPressed(Launcher::SliderRowPointerEventArgs& e)
    {
        (void)e;
    }

    void SliderRowHost::BaseOnPointerMoved(Launcher::SliderRowPointerEventArgs& e)
    {
        (void)e;
    }

    void SliderRowHost::BaseOnPointerReleased(Launcher::SliderRowPointerEventArgs& e)
    {
        (void)e;
    }

    void SliderRowHost::BaseOnPointerExited(Launcher::SliderRowPointerEventArgs& e)
    {
        (void)e;
    }

    void SliderRowHost::BaseOnKeyDown(Launcher::SliderRowKeyEventArgs& e)
    {
        (void)e;
    }

    void SliderRowHost::BaseOnGotFocus(Launcher::SliderRowGotFocusEventArgs& e)
    {
        (void)e;
    }

    void SliderRowHost::BaseOnLostFocus(Launcher::SliderRowRoutedEventArgs& e)
    {
        (void)e;
    }

    // --- KeyRowHost -------------------------------------------------------

    void KeyRowHost::Wire(const Toolkit::ElementPtr& element, KeyRowHost* host)
    {
        element->PointerPressed = [host](Toolkit::PointerEvent& e)
        {
            Launcher::KeyRowPointerPressedEventArgs args;
            args.Native = &e;
            host->_row->OnPointerPressed(args);
            e.Handled = true;
        };
        element->PointerEntered = [host](Toolkit::PointerEvent& e)
        {
            Launcher::KeyRowPointerEventArgs args{&e};
            host->_row->OnPointerEntered(args);
        };
        element->PointerExited = [host](Toolkit::PointerEvent& e)
        {
            Launcher::KeyRowPointerEventArgs args{&e};
            host->_row->OnPointerExited(args);
        };
        element->GotFocus = [host]()
        {
            Launcher::KeyRowGotFocusEventArgs args{nullptr};
            host->_row->OnGotFocus(args);
        };
        element->LostFocus = [host]()
        {
            Launcher::KeyRowRoutedEventArgs args{nullptr};
            host->_row->OnLostFocus(args);
        };
        element->KeyDown = [host](Toolkit::KeyEvent& e)
        {
            Launcher::KeyRowKeyEventArgs args;
            args.Native = &e;
            args.Key = KeyOf(e.Code);
            host->_row->OnKeyDown(args);
            e.Handled = args.Handled;
        };
    }

    Toolkit::ElementPtr KeyRowHost::Create(
        const MphRead::Mods::InputBindingProperty* property, double labelWidth)
    {
        auto host = std::make_shared<KeyRowHost>();
        Toolkit::ElementPtr element = Bind(host);
        host->_row = std::make_unique<Launcher::KeyRow>(*host, property, labelWidth);
        Wire(element, host.get());
        return element;
    }

    Toolkit::ElementPtr KeyRowHost::Create(std::optional<std::u16string> label,
        Launcher::KeyRowGetHandler get, Launcher::KeyRowSetHandler set,
        double labelWidth)
    {
        auto host = std::make_shared<KeyRowHost>();
        Toolkit::ElementPtr element = Bind(host);
        host->_row = std::make_unique<Launcher::KeyRow>(
            *host, std::move(label), std::move(get), std::move(set), labelWidth);
        Wire(element, host.get());
        return element;
    }

    Toolkit::Size KeyRowHost::Measure(
        Toolkit::Element& element, Toolkit::Size available)
    {
        (void)element;
        return Toolkit::Size{available.Width, 0.0};
    }

    void KeyRowHost::Render(Toolkit::Element& element, Toolkit::Renderer& renderer)
    {
        KeyContext context{Surface(renderer, element.Bounds())};
        _row->Render(context);
    }

    void KeyRowHost::SetHeight(double height)
    {
        _visual->Height = height;
    }

    double KeyRowHost::GetHeight() const
    {
        return _visual->Height.value_or(0.0);
    }

    void KeyRowHost::SetFocusable(bool focusable)
    {
        _visual->Focusable = focusable;
    }

    void KeyRowHost::SetHandCursor()
    {
        _visual->CursorKind = Toolkit::Cursor::Hand;
    }

    void KeyRowHost::Focus()
    {
        SetFocusHere();
    }

    bool KeyRowHost::IsFocused() const
    {
        return _visual->IsFocused;
    }

    Launcher::GuiRect KeyRowHost::Bounds() const
    {
        return ControlBounds();
    }

    Launcher::KeyRowPointerUpdateKind KeyRowHost::GetPointerUpdateKind(
        const Launcher::KeyRowPointerPressedEventArgs& e) const
    {
        // The window routes only the left button to a control.
        (void)e;
        return Launcher::KeyRowPointerUpdateKind::LeftButtonPressed;
    }

    Launcher::KeyRowPoint KeyRowHost::GetPosition(
        const Launcher::KeyRowPointerPressedEventArgs& e) const
    {
        const auto* const event = static_cast<const Toolkit::PointerEvent*>(e.Native);
        if (event == nullptr)
        {
            return Launcher::KeyRowPoint{0.0, 0.0};
        }
        const Toolkit::Rect bounds = _visual->Bounds();
        return Launcher::KeyRowPoint{event->X - bounds.X, event->Y - bounds.Y};
    }

    void KeyRowHost::InvalidateVisual()
    {
    }

    void KeyRowHost::BaseOnPointerPressed(Launcher::KeyRowPointerPressedEventArgs& e)
    {
        (void)e;
    }

    void KeyRowHost::BaseOnPointerWheelChanged(
        Launcher::KeyRowPointerWheelEventArgs& e)
    {
        (void)e;
    }

    void KeyRowHost::BaseOnPointerEntered(Launcher::KeyRowPointerEventArgs& e)
    {
        (void)e;
    }

    void KeyRowHost::BaseOnPointerExited(Launcher::KeyRowPointerEventArgs& e)
    {
        (void)e;
    }

    void KeyRowHost::BaseOnKeyDown(Launcher::KeyRowKeyEventArgs& e)
    {
        (void)e;
    }

    void KeyRowHost::BaseOnLostFocus(Launcher::KeyRowRoutedEventArgs& e)
    {
        (void)e;
    }

    void KeyRowHost::BaseOnGotFocus(Launcher::KeyRowGotFocusEventArgs& e)
    {
        (void)e;
    }

    // --- PadRowHost -------------------------------------------------------

    Toolkit::ElementPtr PadRowHost::Create(
        MphRead::Mods::Input::PadAction action, double labelWidth)
    {
        auto host = std::make_shared<PadRowHost>();
        Toolkit::ElementPtr element = Bind(host);
        host->_row = std::make_unique<Launcher::PadRow>(*host, action, labelWidth);

        PadRowHost* const raw = host.get();
        element->PointerPressed = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::PadRowPointerPressedEventArgs args;
            args.Native = &e;
            raw->_row->OnPointerPressed(args);
            e.Handled = true;
        };
        element->PointerEntered = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::PadRowPointerEventArgs args{&e};
            raw->_row->OnPointerEntered(args);
        };
        element->PointerExited = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::PadRowPointerEventArgs args{&e};
            raw->_row->OnPointerExited(args);
        };
        element->GotFocus = [raw]()
        {
            Launcher::PadRowGotFocusEventArgs args{nullptr};
            raw->_row->OnGotFocus(args);
        };
        element->LostFocus = [raw]()
        {
            Launcher::PadRowRoutedEventArgs args{nullptr};
            raw->_row->OnLostFocus(args);
        };
        element->KeyDown = [raw](Toolkit::KeyEvent& e)
        {
            Launcher::PadRowKeyEventArgs args;
            args.Native = &e;
            args.Key = PadKeyOf(e.Which);
            raw->_row->OnKeyDown(args);
            e.Handled = args.Handled;
        };
        return element;
    }

    Toolkit::Size PadRowHost::Measure(
        Toolkit::Element& element, Toolkit::Size available)
    {
        (void)element;
        return Toolkit::Size{available.Width, 0.0};
    }

    void PadRowHost::Render(Toolkit::Element& element, Toolkit::Renderer& renderer)
    {
        PadContext context{Surface(renderer, element.Bounds())};
        _row->Render(context);
    }

    void PadRowHost::SetHeight(double height)
    {
        _visual->Height = height;
    }

    double PadRowHost::GetHeight() const
    {
        return _visual->Height.value_or(0.0);
    }

    void PadRowHost::SetFocusable(bool focusable)
    {
        _visual->Focusable = focusable;
    }

    void PadRowHost::SetHandCursor()
    {
        _visual->CursorKind = Toolkit::Cursor::Hand;
    }

    void PadRowHost::Focus()
    {
        SetFocusHere();
    }

    bool PadRowHost::IsFocused() const
    {
        return _visual->IsFocused;
    }

    Launcher::GuiRect PadRowHost::Bounds() const
    {
        return ControlBounds();
    }

    Launcher::PadRowPoint PadRowHost::GetPosition(
        const Launcher::PadRowPointerPressedEventArgs& e) const
    {
        const auto* const event = static_cast<const Toolkit::PointerEvent*>(e.Native);
        if (event == nullptr)
        {
            return Launcher::PadRowPoint{0.0, 0.0};
        }
        const Toolkit::Rect bounds = _visual->Bounds();
        return Launcher::PadRowPoint{event->X - bounds.X, event->Y - bounds.Y};
    }

    bool PadRowHost::RectContains(
        Launcher::GuiRect rect, Launcher::PadRowPoint point) const
    {
        return point.X >= rect.X && point.X < rect.X + rect.Width
            && point.Y >= rect.Y && point.Y < rect.Y + rect.Height;
    }

    void PadRowHost::InvalidateVisual()
    {
    }

    std::shared_ptr<Launcher::PadRowDispatcherTimer> PadRowHost::CreateDispatcherTimer(
        std::chrono::milliseconds interval, Launcher::PadRowDispatcherPriority priority,
        Tick tick)
    {
        (void)priority;
        const std::shared_ptr<Toolkit::Timer> timer
            = Toolkit::Dispatcher::Instance().CreateTimer();
        timer->IntervalSeconds(static_cast<double>(interval.count()) / 1000.0);
        timer->Tick(std::move(tick));
        // Created stopped, as the row's own Listen() expects.
        timer->Stop();
        return std::make_shared<HostPadTimer>(timer);
    }

    void PadRowHost::BaseOnPointerPressed(Launcher::PadRowPointerPressedEventArgs& e)
    {
        (void)e;
    }

    void PadRowHost::BaseOnKeyDown(Launcher::PadRowKeyEventArgs& e)
    {
        (void)e;
    }

    void PadRowHost::BaseOnPointerEntered(Launcher::PadRowPointerEventArgs& e)
    {
        (void)e;
    }

    void PadRowHost::BaseOnPointerExited(Launcher::PadRowPointerEventArgs& e)
    {
        (void)e;
    }

    void PadRowHost::BaseOnLostFocus(Launcher::PadRowRoutedEventArgs& e)
    {
        (void)e;
    }

    void PadRowHost::BaseOnGotFocus(Launcher::PadRowGotFocusEventArgs& e)
    {
        (void)e;
    }

    void PadRowHost::BaseOnDetachedFromVisualTree(
        Launcher::PadRowVisualTreeAttachmentEventArgs& e)
    {
        (void)e;
    }
}
