#pragma once

// A retained control tree shaped like Avalonia's, because that is the API the
// launcher's ported views were written against: panels, grids, stack panels,
// borders and text blocks, laid out by measure and arrange, with the same
// alignment, margin and grid-definition rules.
//
// This is the toolkit the C# build gets from Avalonia. Nothing here knows how
// to draw; Renderer.cpp walks the arranged tree.

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::NativeRuntime::Gui
{
    struct Thickness final
    {
        double Left = 0.0;
        double Top = 0.0;
        double Right = 0.0;
        double Bottom = 0.0;

        [[nodiscard]] constexpr double Horizontal() const noexcept { return Left + Right; }
        [[nodiscard]] constexpr double Vertical() const noexcept { return Top + Bottom; }
    };

    struct Color final
    {
        float R = 0.0F;
        float G = 0.0F;
        float B = 0.0F;
        float A = 0.0F;

        [[nodiscard]] static constexpr Color FromArgb(std::uint32_t value) noexcept
        {
            return Color{
                static_cast<float>((value >> 16) & 0xFFU) / 255.0F,
                static_cast<float>((value >> 8) & 0xFFU) / 255.0F,
                static_cast<float>(value & 0xFFU) / 255.0F,
                static_cast<float>((value >> 24) & 0xFFU) / 255.0F};
        }
    };

    struct Rect final
    {
        double X = 0.0;
        double Y = 0.0;
        double Width = 0.0;
        double Height = 0.0;

        [[nodiscard]] constexpr bool Contains(double x, double y) const noexcept
        {
            return x >= X && x < X + Width && y >= Y && y < Y + Height;
        }
    };

    struct Size final
    {
        double Width = 0.0;
        double Height = 0.0;
    };

    enum class ElementKind : std::uint8_t
    {
        Panel,
        Grid,
        StackPanel,
        Border,
        TextBlock,
        ScrollViewer,
        // Children take an edge each and the last one may fill the rest.
        DockPanel,
        // Children flow left to right and wrap onto the next line.
        WrapPanel,
        // A control that measures and draws itself; the launcher's own
        // composites (a menu entry, the splash, a slider) are these.
        Custom
    };

    enum class HorizontalAlignment : std::uint8_t { Stretch, Left, Center, Right };
    enum class VerticalAlignment : std::uint8_t { Stretch, Top, Center, Bottom };
    enum class Orientation : std::uint8_t { Vertical, Horizontal };
    enum class Dock : std::uint8_t { Left, Top, Right, Bottom };
    enum class TextAlignment : std::uint8_t { Left, Center, Right };
    enum class FontWeight : std::uint8_t { Normal, SemiBold, Bold };
    enum class Cursor : std::uint8_t { Arrow, Hand, IBeam };

    // One entry of a "Auto,*,2*,120" grid definition.
    struct GridLength final
    {
        enum class Unit : std::uint8_t { Auto, Star, Pixel };

        Unit Kind = Unit::Auto;
        double Value = 1.0;
    };

    class Element;
    class Renderer;
    using ElementPtr = std::shared_ptr<Element>;

    struct PointerEvent final
    {
        double X = 0.0;
        double Y = 0.0;
        bool Handled = false;
    };

    // The keys the launcher's controls read. Anything else is Other, with the
    // platform's own code in Code for a control that wants it.
    enum class Key : std::uint8_t
    {
        Other,
        Escape,
        Enter,
        Space,
        Tab,
        Backspace,
        Delete,
        Left,
        Right,
        Up,
        Down,
        Home,
        End
    };

    struct KeyEvent final
    {
        Key Which = Key::Other;
        std::int32_t Code = 0;
        bool Shift = false;
        bool Control = false;
        bool Handled = false;
    };

    // What a Custom element has to answer.
    class CustomBehaviour
    {
    public:
        virtual ~CustomBehaviour() = default;

        // The size this control wants, given what it may have.
        [[nodiscard]] virtual Size Measure(Element& element, Size available) = 0;
        // Children, if any, are arranged here; the rect is the control's own.
        virtual void Arrange(Element& element, Rect bounds) { (void)element; (void)bounds; }
        // Avalonia's Render(DrawingContext). The rect is the control's own.
        virtual void Render(Element& element, Renderer& renderer)
        {
            (void)element;
            (void)renderer;
        }
    };

    class Element final : public std::enable_shared_from_this<Element>
    {
    public:
        explicit Element(ElementKind kind) noexcept
            : _kind(kind)
        {
        }

        [[nodiscard]] static ElementPtr Create(ElementKind kind);

        [[nodiscard]] ElementKind Kind() const noexcept { return _kind; }

        // --- tree
        void AddChild(const ElementPtr& child);
        void InsertChild(std::int32_t index, const ElementPtr& child);
        void ClearChildren();
        [[nodiscard]] const std::vector<ElementPtr>& Children() const noexcept
        {
            return _children;
        }
        [[nodiscard]] Element* Parent() const noexcept { return _parent; }

        // --- layout inputs
        Thickness Margin{};
        Thickness Padding{};
        std::optional<double> Width{};
        std::optional<double> Height{};
        std::optional<double> MinWidth{};
        std::optional<double> MinHeight{};
        std::optional<double> MaxWidth{};
        std::optional<double> MaxHeight{};
        HorizontalAlignment Horizontal = HorizontalAlignment::Stretch;
        VerticalAlignment Vertical = VerticalAlignment::Stretch;
        bool Visible = true;
        double Spacing = 0.0;
        Orientation StackOrientation = Orientation::Vertical;
        std::int32_t GridRow = 0;
        std::int32_t GridColumn = 0;
        std::int32_t GridRowSpan = 1;
        Dock DockSide = Dock::Left;
        bool LastChildFill = true;
        std::vector<GridLength> RowDefinitions{};
        std::vector<GridLength> ColumnDefinitions{};
        bool HorizontalScrollDisabled = false;
        double ScrollOffset = 0.0;
        // Avalonia's LayoutTransformControl, as the pause menu shrinks itself
        // with: the child is laid out at full size and drawn scaled about this
        // element's top left corner.
        double LayoutScale = 1.0;

        // --- visual inputs
        std::optional<Color> Background{};
        std::optional<Color> BorderColor{};
        Thickness BorderThickness{};
        double CornerRadius = 0.0;
        double Opacity = 1.0;

        // --- text inputs
        std::optional<std::string> Text{};
        double FontSize = 14.0;
        FontWeight Weight = FontWeight::Normal;
        Color Foreground = Color{1.0F, 1.0F, 1.0F, 1.0F};
        TextAlignment Align = TextAlignment::Left;
        bool WrapText = false;
        std::optional<std::string> ToolTip{};
        Cursor CursorKind = Cursor::Arrow;
        bool Focusable = false;

        // --- interaction
        bool IsEnabled = true;
        // Set by the window as the pointer and the focus move.
        bool IsPointerOver = false;
        bool IsFocused = false;

        // --- behaviour
        std::shared_ptr<CustomBehaviour> Behaviour{};
        std::function<void(PointerEvent&)> PointerEntered{};
        std::function<void(PointerEvent&)> PointerExited{};
        std::function<void(PointerEvent&)> PointerPressed{};
        std::function<void(PointerEvent&)> PointerReleased{};
        std::function<void(PointerEvent&)> PointerMoved{};
        std::function<void()> PointerCaptureLost{};
        std::function<void()> GotFocus{};
        std::function<void()> LostFocus{};
        std::function<void(KeyEvent&)> KeyDown{};
        std::function<void(char32_t)> TextInput{};
        // Whatever the host wants to hang off this element.
        std::shared_ptr<void> Tag{};

        // --- layout outputs
        [[nodiscard]] Size Desired() const noexcept { return _desired; }
        [[nodiscard]] Rect Bounds() const noexcept { return _bounds; }

        // Avalonia's two passes. Measure fills Desired; Arrange fills Bounds,
        // honouring alignment inside the rect it is given.
        Size Measure(Size available);
        void Arrange(Rect finalRect);

        // The deepest visible element under the point that wants pointers.
        [[nodiscard]] Element* HitTest(double x, double y);

    private:
        [[nodiscard]] Size MeasureCore(Size available);
        // One axis of a grid: the pixel sizes its definitions resolve to.
        [[nodiscard]] std::vector<double> MeasureGridTrack(
            const std::vector<GridLength>& definitions, double available, bool horizontal);
        void ArrangeCore(Rect bounds);
        [[nodiscard]] Size ClampToConstraints(Size value) const noexcept;

        ElementKind _kind;
        Element* _parent = nullptr;
        std::vector<ElementPtr> _children;
        Size _desired{};
        Rect _bounds{};
    };

    // "Auto,*,2*,120" as Avalonia's GridLength parser reads it.
    [[nodiscard]] std::vector<GridLength> ParseGridDefinitions(std::string_view text);
}
