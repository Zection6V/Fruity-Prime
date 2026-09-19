#pragma once

#include "GuiTheme.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
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
        RowsArgumentException(std::int32_t min, std::int32_t max);
    };

    class RowsDivideByZeroException final : public std::runtime_error
    {
    public:
        RowsDivideByZeroException();
    };

    class RowsOverflowException final : public std::overflow_error
    {
    public:
        RowsOverflowException();
    };

    enum class RowsCulture : std::uint8_t { Invariant };
    enum class RowsFlowDirection : std::uint8_t { LeftToRight };
    enum class RowsTextTrimming : std::uint8_t { CharacterEllipsis };
    enum class RowsKey : std::uint8_t { Other, Left, Right, Enter, Space };
    enum class RowsVerticalAlignment : std::uint8_t { Center };
    enum class RowsHorizontalAlignment : std::uint8_t { Left, Right };
    enum class RowsTextWrapping : std::uint8_t { Wrap };
    enum class RowsPenLineCap : std::uint8_t { Flat };
    enum class RowsPenLineJoin : std::uint8_t { Miter };

    enum class RowsBrushKind : std::uint8_t
    {
        Transparent,
        PanelLightBrush,
        EdgeBrush,
        TextBrush,
        TextDimBrush,
        AccentBrush,
        WarmBrush,
        GoodBrush,
        BadBrush,
        MutableBrush,
        SolidColor
    };

    struct RowsPoint final { double X; double Y; };
    struct RowsSize final { double Width; double Height; };
    struct RowsThickness final { double Left; double Top; double Right; double Bottom; };

    class RowsBrush final
    {
    public:
        RowsBrush(const RowsBrush&) = default;
        RowsBrush& operator=(const RowsBrush&) = default;
        RowsBrush(RowsBrush&&) noexcept = default;
        RowsBrush& operator=(RowsBrush&&) noexcept = default;

        [[nodiscard]] static RowsBrush Transparent() noexcept;
        [[nodiscard]] static RowsBrush PanelLight() noexcept;
        [[nodiscard]] static RowsBrush Edge() noexcept;
        [[nodiscard]] static RowsBrush Text() noexcept;
        [[nodiscard]] static RowsBrush TextDim() noexcept;
        [[nodiscard]] static RowsBrush Accent() noexcept;
        [[nodiscard]] static RowsBrush Warm() noexcept;
        [[nodiscard]] static RowsBrush Good() noexcept;
        [[nodiscard]] static RowsBrush Bad() noexcept;
        [[nodiscard]] static RowsBrush Reference(GuiBrush& brush) noexcept;
        [[nodiscard]] static RowsBrush Solid(GuiColor color);

        [[nodiscard]] RowsBrushKind Kind() const noexcept;
        [[nodiscard]] GuiBrush* MutableBrush() const noexcept;
        [[nodiscard]] const void* Identity() const noexcept;
        [[nodiscard]] std::optional<GuiColor> CurrentColor() const noexcept;

        friend bool operator==(const RowsBrush& left, const RowsBrush& right) noexcept
        {
            return left.Identity() == right.Identity();
        }

    private:
        RowsBrush(RowsBrushKind kind, GuiBrush* shared, std::shared_ptr<GuiBrush> owned) noexcept;

        RowsBrushKind _kind;
        GuiBrush* _shared;
        std::shared_ptr<GuiBrush> _owned;
    };

    class RowsPen final
    {
    public:
        explicit RowsPen(std::optional<RowsBrush> brush, double thickness = 1.0);
        explicit RowsPen(RowsBrush brush, double thickness = 1.0);

        RowsPen(const RowsPen&) = default;
        RowsPen& operator=(const RowsPen&) = default;
        RowsPen(RowsPen&&) noexcept = default;
        RowsPen& operator=(RowsPen&&) noexcept = default;

        [[nodiscard]] std::optional<RowsBrush> Brush() const;
        void Brush(std::optional<RowsBrush> value);
        [[nodiscard]] double Thickness() const noexcept;
        void Thickness(double value) noexcept;
        [[nodiscard]] std::shared_ptr<void> DashStyle() const noexcept;
        void DashStyle(std::shared_ptr<void> value) noexcept;
        [[nodiscard]] RowsPenLineCap LineCap() const noexcept;
        void LineCap(RowsPenLineCap value) noexcept;
        [[nodiscard]] RowsPenLineJoin LineJoin() const noexcept;
        void LineJoin(RowsPenLineJoin value) noexcept;
        [[nodiscard]] double MiterLimit() const noexcept;
        void MiterLimit(double value) noexcept;
        [[nodiscard]] const void* Identity() const noexcept;

    private:
        struct State final
        {
            std::optional<RowsBrush> Brush;
            double Thickness;
            std::shared_ptr<void> DashStyle{};
            RowsPenLineCap LineCap = RowsPenLineCap::Flat;
            RowsPenLineJoin LineJoin = RowsPenLineJoin::Miter;
            double MiterLimit = 10.0;
        };
        std::shared_ptr<State> _state;
    };

    struct RowsRoundedRect final { GuiRect Rect; double Radius; };

    class RowsTriangleGeometry final
    {
    public:
        RowsTriangleGeometry(std::array<RowsPoint, 3> points, bool isFilled, bool isClosed);
        RowsTriangleGeometry(const RowsTriangleGeometry&) = default;
        RowsTriangleGeometry& operator=(const RowsTriangleGeometry&) = default;
        RowsTriangleGeometry(RowsTriangleGeometry&&) noexcept = default;
        RowsTriangleGeometry& operator=(RowsTriangleGeometry&&) noexcept = default;

        [[nodiscard]] const std::array<RowsPoint, 3>& Points() const noexcept;
        [[nodiscard]] bool IsFilled() const noexcept;
        [[nodiscard]] bool IsClosed() const noexcept;
        [[nodiscard]] const void* Identity() const noexcept;

    private:
        struct State final
        {
            std::array<RowsPoint, 3> Points;
            bool IsFilled;
            bool IsClosed;
        };
        std::shared_ptr<State> _state;
    };

    struct RowsFormattedText final
    {
        std::shared_ptr<void> Native;
        double Width;
        double Height;
    };

    struct RowsPointerEventArgs final { void* Native = nullptr; };
    struct RowsKeyEventArgs final { void* Native = nullptr; RowsKey Key = RowsKey::Other; bool Handled = false; };
    struct RowsEventArgs final { static const RowsEventArgs Empty; };

    class RowsEventHandler final
    {
    public:
        using Callback = void (*)(void* context, void* sender, const RowsEventArgs& args);

        RowsEventHandler() = default;
        RowsEventHandler(void* context, Callback function, std::shared_ptr<void> keepAlive = {});

        [[nodiscard]] static RowsEventHandler Static(Callback function);
        [[nodiscard]] static RowsEventHandler Instance(std::shared_ptr<void> target, Callback function);
        [[nodiscard]] static RowsEventHandler Combine(
            const RowsEventHandler& left, const RowsEventHandler& right);

        [[nodiscard]] bool IsNull() const noexcept;

        friend bool operator==(const RowsEventHandler& left, const RowsEventHandler& right) noexcept;

    private:
        struct Invocation final
        {
            void* Context;
            Callback Function;
            std::shared_ptr<void> KeepAlive;

            friend bool operator==(const Invocation& left, const Invocation& right) noexcept
            {
                return left.Context == right.Context && left.Function == right.Function;
            }
        };

        explicit RowsEventHandler(std::shared_ptr<const std::vector<Invocation>> invocations) noexcept;

        std::shared_ptr<const std::vector<Invocation>> _invocations;
        friend class RowsEvent;
    };

    class RowsEvent final
    {
    public:
        void Add(const RowsEventHandler& handler);
        void Remove(const RowsEventHandler& handler);
        void Invoke(void* sender, const RowsEventArgs& args) const;

    private:
        using Invocation = RowsEventHandler::Invocation;
        using InvocationList = std::vector<Invocation>;
        std::atomic<std::shared_ptr<const InvocationList>> _handlers{};
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
        virtual void DrawRectangle(RowsBrush brush, std::optional<RowsPen> pen, RowsRoundedRect rect) = 0;
        virtual void DrawEllipse(RowsBrush brush, std::optional<RowsPen> pen, RowsPoint center, double radiusX, double radiusY) = 0;
        virtual void DrawGeometry(RowsBrush brush, std::optional<RowsPen> pen, const RowsTriangleGeometry& geometry) = 0;
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
        virtual void SetFormattedTextMaxTextWidth(RowsFormattedText& text, double maxTextWidth) = 0;
        virtual void SetFormattedTextTrimming(RowsFormattedText& text, RowsTextTrimming trimming) = 0;
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
        using PreviewRef = std::shared_ptr<const PreviewHandler>;
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
        [[nodiscard]] PreviewRef Preview() const noexcept;
        void Preview(PreviewRef value);
        void AddChanged(const RowsEventHandler& handler);
        void RemoveChanged(const RowsEventHandler& handler);
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
        static void Arrow(RowsDrawingContext& context, GuiRect area, bool pointsLeft, bool hot);
        RowsInteractiveControlAdapter& _interactive;
        std::optional<std::u16string> _label;
        RowsStringListRef _options;
        std::int32_t _index;
        bool _leftHot = false;
        bool _rightHot = false;
        RowsEvent _changed;
        PreviewRef _preview;
    };

    class ToggleRow final : public RowsControl
    {
    public:
        ToggleRow(RowsInteractiveControlAdapter& control, std::optional<std::u16string> label, bool on);
        ToggleRow(const ToggleRow&) = delete;
        ToggleRow& operator=(const ToggleRow&) = delete;
        ToggleRow(ToggleRow&&) = delete;
        ToggleRow& operator=(ToggleRow&&) = delete;
        [[nodiscard]] bool On() const noexcept;
        void On(bool value);
        void AddChanged(const RowsEventHandler& handler);
        void RemoveChanged(const RowsEventHandler& handler);
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
        virtual void SetTextBlockText(ElementHandle textBlock, std::optional<std::u16string_view> text) = 0;
        virtual void SetTextBlockFontFamily(ElementHandle textBlock, GuiFontFamily family) = 0;
        virtual void SetTextBlockFontSize(ElementHandle textBlock, double fontSize) = 0;
        virtual void SetTextBlockForeground(ElementHandle textBlock, RowsBrush brush) = 0;
        virtual void SetTextBlockVerticalAlignment(ElementHandle textBlock, RowsVerticalAlignment alignment) = 0;
        virtual void SetTextBlockHorizontalAlignment(ElementHandle textBlock, RowsHorizontalAlignment alignment) = 0;
        virtual void SetTextBlockMargin(ElementHandle textBlock, RowsThickness margin) = 0;
        [[nodiscard]] virtual ElementHandle CreateTextBox() = 0;
        virtual void SetTextBoxText(ElementHandle textBox, std::optional<std::u16string_view> text) = 0;
        [[nodiscard]] virtual std::optional<std::u16string> GetTextBoxText(ElementHandle textBox) const = 0;
        virtual void SetTextBoxWidth(ElementHandle textBox, double width) = 0;
        virtual void SetTextBoxFontFamily(ElementHandle textBox, GuiFontFamily family) = 0;
        virtual void SetTextBoxFontSize(ElementHandle textBox, double fontSize) = 0;
        virtual void SetTextBoxCornerRadius(ElementHandle textBox, double radius) = 0;
        virtual void SetTextBoxPadding(ElementHandle textBox, RowsThickness padding) = 0;
        virtual void SetTextBoxVerticalAlignment(ElementHandle textBox, RowsVerticalAlignment alignment) = 0;
        virtual void SetTextBoxHorizontalAlignment(ElementHandle textBox, RowsHorizontalAlignment alignment) = 0;
        virtual void SetTextBoxWatermark(ElementHandle textBox, std::optional<std::u16string_view> watermark) = 0;
        virtual void AddTextBoxLostFocus(ElementHandle textBox, const RowsEventHandler& handler) = 0;
        virtual void RemoveTextBoxLostFocus(ElementHandle textBox, const RowsEventHandler& handler) = 0;
        virtual void AddChild(ElementHandle child) = 0;
    };

    class FieldRowTextBox final
    {
    public:
        FieldRowTextBox(FieldRowAdapter& adapter, FieldRowAdapter::ElementHandle handle) noexcept;
        FieldRowTextBox(const FieldRowTextBox&) = delete;
        FieldRowTextBox& operator=(const FieldRowTextBox&) = delete;
        FieldRowTextBox(FieldRowTextBox&&) = delete;
        FieldRowTextBox& operator=(FieldRowTextBox&&) = delete;
        [[nodiscard]] FieldRowAdapter::ElementHandle Native() const noexcept;
        [[nodiscard]] std::optional<std::u16string> Text() const;
        void Text(std::optional<std::u16string_view> value);
        void Watermark(std::optional<std::u16string_view> value);
        void AddLostFocus(const RowsEventHandler& handler);
        void RemoveLostFocus(const RowsEventHandler& handler);
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
        [[nodiscard]] std::shared_ptr<FieldRowTextBox> Box() const noexcept;
        [[nodiscard]] std::u16string Value() const;
        void Value(std::optional<std::u16string_view> value);
    private:
        FieldRowAdapter& _fieldAdapter;
        std::shared_ptr<FieldRowTextBox> _box;
    };

    class NoteAdapter : public RowsControlAdapter
    {
    public:
        virtual void SetText(std::optional<std::u16string_view> text) = 0;
        [[nodiscard]] virtual std::optional<std::u16string> GetText() const = 0;
        virtual void SetFontFamily(GuiFontFamily family) = 0;
        virtual void SetFontSize(double fontSize) = 0;
        virtual void SetForeground(std::optional<RowsBrush> brush) = 0;
        [[nodiscard]] virtual std::optional<RowsBrush> GetForeground() const = 0;
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
        [[nodiscard]] std::optional<RowsBrush> Foreground() const;
        void Foreground(std::optional<RowsBrush> value);
        void Foreground(RowsBrush value);
    private:
        NoteAdapter& _noteAdapter;
    };
}
