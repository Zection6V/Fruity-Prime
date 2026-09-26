#pragma once

// Avalonia.Controls' layout containers and decorators: Panel, StackPanel,
// Grid, DockPanel, Canvas, Decorator, Border, ContentControl, UserControl,
// LayoutTransformControl and Image.

#include "Controls.hpp"

#include <string_view>

namespace MphRead::NativeRuntime::Avalonia::Controls
{
    // Panel.Children: adding and removing makes them visual children.
    class Panel;

    class PanelChildren final
    {
    public:
        explicit PanelChildren(Panel& owner)
            : _owner(owner)
        {
        }
        void Add(const ControlPtr& control);
        void Insert(std::size_t index, const ControlPtr& control);
        bool Remove(const ControlPtr& control);
        void RemoveAt(std::size_t index);
        void Clear();
        void AddRange(const std::vector<ControlPtr>& controls);
        [[nodiscard]] std::size_t Count() const noexcept { return _items.size(); }
        [[nodiscard]] const ControlPtr& operator[](std::size_t index) const { return _items.at(index); }
        [[nodiscard]] std::ptrdiff_t IndexOf(const Control* control) const;
        [[nodiscard]] bool Contains(const Control* control) const { return IndexOf(control) >= 0; }
        [[nodiscard]] auto begin() const noexcept { return _items.begin(); }
        [[nodiscard]] auto end() const noexcept { return _items.end(); }

    private:
        Panel& _owner;
        std::vector<ControlPtr> _items;
    };

    class Panel : public Control
    {
    public:
        Panel();
        static StyledProperty<Media::IBrushPtr>& BackgroundProperty;

        [[nodiscard]] Media::IBrushPtr Background() const { return GetValue(BackgroundProperty); }
        void Background(Media::IBrushPtr value) { SetValue(BackgroundProperty, std::move(value)); }

        PanelChildren Children{*this};

        void Render(Media::DrawingContext& context) override;

    protected:
        friend class PanelChildren;
        virtual void ChildrenChanged() { InvalidateMeasure(); }
        void AttachChild(std::size_t index, const ControlPtr& child);
        void DetachChild(const ControlPtr& child);
    };

    class StackPanel : public Panel
    {
    public:
        StackPanel();
        static StyledProperty<Layout::Orientation>& OrientationProperty;
        static StyledProperty<double>& SpacingProperty;

        [[nodiscard]] Layout::Orientation Orientation() const { return GetValue(OrientationProperty); }
        void Orientation(Layout::Orientation value) { SetValue(OrientationProperty, value); }
        [[nodiscard]] double Spacing() const { return GetValue(SpacingProperty); }
        void Spacing(double value) { SetValue(SpacingProperty, value); }

    protected:
        Size MeasureOverride(Size availableSize) override;
        Size ArrangeOverride(Size finalSize) override;
    };

    enum class GridUnitType : std::int32_t { Auto, Pixel, Star };

    struct GridLength final
    {
        double Value = 1.0;
        Controls::GridUnitType GridUnitType = Controls::GridUnitType::Star;

        constexpr GridLength() noexcept = default;
        constexpr explicit GridLength(double pixels) noexcept
            : Value(pixels), GridUnitType(Controls::GridUnitType::Pixel)
        {
        }
        constexpr GridLength(double value, Controls::GridUnitType type) noexcept
            : Value(value), GridUnitType(type)
        {
        }
        [[nodiscard]] static constexpr GridLength Auto() noexcept { return {1.0, Controls::GridUnitType::Auto}; }
        [[nodiscard]] static constexpr GridLength Star() noexcept { return {1.0, Controls::GridUnitType::Star}; }
        [[nodiscard]] constexpr bool IsAuto() const noexcept { return GridUnitType == Controls::GridUnitType::Auto; }
        [[nodiscard]] constexpr bool IsStar() const noexcept { return GridUnitType == Controls::GridUnitType::Star; }
        [[nodiscard]] constexpr bool IsAbsolute() const noexcept { return GridUnitType == Controls::GridUnitType::Pixel; }
        friend constexpr bool operator==(const GridLength&, const GridLength&) noexcept = default;
    };

    struct DefinitionBase
    {
        GridLength Length{};
        double MinLength = 0.0;
        double MaxLength = std::numeric_limits<double>::infinity();
        // The size the last layout gave it.
        double ActualLength = 0.0;
    };

