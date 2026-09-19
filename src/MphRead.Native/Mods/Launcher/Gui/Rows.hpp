#pragma once

#include "GuiTheme.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    class RowsNullReferenceException final : public std::runtime_error
    {
    public:
        RowsNullReferenceException();
    };

    class RowsArgumentException final : public std::invalid_argument
    {
    public:
        RowsArgumentException();
    };

    class RowsDivideByZeroException final : public std::runtime_error
    {
    public:
        RowsDivideByZeroException();
    };

    enum class RowsCulture : std::uint8_t
    {
        Invariant
    };

    enum class RowsFlowDirection : std::uint8_t
    {
        LeftToRight
    };

    enum class RowsTextTrimming : std::uint8_t
    {
        CharacterEllipsis
    };

    enum class RowsKey : std::uint8_t
    {
        Other,
        Left,
        Right,
        Enter,
        Space
    };

    enum class RowsVerticalAlignment : std::uint8_t
    {
        Center
    };

    enum class RowsHorizontalAlignment : std::uint8_t
    {
        Left,
        Right
    };

    enum class RowsTextWrapping : std::uint8_t
    {
        Wrap
    };

    enum class RowsBrushKind : std::uint8_t
    {
        Transparent,
        PanelLightBrush,
        EdgeBrush,
        TextBrush,
        TextDimBrush,
        AccentBrush,
        SolidColor
    };

    struct RowsPoint final
    {
        double X;
        double Y;
    };

    struct RowsSize final
    {
        double Width;
        double Height;
    };

    struct RowsThickness final
    {
        double Left;
        double Top;
        double Right;
        double Bottom;
    };

    struct RowsBrush final
    {
        RowsBrushKind Kind;
        GuiColor Color{};

        [[nodiscard]] static constexpr RowsBrush Transparent() noexcept
        {
            return RowsBrush{RowsBrushKind::Transparent, {}};
        }

        [[nodiscard]] static constexpr RowsBrush PanelLight() noexcept
        {
            return RowsBrush{RowsBrushKind::PanelLightBrush, {}};
        }

        [[nodiscard]] static constexpr RowsBrush Edge() noexcept
        {
            return RowsBrush{RowsBrushKind::EdgeBrush, {}};
        }

        [[nodiscard]] static constexpr RowsBrush Text() noexcept
        {
            return RowsBrush{RowsBrushKind::TextBrush, {}};
        }

        [[nodiscard]] static constexpr RowsBrush TextDim() noexcept
        {
            return RowsBrush{RowsBrushKind::TextDimBrush, {}};
        }

        [[nodiscard]] static constexpr RowsBrush Accent() noexcept
        {
            return RowsBrush{RowsBrushKind::AccentBrush, {}};
        }

        [[nodiscard]] static constexpr RowsBrush Solid(GuiColor color) noexcept
        {
            return RowsBrush{RowsBrushKind::SolidColor, color};
        }

        friend constexpr bool operator==(const RowsBrush&, const RowsBrush&) noexcept = default;
    };

    struct RowsPen final
    {
        RowsBrush Brush;
        double Thickness;
    };

    struct RowsRoundedRect final
    {
        GuiRect Rect;
        double Radius;
    };

    struct RowsTriangleGeometry final
    {
        std::array<RowsPoint, 3> Points;
        bool IsFilled;
        bool IsClosed;
    };

    struct RowsFormattedText final
    {
        std::shared_ptr<const void> Native;
        double Width;
        double Height;
    };

    struct RowsPointerEventArgs final
    {
        void* Native = nullptr;
    };

    struct RowsKeyEventArgs final
    {
        void* Native = nullptr;
        RowsKey Key = RowsKey::Other;
        bool Handled = false;
    };

    struct RowsEventArgs final
    {
        static const RowsEventArgs Empty;
    };

    struct RowsEventHandler final
    {
        using Callback = void (*)(void* context, void* sender, const RowsEventArgs& args);

        void* Context = nullptr;
        Callback Function = nullptr;

        friend constexpr bool operator==(
            const RowsEventHandler&, const RowsEventHandler&) noexcept = default;
    };

    class RowsEvent final
    {
    public:
        void Add(RowsEventHandler handler);
        void Remove(RowsEventHandler handler);
        void Invoke(void* sender, const RowsEventArgs& args) const;

    private:
        mutable std::mutex _mutex;
        std::vector<RowsEventHandler> _handlers;
    };

    class RowsStringList
    {
    public:
        virtual ~RowsStringList() = default;

        [[nodiscard]] virtual std::int32_t Count() const = 0;
        [[nodiscard]] virtual std::optional<std::u16string> At(std::int32_t index) const = 0;
    };

    using RowsStringListRef = std::shared_ptr<RowsStringList>;

    class RowsDrawingContext
    {
    public:
        virtual ~RowsDrawingContext() = default;

        virtual void DrawText(const RowsFormattedText& text, RowsPoint point) = 0;
        virtual void DrawLine(RowsPen pen, RowsPoint start, RowsPoint end) = 0;
        virtual void FillRectangle(RowsBrush brush, GuiRect rect, double radius = 0.0) = 0;
        virtual void DrawRectangle(RowsBrush brush, std::optional<RowsPen> pen,
            RowsRoundedRect rect) = 0;
        virtual void DrawEllipse(RowsBrush brush, std::optional<RowsPen> pen,
            RowsPoint center, double radiusX, double radiusY) = 0;
        virtual void DrawGeometry(RowsBrush brush, std::optional<RowsPen> pen,
            const RowsTriangleGeometry& geometry) = 0;
    };

    class RowsControlAdapter
    {
    public:
        virtual ~RowsControlAdapter() = default;

        virtual void SetHeight(double height) = 0;
        [[nodiscard]] virtual double GetHeight() const = 0;
        virtual void SetIsVisible(bool isVisible) = 0;
        [[nodiscard]] virtual bool GetIsVisible() const = 0;
        virtual void SetMargin(RowsThickness margin) = 0;
        virtual void InvalidateVisual() = 0;
    };

    class RowsTextControlAdapter : public RowsControlAdapter
    {
    public:
        [[nodiscard]] virtual GuiRect Bounds() const = 0;
        [[nodiscard]] virtual RowsSize BaseMeasureOverride(RowsSize availableSize) = 0;
        [[nodiscard]] virtual std::u16string ToUpperInvariant(std::u16string_view text) = 0;

        [[nodiscard]] virtual RowsFormattedText CreateFormattedText(
            std::optional<std::u16string_view> text, RowsCulture culture,
            RowsFlowDirection flowDirection, GuiTypeface typeface,
            double fontSize, RowsBrush brush) = 0;
        virtual void SetFormattedTextMaxTextWidth(
            RowsFormattedText& text, double maxTextWidth) = 0;
        virtual void SetFormattedTextTrimming(
            RowsFormattedText& text, RowsTextTrimming trimming) = 0;
    };

    class RowsInteractiveControlAdapter : public RowsTextControlAdapter
    {
    public:
        virtual void SetFocusable(bool focusable) = 0;
        virtual void SetHandCursor() = 0;
        virtual void Focus() = 0;
        [[nodiscard]] virtual bool IsFocused() const = 0;
        [[nodiscard]] virtual RowsPoint GetPosition(const RowsPointerEventArgs& e) const = 0;

        virtual void BaseOnPointerMoved(RowsPointerEventArgs& e) = 0;
        virtual void BaseOnPointerExited(RowsPointerEventArgs& e) = 0;
        virtual void BaseOnPointerPressed(RowsPointerEventArgs& e) = 0;
        virtual void BaseOnKeyDown(RowsKeyEventArgs& e) = 0;
    };

    class RowsControl
    {
    public:
        [[nodiscard]] double Height() const;
        void Height(double value);

        [[nodiscard]] bool IsVisible() const;
        void IsVisible(bool value);

        void Margin(RowsThickness value);
        void InvalidateVisual();

    protected:
        explicit RowsControl(RowsControlAdapter& control) noexcept;
        RowsControlAdapter& _control;
    };

    class Caption final : public RowsControl
    {
    public:
        Caption(RowsTextControlAdapter& control, std::optional<std::u16string> text);

        Caption(const Caption&) = delete;
        Caption& operator=(const Caption&) = delete;
        Caption(Caption&&) = delete;
        Caption& operator=(Caption&&) = delete;

        [[nodiscard]] RowsSize MeasureOverride(RowsSize availableSize);
        void Render(RowsDrawingContext& context);

    private:
        [[nodiscard]] RowsFormattedText Label();

        RowsTextControlAdapter& _textControl;
        std::optional<std::u16string> _text;
    };

    class ChoiceRow final : public RowsControl
    {
    public:
        using PreviewHandler = std::function<void(RowsDrawingContext& context, GuiRect area)>;

        ChoiceRow(RowsInteractiveControlAdapter& control,
            std::optional<std::u16string> label, RowsStringListRef options,
            std::int32_t index = 0);

        ChoiceRow(const ChoiceRow&) = delete;
        ChoiceRow& operator=(const ChoiceRow&) = delete;
        ChoiceRow(ChoiceRow&&) = delete;
        ChoiceRow& operator=(ChoiceRow&&) = delete;

        [[nodiscard]] std::int32_t Index() const noexcept;
        void Index(std::int32_t value);
        [[nodiscard]] std::optional<std::u16string> Value() const;

        void SetItems(RowsStringListRef options, std::int32_t index = 0);

        [[nodiscard]] const PreviewHandler& Preview() const noexcept;
        void Preview(PreviewHandler value);

        RowsEvent& Changed() noexcept;
        const RowsEvent& Changed() const noexcept;
        void AddChanged(RowsEventHandler handler);
        void RemoveChanged(RowsEventHandler handler);

        void OnPointerMoved(RowsPointerEventArgs& e);
        void OnPointerExited(RowsPointerEventArgs& e);
        void OnPointerPressed(RowsPointerEventArgs& e);
        void OnKeyDown(RowsKeyEventArgs& e);
        void Render(RowsDrawingContext& context);

    private:
        static constexpr double ArrowWidth = 28.0;
        static constexpr double ValueColumn = 180.0;
        static constexpr double PreviewWidth = 52.0;

        [[nodiscard]] RowsStringList& Options() const;
        [[nodiscard]] double PreviewRoom() const noexcept;
        [[nodiscard]] GuiRect LeftArrow() const;
        [[nodiscard]] GuiRect RightArrow() const;
        void Step(std::int32_t direction);
        static void Arrow(RowsDrawingContext& context, GuiRect area,
            bool pointsLeft, bool hot);

        RowsInteractiveControlAdapter& _interactive;
        std::optional<std::u16string> _label;
        RowsStringListRef _options;
        std::int32_t _index;
        bool _leftHot = false;
        bool _rightHot = false;
        RowsEvent _changed;
        PreviewHandler _preview;
    };

    class ToggleRow final : public RowsControl
    {
    public:
        ToggleRow(RowsInteractiveControlAdapter& control,
            std::optional<std::u16string> label, bool on);

        ToggleRow(const ToggleRow&) = delete;
        ToggleRow& operator=(const ToggleRow&) = delete;
        ToggleRow(ToggleRow&&) = delete;
        ToggleRow& operator=(ToggleRow&&) = delete;

        [[nodiscard]] bool On() const noexcept;
        void On(bool value);

        RowsEvent& Changed() noexcept;
        const RowsEvent& Changed() const noexcept;
        void AddChanged(RowsEventHandler handler);
        void RemoveChanged(RowsEventHandler handler);

        void OnPointerPressed(RowsPointerEventArgs& e);
        void OnKeyDown(RowsKeyEventArgs& e);
        void Render(RowsDrawingContext& context);

    private:
        RowsInteractiveControlAdapter& _interactive;
        std::optional<std::u16string> _label;
        bool _on;
        RowsEvent _changed;
    };

    class FieldRowAdapter : public RowsControlAdapter
    {
    public:
        using ElementHandle = void*;

        [[nodiscard]] virtual ElementHandle CreateTextBlock() = 0;
        virtual void SetTextBlockText(ElementHandle textBlock,
            std::optional<std::u16string_view> text) = 0;
        virtual void SetTextBlockFontFamily(ElementHandle textBlock, GuiFontFamily family) = 0;
        virtual void SetTextBlockFontSize(ElementHandle textBlock, double fontSize) = 0;
        virtual void SetTextBlockForeground(ElementHandle textBlock, RowsBrush brush) = 0;
        virtual void SetTextBlockVerticalAlignment(
            ElementHandle textBlock, RowsVerticalAlignment alignment) = 0;
        virtual void SetTextBlockHorizontalAlignment(
            ElementHandle textBlock, RowsHorizontalAlignment alignment) = 0;
        virtual void SetTextBlockMargin(ElementHandle textBlock, RowsThickness margin) = 0;

        [[nodiscard]] virtual ElementHandle CreateTextBox() = 0;
        virtual void SetTextBoxText(ElementHandle textBox,
            std::optional<std::u16string_view> text) = 0;
        [[nodiscard]] virtual std::optional<std::u16string> GetTextBoxText(
            ElementHandle textBox) const = 0;
        virtual void SetTextBoxWidth(ElementHandle textBox, double width) = 0;
        virtual void SetTextBoxFontFamily(ElementHandle textBox, GuiFontFamily family) = 0;
        virtual void SetTextBoxFontSize(ElementHandle textBox, double fontSize) = 0;
        virtual void SetTextBoxCornerRadius(ElementHandle textBox, double radius) = 0;
        virtual void SetTextBoxPadding(ElementHandle textBox, RowsThickness padding) = 0;
        virtual void SetTextBoxVerticalAlignment(
            ElementHandle textBox, RowsVerticalAlignment alignment) = 0;
        virtual void SetTextBoxHorizontalAlignment(
            ElementHandle textBox, RowsHorizontalAlignment alignment) = 0;
        virtual void SetTextBoxWatermark(ElementHandle textBox,
            std::optional<std::u16string_view> watermark) = 0;
        virtual void AddTextBoxLostFocus(ElementHandle textBox, RowsEventHandler handler) = 0;
        virtual void RemoveTextBoxLostFocus(ElementHandle textBox, RowsEventHandler handler) = 0;

        virtual void AddChild(ElementHandle child) = 0;
    };

    class FieldRowTextBox final
    {
    public:
        FieldRowTextBox(FieldRowAdapter& adapter, FieldRowAdapter::ElementHandle handle) noexcept;

        [[nodiscard]] FieldRowAdapter::ElementHandle Native() const noexcept;
        [[nodiscard]] std::optional<std::u16string> Text() const;
        void Text(std::optional<std::u16string_view> value);
        void Watermark(std::optional<std::u16string_view> value);
        void AddLostFocus(RowsEventHandler handler);
        void RemoveLostFocus(RowsEventHandler handler);

    private:
        FieldRowAdapter& _adapter;
        FieldRowAdapter::ElementHandle _handle;
    };

    class FieldRow final : public RowsControl
    {
    public:
        FieldRow(FieldRowAdapter& adapter, std::optional<std::u16string> label,
            std::optional<std::u16string> value, double boxWidth = 150.0);

        FieldRow(const FieldRow&) = delete;
        FieldRow& operator=(const FieldRow&) = delete;
        FieldRow(FieldRow&&) = delete;
        FieldRow& operator=(FieldRow&&) = delete;

        [[nodiscard]] FieldRowTextBox Box() noexcept;
        [[nodiscard]] std::u16string Value() const;
        void Value(std::optional<std::u16string_view> value);

    private:
        FieldRowAdapter& _fieldAdapter;
        FieldRowAdapter::ElementHandle _box;
    };

    class NoteAdapter : public RowsControlAdapter
    {
    public:
        virtual void SetText(std::optional<std::u16string_view> text) = 0;
        [[nodiscard]] virtual std::optional<std::u16string> GetText() const = 0;
        virtual void SetFontFamily(GuiFontFamily family) = 0;
        virtual void SetFontSize(double fontSize) = 0;
        virtual void SetForeground(RowsBrush brush) = 0;
        [[nodiscard]] virtual RowsBrush GetForeground() const = 0;
        virtual void SetTextWrapping(RowsTextWrapping wrapping) = 0;
    };

    class Note final : public RowsControl
    {
    public:
        Note(NoteAdapter& adapter, std::optional<std::u16string> text,
            std::optional<GuiColor> color = std::nullopt);

        Note(const Note&) = delete;
        Note& operator=(const Note&) = delete;
        Note(Note&&) = delete;
        Note& operator=(Note&&) = delete;

        [[nodiscard]] std::optional<std::u16string> Text() const;
        void Text(std::optional<std::u16string_view> value);

        [[nodiscard]] RowsBrush Foreground() const;
        void Foreground(RowsBrush value);

    private:
        NoteAdapter& _noteAdapter;
    };
}
