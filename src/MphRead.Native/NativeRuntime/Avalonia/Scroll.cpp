#include "Scroll.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace MphRead::NativeRuntime::Avalonia::Controls
{
    namespace
    {
        constexpr double Infinity = std::numeric_limits<double>::infinity();
        // ScrollViewer.DefaultSmallChange, and what one wheel notch moves.
        constexpr double SmallChange = 16.0;
        constexpr double WheelStep = 50.0;
        // ScrollGestureRecognizer.ScrollStartDistance.
        constexpr double ScrollStartDistance = 30.0;
    }

    StyledProperty<ControlPtr>& ScrollViewer::ContentProperty = Register<ScrollViewer, ControlPtr>("Content", nullptr);
    StyledProperty<Vector>& ScrollViewer::OffsetProperty = Register<ScrollViewer, Vector>("Offset", Vector{});
    StyledProperty<ScrollBarVisibility>& ScrollViewer::HorizontalScrollBarVisibilityProperty
        = Register<ScrollViewer, ScrollBarVisibility>("HorizontalScrollBarVisibility", ScrollBarVisibility::Disabled);
    StyledProperty<ScrollBarVisibility>& ScrollViewer::VerticalScrollBarVisibilityProperty
        = Register<ScrollViewer, ScrollBarVisibility>("VerticalScrollBarVisibility", ScrollBarVisibility::Auto);
    StyledProperty<bool>& ScrollViewer::AllowAutoHideProperty = Register<ScrollViewer, bool>("AllowAutoHide", true);

    ScrollViewer::ScrollViewer()
    {
        static const bool registered = []
        {
            AffectsMeasure<ScrollViewer>(ContentProperty, HorizontalScrollBarVisibilityProperty,
                VerticalScrollBarVisibilityProperty);
            AffectsRender<ScrollViewer>(OffsetProperty);
            return true;
        }();
        (void)registered;
        ClipToBounds(true);
        // The gesture is taken on the way down, before any row answers the
        // press, and handled events too -- a row always handles its own.
        _tunnelPressed = AddHandler<Input::PointerPressedEventArgs>(Input::InputElement::PointerPressedEvent,
            [this](Interactivity::Interactive&, Input::PointerPressedEventArgs& e)
            {
                if (e.Pointer->Type == Input::PointerType::Touch || e.Pointer->Type == Input::PointerType::Pen)
                {
                    _gesturePointer = e.Pointer;
                    _gestureStart = e.GetPosition(this);
                    _gestureOffset = Offset();
                    _gestureScrolling = false;
                }
            }, Interactivity::RoutingStrategies::Tunnel, true);
        _tunnelMoved = AddHandler<Input::PointerEventArgs>(Input::InputElement::PointerMovedEvent,
            [this](Interactivity::Interactive&, Input::PointerEventArgs& e)
            {
                if (e.Pointer != _gesturePointer)
                {
                    return;
                }
                const Point p = e.GetPosition(this);
                const Vector travel = p - _gestureStart;
                const bool vertical = VerticalScrollBarVisibility() != ScrollBarVisibility::Disabled;
                const bool horizontal = HorizontalScrollBarVisibility() != ScrollBarVisibility::Disabled;
                if (!_gestureScrolling)
                {
                    const double along = (vertical ? std::abs(travel.Y) : 0.0) + (horizontal ? std::abs(travel.X) : 0.0);
                    if (along <= ScrollStartDistance)
                    {
                        return;
                    }
                    _gestureScrolling = true;
                    // Taking the pointer is what tells the row under the
                    // finger that this press is no longer its.
                    e.Pointer->Capture(this);
                }
                Offset(Vector{horizontal ? _gestureOffset.X - travel.X : Offset().X,
                    vertical ? _gestureOffset.Y - travel.Y : Offset().Y});
                e.Handled = true;
            }, Interactivity::RoutingStrategies::Tunnel, true);
        _tunnelReleased = AddHandler<Input::PointerReleasedEventArgs>(Input::InputElement::PointerReleasedEvent,
            [this](Interactivity::Interactive&, Input::PointerReleasedEventArgs& e)
            {
                if (e.Pointer != _gesturePointer)
                {
                    return;
                }
                if (_gestureScrolling)
                {
                    e.Handled = true;
                    if (e.Pointer->Captured() == this)
                    {
                        e.Pointer->Capture(nullptr);
                    }
                }
                _gesturePointer = nullptr;
                _gestureScrolling = false;
            }, Interactivity::RoutingStrategies::Tunnel, true);
    }

    Vector ScrollViewer::ScrollBarMaximum() const noexcept
    {
        return {std::max(0.0, _extent.Width - _viewport.Width), std::max(0.0, _extent.Height - _viewport.Height)};
    }

    Vector ScrollViewer::Coerce(Vector offset) const
    {
        const Vector max = ScrollBarMaximum();
        return {std::clamp(offset.X, 0.0, max.X), std::clamp(offset.Y, 0.0, max.Y)};
    }

    void ScrollViewer::Offset(Vector value)
    {
        SetValue(OffsetProperty, Coerce(value));
    }

    void ScrollViewer::OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change)
    {
        if (&change.Property == &ContentProperty)
        {
            if (const ControlPtr old = change.GetOldValue<ControlPtr>())
            {
                RemoveVisualChild(old);
            }
            if (const ControlPtr now = change.GetNewValue<ControlPtr>())
            {
                AddVisualChild(now);
            }
        }
        Control::OnPropertyChanged(change);
        if (&change.Property == &OffsetProperty)
        {
            const Vector before = change.GetOldValue<Vector>();
            const Vector after = change.GetNewValue<Vector>();
            ScrollChangedEventArgs args;
            args.OffsetDelta = after - before;
            ScrollChanged(*this, args);
            InvalidateVisual();
        }
    }

    Matrix ScrollViewer::VisualChildTransform() const
    {
        const Vector offset = Offset();
        return Matrix::CreateTranslation(-offset.X, -offset.Y);
    }

    Size ScrollViewer::MeasureOverride(Size availableSize)
    {
        const ControlPtr content = Content();
        if (content == nullptr)
        {
            return {};
        }
        // The overlay bar takes no room, so the content gets the full width
        // across and an unlimited length along whatever scrolls.
        const Size childAvailable{
            HorizontalScrollBarVisibility() == ScrollBarVisibility::Disabled ? availableSize.Width : Infinity,
            VerticalScrollBarVisibility() == ScrollBarVisibility::Disabled ? availableSize.Height : Infinity};
        content->Measure(childAvailable);
        const Size desired = content->DesiredSize();
        return {std::min(desired.Width, availableSize.Width), std::min(desired.Height, availableSize.Height)};
    }

    Size ScrollViewer::ArrangeOverride(Size finalSize)
    {
        const Size oldExtent = _extent;
        const Size oldViewport = _viewport;
        _viewport = finalSize;
        if (const ControlPtr content = Content())
        {
            const Size desired = content->DesiredSize();
            const Size arranged{
                HorizontalScrollBarVisibility() == ScrollBarVisibility::Disabled ? finalSize.Width
                                                                                 : std::max(finalSize.Width, desired.Width),
                VerticalScrollBarVisibility() == ScrollBarVisibility::Disabled ? finalSize.Height
                                                                               : std::max(finalSize.Height, desired.Height)};
            content->Arrange(Rect(arranged));
            _extent = arranged;
        }
        else
        {
            _extent = {};
        }
        const Vector coerced = Coerce(Offset());
        if (coerced != Offset())
        {
            SetValue(OffsetProperty, coerced);
        }
        if (oldExtent != _extent || oldViewport != _viewport)
        {
            ScrollChangedEventArgs args;
            args.ExtentDelta = Vector{_extent.Width - oldExtent.Width, _extent.Height - oldExtent.Height};
            args.ViewportDelta = Vector{_viewport.Width - oldViewport.Width, _viewport.Height - oldViewport.Height};
            ScrollChanged(*this, args);
        }
        return finalSize;
    }

    void ScrollViewer::LineUp()
    {
        Offset(Offset() - Vector{0, SmallChange});
    }

    void ScrollViewer::LineDown()
    {
        Offset(Offset() + Vector{0, SmallChange});
    }

    void ScrollViewer::PageUp()
    {
        Offset(Offset() - Vector{0, _viewport.Height});
    }

    void ScrollViewer::PageDown()
    {
        Offset(Offset() + Vector{0, _viewport.Height});
    }

    void ScrollViewer::ScrollToHome()
    {
        Offset(Vector{Offset().X, 0});
    }

    void ScrollViewer::ScrollToEnd()
    {
        // Laid out first, so the end is the end of what is there now.
        UpdateLayout();
        Offset(Vector{Offset().X, ScrollBarMaximum().Y});
    }

    bool ScrollViewer::BringIntoView(const Rect& rect)
    {
        Vector offset = Offset();
        if (rect.Bottom() > offset.Y + _viewport.Height)
        {
            offset.Y = rect.Bottom() - _viewport.Height;
        }
        if (rect.Y < offset.Y)
        {
            offset.Y = rect.Y;
        }
        if (rect.Right() > offset.X + _viewport.Width)
        {
            offset.X = rect.Right() - _viewport.Width;
        }
        if (rect.X < offset.X)
        {
            offset.X = rect.X;
        }
        const Vector coerced = Coerce(offset);
        if (coerced == Offset())
        {
            return false;
        }
        Offset(coerced);
        return true;
    }

    bool ScrollViewer::BarVisible(bool vertical) const
    {
        const ScrollBarVisibility visibility = vertical ? VerticalScrollBarVisibility() : HorizontalScrollBarVisibility();
        if (visibility == ScrollBarVisibility::Disabled || visibility == ScrollBarVisibility::Hidden)
        {
            return false;
        }
        const Vector max = ScrollBarMaximum();
        return visibility == ScrollBarVisibility::Visible || (vertical ? max.Y : max.X) > 0.5;
    }

    Rect ScrollViewer::ThumbRect(bool vertical) const
    {
        const double width = ScrollBarFixedWidth || _pointerOver || _thumbDrag ? ScrollBarWidth : ScrollBarCollapsedWidth;
        const Vector offset = Offset();
        if (vertical)
        {
            const double track = _viewport.Height;
            const double length = _extent.Height <= 0 ? track
                : std::clamp(track * _viewport.Height / _extent.Height, std::min(ThumbMinLength, track), track);
            const double max = std::max(1e-9, _extent.Height - _viewport.Height);
            const double y = (track - length) * std::clamp(offset.Y / max, 0.0, 1.0);
            return Rect{_viewport.Width - width - (ScrollBarFixedWidth ? 0.0 : 1.0), y, width, length};
        }
        const double track = _viewport.Width;
        const double length = _extent.Width <= 0 ? track
            : std::clamp(track * _viewport.Width / _extent.Width, std::min(ThumbMinLength, track), track);
        const double max = std::max(1e-9, _extent.Width - _viewport.Width);
        const double x = (track - length) * std::clamp(offset.X / max, 0.0, 1.0);
        return Rect{x, _viewport.Height - width - (ScrollBarFixedWidth ? 0.0 : 1.0), length, width};
    }

    void ScrollViewer::Render(Media::DrawingContext& context)
    {
        // The bar is drawn over the content by the renderer's second pass;
        // see TopLevel, which calls RenderOverlay after the children.
        (void)context;
    }

    void ScrollViewer::RenderOverlay(Media::DrawingContext& context)
    {
        // The overlay bar: a thin line at rest, the full thumb under the
        // pointer, as the Fluent theme's auto-hiding bar draws it.
        for (const bool vertical : {true, false})
        {
            if (!BarVisible(vertical))
            {
                continue;
            }
            const Rect thumb = ThumbRect(vertical);
            const bool expanded = ScrollBarFixedWidth || _pointerOver || _thumbDrag;
            if (expanded && ScrollBarBackground != nullptr)
            {
                const Rect track = vertical ? Rect{thumb.X, 0, thumb.Width, _viewport.Height}
                                            : Rect{0, thumb.Y, _viewport.Width, thumb.Height};
                context.FillRectangle(ScrollBarBackground, track);
            }
            Media::IBrushPtr brush = ThumbBackground;
            if (brush == nullptr)
            {
                brush = std::make_shared<Media::SolidColorBrush>(Media::Color::FromArgb(0x8B, 0xFF, 0xFF, 0xFF));
            }
            context.DrawRectangle(brush, nullptr, thumb, ThumbCornerRadius);
        }
    }

    void ScrollViewer::OnPointerWheelChanged(Input::PointerWheelEventArgs& e)
    {
        const Vector before = Offset();
        Vector offset = before;
        if (_extent.Height > _viewport.Height && VerticalScrollBarVisibility() != ScrollBarVisibility::Disabled)
        {
            offset.Y -= e.Delta.Y * WheelStep;
        }
        if (_extent.Width > _viewport.Width && HorizontalScrollBarVisibility() != ScrollBarVisibility::Disabled)
        {
            offset.X -= e.Delta.X * WheelStep;
        }
        Offset(offset);
        // Handled only when it moved, so a list at its end lets the wheel
        // reach the page around it.
        if (Offset() != before)
        {
            e.Handled = true;
        }
    }

    void ScrollViewer::OnPointerEntered(Input::PointerEventArgs& e)
    {
        (void)e;
        _pointerOver = true;
        InvalidateVisual();
    }

    void ScrollViewer::OnPointerExited(Input::PointerEventArgs& e)
    {
        (void)e;
        _pointerOver = false;
        InvalidateVisual();
    }

    void ScrollViewer::OnPointerPressed(Input::PointerPressedEventArgs& e)
    {
        if (e.Pointer->Type != Input::PointerType::Mouse || !BarVisible(true))
        {
            return;
        }
        const Point p = e.GetPosition(this);
        const Rect thumb = ThumbRect(true);
        const Rect track{_viewport.Width - ScrollBarWidth - 1, 0, ScrollBarWidth + 1, _viewport.Height};
        if (!track.Contains(p))
        {
            return;
        }
        if (thumb.Contains(p))
        {
            _thumbDrag = true;
            _thumbGrab = p.Y - thumb.Y;
            e.Pointer->Capture(this);
        }
        else if (p.Y < thumb.Y)
        {
            PageUp();
        }
        else
        {
            PageDown();
        }
        e.Handled = true;
    }

    void ScrollViewer::OnPointerMoved(Input::PointerEventArgs& e)
    {
        if (!_thumbDrag)
        {
            return;
        }
        const Point p = e.GetPosition(this);
        const Rect thumb = ThumbRect(true);
        const double travel = std::max(1e-9, _viewport.Height - thumb.Height);
        const double fraction = std::clamp((p.Y - _thumbGrab) / travel, 0.0, 1.0);
        Offset(Vector{Offset().X, fraction * ScrollBarMaximum().Y});
        e.Handled = true;
    }

    void ScrollViewer::OnPointerReleased(Input::PointerReleasedEventArgs& e)
    {
        if (_thumbDrag)
        {
            _thumbDrag = false;
            if (e.Pointer->Captured() == this)
            {
                e.Pointer->Capture(nullptr);
            }
            e.Handled = true;
            InvalidateVisual();
        }
    }

    void ScrollViewer::OnPointerCaptureLost(Input::PointerCaptureLostEventArgs& e)
    {
        (void)e;
        _thumbDrag = false;
        _gestureScrolling = false;
        _gesturePointer = nullptr;
    }

    void ScrollViewer::OnKeyDown(Input::KeyEventArgs& e)
    {
        switch (e.Key)
        {
        case Input::Key::PageDown:
            PageDown();
            e.Handled = true;
            break;
        case Input::Key::PageUp:
            PageUp();
            e.Handled = true;
            break;
        default:
            break;
        }
    }

    // ---------------------------------------------------- BringIntoView

    void Control::BringIntoView()
    {
        BringIntoView(Rect(Bounds().GetSize()));
    }

    void Control::BringIntoView(const Rect& rect)
    {
        // Each viewer above, innermost first, told where this rect sits in
        // its content.
        for (Visual* v = GetVisualParent(); v != nullptr; v = v->GetVisualParent())
        {
            auto* viewer = dynamic_cast<ScrollViewer*>(v);
            if (viewer == nullptr)
            {
                continue;
            }
            const ControlPtr content = viewer->Content();
            if (content == nullptr)
            {
                continue;
            }
            const std::optional<Point> topLeft = TranslatePoint(rect.TopLeft(), content.get());
            if (!topLeft.has_value())
            {
                continue;
            }
            viewer->BringIntoView(Rect{topLeft->X, topLeft->Y, rect.Width, rect.Height});
        }
    }
}