    struct ColumnDefinition final : DefinitionBase
    {
        ColumnDefinition() = default;
        explicit ColumnDefinition(GridLength width)
        {
            Length = width;
        }
        ColumnDefinition(double value, GridUnitType type)
        {
            Length = GridLength(value, type);
        }
    };

    struct RowDefinition final : DefinitionBase
    {
        RowDefinition() = default;
        explicit RowDefinition(GridLength height)
        {
            Length = height;
        }
        RowDefinition(double value, GridUnitType type)
        {
            Length = GridLength(value, type);
        }
    };

    // "Auto,*,2*,120" as ColumnDefinitions.Parse reads it.
    [[nodiscard]] std::vector<GridLength> ParseGridLengths(std::string_view text);

    class ColumnDefinitions final
    {
    public:
        ColumnDefinitions() = default;
        explicit ColumnDefinitions(std::string_view text);
        void Add(ColumnDefinition definition) { Items.push_back(definition); }
        [[nodiscard]] std::size_t Count() const noexcept { return Items.size(); }
        std::vector<ColumnDefinition> Items;
    };

    class RowDefinitions final
    {
    public:
        RowDefinitions() = default;
        explicit RowDefinitions(std::string_view text);
        void Add(RowDefinition definition) { Items.push_back(definition); }
        [[nodiscard]] std::size_t Count() const noexcept { return Items.size(); }
        std::vector<RowDefinition> Items;
    };

    class Grid : public Panel
    {
    public:
        Grid();
        static AttachedProperty<std::int32_t>& RowProperty;
        static AttachedProperty<std::int32_t>& ColumnProperty;
        static AttachedProperty<std::int32_t>& RowSpanProperty;
        static AttachedProperty<std::int32_t>& ColumnSpanProperty;
        static StyledProperty<double>& RowSpacingProperty;
        static StyledProperty<double>& ColumnSpacingProperty;

        static void SetRow(AvaloniaObject& element, std::int32_t value) { element.SetValue(RowProperty, value); }
        static void SetColumn(AvaloniaObject& element, std::int32_t value) { element.SetValue(ColumnProperty, value); }
        static void SetRowSpan(AvaloniaObject& element, std::int32_t value) { element.SetValue(RowSpanProperty, value); }
        static void SetColumnSpan(AvaloniaObject& element, std::int32_t value) { element.SetValue(ColumnSpanProperty, value); }
        [[nodiscard]] static std::int32_t GetRow(const AvaloniaObject& element) { return element.GetValue(RowProperty); }
        [[nodiscard]] static std::int32_t GetColumn(const AvaloniaObject& element) { return element.GetValue(ColumnProperty); }

        [[nodiscard]] double RowSpacing() const { return GetValue(RowSpacingProperty); }
        void RowSpacing(double value) { SetValue(RowSpacingProperty, value); }
        [[nodiscard]] double ColumnSpacing() const { return GetValue(ColumnSpacingProperty); }
        void ColumnSpacing(double value) { SetValue(ColumnSpacingProperty, value); }

        // Assigning replaces the definitions and measures again.
        [[nodiscard]] Controls::ColumnDefinitions& ColumnDefinitions() noexcept { return _columns; }
        void ColumnDefinitions(Controls::ColumnDefinitions value) { _columns = std::move(value); InvalidateMeasure(); }
        [[nodiscard]] Controls::RowDefinitions& RowDefinitions() noexcept { return _rows; }
        void RowDefinitions(Controls::RowDefinitions value) { _rows = std::move(value); InvalidateMeasure(); }

    protected:
        Size MeasureOverride(Size availableSize) override;
        Size ArrangeOverride(Size finalSize) override;

    private:
        Controls::ColumnDefinitions _columns;
        Controls::RowDefinitions _rows;
        std::vector<double> _columnSizes;
        std::vector<double> _rowSizes;
    };

    enum class Dock : std::int32_t { Left, Bottom, Right, Top };

    class DockPanel : public Panel
    {
    public:
        DockPanel();
        static AttachedProperty<Controls::Dock>& DockProperty;
        static StyledProperty<bool>& LastChildFillProperty;

