#pragma once

// ScrollViewer, with the Fluent theme's overlay scroll bar folded in: the bar
// and its thumb are drawn by the viewer rather than being controls of their
// own, and the knobs a Style would set on them are properties here.

#include "Controls.hpp"

namespace MphRead::NativeRuntime::Avalonia::Controls
{
    namespace Primitives
    {
        enum class ScrollBarVisibility : std::int32_t { Disabled, Auto, Hidden, Visible };
    }

    using Primitives::ScrollBarVisibility;

    class ScrollChangedEventArgs final : public Interactivity::RoutedEventArgs
    {
    public:
        Vector ExtentDelta{};
        Vector OffsetDelta{};
        Vector ViewportDelta{};
    };

    class ScrollViewer : public Control
    {
    public:
        ScrollViewer();

        static StyledProperty<ControlPtr>& ContentProperty;
        static StyledProperty<Vector>& OffsetProperty;
        static StyledProperty<ScrollBarVisibility>& HorizontalScrollBarVisibilityProperty;
        static StyledProperty<ScrollBarVisibility>& VerticalScrollBarVisibilityProperty;
        static StyledProperty<bool>& AllowAutoHideProperty;

        [[nodiscard]] ControlPtr Content() const { return GetValue(ContentProperty); }
        void Content(ControlPtr value) { SetValue(ContentProperty, std::move(value)); }
        [[nodiscard]] Vector Offset() const { return GetValue(OffsetProperty); }
        void Offset(Vector value);
        [[nodiscard]] ScrollBarVisibility HorizontalScrollBarVisibility() const
        {
            return GetValue(HorizontalScrollBarVisibilityProperty);
        }
        void HorizontalScrollBarVisibility(ScrollBarVisibility value) { SetValue(HorizontalScrollBarVisibilityProperty, value); }
        [[nodiscard]] ScrollBarVisibility VerticalScrollBarVisibility() const
        {
            return GetValue(VerticalScrollBarVisibilityProperty);
        }
        void VerticalScrollBarVisibility(ScrollBarVisibility value) { SetValue(VerticalScrollBarVisibilityProperty, value); }
        [[nodiscard]] bool AllowAutoHide() const { return GetValue(AllowAutoHideProperty); }
        void AllowAutoHide(bool value) { SetValue(AllowAutoHideProperty, value); }

        [[nodiscard]] Size Extent() const noexcept { return _extent; }
        [[nodiscard]] Size Viewport() const noexcept { return _viewport; }
        [[nodiscard]] Vector ScrollBarMaximum() const noexcept;

        void LineUp();
        void LineDown();
        void PageUp();
        void PageDown();
        void ScrollToHome();
        void ScrollToEnd();
        // The smallest offset change that puts the rect (in the content's
        // coordinates) inside the viewport.
        bool BringIntoView(const Rect& rect);

        Event<ScrollViewer&, ScrollChangedEventArgs&> ScrollChanged;

        // The scroll bar's look, as a Style on ScrollBar and Thumb sets it.
        double ScrollBarWidth = 12.0;
        double ScrollBarCollapsedWidth = 2.0;
        Media::IBrushPtr ScrollBarBackground{};
        Media::IBrushPtr ThumbBackground{};
        Avalonia::CornerRadius ThumbCornerRadius{3.0};
        double ThumbMinLength = 24.0;
        // Set by a style that sizes the bar outright: no collapsed state.
        bool ScrollBarFixedWidth = false;

        void Render(Media::DrawingContext& context) override;
        void RenderOverlay(Media::DrawingContext& context) override;

    protected:
        Size MeasureOverride(Size availableSize) override;
        Size ArrangeOverride(Size finalSize) override;
        void OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change) override;
        void OnPointerWheelChanged(Input::PointerWheelEventArgs& e) override;
        void OnPointerEntered(Input::PointerEventArgs& e) override;
        void OnPointerExited(Input::PointerEventArgs& e) override;
        void OnPointerPressed(Input::PointerPressedEventArgs& e) override;
        void OnPointerMoved(Input::PointerEventArgs& e) override;
        void OnPointerReleased(Input::PointerReleasedEventArgs& e) override;
        void OnPointerCaptureLost(Input::PointerCaptureLostEventArgs& e) override;
        void OnKeyDown(Input::KeyEventArgs& e) override;
        [[nodiscard]] Matrix VisualChildTransform() const override;

    private:
        [[nodiscard]] Vector Coerce(Vector offset) const;
        [[nodiscard]] Rect ThumbRect(bool vertical) const;
        [[nodiscard]] bool BarVisible(bool vertical) const;

        Size _extent{};
        Size _viewport{};
        bool _pointerOver = false;
        // The touch gesture: where the finger went down and whether it has
        // become a scroll yet.
        const Input::IPointer* _gesturePointer = nullptr;
        Point _gestureStart{};
        Vector _gestureOffset{};
        bool _gestureScrolling = false;
        // Dragging the thumb.
        bool _thumbDrag = false;
        double _thumbGrab = 0.0;
        std::size_t _tunnelPressed = 0;
        std::size_t _tunnelMoved = 0;
        std::size_t _tunnelReleased = 0;
    };
}
