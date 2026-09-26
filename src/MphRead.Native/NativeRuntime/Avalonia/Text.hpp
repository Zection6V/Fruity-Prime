#pragma once

// TextBlock and TextBox.

#include "Controls.hpp"
#include "Threading.hpp"

#include <memory>
#include <string>

namespace MphRead::NativeRuntime::Avalonia::Controls
{
    class TextBlock : public Control
    {
    public:
        TextBlock();
        static StyledProperty<std::string>& TextProperty;
        static StyledProperty<Media::IBrushPtr>& BackgroundProperty;
        static StyledProperty<Thickness>& PaddingProperty;
        static StyledProperty<Media::TextWrapping>& TextWrappingProperty;
        static StyledProperty<Media::TextTrimming>& TextTrimmingProperty;
        static StyledProperty<Media::TextAlignment>& TextAlignmentProperty;
        static StyledProperty<double>& LineHeightProperty;
        static StyledProperty<std::int32_t>& MaxLinesProperty;
        static StyledProperty<double>& LetterSpacingProperty;

        [[nodiscard]] std::string Text() const { return GetValue(TextProperty); }
        void Text(std::string value) { SetValue(TextProperty, std::move(value)); }
        [[nodiscard]] Media::IBrushPtr Background() const { return GetValue(BackgroundProperty); }
        void Background(Media::IBrushPtr value) { SetValue(BackgroundProperty, std::move(value)); }
        [[nodiscard]] Thickness Padding() const { return GetValue(PaddingProperty); }
        void Padding(Thickness value) { SetValue(PaddingProperty, value); }
        [[nodiscard]] Media::TextWrapping TextWrapping() const { return GetValue(TextWrappingProperty); }
        void TextWrapping(Media::TextWrapping value) { SetValue(TextWrappingProperty, value); }
        [[nodiscard]] Media::TextTrimming TextTrimming() const { return GetValue(TextTrimmingProperty); }
        void TextTrimming(Media::TextTrimming value) { SetValue(TextTrimmingProperty, value); }
        [[nodiscard]] Media::TextAlignment TextAlignment() const { return GetValue(TextAlignmentProperty); }
        void TextAlignment(Media::TextAlignment value) { SetValue(TextAlignmentProperty, value); }
        [[nodiscard]] double LineHeight() const { return GetValue(LineHeightProperty); }
        void LineHeight(double value) { SetValue(LineHeightProperty, value); }
        [[nodiscard]] std::int32_t MaxLines() const { return GetValue(MaxLinesProperty); }
        void MaxLines(std::int32_t value) { SetValue(MaxLinesProperty, value); }
        [[nodiscard]] double LetterSpacing() const { return GetValue(LetterSpacingProperty); }
        void LetterSpacing(double value) { SetValue(LetterSpacingProperty, value); }

        [[nodiscard]] Media::FontFamilyPtr FontFamily() const { return GetValue(TextElement::FontFamilyProperty); }
        void FontFamily(Media::FontFamilyPtr value) { SetValue(TextElement::FontFamilyProperty, std::move(value)); }
        [[nodiscard]] double FontSize() const { return GetValue(TextElement::FontSizeProperty); }
        void FontSize(double value) { SetValue(TextElement::FontSizeProperty, value); }
        [[nodiscard]] Media::FontWeight FontWeight() const { return GetValue(TextElement::FontWeightProperty); }
        void FontWeight(Media::FontWeight value) { SetValue(TextElement::FontWeightProperty, value); }
        [[nodiscard]] Media::FontStyle FontStyle() const { return GetValue(TextElement::FontStyleProperty); }
        void FontStyle(Media::FontStyle value) { SetValue(TextElement::FontStyleProperty, value); }
        [[nodiscard]] Media::IBrushPtr Foreground() const { return GetValue(TextElement::ForegroundProperty); }
        void Foreground(Media::IBrushPtr value) { SetValue(TextElement::ForegroundProperty, std::move(value)); }

        void Render(Media::DrawingContext& context) override;

    protected:
        Size MeasureOverride(Size availableSize) override;
        Size ArrangeOverride(Size finalSize) override;
        void OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change) override;
        [[nodiscard]] std::shared_ptr<Media::TextLayout> CreateTextLayout(double maxWidth, double maxHeight) const;