        static void SetDock(AvaloniaObject& element, Controls::Dock value) { element.SetValue(DockProperty, value); }
        [[nodiscard]] static Controls::Dock GetDock(const AvaloniaObject& element) { return element.GetValue(DockProperty); }
        [[nodiscard]] bool LastChildFill() const { return GetValue(LastChildFillProperty); }
        void LastChildFill(bool value) { SetValue(LastChildFillProperty, value); }

    protected:
        Size MeasureOverride(Size availableSize) override;
        Size ArrangeOverride(Size finalSize) override;
    };

    class Canvas : public Panel
    {
    public:
        Canvas();
        static AttachedProperty<double>& LeftProperty;
        static AttachedProperty<double>& TopProperty;
        static AttachedProperty<double>& RightProperty;
        static AttachedProperty<double>& BottomProperty;

        static void SetLeft(AvaloniaObject& element, double value) { element.SetValue(LeftProperty, value); }
        static void SetTop(AvaloniaObject& element, double value) { element.SetValue(TopProperty, value); }
        static void SetRight(AvaloniaObject& element, double value) { element.SetValue(RightProperty, value); }
        static void SetBottom(AvaloniaObject& element, double value) { element.SetValue(BottomProperty, value); }
        [[nodiscard]] static double GetLeft(const AvaloniaObject& element) { return element.GetValue(LeftProperty); }
        [[nodiscard]] static double GetTop(const AvaloniaObject& element) { return element.GetValue(TopProperty); }

    protected:
        Size MeasureOverride(Size availableSize) override;
        Size ArrangeOverride(Size finalSize) override;
    };

    class Decorator : public Control
    {
    public:
        Decorator();
        static StyledProperty<ControlPtr>& ChildProperty;
        static StyledProperty<Thickness>& PaddingProperty;

        [[nodiscard]] ControlPtr Child() const { return GetValue(ChildProperty); }
        void Child(ControlPtr value) { SetValue(ChildProperty, std::move(value)); }
        [[nodiscard]] Thickness Padding() const { return GetValue(PaddingProperty); }
        void Padding(Thickness value) { SetValue(PaddingProperty, value); }

    protected:
        Size MeasureOverride(Size availableSize) override;
        Size ArrangeOverride(Size finalSize) override;
        void OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change) override;
    };

    class Border : public Decorator
    {
    public:
        Border();
        static StyledProperty<Media::IBrushPtr>& BackgroundProperty;
        static StyledProperty<Media::IBrushPtr>& BorderBrushProperty;
        static StyledProperty<Thickness>& BorderThicknessProperty;
        static StyledProperty<Avalonia::CornerRadius>& CornerRadiusProperty;
        static StyledProperty<Media::BoxShadows>& BoxShadowProperty;

        [[nodiscard]] Media::IBrushPtr Background() const { return GetValue(BackgroundProperty); }
        void Background(Media::IBrushPtr value) { SetValue(BackgroundProperty, std::move(value)); }
        [[nodiscard]] Media::IBrushPtr BorderBrush() const { return GetValue(BorderBrushProperty); }
        void BorderBrush(Media::IBrushPtr value) { SetValue(BorderBrushProperty, std::move(value)); }
        [[nodiscard]] Thickness BorderThickness() const { return GetValue(BorderThicknessProperty); }
        void BorderThickness(Thickness value) { SetValue(BorderThicknessProperty, value); }
        [[nodiscard]] Avalonia::CornerRadius CornerRadius() const { return GetValue(CornerRadiusProperty); }
        void CornerRadius(Avalonia::CornerRadius value) { SetValue(CornerRadiusProperty, value); }
        [[nodiscard]] Media::BoxShadows BoxShadow() const { return GetValue(BoxShadowProperty); }
        void BoxShadow(Media::BoxShadows value) { SetValue(BoxShadowProperty, std::move(value)); }

        void Render(Media::DrawingContext& context) override;
        [[nodiscard]] Avalonia::CornerRadius ClipCornerRadius() const override { return CornerRadius(); }

    protected:
        Size MeasureOverride(Size availableSize) override;
        Size ArrangeOverride(Size finalSize) override;
    };

    // Renders a Background, BorderBrush and CornerRadius the way Avalonia's
    // BorderRenderHelper does, for Border and the templated controls.
    void RenderBorder(Media::DrawingContext& context, const Rect& bounds, const Media::IBrushPtr& background,
        const Media::IBrushPtr& borderBrush, const Thickness& thickness, const Avalonia::CornerRadius& radius,
        const Media::BoxShadows& shadows);

    class ContentControl : public TemplatedControl
    {
    public:
        ContentControl();
        static StyledProperty<ControlPtr>& ContentProperty;
        static StyledProperty<Layout::HorizontalAlignment>& HorizontalContentAlignmentProperty;
        static StyledProperty<Layout::VerticalAlignment>& VerticalContentAlignmentProperty;

        [[nodiscard]] ControlPtr Content() const { return GetValue(ContentProperty); }
        void Content(ControlPtr value) { SetValue(ContentProperty, std::move(value)); }
        // Content = "text": a string is shown as a text block.
        void Content(std::string_view text);
        [[nodiscard]] Layout::HorizontalAlignment HorizontalContentAlignment() const
        {
            return GetValue(HorizontalContentAlignmentProperty);
        }
        void HorizontalContentAlignment(Layout::HorizontalAlignment value) { SetValue(HorizontalContentAlignmentProperty, value); }
        [[nodiscard]] Layout::VerticalAlignment VerticalContentAlignment() const
        {
            return GetValue(VerticalContentAlignmentProperty);
        }
        void VerticalContentAlignment(Layout::VerticalAlignment value) { SetValue(VerticalContentAlignmentProperty, value); }

        void Render(Media::DrawingContext& context) override;

    protected:
        Size MeasureOverride(Size availableSize) override;
        Size ArrangeOverride(Size finalSize) override;
        void OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change) override;
    };

    class UserControl : public ContentControl
    {
    };

    class LayoutTransformControl : public Decorator
    {
    public:
        LayoutTransformControl();
        static StyledProperty<Media::TransformPtr>& LayoutTransformProperty;

        [[nodiscard]] Media::TransformPtr LayoutTransform() const { return GetValue(LayoutTransformProperty); }
        void LayoutTransform(Media::TransformPtr value) { SetValue(LayoutTransformProperty, std::move(value)); }

        [[nodiscard]] Matrix VisualChildTransform() const override;

    protected:
        Size MeasureOverride(Size availableSize) override;
        Size ArrangeOverride(Size finalSize) override;
        void OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change) override;
    };

}

