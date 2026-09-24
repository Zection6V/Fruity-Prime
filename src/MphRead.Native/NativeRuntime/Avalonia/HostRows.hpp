#pragma once

// The settings rows and the server list, in the Avalonia controls they were
// written against: a caption, a note, a choice row, a toggle, a field with a
// text box in it, and the server header and row.
//
// The text box is the one control here that Avalonia supplies whole and the
// launcher never drew itself, so it is written out: a single line, a caret, a
// watermark and a lost-focus event.

#include "HostControls.hpp"

#include "../../Mods/Launcher/Gui/Rows.hpp"
#include "../../Mods/Launcher/Gui/ServerRow.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MphRead::NativeRuntime::Avalonia
{
    // A run measured for the row controls. Avalonia's FormattedText carries
    // the trimming and the width it is allowed, so this does too.
    struct RowsRun final
    {
        std::string Text;
        double FontSize = 0.0;
        bool Bold = false;
        Launcher::GuiColor Color{};
        double MaxTextWidth = 0.0;
        bool Trim = false;
    };

    // The item list a choice row reads, over strings the caller owns.
    [[nodiscard]] Launcher::RowsStringListRef MakeStringList(
        std::vector<std::u16string> items);

    // One line of editable text: Avalonia's TextBox, as the field rows use it.
    class TextBoxHost final : public HostControl
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create();

        [[nodiscard]] const std::u16string& Text() const noexcept { return _text; }
        void Text(std::u16string value);
        void Watermark(std::u16string value) { _watermark = std::move(value); }
        void LostFocus(std::function<void()> action);

        [[nodiscard]] Toolkit::Size Measure(
            Toolkit::Element& element, Toolkit::Size available) override;
        void Render(Toolkit::Element& element, Toolkit::Renderer& renderer) override;

        double FontSize = 13.0;
        double CornerRadius = 6.0;
        Toolkit::Thickness Padding{8.0, 4.0, 8.0, 4.0};

    private:
        void Insert(char32_t code);
        void Key(Toolkit::KeyEvent& e);

        std::u16string _text;
        std::u16string _watermark;
        std::function<void()> _lostFocus;
        std::size_t _caret = 0;
    };

    // The shared half of every row control: the height, the visibility and
    // the margin Avalonia's Control carries.
    class RowsHost : public HostControl, public Launcher::RowsControlAdapter
    {
    public:
        void SetHeight(double height) override;
        [[nodiscard]] double GetHeight() const override;
        void SetIsVisible(bool isVisible) override;
        [[nodiscard]] bool GetIsVisible() const override;
        void SetMargin(Launcher::RowsThickness margin) override;
        void InvalidateVisual() override;
    };

    // The half a drawn row adds: measurement and formatted text.
    class RowsTextHost : public RowsHost, public Launcher::RowsTextControlAdapter
    {
    public:
        [[nodiscard]] Launcher::GuiRect Bounds() const override;
        [[nodiscard]] Launcher::RowsSize BaseMeasureOverride(
            Launcher::RowsSize availableSize) override;
        [[nodiscard]] std::u16string ToUpperInvariant(
            std::u16string_view text) override;
        [[nodiscard]] Launcher::RowsFormattedText CreateFormattedText(
            std::optional<std::u16string_view> text, Launcher::RowsCulture culture,
            Launcher::RowsFlowDirection flowDirection, Launcher::GuiTypeface typeface,
            double fontSize, Launcher::RowsBrush brush) override;
        void SetFormattedTextMaxTextWidth(
            Launcher::RowsFormattedText& text, double maxTextWidth) override;
        void SetFormattedTextTrimming(Launcher::RowsFormattedText& text,
            Launcher::RowsTextTrimming trimming) override;

        // RowsControlAdapter, again: the two bases both declare it.
        void SetHeight(double height) override { RowsHost::SetHeight(height); }
        [[nodiscard]] double GetHeight() const override { return RowsHost::GetHeight(); }
        void SetIsVisible(bool isVisible) override { RowsHost::SetIsVisible(isVisible); }
        [[nodiscard]] bool GetIsVisible() const override
        {
            return RowsHost::GetIsVisible();
        }
        void SetMargin(Launcher::RowsThickness margin) override
        {
            RowsHost::SetMargin(margin);
        }
        void InvalidateVisual() override { RowsHost::InvalidateVisual(); }
    };

    // And the half an interactive row adds.
    class RowsInteractiveHost : public RowsTextHost,
                                public Launcher::RowsInteractiveControlAdapter
    {
    public:
        void SetFocusable(bool focusable) override;
        void SetHandCursor() override;
        void Focus() override;
        [[nodiscard]] bool IsFocused() const override;
        [[nodiscard]] Launcher::RowsPoint GetPosition(
            const Launcher::RowsPointerEventArgs& e) const override;
        void BaseOnPointerMoved(Launcher::RowsPointerEventArgs& e) override;
        void BaseOnPointerExited(Launcher::RowsPointerEventArgs& e) override;
        void BaseOnPointerPressed(Launcher::RowsPointerEventArgs& e) override;
        void BaseOnKeyDown(Launcher::RowsKeyEventArgs& e) override;

        // The text half, forwarded once more.
        [[nodiscard]] Launcher::GuiRect Bounds() const override
        {
            return RowsTextHost::Bounds();
        }
        [[nodiscard]] Launcher::RowsSize BaseMeasureOverride(
            Launcher::RowsSize availableSize) override
        {
            return RowsTextHost::BaseMeasureOverride(availableSize);
        }
        [[nodiscard]] std::u16string ToUpperInvariant(std::u16string_view text) override
        {
            return RowsTextHost::ToUpperInvariant(text);
        }
        [[nodiscard]] Launcher::RowsFormattedText CreateFormattedText(
            std::optional<std::u16string_view> text, Launcher::RowsCulture culture,
            Launcher::RowsFlowDirection flowDirection, Launcher::GuiTypeface typeface,
            double fontSize, Launcher::RowsBrush brush) override
        {
            return RowsTextHost::CreateFormattedText(
                text, culture, flowDirection, typeface, fontSize, brush);
        }
        void SetFormattedTextMaxTextWidth(
            Launcher::RowsFormattedText& text, double maxTextWidth) override
        {
            RowsTextHost::SetFormattedTextMaxTextWidth(text, maxTextWidth);
        }
        void SetFormattedTextTrimming(Launcher::RowsFormattedText& text,
            Launcher::RowsTextTrimming trimming) override
        {
            RowsTextHost::SetFormattedTextTrimming(text, trimming);
        }
        void SetHeight(double height) override { RowsTextHost::SetHeight(height); }
        [[nodiscard]] double GetHeight() const override
        {
            return RowsTextHost::GetHeight();
        }
        void SetIsVisible(bool isVisible) override
        {
            RowsTextHost::SetIsVisible(isVisible);
        }
        [[nodiscard]] bool GetIsVisible() const override
        {
            return RowsTextHost::GetIsVisible();
        }
        void SetMargin(Launcher::RowsThickness margin) override
        {
            RowsTextHost::SetMargin(margin);
        }
        void InvalidateVisual() override { RowsTextHost::InvalidateVisual(); }
    };

    class CaptionHost final : public RowsTextHost
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create(std::u16string text);

        [[nodiscard]] Launcher::Caption& Caption() const noexcept { return *_caption; }

        [[nodiscard]] Toolkit::Size Measure(
            Toolkit::Element& element, Toolkit::Size available) override;
        void Render(Toolkit::Element& element, Toolkit::Renderer& renderer) override;

    private:
        std::unique_ptr<Launcher::Caption> _caption;
    };

    class ChoiceRowHost final : public RowsInteractiveHost
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create(std::u16string label,
            const std::vector<std::string>& items, std::int32_t index);
        [[nodiscard]] static Toolkit::ElementPtr CreateFromList(std::u16string label,
            Launcher::RowsStringListRef options, std::int32_t index);

        [[nodiscard]] Launcher::ChoiceRow& Row() const noexcept { return *_row; }
        void Changed(std::function<void()> action);

        [[nodiscard]] Toolkit::Size Measure(
            Toolkit::Element& element, Toolkit::Size available) override;
        void Render(Toolkit::Element& element, Toolkit::Renderer& renderer) override;

    private:
        std::unique_ptr<Launcher::ChoiceRow> _row;
        std::function<void()> _changed;
    };

    class ToggleRowHost final : public RowsInteractiveHost
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create(std::u16string label, bool on);

        [[nodiscard]] Launcher::ToggleRow& Row() const noexcept { return *_row; }
        void Changed(std::function<void()> action);

        [[nodiscard]] Toolkit::Size Measure(
            Toolkit::Element& element, Toolkit::Size available) override;
        void Render(Toolkit::Element& element, Toolkit::Renderer& renderer) override;

    private:
        std::unique_ptr<Launcher::ToggleRow> _row;
        std::function<void()> _changed;
    };

    // A field row is a stack panel Avalonia fills with a label and a text box,
    // so its host is a panel rather than a drawn control.
    class FieldRowHost final : public Launcher::FieldRowAdapter
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create(std::u16string label,
            std::u16string value, double boxWidth);

        [[nodiscard]] Launcher::FieldRow& Row() const noexcept { return *_row; }
        void LostFocus(std::function<void()> action);

        void SetHeight(double height) override;
        [[nodiscard]] double GetHeight() const override;
        void SetIsVisible(bool isVisible) override;
        [[nodiscard]] bool GetIsVisible() const override;
        void SetMargin(Launcher::RowsThickness margin) override;
        void InvalidateVisual() override;

        [[nodiscard]] ElementHandle CreateTextBlock() override;
        void SetTextBlockText(ElementHandle textBlock,
            std::optional<std::u16string_view> text) override;
        void SetTextBlockFontFamily(
            ElementHandle textBlock, Launcher::GuiFontFamily family) override;
        void SetTextBlockFontSize(ElementHandle textBlock, double fontSize) override;
        void SetTextBlockForeground(
            ElementHandle textBlock, Launcher::RowsBrush brush) override;
        void SetTextBlockVerticalAlignment(ElementHandle textBlock,
            Launcher::RowsVerticalAlignment alignment) override;
        void SetTextBlockHorizontalAlignment(ElementHandle textBlock,
            Launcher::RowsHorizontalAlignment alignment) override;
        void SetTextBlockMargin(
            ElementHandle textBlock, Launcher::RowsThickness margin) override;
        [[nodiscard]] ElementHandle CreateTextBox() override;
        void SetTextBoxText(
            ElementHandle textBox, std::optional<std::u16string_view> text) override;
        [[nodiscard]] std::optional<std::u16string> GetTextBoxText(
            ElementHandle textBox) const override;
        void SetTextBoxWidth(ElementHandle textBox, double width) override;
        void SetTextBoxFontFamily(
            ElementHandle textBox, Launcher::GuiFontFamily family) override;
        void SetTextBoxFontSize(ElementHandle textBox, double fontSize) override;
        void SetTextBoxCornerRadius(ElementHandle textBox, double radius) override;
        void SetTextBoxPadding(
            ElementHandle textBox, Launcher::RowsThickness padding) override;
        void SetTextBoxVerticalAlignment(ElementHandle textBox,
            Launcher::RowsVerticalAlignment alignment) override;
        void SetTextBoxHorizontalAlignment(ElementHandle textBox,
            Launcher::RowsHorizontalAlignment alignment) override;
        void SetTextBoxWatermark(ElementHandle textBox,
            std::optional<std::u16string_view> watermark) override;
        void AddTextBoxLostFocus(
            ElementHandle textBox, const Launcher::RowsEventHandler& handler) override;
        void RemoveTextBoxLostFocus(
            ElementHandle textBox, const Launcher::RowsEventHandler& handler) override;
        void AddChild(ElementHandle child) override;

    private:
        Toolkit::Element* _visual = nullptr;
        std::vector<Toolkit::ElementPtr> _owned;
        Toolkit::ElementPtr _box;
        std::unique_ptr<Launcher::FieldRow> _row;
        std::function<void()> _lostFocus;
    };

    // A note is a wrapping text block.
    class NoteHost final : public Launcher::NoteAdapter
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create(
            std::u16string text, std::optional<Launcher::GuiColor> color);

        [[nodiscard]] Launcher::Note& Note() const noexcept { return *_note; }

        void SetHeight(double height) override;
        [[nodiscard]] double GetHeight() const override;
        void SetIsVisible(bool isVisible) override;
        [[nodiscard]] bool GetIsVisible() const override;
        void SetMargin(Launcher::RowsThickness margin) override;
        void InvalidateVisual() override;

        void SetText(std::optional<std::u16string_view> text) override;
        [[nodiscard]] std::optional<std::u16string> GetText() const override;
        void SetFontFamily(Launcher::GuiFontFamily family) override;
        void SetFontSize(double fontSize) override;
        void SetForeground(std::optional<Launcher::RowsBrush> brush) override;
        [[nodiscard]] std::optional<Launcher::RowsBrush> GetForeground() const override;
        void SetTextWrapping(Launcher::RowsTextWrapping wrapping) override;

    private:
        Toolkit::Element* _visual = nullptr;
        std::unique_ptr<Launcher::Note> _note;
        std::optional<Launcher::RowsBrush> _foreground;
    };

    class ServerRowHost final : public HostControl,
                                public Launcher::ServerRowControlAdapter
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create(
            std::u16string name, std::u16string endpoint);

        [[nodiscard]] Launcher::ServerRow& Row() const noexcept { return *_row; }
        void Clicked(std::function<void()> action);

        [[nodiscard]] Toolkit::Size Measure(
            Toolkit::Element& element, Toolkit::Size available) override;
        void Render(Toolkit::Element& element, Toolkit::Renderer& renderer) override;

        void SetHeight(double height) override;
        [[nodiscard]] double GetHeight() const override;
        void SetFocusable(bool focusable) override;
        void SetHandCursor() override;
        void Focus() override;
        [[nodiscard]] bool IsFocused() const override;
        [[nodiscard]] Launcher::GuiRect Bounds() const override;
        [[nodiscard]] std::u16string FormatCurrentInt32(std::int32_t value) const override;
        void InvalidateVisual() override;
        void BaseOnPointerEntered(Launcher::ServerRowPointerEventArgs& e) override;
        void BaseOnPointerExited(Launcher::ServerRowPointerEventArgs& e) override;
        void BaseOnPointerPressed(
            Launcher::ServerRowPointerPressedEventArgs& e) override;
        void BaseOnKeyDown(Launcher::ServerRowKeyEventArgs& e) override;

    private:
        std::unique_ptr<Launcher::ServerRow> _row;
        std::function<void()> _clicked;
    };

    class ServerHeaderHost final : public HostControl,
                                   public Launcher::ServerHeaderControlAdapter
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create();

        [[nodiscard]] Toolkit::Size Measure(
            Toolkit::Element& element, Toolkit::Size available) override;
        void Render(Toolkit::Element& element, Toolkit::Renderer& renderer) override;

        void SetHeight(double height) override;
        [[nodiscard]] double GetHeight() const override;
        void SetIsHitTestVisible(bool isHitTestVisible) override;
        [[nodiscard]] Launcher::GuiRect Bounds() const override;

    private:
        std::unique_ptr<Launcher::ServerHeader> _header;
    };
}
