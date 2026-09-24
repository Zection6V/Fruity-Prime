#include "HostRows.hpp"

#include "../Gui/Host.hpp"
#include "../Gui/Text.hpp"

#include <algorithm>
#include <cmath>
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

        [[nodiscard]] Launcher::GuiColor ColorOf(const Launcher::RowsBrush& brush)
        {
            const std::optional<Launcher::GuiColor> color = brush.CurrentColor();
            return color.has_value() ? *color
                                     : Launcher::GuiColor{0, 0, 0, 0};
        }

        [[nodiscard]] Launcher::GuiColor ColorOf(Launcher::ServerRowBrush brush)
        {
            if (brush.Kind == Launcher::ServerRowBrushKind::Transparent
                || brush.Brush == nullptr)
            {
                return Launcher::GuiColor{0, 0, 0, 0};
            }
            return brush.Brush->Color;
        }

        [[nodiscard]] Launcher::GuiColor ColorOfTracked(Launcher::TrackedTextBrush brush)
        {
            const auto* const value
                = static_cast<const Launcher::GuiBrush*>(brush.Native);
            return value != nullptr ? value->Color : Launcher::GuiTheme::Text;
        }

        [[nodiscard]] Toolkit::FontWeight WeightOf(bool bold) noexcept
        {
            return bold ? Toolkit::FontWeight::Bold : Toolkit::FontWeight::Normal;
        }

        // What Avalonia draws for a trimmed run: as much as fits, then an
        // ellipsis.
        [[nodiscard]] std::string Trimmed(
            const std::string& text, double fontSize, bool bold, double maxWidth)
        {
            if (maxWidth <= 0.0
                || Toolkit::MeasureText(text, fontSize, WeightOf(bold), 0.0).Width
                    <= maxWidth)
            {
                return text;
            }
            std::string result = text;
            while (!result.empty())
            {
                // One UTF-8 code point at a time, from the end.
                do
                {
                    result.pop_back();
                } while (!result.empty()
                    && (static_cast<unsigned char>(result.back()) & 0xC0) == 0x80);
                const std::string candidate = result + "\xE2\x80\xA6";
                if (Toolkit::MeasureText(candidate, fontSize, WeightOf(bold), 0.0).Width
                    <= maxWidth)
                {
                    return candidate;
                }
            }
            return std::string();
        }

        void DrawRun(const Surface& surface, const RowsRun& run, double x, double y)
        {
            const std::string text = run.Trim
                ? Trimmed(run.Text, run.FontSize, run.Bold, run.MaxTextWidth)
                : run.Text;
            surface.Renderer().DrawTextAt(text, surface.Bounds().X + x,
                surface.Bounds().Y + y, run.FontSize, WeightOf(run.Bold),
                ToColor(run.Color));
        }

        // --- Rows drawing -----------------------------------------------

        class RowsContext final : public Launcher::RowsDrawingContext
        {
        public:
            explicit RowsContext(const Surface& surface) noexcept
                : _surface(surface)
            {
            }

            void DrawText(const Launcher::RowsFormattedText& text,
                Launcher::RowsPoint point) override
            {
                const auto* const run = static_cast<const RowsRun*>(text.Native.get());
                if (run != nullptr)
                {
                    DrawRun(_surface, *run, point.X, point.Y);
                }
            }

            void DrawLine(Launcher::RowsPen pen, Launcher::RowsPoint start,
                Launcher::RowsPoint end) override
            {
                const std::optional<Launcher::RowsBrush> brush = pen.Brush();
                if (!brush.has_value())
                {
                    return;
                }
                // Every line the rows draw is axis-aligned, so it is a band.
                const double thickness = std::max(1.0, pen.Thickness());
                const double x = std::min(start.X, end.X);
                const double y = std::min(start.Y, end.Y);
                const double width = std::max(std::abs(end.X - start.X), thickness);
                const double height = std::max(std::abs(end.Y - start.Y), thickness);
                _surface.FillRect(
                    Launcher::GuiRect{x, y, width, height}, ColorOf(*brush));
            }

            void FillRectangle(Launcher::RowsBrush brush, Launcher::GuiRect rect,
                double radius) override
            {
                if (radius <= 0.0)
                {
                    _surface.FillRect(rect, ColorOf(brush));
                    return;
                }
                _surface.FillRounded(
                    Launcher::GuiTheme::Round(rect, radius), ColorOf(brush));
            }

            void DrawRectangle(Launcher::RowsBrush brush,
                std::optional<Launcher::RowsPen> pen,
                Launcher::RowsRoundedRect rect) override
            {
                _surface.FillRounded(
                    Launcher::GuiTheme::Round(rect.Rect, rect.Radius), ColorOf(brush));
                if (pen.has_value() && pen->Brush().has_value())
                {
                    _surface.StrokeRounded(
                        Launcher::GuiTheme::Round(rect.Rect, rect.Radius),
                        ColorOf(*pen->Brush()), pen->Thickness());
                }
            }

            void DrawEllipse(Launcher::RowsBrush brush,
                std::optional<Launcher::RowsPen> pen, Launcher::RowsPoint center,
                double radiusX, double radiusY) override
            {
                (void)pen;
                _surface.DrawEllipse(
                    center.X, center.Y, radiusX, radiusY, ColorOf(brush));
            }

            void DrawGeometry(Launcher::RowsBrush brush,
                std::optional<Launcher::RowsPen> pen,
                const Launcher::RowsTriangleGeometry& geometry) override
            {
                (void)pen;
                // The only geometry the rows draw is the choice arrow: a
                // triangle, laid down as rows of one-pixel bands between its
                // two edges.
                const std::array<Launcher::RowsPoint, 3>& points = geometry.Points();
                double top = points[0].Y;
                double bottom = points[0].Y;
                for (const Launcher::RowsPoint& point : points)
                {
                    top = std::min(top, point.Y);
                    bottom = std::max(bottom, point.Y);
                }
                const Launcher::GuiColor color = ColorOf(brush);
                for (double y = std::floor(top); y < bottom; y += 1.0)
                {
                    double left = 0.0;
                    double right = 0.0;
                    bool any = false;
                    for (std::size_t i = 0; i < points.size(); ++i)
                    {
                        const Launcher::RowsPoint& a = points[i];
                        const Launcher::RowsPoint& b = points[(i + 1) % points.size()];
                        if ((a.Y <= y && b.Y > y) || (b.Y <= y && a.Y > y))
                        {
                            const double x
                                = a.X + (y - a.Y) / (b.Y - a.Y) * (b.X - a.X);
                            if (!any)
                            {
                                left = x;
                                right = x;
                                any = true;
                                continue;
                            }
                            left = std::min(left, x);
                            right = std::max(right, x);
                        }
                    }
                    if (any && right > left)
                    {
                        _surface.FillRect(
                            Launcher::GuiRect{left, y, right - left, 1.0}, color);
                    }
                }
            }

        private:
            Surface _surface;
        };

        // --- ServerRow drawing ------------------------------------------

        class ServerRowContext final : public Launcher::ServerRowDrawingContext
        {
        public:
            explicit ServerRowContext(const Surface& surface) noexcept
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
                const std::string value = run->Trim
                    ? Trimmed(run->Text, run->FontSize, run->Bold, run->MaxTextWidth)
                    : run->Text;
                _surface.Renderer().DrawTextAt(value, _surface.Bounds().X + point.X,
                    _surface.Bounds().Y + point.Y, run->FontSize,
                    WeightOf(run->Bold), ToColor(run->Color));
            }

            void FillRectangle(Launcher::ServerRowBrush brush, Launcher::GuiRect rect,
                double radius) override
            {
                if (radius <= 0.0)
                {
                    _surface.FillRect(rect, ColorOf(brush));
                    return;
                }
                _surface.FillRounded(
                    Launcher::GuiTheme::Round(rect, radius), ColorOf(brush));
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
                Launcher::ServerRowTextTrimming trimming) override
            {
                (void)trimming;
                if (FormattedRun* const run = RunOf(text.Native))
                {
                    run->Trim = true;
                }
            }

            Launcher::ServerRowClipHandle PushClip(Launcher::GuiRect rect) override
            {
                _surface.PushClip(rect);
                return Launcher::ServerRowClipHandle{1};
            }

            void DisposeClip(Launcher::ServerRowClipHandle handle) override
            {
                if (handle.Native != 0)
                {
                    _surface.PopClip();
                }
            }

        private:
            Surface _surface;
        };

        // The item list a choice row reads.
        class StringList final : public Launcher::RowsStringList
        {
        public:
            explicit StringList(std::vector<std::u16string> items) noexcept
                : _items(std::move(items))
            {
            }

            [[nodiscard]] std::int32_t Count() const override
            {
                return static_cast<std::int32_t>(_items.size());
            }

            [[nodiscard]] std::optional<std::u16string> At(
                std::int32_t index) const override
            {
                if (index < 0 || index >= static_cast<std::int32_t>(_items.size()))
                {
                    return std::nullopt;
                }
                return _items[static_cast<std::size_t>(index)];
            }

        private:
            std::vector<std::u16string> _items;
        };


        [[nodiscard]] std::vector<std::u16string> Utf16All(
            const std::vector<std::string>& items)
        {
            std::vector<std::u16string> values;
            values.reserve(items.size());
            for (const std::string& item : items)
            {
                values.push_back(Utf8ToUtf16(item));
            }
            return values;
        }

        [[nodiscard]] Launcher::RowsKey RowKeyOf(Toolkit::Key key) noexcept
        {
            switch (key)
            {
            case Toolkit::Key::Left:
                return Launcher::RowsKey::Left;
            case Toolkit::Key::Right:
                return Launcher::RowsKey::Right;
            case Toolkit::Key::Enter:
                return Launcher::RowsKey::Enter;
            case Toolkit::Key::Space:
                return Launcher::RowsKey::Space;
            default:
                return Launcher::RowsKey::Other;
            }
        }

        // A handle that does not own, for an event whose target is the object
        // that owns the event.
        [[nodiscard]] std::shared_ptr<void> Weak(void* target)
        {
            return std::shared_ptr<void>(target, [](void*) {});
        }
    }

    Launcher::RowsStringListRef MakeStringList(std::vector<std::u16string> items)
    {
        return std::make_shared<StringList>(std::move(items));
    }

    // --- TextBoxHost ------------------------------------------------------

    Toolkit::ElementPtr TextBoxHost::Create()
    {
        auto host = std::make_shared<TextBoxHost>();
        Toolkit::ElementPtr element = Bind(host);
        element->Focusable = true;
        element->CursorKind = Toolkit::Cursor::IBeam;
        element->Height = 26.0;

        TextBoxHost* const raw = host.get();
        element->TextInput = [raw](char32_t code) { raw->Insert(code); };
        element->KeyDown = [raw](Toolkit::KeyEvent& e) { raw->Key(e); };
        element->LostFocus = [raw]()
        {
            if (raw->_lostFocus)
            {
                raw->_lostFocus();
            }
        };
        element->PointerPressed = [](Toolkit::PointerEvent& e) { e.Handled = true; };
        return element;
    }

    void TextBoxHost::Text(std::u16string value)
    {
        _text = std::move(value);
        _caret = _text.size();
    }

    void TextBoxHost::LostFocus(std::function<void()> action)
    {
        _lostFocus = std::move(action);
    }

    void TextBoxHost::Insert(char32_t code)
    {
        if (code < 0x20)
        {
            return;
        }
        const std::u16string piece = Utf8ToUtf16(Utf16ToUtf8(std::u16string(1,
            static_cast<char16_t>(code < 0x10000 ? code : u'?'))));
        _text.insert(std::min(_caret, _text.size()), piece);
        _caret = std::min(_caret + piece.size(), _text.size());
    }

    void TextBoxHost::Key(Toolkit::KeyEvent& e)
    {
        switch (e.Which)
        {
        case Toolkit::Key::Backspace:
            if (_caret > 0)
            {
                _text.erase(_caret - 1, 1);
                --_caret;
            }
            e.Handled = true;
            break;
        case Toolkit::Key::Delete:
            if (_caret < _text.size())
            {
                _text.erase(_caret, 1);
            }
            e.Handled = true;
            break;
        case Toolkit::Key::Left:
            if (_caret > 0)
            {
                --_caret;
            }
            e.Handled = true;
            break;
        case Toolkit::Key::Right:
            if (_caret < _text.size())
            {
                ++_caret;
            }
            e.Handled = true;
            break;
        case Toolkit::Key::Home:
            _caret = 0;
            e.Handled = true;
            break;
        case Toolkit::Key::End:
            _caret = _text.size();
            e.Handled = true;
            break;
        case Toolkit::Key::Enter:
            // Committing is what leaving the box does, and the rows listen
            // for exactly that.
            if (Toolkit::Window* const window = Toolkit::Window::Of(*_visual))
            {
                window->Focus(nullptr);
            }
            e.Handled = true;
            break;
        default:
            break;
        }
    }

    Toolkit::Size TextBoxHost::Measure(
        Toolkit::Element& element, Toolkit::Size available)
    {
        (void)available;
        const double height = Toolkit::FontLineHeight(FontSize,
            Toolkit::FontWeight::Normal) + Padding.Vertical();
        return Toolkit::Size{element.Width.value_or(150.0), height};
    }

    void TextBoxHost::Render(Toolkit::Element& element, Toolkit::Renderer& renderer)
    {
        const Toolkit::Rect bounds = element.Bounds();
        renderer.FillRoundedRect(bounds, CornerRadius,
            ToColor(Launcher::GuiTheme::PanelLight));
        renderer.StrokeRoundedRect(bounds, CornerRadius,
            Toolkit::Thickness{1.0, 1.0, 1.0, 1.0},
            ToColor(element.IsFocused ? Launcher::GuiTheme::Accent
                                      : Launcher::GuiTheme::Edge));

        const bool empty = _text.empty();
        const std::string shown = empty ? Utf16ToUtf8(_watermark) : Utf16ToUtf8(_text);
        const double x = bounds.X + Padding.Left;
        const double y = bounds.Y + (bounds.Height
            - Toolkit::FontLineHeight(FontSize, Toolkit::FontWeight::Normal)) / 2.0;
        renderer.DrawTextAt(shown, x, y, FontSize, Toolkit::FontWeight::Normal,
            ToColor(empty ? Launcher::GuiTheme::TextDim : Launcher::GuiTheme::Text));

        if (element.IsFocused)
        {
            const std::string before = Utf16ToUtf8(
                std::u16string_view(_text).substr(0, std::min(_caret, _text.size())));
            const double offset = Toolkit::MeasureText(
                before, FontSize, Toolkit::FontWeight::Normal, 0.0).Width;
            renderer.FillRect(
                Toolkit::Rect{x + offset, y, 1.0,
                    Toolkit::FontLineHeight(FontSize, Toolkit::FontWeight::Normal)},
                ToColor(Launcher::GuiTheme::Text));
        }
    }

    // --- RowsHost ---------------------------------------------------------

    void RowsHost::SetHeight(double height)
    {
        _visual->Height = height;
    }

    double RowsHost::GetHeight() const
    {
        return _visual->Height.value_or(0.0);
    }

    void RowsHost::SetIsVisible(bool isVisible)
    {
        _visual->Visible = isVisible;
    }

    bool RowsHost::GetIsVisible() const
    {
        return _visual->Visible;
    }

    void RowsHost::SetMargin(Launcher::RowsThickness margin)
    {
        _visual->Margin = Toolkit::Thickness{
            margin.Left, margin.Top, margin.Right, margin.Bottom};
    }

    void RowsHost::InvalidateVisual()
    {
    }

    // --- RowsTextHost -----------------------------------------------------

    Launcher::GuiRect RowsTextHost::Bounds() const
    {
        return ControlBounds();
    }

    Launcher::RowsSize RowsTextHost::BaseMeasureOverride(
        Launcher::RowsSize availableSize)
    {
        (void)availableSize;
        return Launcher::RowsSize{0.0, 0.0};
    }

    std::u16string RowsTextHost::ToUpperInvariant(std::u16string_view text)
    {
        return Upper(text);
    }

    Launcher::RowsFormattedText RowsTextHost::CreateFormattedText(
        std::optional<std::u16string_view> text, Launcher::RowsCulture culture,
        Launcher::RowsFlowDirection flowDirection, Launcher::GuiTypeface typeface,
        double fontSize, Launcher::RowsBrush brush)
    {
        (void)culture;
        (void)flowDirection;
        auto run = std::make_shared<RowsRun>();
        run->Text = text.has_value() ? Utf16ToUtf8(*text) : std::string();
        run->FontSize = fontSize;
        run->Bold = typeface.Weight >= Launcher::GuiFontWeight::SemiBold;
        run->Color = ColorOf(brush);
        const Toolkit::Size size = Toolkit::MeasureText(
            run->Text, fontSize, WeightOf(run->Bold), 0.0);
        return Launcher::RowsFormattedText{
            std::move(run), size.Width, size.Height};
    }

    void RowsTextHost::SetFormattedTextMaxTextWidth(
        Launcher::RowsFormattedText& text, double maxTextWidth)
    {
        if (auto* const run = static_cast<RowsRun*>(text.Native.get()))
        {
            run->MaxTextWidth = maxTextWidth;
            text.Width = std::min(text.Width, maxTextWidth);
        }
    }

    void RowsTextHost::SetFormattedTextTrimming(
        Launcher::RowsFormattedText& text, Launcher::RowsTextTrimming trimming)
    {
        (void)trimming;
        if (auto* const run = static_cast<RowsRun*>(text.Native.get()))
        {
            run->Trim = true;
        }
    }

    // --- RowsInteractiveHost ----------------------------------------------

    void RowsInteractiveHost::SetFocusable(bool focusable)
    {
        _visual->Focusable = focusable;
    }

    void RowsInteractiveHost::SetHandCursor()
    {
        _visual->CursorKind = Toolkit::Cursor::Hand;
    }

    void RowsInteractiveHost::Focus()
    {
        SetFocusHere();
    }

    bool RowsInteractiveHost::IsFocused() const
    {
        return _visual->IsFocused;
    }

    Launcher::RowsPoint RowsInteractiveHost::GetPosition(
        const Launcher::RowsPointerEventArgs& e) const
    {
        const auto* const event = static_cast<const Toolkit::PointerEvent*>(e.Native);
        if (event == nullptr)
        {
            return Launcher::RowsPoint{0.0, 0.0};
        }
        const Toolkit::Rect bounds = _visual->Bounds();
        return Launcher::RowsPoint{event->X - bounds.X, event->Y - bounds.Y};
    }

    void RowsInteractiveHost::BaseOnPointerMoved(Launcher::RowsPointerEventArgs& e)
    {
        (void)e;
    }

    void RowsInteractiveHost::BaseOnPointerExited(Launcher::RowsPointerEventArgs& e)
    {
        (void)e;
    }

    void RowsInteractiveHost::BaseOnPointerPressed(Launcher::RowsPointerEventArgs& e)
    {
        (void)e;
    }

    void RowsInteractiveHost::BaseOnKeyDown(Launcher::RowsKeyEventArgs& e)
    {
        (void)e;
    }

    // --- CaptionHost ------------------------------------------------------

    Toolkit::ElementPtr CaptionHost::Create(std::u16string text)
    {
        auto host = std::make_shared<CaptionHost>();
        Toolkit::ElementPtr element = Bind(host);
        host->_caption = std::make_unique<Launcher::Caption>(*host, std::move(text));
        return element;
    }

    Toolkit::Size CaptionHost::Measure(
        Toolkit::Element& element, Toolkit::Size available)
    {
        (void)element;
        const Launcher::RowsSize size = _caption->MeasureOverride(
            Launcher::RowsSize{available.Width, available.Height});
        return Toolkit::Size{size.Width, size.Height};
    }

    void CaptionHost::Render(Toolkit::Element& element, Toolkit::Renderer& renderer)
    {
        RowsContext context(Surface(renderer, element.Bounds()));
        _caption->Render(context);
    }

    // --- ChoiceRowHost ----------------------------------------------------

    Toolkit::ElementPtr ChoiceRowHost::Create(std::u16string label,
        const std::vector<std::string>& items, std::int32_t index)
    {
        return CreateFromList(
            std::move(label), MakeStringList(Utf16All(items)), index);
    }

    Toolkit::ElementPtr ChoiceRowHost::CreateFromList(std::u16string label,
        Launcher::RowsStringListRef options, std::int32_t index)
    {
        auto host = std::make_shared<ChoiceRowHost>();
        Toolkit::ElementPtr element = Bind(host);
        host->_row = std::make_unique<Launcher::ChoiceRow>(
            *host, std::move(label), std::move(options), index);

        ChoiceRowHost* const raw = host.get();
        element->PointerMoved = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::RowsPointerEventArgs args{&e};
            raw->_row->OnPointerMoved(args);
        };
        element->PointerExited = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::RowsPointerEventArgs args{&e};
            raw->_row->OnPointerExited(args);
        };
        element->PointerPressed = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::RowsPointerEventArgs args{&e};
            raw->_row->OnPointerPressed(args);
            e.Handled = true;
        };
        element->KeyDown = [raw](Toolkit::KeyEvent& e)
        {
            Launcher::RowsKeyEventArgs args;
            args.Native = &e;
            args.Key = RowKeyOf(e.Which);
            raw->_row->OnKeyDown(args);
            e.Handled = args.Handled;
        };
        return element;
    }

    void ChoiceRowHost::Changed(std::function<void()> action)
    {
        _changed = std::move(action);
        _row->AddChanged(Launcher::RowsEventHandler(this,
            [](void* context, void*, const Launcher::RowsEventArgs&)
            {
                ChoiceRowHost* const self = static_cast<ChoiceRowHost*>(context);
                if (self->_changed)
                {
                    self->_changed();
                }
            }));
    }

    Toolkit::Size ChoiceRowHost::Measure(
        Toolkit::Element& element, Toolkit::Size available)
    {
        (void)element;
        return Toolkit::Size{available.Width, 0.0};
    }

    void ChoiceRowHost::Render(Toolkit::Element& element, Toolkit::Renderer& renderer)
    {
        RowsContext context(Surface(renderer, element.Bounds()));
        _row->Render(context);
    }

    // --- ToggleRowHost ----------------------------------------------------

    Toolkit::ElementPtr ToggleRowHost::Create(std::u16string label, bool on)
    {
        auto host = std::make_shared<ToggleRowHost>();
        Toolkit::ElementPtr element = Bind(host);
        host->_row = std::make_unique<Launcher::ToggleRow>(
            *host, std::move(label), on);

        ToggleRowHost* const raw = host.get();
        element->PointerPressed = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::RowsPointerEventArgs args{&e};
            raw->_row->OnPointerPressed(args);
            e.Handled = true;
        };
        element->KeyDown = [raw](Toolkit::KeyEvent& e)
        {
            Launcher::RowsKeyEventArgs args;
            args.Native = &e;
            args.Key = RowKeyOf(e.Which);
            raw->_row->OnKeyDown(args);
            e.Handled = args.Handled;
        };
        return element;
    }

    void ToggleRowHost::Changed(std::function<void()> action)
    {
        _changed = std::move(action);
        _row->AddChanged(Launcher::RowsEventHandler(this,
            [](void* context, void*, const Launcher::RowsEventArgs&)
            {
                ToggleRowHost* const self = static_cast<ToggleRowHost*>(context);
                if (self->_changed)
                {
                    self->_changed();
                }
            }));
    }

    Toolkit::Size ToggleRowHost::Measure(
        Toolkit::Element& element, Toolkit::Size available)
    {
        (void)element;
        return Toolkit::Size{available.Width, 0.0};
    }

    void ToggleRowHost::Render(Toolkit::Element& element, Toolkit::Renderer& renderer)
    {
        RowsContext context(Surface(renderer, element.Bounds()));
        _row->Render(context);
    }

    // --- FieldRowHost -----------------------------------------------------

    Toolkit::ElementPtr FieldRowHost::Create(
        std::u16string label, std::u16string value, double boxWidth)
    {
        auto host = std::make_shared<FieldRowHost>();
        Toolkit::ElementPtr element
            = Toolkit::Element::Create(Toolkit::ElementKind::StackPanel);
        element->StackOrientation = Toolkit::Orientation::Horizontal;
        element->Tag = host;
        host->_visual = element.get();
        host->_row = std::make_unique<Launcher::FieldRow>(
            *host, std::move(label), std::move(value), boxWidth);
        return element;
    }

    void FieldRowHost::LostFocus(std::function<void()> action)
    {
        _lostFocus = std::move(action);
        if (auto* const box = HostOf<TextBoxHost>(_box))
        {
            box->LostFocus([this]()
            {
                if (_lostFocus)
                {
                    _lostFocus();
                }
            });
        }
    }

    void FieldRowHost::SetHeight(double height)
    {
        _visual->Height = height;
    }

    double FieldRowHost::GetHeight() const
    {
        return _visual->Height.value_or(0.0);
    }

    void FieldRowHost::SetIsVisible(bool isVisible)
    {
        _visual->Visible = isVisible;
    }

    bool FieldRowHost::GetIsVisible() const
    {
        return _visual->Visible;
    }

    void FieldRowHost::SetMargin(Launcher::RowsThickness margin)
    {
        _visual->Margin = Toolkit::Thickness{
            margin.Left, margin.Top, margin.Right, margin.Bottom};
    }

    void FieldRowHost::InvalidateVisual()
    {
    }

    FieldRowHost::ElementHandle FieldRowHost::CreateTextBlock()
    {
        Toolkit::ElementPtr block
            = Toolkit::Element::Create(Toolkit::ElementKind::TextBlock);
        _owned.push_back(block);
        return block.get();
    }

    void FieldRowHost::SetTextBlockText(
        ElementHandle textBlock, std::optional<std::u16string_view> text)
    {
        auto* const element = static_cast<Toolkit::Element*>(textBlock);
        element->Text = text.has_value() ? std::optional<std::string>(Utf16ToUtf8(*text))
                                         : std::nullopt;
    }

    void FieldRowHost::SetTextBlockFontFamily(
        ElementHandle textBlock, Launcher::GuiFontFamily family)
    {
        (void)textBlock;
        (void)family;
    }

    void FieldRowHost::SetTextBlockFontSize(ElementHandle textBlock, double fontSize)
    {
        static_cast<Toolkit::Element*>(textBlock)->FontSize = fontSize;
    }

    void FieldRowHost::SetTextBlockForeground(
        ElementHandle textBlock, Launcher::RowsBrush brush)
    {
        static_cast<Toolkit::Element*>(textBlock)->Foreground = ToColor(ColorOf(brush));
    }

    void FieldRowHost::SetTextBlockVerticalAlignment(
        ElementHandle textBlock, Launcher::RowsVerticalAlignment alignment)
    {
        (void)alignment;
        static_cast<Toolkit::Element*>(textBlock)->Vertical
            = Toolkit::VerticalAlignment::Center;
    }

    void FieldRowHost::SetTextBlockHorizontalAlignment(
        ElementHandle textBlock, Launcher::RowsHorizontalAlignment alignment)
    {
        static_cast<Toolkit::Element*>(textBlock)->Horizontal
            = alignment == Launcher::RowsHorizontalAlignment::Right
            ? Toolkit::HorizontalAlignment::Right
            : Toolkit::HorizontalAlignment::Left;
    }

    void FieldRowHost::SetTextBlockMargin(
        ElementHandle textBlock, Launcher::RowsThickness margin)
    {
        static_cast<Toolkit::Element*>(textBlock)->Margin = Toolkit::Thickness{
            margin.Left, margin.Top, margin.Right, margin.Bottom};
    }

    FieldRowHost::ElementHandle FieldRowHost::CreateTextBox()
    {
        _box = TextBoxHost::Create();
        _owned.push_back(_box);
        return _box.get();
    }

    void FieldRowHost::SetTextBoxText(
        ElementHandle textBox, std::optional<std::u16string_view> text)
    {
        auto* const host = static_cast<TextBoxHost*>(
            static_cast<Toolkit::Element*>(textBox)->Tag.get());
        host->Text(text.has_value() ? std::u16string(*text) : std::u16string());
    }

    std::optional<std::u16string> FieldRowHost::GetTextBoxText(
        ElementHandle textBox) const
    {
        const auto* const host = static_cast<const TextBoxHost*>(
            static_cast<const Toolkit::Element*>(textBox)->Tag.get());
        return host->Text();
    }

    void FieldRowHost::SetTextBoxWidth(ElementHandle textBox, double width)
    {
        static_cast<Toolkit::Element*>(textBox)->Width = width;
    }

    void FieldRowHost::SetTextBoxFontFamily(
        ElementHandle textBox, Launcher::GuiFontFamily family)
    {
        (void)textBox;
        (void)family;
    }

    void FieldRowHost::SetTextBoxFontSize(ElementHandle textBox, double fontSize)
    {
        static_cast<TextBoxHost*>(
            static_cast<Toolkit::Element*>(textBox)->Tag.get())->FontSize = fontSize;
    }

    void FieldRowHost::SetTextBoxCornerRadius(ElementHandle textBox, double radius)
    {
        static_cast<TextBoxHost*>(
            static_cast<Toolkit::Element*>(textBox)->Tag.get())->CornerRadius = radius;
    }

    void FieldRowHost::SetTextBoxPadding(
        ElementHandle textBox, Launcher::RowsThickness padding)
    {
        static_cast<TextBoxHost*>(
            static_cast<Toolkit::Element*>(textBox)->Tag.get())->Padding
            = Toolkit::Thickness{
                padding.Left, padding.Top, padding.Right, padding.Bottom};
    }

    void FieldRowHost::SetTextBoxVerticalAlignment(
        ElementHandle textBox, Launcher::RowsVerticalAlignment alignment)
    {
        (void)alignment;
        static_cast<Toolkit::Element*>(textBox)->Vertical
            = Toolkit::VerticalAlignment::Center;
    }

    void FieldRowHost::SetTextBoxHorizontalAlignment(
        ElementHandle textBox, Launcher::RowsHorizontalAlignment alignment)
    {
        static_cast<Toolkit::Element*>(textBox)->Horizontal
            = alignment == Launcher::RowsHorizontalAlignment::Right
            ? Toolkit::HorizontalAlignment::Right
            : Toolkit::HorizontalAlignment::Left;
    }

    void FieldRowHost::SetTextBoxWatermark(
        ElementHandle textBox, std::optional<std::u16string_view> watermark)
    {
        static_cast<TextBoxHost*>(
            static_cast<Toolkit::Element*>(textBox)->Tag.get())
            ->Watermark(watermark.has_value() ? std::u16string(*watermark)
                                              : std::u16string());
    }

    void FieldRowHost::AddTextBoxLostFocus(
        ElementHandle textBox, const Launcher::RowsEventHandler& handler)
    {
        // The row keeps its own handler list; the host raises through the one
        // callback the text box carries.
        (void)textBox;
        (void)handler;
    }

    void FieldRowHost::RemoveTextBoxLostFocus(
        ElementHandle textBox, const Launcher::RowsEventHandler& handler)
    {
        (void)textBox;
        (void)handler;
    }

    void FieldRowHost::AddChild(ElementHandle child)
    {
        for (const Toolkit::ElementPtr& owned : _owned)
        {
            if (owned.get() == child)
            {
                _visual->AddChild(owned);
                return;
            }
        }
    }

    // --- NoteHost ---------------------------------------------------------

    Toolkit::ElementPtr NoteHost::Create(
        std::u16string text, std::optional<Launcher::GuiColor> color)
    {
        auto host = std::make_shared<NoteHost>();
        Toolkit::ElementPtr element
            = Toolkit::Element::Create(Toolkit::ElementKind::TextBlock);
        element->Tag = host;
        host->_visual = element.get();
        host->_note = std::make_unique<Launcher::Note>(*host, std::move(text), color);
        return element;
    }

    void NoteHost::SetHeight(double height)
    {
        _visual->Height = height;
    }

    double NoteHost::GetHeight() const
    {
        return _visual->Height.value_or(0.0);
    }

    void NoteHost::SetIsVisible(bool isVisible)
    {
        _visual->Visible = isVisible;
    }

    bool NoteHost::GetIsVisible() const
    {
        return _visual->Visible;
    }

    void NoteHost::SetMargin(Launcher::RowsThickness margin)
    {
        _visual->Margin = Toolkit::Thickness{
            margin.Left, margin.Top, margin.Right, margin.Bottom};
    }

    void NoteHost::InvalidateVisual()
    {
    }

    void NoteHost::SetText(std::optional<std::u16string_view> text)
    {
        _visual->Text = text.has_value() ? std::optional<std::string>(Utf16ToUtf8(*text))
                                         : std::nullopt;
    }

    std::optional<std::u16string> NoteHost::GetText() const
    {
        return _visual->Text.has_value()
            ? std::optional<std::u16string>(Utf8ToUtf16(*_visual->Text))
            : std::nullopt;
    }

    void NoteHost::SetFontFamily(Launcher::GuiFontFamily family)
    {
        (void)family;
    }

    void NoteHost::SetFontSize(double fontSize)
    {
        _visual->FontSize = fontSize;
    }

    void NoteHost::SetForeground(std::optional<Launcher::RowsBrush> brush)
    {
        _foreground = brush;
        if (brush.has_value())
        {
            _visual->Foreground = ToColor(ColorOf(*brush));
        }
    }

    std::optional<Launcher::RowsBrush> NoteHost::GetForeground() const
    {
        return _foreground;
    }

    void NoteHost::SetTextWrapping(Launcher::RowsTextWrapping wrapping)
    {
        (void)wrapping;
        _visual->WrapText = true;
    }

    // --- ServerRowHost ----------------------------------------------------

    Toolkit::ElementPtr ServerRowHost::Create(
        std::u16string name, std::u16string endpoint)
    {
        auto host = std::make_shared<ServerRowHost>();
        Toolkit::ElementPtr element = Bind(host);
        host->_row = std::make_unique<Launcher::ServerRow>(*host,
            std::make_shared<const std::u16string>(std::move(name)),
            std::make_shared<const std::u16string>(std::move(endpoint)));

        ServerRowHost* const raw = host.get();
        element->PointerEntered = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::ServerRowPointerEventArgs args{&e};
            raw->_row->OnPointerEntered(args);
        };
        element->PointerExited = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::ServerRowPointerEventArgs args{&e};
            raw->_row->OnPointerExited(args);
        };
        element->PointerPressed = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::ServerRowPointerPressedEventArgs args{&e};
            raw->_row->OnPointerPressed(args);
            e.Handled = true;
        };
        element->KeyDown = [raw](Toolkit::KeyEvent& e)
        {
            Launcher::ServerRowKeyEventArgs args;
            args.Native = &e;
            args.Key = e.Which == Toolkit::Key::Enter ? Launcher::ServerRowKey::Enter
                : e.Which == Toolkit::Key::Space      ? Launcher::ServerRowKey::Space
                                                      : Launcher::ServerRowKey::Other;
            raw->_row->OnKeyDown(args);
            e.Handled = args.Handled;
        };
        return element;
    }

    void ServerRowHost::Clicked(std::function<void()> action)
    {
        _clicked = std::move(action);
        _row->AddClicked(Launcher::ServerRowEventHandler(Weak(this),
            [](void* target, void*, const Launcher::ServerRowEventArgs&)
            {
                ServerRowHost* const self = static_cast<ServerRowHost*>(target);
                if (self->_clicked)
                {
                    self->_clicked();
                }
            }));
    }

    Toolkit::Size ServerRowHost::Measure(
        Toolkit::Element& element, Toolkit::Size available)
    {
        (void)element;
        return Toolkit::Size{available.Width, 0.0};
    }

    void ServerRowHost::Render(Toolkit::Element& element, Toolkit::Renderer& renderer)
    {
        ServerRowContext context(Surface(renderer, element.Bounds()));
        _row->Render(context);
    }

    void ServerRowHost::SetHeight(double height)
    {
        _visual->Height = height;
    }

    double ServerRowHost::GetHeight() const
    {
        return _visual->Height.value_or(0.0);
    }

    void ServerRowHost::SetFocusable(bool focusable)
    {
        _visual->Focusable = focusable;
    }

    void ServerRowHost::SetHandCursor()
    {
        _visual->CursorKind = Toolkit::Cursor::Hand;
    }

    void ServerRowHost::Focus()
    {
        SetFocusHere();
    }

    bool ServerRowHost::IsFocused() const
    {
        return _visual->IsFocused;
    }

    Launcher::GuiRect ServerRowHost::Bounds() const
    {
        return ControlBounds();
    }

    std::u16string ServerRowHost::FormatCurrentInt32(std::int32_t value) const
    {
        return Utf8ToUtf16(std::to_string(value));
    }

    void ServerRowHost::InvalidateVisual()
    {
    }

    void ServerRowHost::BaseOnPointerEntered(Launcher::ServerRowPointerEventArgs& e)
    {
        (void)e;
    }

    void ServerRowHost::BaseOnPointerExited(Launcher::ServerRowPointerEventArgs& e)
    {
        (void)e;
    }

    void ServerRowHost::BaseOnPointerPressed(
        Launcher::ServerRowPointerPressedEventArgs& e)
    {
        (void)e;
    }

    void ServerRowHost::BaseOnKeyDown(Launcher::ServerRowKeyEventArgs& e)
    {
        (void)e;
    }

    // --- ServerHeaderHost -------------------------------------------------

    Toolkit::ElementPtr ServerHeaderHost::Create()
    {
        auto host = std::make_shared<ServerHeaderHost>();
        Toolkit::ElementPtr element = Bind(host);
        host->_header = std::make_unique<Launcher::ServerHeader>(*host);
        return element;
    }

    Toolkit::Size ServerHeaderHost::Measure(
        Toolkit::Element& element, Toolkit::Size available)
    {
        (void)element;
        return Toolkit::Size{available.Width, 0.0};
    }

    void ServerHeaderHost::Render(
        Toolkit::Element& element, Toolkit::Renderer& renderer)
    {
        ServerRowContext context(Surface(renderer, element.Bounds()));
        _header->Render(context);
    }

    void ServerHeaderHost::SetHeight(double height)
    {
        _visual->Height = height;
    }

    double ServerHeaderHost::GetHeight() const
    {
        return _visual->Height.value_or(0.0);
    }

    void ServerHeaderHost::SetIsHitTestVisible(bool isHitTestVisible)
    {
        // Nothing here reacts to the pointer, so this is only whether it may
        // be hit at all.
        _visual->IsEnabled = isHitTestVisible;
    }

    Launcher::GuiRect ServerHeaderHost::Bounds() const
    {
        return ControlBounds();
    }
}