namespace MphRead::NativeRuntime::Avalonia::Media
{
    enum class Stretch : std::int32_t { None, Fill, Uniform, UniformToFill };
    enum class StretchDirection : std::int32_t { UpOnly, DownOnly, Both };

    [[nodiscard]] Vector CalculateScaling(Stretch stretch, Size destinationSize, Size sourceSize,
        StretchDirection direction = StretchDirection::Both);
}

namespace MphRead::NativeRuntime::Avalonia::Controls
{
    class Image : public Control
    {
    public:
        Image();
        static StyledProperty<std::shared_ptr<Media::IImage>>& SourceProperty;
        static StyledProperty<Media::Stretch>& StretchProperty;
        static StyledProperty<Media::StretchDirection>& StretchDirectionProperty;

        [[nodiscard]] std::shared_ptr<Media::IImage> Source() const { return GetValue(SourceProperty); }
        void Source(std::shared_ptr<Media::IImage> value) { SetValue(SourceProperty, std::move(value)); }
        [[nodiscard]] Media::Stretch Stretch() const { return GetValue(StretchProperty); }
        void Stretch(Media::Stretch value) { SetValue(StretchProperty, value); }
        [[nodiscard]] Media::StretchDirection StretchDirection() const { return GetValue(StretchDirectionProperty); }
        void StretchDirection(Media::StretchDirection value) { SetValue(StretchDirectionProperty, value); }

        void Render(Media::DrawingContext& context) override;

    protected:
        Size MeasureOverride(Size availableSize) override;
        Size ArrangeOverride(Size finalSize) override;
    };

    // RenderOptions.SetBitmapInterpolationMode(visual, mode), attached.
    class RenderOptions final
    {
    public:
        RenderOptions() = delete;
        static AttachedProperty<Media::BitmapInterpolationMode>& BitmapInterpolationModeProperty;
        static void SetBitmapInterpolationMode(AvaloniaObject& element, Media::BitmapInterpolationMode mode)
        {
            element.SetValue(BitmapInterpolationModeProperty, mode);
        }
        [[nodiscard]] static Media::BitmapInterpolationMode GetBitmapInterpolationMode(const AvaloniaObject& element)
        {
            return element.GetValue(BitmapInterpolationModeProperty);
        }
    };
}
