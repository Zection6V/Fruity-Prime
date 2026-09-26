#include "Text.hpp"

#include "TopLevel.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace MphRead::NativeRuntime::Avalonia::Controls
{
    namespace
    {
        constexpr double Infinity = std::numeric_limits<double>::infinity();
        constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
    }

    // ---------------------------------------------------------- TextBlock

    StyledProperty<std::string>& TextBlock::TextProperty = Register<TextBlock, std::string>("Text", std::string{});
    StyledProperty<Media::IBrushPtr>& TextBlock::BackgroundProperty
        = Register<TextBlock, Media::IBrushPtr>("Background", nullptr);
    StyledProperty<Thickness>& TextBlock::PaddingProperty = Register<TextBlock, Thickness>("Padding", Thickness{});
    StyledProperty<Media::TextWrapping>& TextBlock::TextWrappingProperty
        = Register<TextBlock, Media::TextWrapping>("TextWrapping", Media::TextWrapping::NoWrap);
    StyledProperty<Media::TextTrimming>& TextBlock::TextTrimmingProperty
        = Register<TextBlock, Media::TextTrimming>("TextTrimming", Media::TextTrimming::None);
    StyledProperty<Media::TextAlignment>& TextBlock::TextAlignmentProperty
        = Register<TextBlock, Media::TextAlignment>("TextAlignment", Media::TextAlignment::Left);
    StyledProperty<double>& TextBlock::LineHeightProperty = Register<TextBlock, double>("LineHeight", NaN);
    StyledProperty<std::int32_t>& TextBlock::MaxLinesProperty = Register<TextBlock, std::int32_t>("MaxLines", 0);
    StyledProperty<double>& TextBlock::LetterSpacingProperty = Register<TextBlock, double>("LetterSpacing", 0.0);

    TextBlock::TextBlock()
    {
        static const bool registered = []
        {
            AffectsMeasure<TextBlock>(TextProperty, PaddingProperty, TextWrappingProperty, TextTrimmingProperty,
                LineHeightProperty, MaxLinesProperty, LetterSpacingProperty, TextElement::FontFamilyProperty,
                TextElement::FontSizeProperty, TextElement::FontWeightProperty, TextElement::FontStyleProperty);
            AffectsRender<TextBlock>(BackgroundProperty, TextAlignmentProperty, TextElement::ForegroundProperty);
            return true;
        }();
        (void)registered;
    }

    std::shared_ptr<Media::TextLayout> TextBlock::CreateTextLayout(double maxWidth, double maxHeight) const
    {
        const Media::Typeface face(FontFamily(), FontStyle(), FontWeight());
        return std::make_shared<Media::TextLayout>(Media::ToUtf32(Text()), face, FontSize(), Foreground(),
            TextAlignment(), TextWrapping(), TextTrimming(), maxWidth, maxHeight, LineHeight(), MaxLines(),
            LetterSpacing());
    }

    void TextBlock::OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change)
    {
        Control::OnPropertyChanged(change);
        if ((change.Property.AffectsFor(*this) & (static_cast<std::uint8_t>(AvaloniaProperty::Affects::Measure)
                | static_cast<std::uint8_t>(AvaloniaProperty::Affects::Render))) != 0)
        {
            _layout.reset();
        }
    }

    Size TextBlock::MeasureOverride(Size availableSize)
    {
        const Thickness padding = Padding();
        const Size inner = availableSize.Deflate(padding);
        _layout = CreateTextLayout(inner.Width, inner.Height);
        _constraint = inner;
        if (Text().empty())
        {
            // An empty block still takes a line's height, as Avalonia's does.
            return Size{0, _layout->Height()}.Inflate(padding);
        }
        return Size{_layout->WidthIncludingTrailingWhitespace(), _layout->Height()}.Inflate(padding);
    }

    Size TextBlock::ArrangeOverride(Size finalSize)
    {
        const Size inner = finalSize.Deflate(Padding());
        if (_layout == nullptr || _constraint != inner)
        {
            // Aligned text lays out against the width it was given.
            _layout = CreateTextLayout(inner.Width, inner.Height);
            _constraint = inner;
        }
        return finalSize;
    }

    void TextBlock::Render(Media::DrawingContext& context)
    {
        const Rect bounds(Bounds().GetSize());
        if (const Media::IBrushPtr background = Background())
        {
            context.FillRectangle(background, bounds);
        }
        const Thickness padding = Padding();
        if (_layout == nullptr)
        {
            _layout = CreateTextLayout(std::max(0.0, bounds.Width - padding.Left - padding.Right), Infinity);
        }
        if (Text().empty())
        {
            return;
        }
        // Clipped to its bounds, as a TextBlock clips what overflows.
        auto clip = context.PushClip(bounds);
        _layout->Draw(context.Canvas(), Point{padding.Left, padding.Top});
    }

    // ------------------------------------------------------------ TextBox

    StyledProperty<std::string>& TextBox::TextProperty = Register<TextBox, std::string>("Text", std::string{});
    StyledProperty<std::string>& TextBox::PlaceholderTextProperty
        = Register<TextBox, std::string>("PlaceholderText", std::string{});
    StyledProperty<std::int32_t>& TextBox::MaxLengthProperty = Register<TextBox, std::int32_t>("MaxLength", 0);
    StyledProperty<Media::IBrushPtr>& TextBox::CaretBrushProperty
        = Register<TextBox, Media::IBrushPtr>("CaretBrush", nullptr);
    StyledProperty<Media::IBrushPtr>& TextBox::SelectionBrushProperty
        = Register<TextBox, Media::IBrushPtr>("SelectionBrush", nullptr);
    StyledProperty<Media::TextAlignment>& TextBox::TextAlignmentProperty
        = Register<TextBox, Media::TextAlignment>("TextAlignment", Media::TextAlignment::Left);
    StyledProperty<Layout::VerticalAlignment>& TextBox::VerticalContentAlignmentProperty
        = Register<TextBox, Layout::VerticalAlignment>("VerticalContentAlignment", Layout::VerticalAlignment::Center);
    StyledProperty<bool>& TextBox::IsReadOnlyProperty = Register<TextBox, bool>("IsReadOnly", false);

    TextBox::TextBox()
    {
        static const bool registered = []
        {
            AffectsMeasure<TextBox>(TextElement::FontFamilyProperty, TextElement::FontSizeProperty,
                TextElement::FontWeightProperty);
            AffectsRender<TextBox>(TextProperty, PlaceholderTextProperty, CaretBrushProperty, SelectionBrushProperty,
                TextAlignmentProperty, TextElement::ForegroundProperty, BackgroundProperty, BorderBrushProperty);
            return true;
        }();
        (void)registered;
        Focusable(true);
        Cursor(std::make_shared<Input::Cursor>(Input::StandardCursorType::Ibeam));
        // The Fluent theme's text box: a four-point corner, a thin edge, and
        // the body padding.
        CornerRadius(Avalonia::CornerRadius(4));
        BorderThickness(Thickness(1));
        Padding(Thickness(10, 5, 6, 6));
        MinHeight(32);
    }

    TextBox::~TextBox() = default;

    Media::Typeface TextBox::Face() const
    {
        return Media::Typeface(FontFamily(), FontStyle(), FontWeight());
    }

    std::shared_ptr<Media::TextLayout> TextBox::MakeLayout(const std::u32string& text) const
    {
        return std::make_shared<Media::TextLayout>(text, Face(), FontSize(), Foreground());
    }

    Rect TextBox::TextArea() const
    {
        return Rect(Bounds().GetSize()).Deflate(BorderThickness()).Deflate(Padding());
    }

    Size TextBox::MeasureOverride(Size availableSize)
    {
        const auto layout = MakeLayout(_chars.empty() ? std::u32string(U"W") : _chars);
        const Thickness t = BorderThickness();
        const Thickness p = Padding();
        const double width = std::isfinite(availableSize.Width) ? 0.0 : layout->Width();
        return Size{width, layout->Height()}.Inflate(Thickness{t.Left + p.Left, t.Top + p.Top, t.Right + p.Right,
            t.Bottom + p.Bottom});
    }

    Size TextBox::ArrangeOverride(Size finalSize)
    {
        return finalSize;
    }

    void TextBox::OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change)
    {
        TemplatedControl::OnPropertyChanged(change);
        if (&change.Property == &TextProperty && !_syncing)
        {
            _chars = Media::ToUtf32(Text());
            _caret = std::min(_caret, static_cast<std::int32_t>(_chars.size()));
            _anchor = _caret;
            KeepCaretVisible();
            TextChanged(*this);
        }
    }

    void TextBox::SetText(std::u32string text)
    {
        _chars = std::move(text);
        _syncing = true;
        Text(Media::ToUtf8(_chars));
        _syncing = false;
        InvalidateVisual();
        TextChanged(*this);
    }

    void TextBox::CaretIndex(std::int32_t value)
    {
        _caret = std::clamp(value, 0, static_cast<std::int32_t>(_chars.size()));
        _anchor = _caret;
        KeepCaretVisible();
        InvalidateVisual();
    }

    void TextBox::SelectAll()
    {
        _anchor = 0;
        _caret = static_cast<std::int32_t>(_chars.size());
        InvalidateVisual();
    }

    void TextBox::DeleteSelection()
    {
        if (_anchor == _caret)
        {
            return;
        }
        const std::int32_t from = std::min(_anchor, _caret);
        const std::int32_t to = std::max(_anchor, _caret);
        std::u32string text = _chars;
        text.erase(static_cast<std::size_t>(from), static_cast<std::size_t>(to - from));
        _caret = from;
        _anchor = from;
        SetText(std::move(text));
    }

    void TextBox::Insert(std::u32string_view text)
    {
        if (IsReadOnly())
        {
            return;
        }
        DeleteSelection();
        std::u32string insert(text);
        const std::int32_t max = MaxLength();
        if (max > 0)
        {
            const std::int32_t room = max - static_cast<std::int32_t>(_chars.size());
            if (room <= 0)
            {
                return;
            }
            if (static_cast<std::int32_t>(insert.size()) > room)
            {
                insert.resize(static_cast<std::size_t>(room));
            }
        }
        std::u32string next = _chars;
        next.insert(static_cast<std::size_t>(_caret), insert);
        _caret += static_cast<std::int32_t>(insert.size());
        _anchor = _caret;
        SetText(std::move(next));
        KeepCaretVisible();
        ResetBlink();
    }

    void TextBox::KeepCaretVisible()
    {
        const Rect area = TextArea();
        if (area.Width <= 0)
        {
            _scroll = 0;
            return;
        }
        const double x = MakeLayout(_chars)->CaretX(static_cast<std::size_t>(_caret));
        if (x - _scroll > area.Width)
        {
            _scroll = x - area.Width;
        }
        if (x - _scroll < 0)
        {
            _scroll = x;
        }
        _scroll = std::max(0.0, _scroll);
    }

    void TextBox::ResetBlink()
    {
        _caretVisible = true;
        if (_blink != nullptr)
        {
            _blink->Stop();
            _blink->Start();
        }
        InvalidateVisual();
    }

    std::size_t TextBox::IndexAt(Point point) const
    {
        const Rect area = TextArea();
        return MakeLayout(_chars)->HitTest(point.X - area.X + _scroll);
    }

    void TextBox::OnKeyDown(Input::KeyEventArgs& e)
    {
        using Input::Key;
        const bool shift = Input::HasFlag(e.KeyModifiers, Input::KeyModifiers::Shift);
        const bool control = Input::HasFlag(e.KeyModifiers, Input::KeyModifiers::Control)
            || Input::HasFlag(e.KeyModifiers, Input::KeyModifiers::Meta);
        const auto move = [&](std::int32_t to)
        {
            _caret = std::clamp(to, 0, static_cast<std::int32_t>(_chars.size()));
            if (!shift)
            {
                _anchor = _caret;
            }
            KeepCaretVisible();
            ResetBlink();
            e.Handled = true;
        };
        const auto wordLeft = [&]
        {
            std::int32_t i = _caret;
            while (i > 0 && _chars[static_cast<std::size_t>(i - 1)] == U' ')
            {
                i--;
            }
            while (i > 0 && _chars[static_cast<std::size_t>(i - 1)] != U' ')
            {
                i--;
            }
            return i;
        };
        const auto wordRight = [&]
        {
            std::int32_t i = _caret;
            const auto n = static_cast<std::int32_t>(_chars.size());
            while (i < n && _chars[static_cast<std::size_t>(i)] != U' ')
            {
                i++;
            }
            while (i < n && _chars[static_cast<std::size_t>(i)] == U' ')
            {
                i++;
            }
            return i;
        };
        switch (e.Key)
        {
        case Key::Left:
            if (!shift && _anchor != _caret)
            {
                move(std::min(_anchor, _caret));
            }
            else
            {
                move(control ? wordLeft() : _caret - 1);
            }
            return;
        case Key::Right:
            if (!shift && _anchor != _caret)
            {
                move(std::max(_anchor, _caret));
            }
            else
            {
                move(control ? wordRight() : _caret + 1);
            }
            return;
        case Key::Home:
            move(0);
            return;
        case Key::End:
            move(static_cast<std::int32_t>(_chars.size()));
            return;
        case Key::Back:
            if (IsReadOnly())
            {
                return;
            }
            if (_anchor != _caret)
            {
                DeleteSelection();
            }
            else if (_caret > 0)
            {
                const std::int32_t from = control ? wordLeft() : _caret - 1;
                std::u32string text = _chars;
                text.erase(static_cast<std::size_t>(from), static_cast<std::size_t>(_caret - from));
                _caret = from;
                _anchor = from;
                SetText(std::move(text));
            }
            KeepCaretVisible();
            ResetBlink();
            e.Handled = true;
            return;
        case Key::Delete:
            if (IsReadOnly())
            {
                return;
            }
            if (_anchor != _caret)
            {
                DeleteSelection();
            }
            else if (_caret < static_cast<std::int32_t>(_chars.size()))
            {
                const std::int32_t to = control ? wordRight() : _caret + 1;
                std::u32string text = _chars;
                text.erase(static_cast<std::size_t>(_caret), static_cast<std::size_t>(to - _caret));
                SetText(std::move(text));
            }
            ResetBlink();
            e.Handled = true;
            return;
        case Key::A:
            if (control)
            {
                SelectAll();
                e.Handled = true;
            }
            return;
        case Key::C:
        case Key::X:
            if (control && _anchor != _caret)
            {
                const std::int32_t from = std::min(_anchor, _caret);
                const std::int32_t to = std::max(_anchor, _caret);
                Clipboard::SetText(Media::ToUtf8(_chars.substr(static_cast<std::size_t>(from),
                    static_cast<std::size_t>(to - from))));
                if (e.Key == Key::X && !IsReadOnly())
                {
                    DeleteSelection();
                }
                e.Handled = true;
            }
            return;
        case Key::V:
            if (control)
            {
                std::u32string pasted = Media::ToUtf32(Clipboard::GetText());
                // One line: the box has no second one to put it on.
                std::erase_if(pasted, [](char32_t c) { return c == U'\r' || c == U'\n'; });
                Insert(pasted);
                e.Handled = true;
            }
            return;
        default:
            return;
        }
    }

    void TextBox::OnTextInput(Input::TextInputEventArgs& e)
    {
        if (!e.Text.has_value() || e.Text->empty())
        {
            return;
        }
        std::u32string text = Media::ToUtf32(*e.Text);
        std::erase_if(text, [](char32_t c) { return c < 0x20 || c == 0x7F; });
        if (!text.empty())
        {
            Insert(text);
        }
        e.Handled = true;
    }

    void TextBox::OnPointerPressed(Input::PointerPressedEventArgs& e)
    {
        Focus(Input::NavigationMethod::Pointer);
        const auto index = static_cast<std::int32_t>(IndexAt(e.GetPosition(this)));
        if (e.ClickCount >= 2)
        {
            SelectAll();
        }
        else
        {
            _caret = index;
            if (!Input::HasFlag(e.KeyModifiers(), Input::KeyModifiers::Shift))
            {
                _anchor = index;
            }
        }
        _selecting = true;
        e.Pointer->Capture(this);
        ResetBlink();
        e.Handled = true;
    }

    void TextBox::OnPointerMoved(Input::PointerEventArgs& e)
    {
        if (_selecting)
        {
            _caret = static_cast<std::int32_t>(IndexAt(e.GetPosition(this)));
            KeepCaretVisible();
            InvalidateVisual();
        }
    }

    void TextBox::OnPointerReleased(Input::PointerReleasedEventArgs& e)
    {
        _selecting = false;
        if (e.Pointer->Captured() == this)
        {
            e.Pointer->Capture(nullptr);
        }
    }

    void TextBox::OnPointerEntered(Input::PointerEventArgs& e)
    {
        (void)e;
        InvalidateVisual();
    }

    void TextBox::OnPointerExited(Input::PointerEventArgs& e)
    {
        (void)e;
        InvalidateVisual();
    }

    void TextBox::OnGotFocus(Input::GotFocusEventArgs& e)
    {
        (void)e;
        if (_blink == nullptr)
        {
            _blink = std::make_unique<Threading::DispatcherTimer>(Threading::DispatcherPriority::Normal);
            _blink->Interval(Threading::TimeSpan(0.5));
            _blink->Tick += [this](Threading::DispatcherTimer&)
            {
                _caretVisible = !_caretVisible;
                InvalidateVisual();
            };
        }
        _blink->Start();
        ResetBlink();
    }

    void TextBox::OnLostFocus(Input::FocusChangedEventArgs& e)
    {
        (void)e;
        if (_blink != nullptr)
        {
            _blink->Stop();
        }
        _anchor = _caret;
        InvalidateVisual();
    }

    void TextBox::Render(Media::DrawingContext& context)
    {
        const Rect bounds(Bounds().GetSize());
        // Fluent dark's text box: a faint fill that lifts under the pointer
        // and goes near black when focused, a hairline edge, and an accent
        // line along the bottom while focused.
        if (!TemplateChromeTransparent)
        {
            Media::IBrushPtr background = Background();
            if (background == nullptr || IsPointerOver() || IsFocused())
            {
                background = std::make_shared<Media::SolidColorBrush>(IsFocused()
                        ? Media::Color::FromArgb(0xB3, 0x1E, 0x1E, 0x1E)
                        : IsPointerOver() ? Media::Color::FromArgb(0x15, 0xFF, 0xFF, 0xFF)
                                          : Media::Color::FromArgb(0x0F, 0xFF, 0xFF, 0xFF));
            }
            Media::IBrushPtr border = BorderBrush();
            if (border == nullptr)
            {
                border = std::make_shared<Media::SolidColorBrush>(Media::Color::FromArgb(0x33, 0xFF, 0xFF, 0xFF));
            }
            RenderBorder(context, bounds, background, border, BorderThickness(), CornerRadius(), {});
            if (IsFocused())
            {
                context.FillRectangle(std::make_shared<Media::SolidColorBrush>(Media::Color::FromRgb(0x99, 0xEB, 0xFF)),
                    Rect{bounds.X + CornerRadius().BottomLeft, bounds.Bottom() - 2,
                        std::max(0.0, bounds.Width - CornerRadius().BottomLeft - CornerRadius().BottomRight), 2});
            }
        }
        else if (const Media::IBrushPtr background = Background())
        {
            context.FillRectangle(background, bounds);
        }
        const Rect area = TextArea();
        auto clip = context.PushClip(area);
        const bool empty = _chars.empty();
        const auto layout = MakeLayout(empty ? Media::ToUtf32(PlaceholderText()) : _chars);
        double y = area.Y;
        switch (VerticalContentAlignment())
        {
        case Layout::VerticalAlignment::Center:
        case Layout::VerticalAlignment::Stretch:
            y += (area.Height - layout->Height()) / 2;
            break;
        case Layout::VerticalAlignment::Bottom:
            y += area.Height - layout->Height();
            break;
        default:
            break;
        }
        double x = area.X - _scroll;
        if (TextAlignment() == Media::TextAlignment::Center)
        {
            x = area.X + (area.Width - layout->Width()) / 2;
        }
        else if (TextAlignment() == Media::TextAlignment::Right)
        {
            x = area.Right() - layout->Width();
        }
        if (!empty && _anchor != _caret && IsFocused())
        {
            const auto from = static_cast<std::size_t>(std::min(_anchor, _caret));
            const auto to = static_cast<std::size_t>(std::max(_anchor, _caret));
            Media::IBrushPtr selection = SelectionBrush();
            if (selection == nullptr)
            {
                selection = std::make_shared<Media::SolidColorBrush>(Media::Color::FromArgb(0xFF, 0x00, 0x78, 0xD4));
            }
            const double left = x + layout->CaretX(from);
            const double right = x + layout->CaretX(to);
            context.FillRectangle(selection, Rect{left, y, right - left, layout->Height()});
        }
        if (empty)
        {
            const Media::IBrushPtr dim = std::make_shared<Media::SolidColorBrush>(
                Media::Color::FromArgb(0x8B, 0xFF, 0xFF, 0xFF));
            layout->Draw(context.Canvas(), Point{x, y}, dim);
        }
        else
        {
            layout->Draw(context.Canvas(), Point{x, y});
        }
        if (IsFocused() && _caretVisible && !IsReadOnly())
        {
            Media::IBrushPtr caret = CaretBrush();
            if (caret == nullptr)
            {
                caret = Foreground();
            }
            const double caretX = std::round(x + (empty ? 0.0 : layout->CaretX(static_cast<std::size_t>(_caret))));
            context.FillRectangle(caret, Rect{caretX, y, 1, layout->Height()});
        }
    }

    std::string Clipboard::GetText()
    {
        return Read ? Read() : std::string();
    }

    void Clipboard::SetText(const std::string& text)
    {
        if (Write)
        {
            Write(text);
        }
    }
}
