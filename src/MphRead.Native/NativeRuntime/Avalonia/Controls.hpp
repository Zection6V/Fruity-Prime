#pragma once

// The control tree: Visual, Layoutable, Interactive, InputElement, Control
// and TemplatedControl, with Avalonia's measure/arrange rules, routed events,
// focus and pointer capture. Panels and the leaf controls build on this in
// Panels.hpp, Text.hpp and the rest.

#include "Base.hpp"
#include "Input.hpp"
#include "Media.hpp"

#include <any>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MphRead::NativeRuntime::Avalonia
{
    class TopLevel;

    namespace Layout
    {
        enum class HorizontalAlignment : std::int32_t { Stretch, Left, Center, Right };
        enum class VerticalAlignment : std::int32_t { Stretch, Top, Center, Bottom };
        enum class Orientation : std::int32_t { Horizontal, Vertical };
    }

    // Avalonia.Visual.
    class Visual : public AvaloniaObject
    {
    public:
        Visual();
        ~Visual() override;

        static StyledProperty<bool>& IsVisibleProperty;
        static StyledProperty<double>& OpacityProperty;
        static StyledProperty<bool>& ClipToBoundsProperty;
        static StyledProperty<Media::TransformPtr>& RenderTransformProperty;
        static StyledProperty<RelativePoint>& RenderTransformOriginProperty;
        static StyledProperty<std::int32_t>& ZIndexProperty;

        [[nodiscard]] bool IsVisible() const { return GetValue(IsVisibleProperty); }
        void IsVisible(bool value) { SetValue(IsVisibleProperty, value); }
        [[nodiscard]] double Opacity() const { return GetValue(OpacityProperty); }
        void Opacity(double value) { SetValue(OpacityProperty, value); }
        [[nodiscard]] bool ClipToBounds() const { return GetValue(ClipToBoundsProperty); }
        void ClipToBounds(bool value) { SetValue(ClipToBoundsProperty, value); }
        [[nodiscard]] Media::TransformPtr RenderTransform() const { return GetValue(RenderTransformProperty); }
        void RenderTransform(Media::TransformPtr value) { SetValue(RenderTransformProperty, std::move(value)); }
        [[nodiscard]] RelativePoint RenderTransformOrigin() const { return GetValue(RenderTransformOriginProperty); }
        void RenderTransformOrigin(RelativePoint value) { SetValue(RenderTransformOriginProperty, value); }
        [[nodiscard]] std::int32_t ZIndex() const { return GetValue(ZIndexProperty); }
        void ZIndex(std::int32_t value) { SetValue(ZIndexProperty, value); }

        // Where arrange put it, relative to its visual parent.
        [[nodiscard]] Rect Bounds() const noexcept { return _bounds; }

        [[nodiscard]] Visual* GetVisualParent() const noexcept { return _visualParent; }
        [[nodiscard]] const std::vector<std::shared_ptr<Visual>>& VisualChildren() const noexcept { return _visualChildren; }
        [[nodiscard]] TopLevel* GetVisualRoot() const noexcept;
        [[nodiscard]] bool IsAttachedToVisualTree() const noexcept { return GetVisualRoot() != nullptr; }
        [[nodiscard]] bool IsEffectivelyVisible() const;

        // The visual's transform relative to its parent: arrange offset and
        // render transform.
        [[nodiscard]] Matrix LocalTransform() const;
        [[nodiscard]] std::optional<Matrix> TransformToVisual(const Visual* relativeTo) const;
        [[nodiscard]] std::optional<Point> TranslatePoint(Point point, const Visual* relativeTo) const;
        [[nodiscard]] PixelPoint PointToScreen(Point point) const;

        virtual void Render(Media::DrawingContext& context);
        // Drawn after the children: what a template puts in front of its
        // content, like a scroll bar over a list.
        virtual void RenderOverlay(Media::DrawingContext& context) { (void)context; }
        void InvalidateVisual();

        // A transform this visual applies to all of its children, as
        // LayoutTransformControl does. Identity for everything else.
        [[nodiscard]] virtual Matrix VisualChildTransform() const { return Matrix::Identity(); }
        // The shape ClipToBounds clips to: the bounds, rounded as a Border's
        // corners are rounded.
        [[nodiscard]] virtual Avalonia::CornerRadius ClipCornerRadius() const { return {}; }

        // Every descendant, depth first, as GetVisualDescendants() yields them.
        [[nodiscard]] std::vector<Visual*> GetVisualDescendants() const;
        [[nodiscard]] std::vector<Visual*> GetVisualAncestors() const;

        // Visual.AffectsRender<T>(...).
        template <typename T, typename... Properties>
        static void AffectsRender(Properties&... properties)
        {
            (properties.AddAffects([](const AvaloniaObject& o) { return dynamic_cast<const T*>(&o) != nullptr; },
                 AvaloniaProperty::Affects::Render),
                ...);
        }

        // Set by the renderer: whether the last render drew anything, which
        // is what a visual is hit-tested by.
        bool RenderedContent = false;

    protected:
        void AddVisualChild(const std::shared_ptr<Visual>& child);
        void InsertVisualChild(std::size_t index, const std::shared_ptr<Visual>& child);
        void RemoveVisualChild(const std::shared_ptr<Visual>& child);
        void ClearVisualChildren();
        void SetBounds(const Rect& bounds);

        virtual void OnAttachedToVisualTree();
        virtual void OnDetachedFromVisualTree();
        void OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change) override;
        void ForEachInheritanceChild(const std::function<void(AvaloniaObject&)>& visit) override;

        // A top level is its own root.
        TopLevel* _root = nullptr;

    private:
        void SetRoot(TopLevel* root);

        Visual* _visualParent = nullptr;
        std::vector<std::shared_ptr<Visual>> _visualChildren;
        Rect _bounds{};
        friend class TopLevel;
    };

    using VisualPtr = std::shared_ptr<Visual>;

    namespace Layout
    {
        // Avalonia.Layout.Layoutable.
        class Layoutable : public Visual
        {
        public:
            static StyledProperty<double>& WidthProperty;
            static StyledProperty<double>& HeightProperty;
            static StyledProperty<double>& MinWidthProperty;
            static StyledProperty<double>& MaxWidthProperty;
            static StyledProperty<double>& MinHeightProperty;
            static StyledProperty<double>& MaxHeightProperty;
            static StyledProperty<Thickness>& MarginProperty;
            static StyledProperty<Layout::HorizontalAlignment>& HorizontalAlignmentProperty;
            static StyledProperty<Layout::VerticalAlignment>& VerticalAlignmentProperty;
            static StyledProperty<bool>& UseLayoutRoundingProperty;

            [[nodiscard]] double Width() const { return GetValue(WidthProperty); }
            void Width(double value) { SetValue(WidthProperty, value); }
            [[nodiscard]] double Height() const { return GetValue(HeightProperty); }
            void Height(double value) { SetValue(HeightProperty, value); }
            [[nodiscard]] double MinWidth() const { return GetValue(MinWidthProperty); }
            void MinWidth(double value) { SetValue(MinWidthProperty, value); }
            [[nodiscard]] double MaxWidth() const { return GetValue(MaxWidthProperty); }
            void MaxWidth(double value) { SetValue(MaxWidthProperty, value); }
            [[nodiscard]] double MinHeight() const { return GetValue(MinHeightProperty); }
            void MinHeight(double value) { SetValue(MinHeightProperty, value); }
            [[nodiscard]] double MaxHeight() const { return GetValue(MaxHeightProperty); }
            void MaxHeight(double value) { SetValue(MaxHeightProperty, value); }
            [[nodiscard]] Thickness Margin() const { return GetValue(MarginProperty); }
            void Margin(Thickness value) { SetValue(MarginProperty, value); }
            [[nodiscard]] Layout::HorizontalAlignment HorizontalAlignment() const { return GetValue(HorizontalAlignmentProperty); }
            void HorizontalAlignment(Layout::HorizontalAlignment value) { SetValue(HorizontalAlignmentProperty, value); }
            [[nodiscard]] Layout::VerticalAlignment VerticalAlignment() const { return GetValue(VerticalAlignmentProperty); }
            void VerticalAlignment(Layout::VerticalAlignment value) { SetValue(VerticalAlignmentProperty, value); }
            [[nodiscard]] bool UseLayoutRounding() const { return GetValue(UseLayoutRoundingProperty); }
            void UseLayoutRounding(bool value) { SetValue(UseLayoutRoundingProperty, value); }

            [[nodiscard]] Size DesiredSize() const noexcept { return _desiredSize; }
            [[nodiscard]] bool IsMeasureValid() const noexcept { return _measureValid; }
            [[nodiscard]] bool IsArrangeValid() const noexcept { return _arrangeValid; }
            [[nodiscard]] std::optional<Size> PreviousMeasure() const noexcept { return _previousMeasure; }
            [[nodiscard]] std::optional<Rect> PreviousArrange() const noexcept { return _previousArrange; }

            void Measure(Size availableSize);
            void Arrange(Rect rect);
            void InvalidateMeasure();
            void InvalidateArrange();
            // Layoutable.UpdateLayout: a layout pass now, from the root.
            void UpdateLayout();

            // LayoutUpdated / EffectiveViewportChanged are raised after a pass.
            Event<Layoutable&> LayoutUpdated;

            template <typename T, typename... Properties>
            static void AffectsMeasure(Properties&... properties)
            {
                (properties.AddAffects([](const AvaloniaObject& o) { return dynamic_cast<const T*>(&o) != nullptr; },
                     AvaloniaProperty::Affects::Measure),
                    ...);
            }
            template <typename T, typename... Properties>
            static void AffectsArrange(Properties&... properties)
            {
                (properties.AddAffects([](const AvaloniaObject& o) { return dynamic_cast<const T*>(&o) != nullptr; },
                     AvaloniaProperty::Affects::Arrange),
                    ...);
            }

        protected:
            virtual Size MeasureCore(Size availableSize);
            virtual void ArrangeCore(Rect finalRect);
            virtual Size MeasureOverride(Size availableSize);
            virtual Size ArrangeOverride(Size finalSize);
            void OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change) override;
            virtual void OnMeasureInvalidated() {}

        private:
            Size _desiredSize{};
            bool _measureValid = false;
            bool _arrangeValid = false;
            std::optional<Size> _previousMeasure{};
            std::optional<Rect> _previousArrange{};
        };

        // LayoutHelper.RoundLayoutValue at a render scaling of one.
        [[nodiscard]] double RoundLayoutValue(double value, double scale = 1.0) noexcept;
        [[nodiscard]] Size RoundLayoutSizeUp(Size size, double scale = 1.0) noexcept;
    }

    namespace Interactivity
    {
        // Avalonia.Interactivity.Interactive: routed events on the tree.
        class Interactive : public Layout::Layoutable
        {
        public:
            using Handler = std::function<void(Interactive& sender, RoutedEventArgs& e)>;

            std::size_t AddHandler(const RoutedEvent& routedEvent, Handler handler,
                RoutingStrategies routes = RoutingStrategies::Direct | RoutingStrategies::Bubble,
                bool handledEventsToo = false);
            template <typename TArgs>
            std::size_t AddHandler(const RoutedEvent& routedEvent, std::function<void(Interactive&, TArgs&)> handler,
                RoutingStrategies routes = RoutingStrategies::Direct | RoutingStrategies::Bubble,
                bool handledEventsToo = false)
            {
                return AddHandler(routedEvent, Handler([handler = std::move(handler)](Interactive& s, RoutedEventArgs& e)
                    {
                        if (auto* typed = dynamic_cast<TArgs*>(&e))
                        {
                            handler(s, *typed);
                        }
                    }), routes, handledEventsToo);
            }
            void RemoveHandler(std::size_t token);

            // Tunnels from the root to this element, then bubbles back, as
            // the event's own strategies say.
            void RaiseEvent(RoutedEventArgs& e);

        protected:
            // The class handler: InputElement's On* virtuals.
            virtual void ClassHandle(RoutedEventArgs& e) { (void)e; }
            // The C# event members, which are instance handlers on the route.
            virtual void RaiseInstanceEvents(RoutedEventArgs& e) { (void)e; }

        private:
            struct Subscription final
            {
                std::size_t Token;
                const RoutedEvent* Event;
                Handler Callback;
                RoutingStrategies Routes;
                bool HandledEventsToo;
            };
            void Invoke(RoutedEventArgs& e, RoutingStrategies phase);
            std::vector<Subscription> _handlers;
            std::size_t _next = 0;
        };
    }

    namespace Input
    {
        class IInputElement
        {
        public:
            virtual ~IInputElement() = default;
            [[nodiscard]] virtual Interactivity::Interactive& AsInteractive() = 0;
        };

        // Avalonia.Input.InputElement.
        class InputElement : public Interactivity::Interactive, public IInputElement
        {
        public:
            InputElement();

            static StyledProperty<bool>& FocusableProperty;
            static StyledProperty<bool>& IsEnabledProperty;
            static StyledProperty<bool>& IsHitTestVisibleProperty;
            static StyledProperty<std::shared_ptr<Input::Cursor>>& CursorProperty;
            static StyledProperty<bool>& IsTabStopProperty;

            static Interactivity::RoutedEvent PointerEnteredEvent;
            static Interactivity::RoutedEvent PointerExitedEvent;
            static Interactivity::RoutedEvent PointerPressedEvent;
            static Interactivity::RoutedEvent PointerMovedEvent;
            static Interactivity::RoutedEvent PointerReleasedEvent;
            static Interactivity::RoutedEvent PointerCaptureLostEvent;
            static Interactivity::RoutedEvent PointerWheelChangedEvent;
            static Interactivity::RoutedEvent KeyDownEvent;
            static Interactivity::RoutedEvent KeyUpEvent;
            static Interactivity::RoutedEvent TextInputEvent;
            static Interactivity::RoutedEvent GotFocusEvent;
            static Interactivity::RoutedEvent LostFocusEvent;

            [[nodiscard]] bool Focusable() const { return GetValue(FocusableProperty); }
            void Focusable(bool value) { SetValue(FocusableProperty, value); }
            [[nodiscard]] bool IsEnabled() const { return GetValue(IsEnabledProperty); }
            void IsEnabled(bool value) { SetValue(IsEnabledProperty, value); }
            [[nodiscard]] bool IsHitTestVisible() const { return GetValue(IsHitTestVisibleProperty); }
            void IsHitTestVisible(bool value) { SetValue(IsHitTestVisibleProperty, value); }
            [[nodiscard]] std::shared_ptr<Input::Cursor> Cursor() const { return GetValue(CursorProperty); }
            void Cursor(std::shared_ptr<Input::Cursor> value) { SetValue(CursorProperty, std::move(value)); }
            [[nodiscard]] bool IsTabStop() const { return GetValue(IsTabStopProperty); }
            void IsTabStop(bool value) { SetValue(IsTabStopProperty, value); }

            [[nodiscard]] bool IsPointerOver() const noexcept { return _isPointerOver; }
            [[nodiscard]] bool IsFocused() const noexcept { return _isFocused; }
            [[nodiscard]] bool IsKeyboardFocusWithin() const noexcept { return _isKeyboardFocusWithin; }
            [[nodiscard]] bool IsEffectivelyEnabled() const;

            bool Focus(NavigationMethod method = NavigationMethod::Unspecified, KeyModifiers keyModifiers = KeyModifiers::None);

            Interactivity::Interactive& AsInteractive() override { return *this; }

            // The C# events, raised as instance handlers on the route.
            Event<InputElement&, PointerEventArgs&> PointerEntered;
            Event<InputElement&, PointerEventArgs&> PointerExited;
            Event<InputElement&, PointerPressedEventArgs&> PointerPressed;
            Event<InputElement&, PointerEventArgs&> PointerMoved;
            Event<InputElement&, PointerReleasedEventArgs&> PointerReleased;
            Event<InputElement&, PointerCaptureLostEventArgs&> PointerCaptureLost;
            Event<InputElement&, PointerWheelEventArgs&> PointerWheelChanged;
            Event<InputElement&, KeyEventArgs&> KeyDown;
            Event<InputElement&, KeyEventArgs&> KeyUp;
            Event<InputElement&, TextInputEventArgs&> TextInput;
            Event<InputElement&, GotFocusEventArgs&> GotFocus;
            Event<InputElement&, FocusChangedEventArgs&> LostFocus;

        protected:
            virtual void OnPointerEntered(PointerEventArgs& e) { (void)e; }
            virtual void OnPointerExited(PointerEventArgs& e) { (void)e; }
            virtual void OnPointerPressed(PointerPressedEventArgs& e) { (void)e; }
            virtual void OnPointerMoved(PointerEventArgs& e) { (void)e; }
            virtual void OnPointerReleased(PointerReleasedEventArgs& e) { (void)e; }
            virtual void OnPointerCaptureLost(PointerCaptureLostEventArgs& e) { (void)e; }
            virtual void OnPointerWheelChanged(PointerWheelEventArgs& e) { (void)e; }
            virtual void OnKeyDown(KeyEventArgs& e) { (void)e; }
            virtual void OnKeyUp(KeyEventArgs& e) { (void)e; }
            virtual void OnTextInput(TextInputEventArgs& e) { (void)e; }
            virtual void OnGotFocus(GotFocusEventArgs& e) { (void)e; }
            virtual void OnLostFocus(FocusChangedEventArgs& e) { (void)e; }

            void ClassHandle(Interactivity::RoutedEventArgs& e) override;
            void RaiseInstanceEvents(Interactivity::RoutedEventArgs& e) override;
            void OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change) override;
            void OnDetachedFromVisualTree() override;

        private:
            bool _isPointerOver = false;
            bool _isFocused = false;
            bool _isKeyboardFocusWithin = false;
            friend class ::MphRead::NativeRuntime::Avalonia::TopLevel;
        };
    }

    namespace Controls
    {
        class Control;
        using ControlPtr = std::shared_ptr<Control>;

        // Avalonia.Controls.Control.
        class Control : public Input::InputElement
        {
        public:
            static StyledProperty<std::any>& TagProperty;

            [[nodiscard]] std::any Tag() const { return GetValue(TagProperty); }
            void Tag(std::any value) { SetValue(TagProperty, std::move(value)); }

            std::string Name;
            std::vector<std::string> Classes;

            // The logical parent: the control that holds this one as content.
            [[nodiscard]] Control* Parent() const noexcept { return dynamic_cast<Control*>(GetVisualParent()); }

            // Scrolls every ScrollViewer above it until it is in view.
            void BringIntoView();
            void BringIntoView(const Rect& rect);

            Event<Control&> AttachedToVisualTree;
            Event<Control&> DetachedFromVisualTree;
            Event<Control&> Loaded;
            Event<Control&, Size> SizeChanged;

        protected:
            void OnAttachedToVisualTree() override;
            void OnDetachedFromVisualTree() override;
            virtual void OnLoaded() {}
            virtual void OnSizeChanged(Size newSize) { (void)newSize; }
            void ArrangeCore(Rect finalRect) override;
        };

        // The text properties every control shares and inherits down the tree.
        class TextElement final
        {
        public:
            TextElement() = delete;
            static StyledProperty<Media::FontFamilyPtr>& FontFamilyProperty;
            static StyledProperty<double>& FontSizeProperty;
            static StyledProperty<Media::FontWeight>& FontWeightProperty;
            static StyledProperty<Media::FontStyle>& FontStyleProperty;
            static StyledProperty<Media::IBrushPtr>& ForegroundProperty;
        };

        // Avalonia.Controls.Primitives.TemplatedControl.
        class TemplatedControl : public Control
        {
        public:
            static StyledProperty<Media::IBrushPtr>& BackgroundProperty;
            static StyledProperty<Media::IBrushPtr>& BorderBrushProperty;
            static StyledProperty<Thickness>& BorderThicknessProperty;
            static StyledProperty<Avalonia::CornerRadius>& CornerRadiusProperty;
            static StyledProperty<Thickness>& PaddingProperty;

            [[nodiscard]] Media::IBrushPtr Background() const { return GetValue(BackgroundProperty); }
            void Background(Media::IBrushPtr value) { SetValue(BackgroundProperty, std::move(value)); }
            [[nodiscard]] Media::IBrushPtr BorderBrush() const { return GetValue(BorderBrushProperty); }
            void BorderBrush(Media::IBrushPtr value) { SetValue(BorderBrushProperty, std::move(value)); }
            [[nodiscard]] Thickness BorderThickness() const { return GetValue(BorderThicknessProperty); }
            void BorderThickness(Thickness value) { SetValue(BorderThicknessProperty, value); }
            [[nodiscard]] Avalonia::CornerRadius CornerRadius() const { return GetValue(CornerRadiusProperty); }
            void CornerRadius(Avalonia::CornerRadius value) { SetValue(CornerRadiusProperty, value); }
            [[nodiscard]] Thickness Padding() const { return GetValue(PaddingProperty); }
            void Padding(Thickness value) { SetValue(PaddingProperty, value); }

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
        };
    }
}