    private:
        std::shared_ptr<Media::TextLayout> _layout;
        Size _constraint{};
    };

    class TextBox : public TemplatedControl
    {
    public:
        TextBox();
        ~TextBox() override;

        static StyledProperty<std::string>& TextProperty;
        static StyledProperty<std::string>& PlaceholderTextProperty;
        static StyledProperty<std::int32_t>& MaxLengthProperty;
        static StyledProperty<Media::IBrushPtr>& CaretBrushProperty;
        static StyledProperty<Media::IBrushPtr>& SelectionBrushProperty;
        static StyledProperty<Media::TextAlignment>& TextAlignmentProperty;
        static StyledProperty<Layout::VerticalAlignment>& VerticalContentAlignmentProperty;
        static StyledProperty<bool>& IsReadOnlyProperty;

        [[nodiscard]] std::string Text() const { return GetValue(TextProperty); }
        void Text(std::string value) { SetValue(TextProperty, std::move(value)); }
        [[nodiscard]] std::string PlaceholderText() const { return GetValue(PlaceholderTextProperty); }
        void PlaceholderText(std::string value) { SetValue(PlaceholderTextProperty, std::move(value)); }
        [[nodiscard]] std::string Watermark() const { return PlaceholderText(); }
        void Watermark(std::string value) { PlaceholderText(std::move(value)); }
        [[nodiscard]] std::int32_t MaxLength() const { return GetValue(MaxLengthProperty); }
        void MaxLength(std::int32_t value) { SetValue(MaxLengthProperty, value); }
        [[nodiscard]] Media::IBrushPtr CaretBrush() const { return GetValue(CaretBrushProperty); }
        void CaretBrush(Media::IBrushPtr value) { SetValue(CaretBrushProperty, std::move(value)); }
        [[nodiscard]] Media::IBrushPtr SelectionBrush() const { return GetValue(SelectionBrushProperty); }
        void SelectionBrush(Media::IBrushPtr value) { SetValue(SelectionBrushProperty, std::move(value)); }
        [[nodiscard]] Media::TextAlignment TextAlignment() const { return GetValue(TextAlignmentProperty); }
        void TextAlignment(Media::TextAlignment value) { SetValue(TextAlignmentProperty, value); }
        [[nodiscard]] Layout::VerticalAlignment VerticalContentAlignment() const
        {
            return GetValue(VerticalContentAlignmentProperty);
        }
        void VerticalContentAlignment(Layout::VerticalAlignment value) { SetValue(VerticalContentAlignmentProperty, value); }
        [[nodiscard]] bool IsReadOnly() const { return GetValue(IsReadOnlyProperty); }
        void IsReadOnly(bool value) { SetValue(IsReadOnlyProperty, value); }

        [[nodiscard]] std::int32_t CaretIndex() const noexcept { return _caret; }
        void CaretIndex(std::int32_t value);
        void SelectAll();

        // What a style replacing the Fluent template's border would set, in
        // every state: the chrome drawn transparent with no edge. DeckField's
        // four Styles are exactly this.
        bool TemplateChromeTransparent = false;

        Event<TextBox&> TextChanged;

        void Render(Media::DrawingContext& context) override;

    protected:
        Size MeasureOverride(Size availableSize) override;
        Size ArrangeOverride(Size finalSize) override;
        void OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change) override;
        void OnKeyDown(Input::KeyEventArgs& e) override;
        void OnTextInput(Input::TextInputEventArgs& e) override;
        void OnPointerPressed(Input::PointerPressedEventArgs& e) override;
        void OnPointerMoved(Input::PointerEventArgs& e) override;
        void OnPointerReleased(Input::PointerReleasedEventArgs& e) override;
        void OnGotFocus(Input::GotFocusEventArgs& e) override;
        void OnLostFocus(Input::FocusChangedEventArgs& e) override;
        void OnPointerEntered(Input::PointerEventArgs& e) override;
        void OnPointerExited(Input::PointerEventArgs& e) override;

    private:
        [[nodiscard]] Media::Typeface Face() const;
        [[nodiscard]] std::shared_ptr<Media::TextLayout> MakeLayout(const std::u32string& text) const;
        [[nodiscard]] Rect TextArea() const;
        [[nodiscard]] std::size_t IndexAt(Point point) const;
        void Insert(std::u32string_view text);
        void DeleteSelection();
        void SetText(std::u32string text);
        void KeepCaretVisible();
        void ResetBlink();

        std::u32string _chars;
        std::int32_t _caret = 0;
        std::int32_t _anchor = 0;
        bool _selecting = false;
        double _scroll = 0;
        bool _caretVisible = true;
        bool _syncing = false;
        std::unique_ptr<Threading::DispatcherTimer> _blink;
    };

    // The system clipboard, as TopLevel.Clipboard gives it.
    class Clipboard final
    {
    public:
        Clipboard() = delete;
        [[nodiscard]] static std::string GetText();
        static void SetText(const std::string& text);
        // The host provides these; nothing is copied anywhere until it does.
        static inline std::function<std::string()> Read{};
        static inline std::function<void(const std::string&)> Write{};
    };
}
