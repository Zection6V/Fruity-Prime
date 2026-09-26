#include "Controls.hpp"

#include "TopLevel.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace MphRead::NativeRuntime::Avalonia
{
    namespace
    {
        constexpr double Infinity = std::numeric_limits<double>::infinity();
        constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
    }

    // ------------------------------------------------------------ Visual

    StyledProperty<bool>& Visual::IsVisibleProperty = Register<Visual, bool>("IsVisible", true);
    StyledProperty<double>& Visual::OpacityProperty = Register<Visual, double>("Opacity", 1.0);
    StyledProperty<bool>& Visual::ClipToBoundsProperty = Register<Visual, bool>("ClipToBounds", false);
    StyledProperty<Media::TransformPtr>& Visual::RenderTransformProperty
        = Register<Visual, Media::TransformPtr>("RenderTransform", nullptr);
    StyledProperty<RelativePoint>& Visual::RenderTransformOriginProperty
        = Register<Visual, RelativePoint>("RenderTransformOrigin", RelativePoint::Center());
    StyledProperty<std::int32_t>& Visual::ZIndexProperty = Register<Visual, std::int32_t>("ZIndex", 0);

    Visual::Visual()
    {
        static const bool registered = []
        {
            AffectsRender<Visual>(IsVisibleProperty, OpacityProperty, ClipToBoundsProperty, RenderTransformProperty,
                RenderTransformOriginProperty, ZIndexProperty);
            return true;
        }();
        (void)registered;
    }

    Visual::~Visual()
    {
        for (const std::shared_ptr<Visual>& child : _visualChildren)
        {
            child->_visualParent = nullptr;
        }
    }

    TopLevel* Visual::GetVisualRoot() const noexcept
    {
        return _root;
    }

    bool Visual::IsEffectivelyVisible() const
    {
        for (const Visual* v = this; v != nullptr; v = v->_visualParent)
        {
            if (!v->IsVisible())
            {
                return false;
            }
        }
        return true;
    }

    Matrix Visual::LocalTransform() const
    {
        Matrix m = Matrix::CreateTranslation(_bounds.X, _bounds.Y);
        if (const Media::TransformPtr transform = RenderTransform())
        {
            const Point origin = RenderTransformOrigin().ToPixels(_bounds.GetSize());
            m = Matrix::CreateTranslation(-origin.X, -origin.Y) * transform->Value()
                * Matrix::CreateTranslation(origin.X, origin.Y) * m;
        }
        if (_visualParent != nullptr)
        {
            m = m * _visualParent->VisualChildTransform();
        }
        return m;
    }

    std::optional<Matrix> Visual::TransformToVisual(const Visual* relativeTo) const
    {
        // Both to the root, then one inverted.
        const auto toRoot = [](const Visual* v)
        {
            Matrix m = Matrix::Identity();
            for (; v != nullptr; v = v->_visualParent)
            {
                m = m * v->LocalTransform();
            }
            return m;
        };
        if (relativeTo == nullptr)
        {
            return toRoot(this);
        }
        if (GetVisualRoot() != relativeTo->GetVisualRoot())
        {
            return std::nullopt;
        }
        const std::optional<Matrix> inverse = toRoot(relativeTo).TryInvert();
        if (!inverse.has_value())
        {
            return std::nullopt;
        }
        return toRoot(this) * *inverse;
    }

    std::optional<Point> Visual::TranslatePoint(Point point, const Visual* relativeTo) const
    {
        const std::optional<Matrix> m = TransformToVisual(relativeTo);
        if (!m.has_value())
        {
            return std::nullopt;
        }
        return m->Transform(point);
    }

    PixelPoint Visual::PointToScreen(Point point) const
    {
        const std::optional<Point> p = TranslatePoint(point, nullptr);
        return p.has_value() ? PixelPoint{static_cast<std::int32_t>(std::lround(p->X)), static_cast<std::int32_t>(std::lround(p->Y))}
                             : PixelPoint{};
    }

    void Visual::Render(Media::DrawingContext& context)
    {
        (void)context;
    }

    void Visual::InvalidateVisual()
    {
        if (_root != nullptr)
        {
            _root->InvalidateRender();
        }
    }

    std::vector<Visual*> Visual::GetVisualDescendants() const
    {
        std::vector<Visual*> result;
        const std::function<void(const Visual&)> walk = [&](const Visual& v)
        {
            for (const std::shared_ptr<Visual>& child : v._visualChildren)
            {
                result.push_back(child.get());
                walk(*child);
            }
        };
        walk(*this);
        return result;
    }

    std::vector<Visual*> Visual::GetVisualAncestors() const
    {
        std::vector<Visual*> result;
        for (Visual* v = _visualParent; v != nullptr; v = v->_visualParent)
        {
            result.push_back(v);
        }
        return result;
    }

    void Visual::AddVisualChild(const std::shared_ptr<Visual>& child)
    {
        InsertVisualChild(_visualChildren.size(), child);
    }

    void Visual::InsertVisualChild(std::size_t index, const std::shared_ptr<Visual>& child)
    {
        if (child == nullptr)
        {
            return;
        }
        if (child->_visualParent != nullptr)
        {
            if (child->_visualParent == this)
            {
                return;
            }
            throw std::invalid_argument("The control already has a visual parent.");
        }
        index = std::min(index, _visualChildren.size());
        _visualChildren.insert(_visualChildren.begin() + static_cast<std::ptrdiff_t>(index), child);
        child->_visualParent = this;
        child->SetInheritanceParent(this);
        if (_root != nullptr)
        {
            child->SetRoot(_root);
        }
        if (auto* layoutable = dynamic_cast<Layout::Layoutable*>(this))
        {
            layoutable->InvalidateMeasure();
        }
        InvalidateVisual();
    }

    void Visual::RemoveVisualChild(const std::shared_ptr<Visual>& child)
    {
        const auto found = std::find(_visualChildren.begin(), _visualChildren.end(), child);
        if (found == _visualChildren.end())
        {
            return;
        }
        std::shared_ptr<Visual> keep = *found;
        _visualChildren.erase(found);
        keep->SetRoot(nullptr);
        keep->_visualParent = nullptr;
        keep->SetInheritanceParent(nullptr);
        if (auto* layoutable = dynamic_cast<Layout::Layoutable*>(this))
        {
            layoutable->InvalidateMeasure();
        }
        InvalidateVisual();
    }

    void Visual::ClearVisualChildren()
    {
        const std::vector<std::shared_ptr<Visual>> children = _visualChildren;
        for (const std::shared_ptr<Visual>& child : children)
        {
            RemoveVisualChild(child);
        }
    }

    void Visual::SetBounds(const Rect& bounds)
    {
        if (_bounds != bounds)
        {
            _bounds = bounds;
            InvalidateVisual();
        }
    }

    void Visual::SetRoot(TopLevel* root)
    {
        if (_root == root)
        {
            return;
        }
        if (_root != nullptr && root == nullptr)
        {
            // Children first, as Avalonia detaches the subtree bottom up.
            for (const std::shared_ptr<Visual>& child : _visualChildren)
            {
                child->SetRoot(nullptr);
            }
            OnDetachedFromVisualTree();
            _root = nullptr;
            return;
        }
        _root = root;
        OnAttachedToVisualTree();
        for (const std::shared_ptr<Visual>& child : _visualChildren)
        {
            child->SetRoot(root);
        }
    }

    void Visual::OnAttachedToVisualTree()
    {
    }

    void Visual::OnDetachedFromVisualTree()
    {
    }

    void Visual::OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change)
    {
        AvaloniaObject::OnPropertyChanged(change);
        if ((change.Property.AffectsFor(*this) & static_cast<std::uint8_t>(AvaloniaProperty::Affects::Render)) != 0)
        {
            InvalidateVisual();
        }
    }

    void Visual::ForEachInheritanceChild(const std::function<void(AvaloniaObject&)>& visit)
    {
        for (const std::shared_ptr<Visual>& child : _visualChildren)
        {
            visit(*child);
        }
    }

    // -------------------------------------------------------- Layoutable

    namespace Layout
    {
        StyledProperty<double>& Layoutable::WidthProperty = Register<Layoutable, double>("Width", NaN);
        StyledProperty<double>& Layoutable::HeightProperty = Register<Layoutable, double>("Height", NaN);
        StyledProperty<double>& Layoutable::MinWidthProperty = Register<Layoutable, double>("MinWidth", 0.0);
        StyledProperty<double>& Layoutable::MaxWidthProperty = Register<Layoutable, double>("MaxWidth", Infinity);
        StyledProperty<double>& Layoutable::MinHeightProperty = Register<Layoutable, double>("MinHeight", 0.0);
        StyledProperty<double>& Layoutable::MaxHeightProperty = Register<Layoutable, double>("MaxHeight", Infinity);
        StyledProperty<Thickness>& Layoutable::MarginProperty = Register<Layoutable, Thickness>("Margin", Thickness{});
        StyledProperty<Layout::HorizontalAlignment>& Layoutable::HorizontalAlignmentProperty
            = Register<Layoutable, Layout::HorizontalAlignment>("HorizontalAlignment", Layout::HorizontalAlignment::Stretch);
        StyledProperty<Layout::VerticalAlignment>& Layoutable::VerticalAlignmentProperty
            = Register<Layoutable, Layout::VerticalAlignment>("VerticalAlignment", Layout::VerticalAlignment::Stretch);
        StyledProperty<bool>& Layoutable::UseLayoutRoundingProperty
            = Register<Layoutable, bool>("UseLayoutRounding", true);

        double RoundLayoutValue(double value, double scale) noexcept
        {
            if (!std::isfinite(value))
            {
                return value;
            }
            if (scale == 1.0)
            {
                return std::round(value);
            }
            const double rounded = std::round(value * scale) / scale;
            return std::isnan(rounded) || std::isinf(rounded) ? value : rounded;
        }

        Size RoundLayoutSizeUp(Size size, double scale) noexcept
        {
            // Avalonia rounds up with a small tolerance, so 10.00000001 stays 10.
            const auto up = [scale](double v)
            {
                if (!std::isfinite(v))
                {
                    return v;
                }
                return std::ceil(v * scale - 0.0001) / scale;
            };
            return {up(size.Width), up(size.Height)};
        }

        namespace
        {
            struct MinMax final
            {
                double MinWidth;
                double MaxWidth;
                double MinHeight;
                double MaxHeight;

                explicit MinMax(const Layoutable& e)
                {
                    MaxHeight = e.MaxHeight();
                    MinHeight = e.MinHeight();
                    double l = e.Height();
                    double height = std::isnan(l) ? Infinity : l;
                    MaxHeight = std::max(std::min(height, MaxHeight), MinHeight);
                    height = std::isnan(l) ? 0 : l;
                    MinHeight = std::max(std::min(MaxHeight, height), MinHeight);

                    MaxWidth = e.MaxWidth();
                    MinWidth = e.MinWidth();
                    l = e.Width();
                    double width = std::isnan(l) ? Infinity : l;
                    MaxWidth = std::max(std::min(width, MaxWidth), MinWidth);
                    width = std::isnan(l) ? 0 : l;
                    MinWidth = std::max(std::min(MaxWidth, width), MinWidth);
                }
            };

            [[nodiscard]] Size ApplyLayoutConstraints(const Layoutable& e, Size constraints)
            {
                const MinMax m(e);
                return {std::clamp(constraints.Width, m.MinWidth, m.MaxWidth),
                    std::clamp(constraints.Height, m.MinHeight, m.MaxHeight)};
            }

            [[nodiscard]] Size NonNegative(Size s) noexcept
            {
                return {std::max(0.0, s.Width), std::max(0.0, s.Height)};
            }
        }

        void Layoutable::Measure(Size availableSize)
        {
            if (std::isnan(availableSize.Width) || std::isnan(availableSize.Height))
            {
                throw std::invalid_argument("Cannot call Measure using a size with NaN values.");
            }
            if (!_measureValid || _previousMeasure != availableSize)
            {
                const Size previous = _desiredSize;
                _measureValid = true;
                Size desired = MeasureCore(availableSize);
                if (!std::isfinite(desired.Width))
                {
                    desired.Width = 0;
                }
                if (!std::isfinite(desired.Height))
                {
                    desired.Height = 0;
                }
                _previousMeasure = availableSize;
                _desiredSize = desired;
                if (previous != desired)
                {
                    if (auto* parent = dynamic_cast<Layoutable*>(GetVisualParent()))
                    {
                        if (parent->_measureValid)
                        {
                            parent->InvalidateMeasure();
                        }
                    }
                }
            }
        }

        void Layoutable::Arrange(Rect rect)
        {
            if (!_measureValid)
            {
                Measure(_previousMeasure.value_or(rect.GetSize()));
            }
            if (!_arrangeValid || _previousArrange != rect)
            {
                _arrangeValid = true;
                ArrangeCore(rect);
                _previousArrange = rect;
            }
        }

        void Layoutable::InvalidateMeasure()
        {
            const bool wasValid = _measureValid;
            _measureValid = false;
            _arrangeValid = false;
            if (wasValid)
            {
                OnMeasureInvalidated();
                // Every ancestor measures again; its cached children stay put.
                if (auto* parent = dynamic_cast<Layoutable*>(GetVisualParent()))
                {
                    parent->InvalidateMeasure();
                }
            }
            if (_root != nullptr)
            {
                _root->InvalidateLayout();
            }
            InvalidateVisual();
        }

        void Layoutable::InvalidateArrange()
        {
            if (_arrangeValid)
            {
                _arrangeValid = false;
                if (auto* parent = dynamic_cast<Layoutable*>(GetVisualParent()))
                {
                    parent->InvalidateArrange();
                }
            }
            if (_root != nullptr)
            {
                _root->InvalidateLayout();
            }
            InvalidateVisual();
        }

        void Layoutable::UpdateLayout()
        {
            if (_root != nullptr)
            {
                _root->ExecuteLayoutPass();
            }
        }

        Size Layoutable::MeasureCore(Size availableSize)
        {
            if (!IsVisible())
            {
                return {};
            }
            const Thickness margin = Margin();
            const bool rounding = UseLayoutRounding();
            const Size constrained = ApplyLayoutConstraints(*this, availableSize.Deflate(margin));
            const Size measured = MeasureOverride(constrained);
            double width = measured.Width;
            double height = measured.Height;
            if (const double w = Width(); !std::isnan(w))
            {
                width = w;
            }
            width = std::max(std::min(width, MaxWidth()), MinWidth());
            if (const double h = Height(); !std::isnan(h))
            {
                height = h;
            }
            height = std::max(std::min(height, MaxHeight()), MinHeight());
            if (rounding)
            {
                const Size rounded = RoundLayoutSizeUp({width, height});
                width = rounded.Width;
                height = rounded.Height;
            }
            width = std::min(width, availableSize.Width);
            height = std::min(height, availableSize.Height);
            return NonNegative(Size{width, height}.Inflate(margin));
        }

        void Layoutable::ArrangeCore(Rect finalRect)
        {
            if (!IsVisible())
            {
                return;
            }
            const bool rounding = UseLayoutRounding();
            const Thickness margin = Margin();
            double originX = finalRect.X + margin.Left;
            double originY = finalRect.Y + margin.Top;
            Size available{std::max(0.0, finalRect.Width - margin.Left - margin.Right),
                std::max(0.0, finalRect.Height - margin.Top - margin.Bottom)};
            const Layout::HorizontalAlignment horizontal = HorizontalAlignment();
            const Layout::VerticalAlignment vertical = VerticalAlignment();
            Size size = available;
            if (horizontal != Layout::HorizontalAlignment::Stretch)
            {
                size.Width = std::min(size.Width, _desiredSize.Width - margin.Left - margin.Right);
            }
            if (vertical != Layout::VerticalAlignment::Stretch)
            {
                size.Height = std::min(size.Height, _desiredSize.Height - margin.Top - margin.Bottom);
            }
            size = ApplyLayoutConstraints(*this, size);
            if (rounding)
            {
                size = RoundLayoutSizeUp(size);
                available = RoundLayoutSizeUp(available);
            }
            const Size arranged = ArrangeOverride(size);
            size = Size{std::min(arranged.Width, size.Width), std::min(arranged.Height, size.Height)};
            switch (horizontal)
            {
            case Layout::HorizontalAlignment::Center:
            case Layout::HorizontalAlignment::Stretch:
                originX += (available.Width - size.Width) / 2;
                break;
            case Layout::HorizontalAlignment::Right:
                originX += available.Width - size.Width;
                break;
            default:
                break;
            }
            switch (vertical)
            {
            case Layout::VerticalAlignment::Center:
            case Layout::VerticalAlignment::Stretch:
                originY += (available.Height - size.Height) / 2;
                break;
            case Layout::VerticalAlignment::Bottom:
                originY += available.Height - size.Height;
                break;
            default:
                break;
            }
            if (rounding)
            {
                originX = RoundLayoutValue(originX);
                originY = RoundLayoutValue(originY);
            }
            SetBounds(Rect{originX, originY, size.Width, size.Height});
        }

        Size Layoutable::MeasureOverride(Size availableSize)
        {
            double width = 0;
            double height = 0;
            for (const std::shared_ptr<Visual>& child : VisualChildren())
            {
                if (auto* layoutable = dynamic_cast<Layoutable*>(child.get()))
                {
                    layoutable->Measure(availableSize);
                    width = std::max(width, layoutable->DesiredSize().Width);
                    height = std::max(height, layoutable->DesiredSize().Height);
                }
            }
            return {width, height};
        }

        Size Layoutable::ArrangeOverride(Size finalSize)
        {
            for (const std::shared_ptr<Visual>& child : VisualChildren())
            {
                if (auto* layoutable = dynamic_cast<Layoutable*>(child.get()))
                {
                    layoutable->Arrange(Rect(finalSize));
                }
            }
            return finalSize;
        }

        void Layoutable::OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change)
        {
            Visual::OnPropertyChanged(change);
            const std::uint8_t affects = change.Property.AffectsFor(*this);
            const bool layoutProperty = &change.Property == &WidthProperty || &change.Property == &HeightProperty
                || &change.Property == &MinWidthProperty || &change.Property == &MaxWidthProperty
                || &change.Property == &MinHeightProperty || &change.Property == &MaxHeightProperty
                || &change.Property == &MarginProperty || &change.Property == &IsVisibleProperty
                || &change.Property == &HorizontalAlignmentProperty || &change.Property == &VerticalAlignmentProperty;
            if (layoutProperty || (affects & static_cast<std::uint8_t>(AvaloniaProperty::Affects::Measure)) != 0)
            {
                InvalidateMeasure();
            }
            else if ((affects & static_cast<std::uint8_t>(AvaloniaProperty::Affects::Arrange)) != 0)
            {
                InvalidateArrange();
            }
        }
    }

    // ------------------------------------------------------- Interactive

    namespace Interactivity
    {
        std::size_t Interactive::AddHandler(const RoutedEvent& routedEvent, Handler handler, RoutingStrategies routes,
            bool handledEventsToo)
        {
            _handlers.push_back(Subscription{++_next, &routedEvent, std::move(handler), routes, handledEventsToo});
            return _next;
        }

        void Interactive::RemoveHandler(std::size_t token)
        {
            std::erase_if(_handlers, [token](const Subscription& s) { return s.Token == token; });
        }

        void Interactive::Invoke(RoutedEventArgs& e, RoutingStrategies phase)
        {
            e.Route = phase;
            if (phase != RoutingStrategies::Tunnel && !e.Handled)
            {
                ClassHandle(e);
            }
            const std::vector<Subscription> handlers = _handlers;
            for (const Subscription& s : handlers)
            {
                if (s.Event == e.RoutedEvent && HasFlag(s.Routes, phase) && (!e.Handled || s.HandledEventsToo))
                {
                    s.Callback(*this, e);
                }
            }
            if (phase != RoutingStrategies::Tunnel && !e.Handled)
            {
                RaiseInstanceEvents(e);
            }
        }

        void Interactive::RaiseEvent(RoutedEventArgs& e)
        {
            if (e.RoutedEvent == nullptr)
            {
                return;
            }
            e.Source = this;
            const RoutingStrategies strategies = e.RoutedEvent->RoutingStrategies;
            // Keep the route alive while it runs: a handler may take a
            // control out of the tree.
            std::vector<std::shared_ptr<AvaloniaObject>> keep;
            std::vector<Interactive*> route;
            for (Visual* v = this; v != nullptr; v = v->GetVisualParent())
            {
                if (auto* interactive = dynamic_cast<Interactive*>(v))
                {
                    route.push_back(interactive);
                    if (auto shared = interactive->weak_from_this().lock())
                    {
                        keep.push_back(std::move(shared));
                    }
                }
            }
            if (HasFlag(strategies, RoutingStrategies::Direct) && !HasFlag(strategies, RoutingStrategies::Bubble)
                && !HasFlag(strategies, RoutingStrategies::Tunnel))
            {
                Invoke(e, RoutingStrategies::Direct);
                return;
            }
            if (HasFlag(strategies, RoutingStrategies::Tunnel))
            {
                for (auto it = route.rbegin(); it != route.rend(); ++it)
                {
                    (*it)->Invoke(e, RoutingStrategies::Tunnel);
                }
            }
            if (HasFlag(strategies, RoutingStrategies::Bubble))
            {
                for (Interactive* element : route)
                {
                    element->Invoke(e, RoutingStrategies::Bubble);
                }
            }
        }
    }

    // ------------------------------------------------------ InputElement

    namespace Input
    {
        using Interactivity::RoutedEvent;
        using Interactivity::RoutingStrategies;

        StyledProperty<bool>& InputElement::FocusableProperty = Register<InputElement, bool>("Focusable", false);
        StyledProperty<bool>& InputElement::IsEnabledProperty = Register<InputElement, bool>("IsEnabled", true);
        StyledProperty<bool>& InputElement::IsHitTestVisibleProperty
            = Register<InputElement, bool>("IsHitTestVisible", true);
        StyledProperty<std::shared_ptr<Input::Cursor>>& InputElement::CursorProperty
            = Register<InputElement, std::shared_ptr<Input::Cursor>>("Cursor", nullptr, true);
        StyledProperty<bool>& InputElement::IsTabStopProperty = Register<InputElement, bool>("IsTabStop", true);

        RoutedEvent InputElement::PointerEnteredEvent("PointerEntered", RoutingStrategies::Direct);
        RoutedEvent InputElement::PointerExitedEvent("PointerExited", RoutingStrategies::Direct);
        RoutedEvent InputElement::PointerPressedEvent("PointerPressed", RoutingStrategies::Tunnel | RoutingStrategies::Bubble);
        RoutedEvent InputElement::PointerMovedEvent("PointerMoved", RoutingStrategies::Tunnel | RoutingStrategies::Bubble);
        RoutedEvent InputElement::PointerReleasedEvent("PointerReleased", RoutingStrategies::Tunnel | RoutingStrategies::Bubble);
        RoutedEvent InputElement::PointerCaptureLostEvent("PointerCaptureLost", RoutingStrategies::Direct);
        RoutedEvent InputElement::PointerWheelChangedEvent("PointerWheelChanged", RoutingStrategies::Tunnel | RoutingStrategies::Bubble);
        RoutedEvent InputElement::KeyDownEvent("KeyDown", RoutingStrategies::Tunnel | RoutingStrategies::Bubble);
        RoutedEvent InputElement::KeyUpEvent("KeyUp", RoutingStrategies::Tunnel | RoutingStrategies::Bubble);
        RoutedEvent InputElement::TextInputEvent("TextInput", RoutingStrategies::Tunnel | RoutingStrategies::Bubble);
        RoutedEvent InputElement::GotFocusEvent("GotFocus", RoutingStrategies::Bubble);
        RoutedEvent InputElement::LostFocusEvent("LostFocus", RoutingStrategies::Bubble);

        InputElement::InputElement()
        {
            static const bool registered = []
            {
                AffectsRender<InputElement>(IsEnabledProperty);
                return true;
            }();
            (void)registered;
        }

        bool InputElement::IsEffectivelyEnabled() const
        {
            for (const Visual* v = this; v != nullptr; v = v->GetVisualParent())
            {
                if (const auto* e = dynamic_cast<const InputElement*>(v); e != nullptr && !e->IsEnabled())
                {
                    return false;
                }
            }
            return true;
        }

        bool InputElement::Focus(NavigationMethod method, KeyModifiers keyModifiers)
        {
            TopLevel* root = GetVisualRoot();
            if (root == nullptr || !Focusable() || !IsEffectivelyEnabled() || !IsEffectivelyVisible())
            {
                return false;
            }
            root->SetFocusedElement(this, method, keyModifiers);
            return true;
        }

        void InputElement::ClassHandle(Interactivity::RoutedEventArgs& e)
        {
            const RoutedEvent* r = e.RoutedEvent;
            if (r == &PointerEnteredEvent)
            {
                OnPointerEntered(static_cast<PointerEventArgs&>(e));
            }
            else if (r == &PointerExitedEvent)
            {
                OnPointerExited(static_cast<PointerEventArgs&>(e));
            }
            else if (r == &PointerPressedEvent)
            {
                OnPointerPressed(static_cast<PointerPressedEventArgs&>(e));
            }
            else if (r == &PointerMovedEvent)
            {
                OnPointerMoved(static_cast<PointerEventArgs&>(e));
            }
            else if (r == &PointerReleasedEvent)
            {
                OnPointerReleased(static_cast<PointerReleasedEventArgs&>(e));
            }
            else if (r == &PointerCaptureLostEvent)
            {
                OnPointerCaptureLost(static_cast<PointerCaptureLostEventArgs&>(e));
            }
            else if (r == &PointerWheelChangedEvent)
            {
                OnPointerWheelChanged(static_cast<PointerWheelEventArgs&>(e));
            }
            else if (r == &KeyDownEvent)
            {
                OnKeyDown(static_cast<KeyEventArgs&>(e));
            }
            else if (r == &KeyUpEvent)
            {
                OnKeyUp(static_cast<KeyEventArgs&>(e));
            }
            else if (r == &TextInputEvent)
            {
                OnTextInput(static_cast<TextInputEventArgs&>(e));
            }
            else if (r == &GotFocusEvent)
            {
                OnGotFocus(static_cast<GotFocusEventArgs&>(e));
            }
            else if (r == &LostFocusEvent)
            {
                OnLostFocus(static_cast<FocusChangedEventArgs&>(e));
            }
        }

        void InputElement::RaiseInstanceEvents(Interactivity::RoutedEventArgs& e)
        {
            const RoutedEvent* r = e.RoutedEvent;
            if (r == &PointerEnteredEvent)
            {
                PointerEntered(*this, static_cast<PointerEventArgs&>(e));
            }
            else if (r == &PointerExitedEvent)
            {
                PointerExited(*this, static_cast<PointerEventArgs&>(e));
            }
            else if (r == &PointerPressedEvent)
            {
                PointerPressed(*this, static_cast<PointerPressedEventArgs&>(e));
            }
            else if (r == &PointerMovedEvent)
            {
                PointerMoved(*this, static_cast<PointerEventArgs&>(e));
            }
            else if (r == &PointerReleasedEvent)
            {
                PointerReleased(*this, static_cast<PointerReleasedEventArgs&>(e));
            }
            else if (r == &PointerCaptureLostEvent)
            {
                PointerCaptureLost(*this, static_cast<PointerCaptureLostEventArgs&>(e));
            }
            else if (r == &PointerWheelChangedEvent)
            {
                PointerWheelChanged(*this, static_cast<PointerWheelEventArgs&>(e));
            }
            else if (r == &KeyDownEvent)
            {
                KeyDown(*this, static_cast<KeyEventArgs&>(e));
            }
            else if (r == &KeyUpEvent)
            {
                KeyUp(*this, static_cast<KeyEventArgs&>(e));
            }
            else if (r == &TextInputEvent)
            {
                TextInput(*this, static_cast<TextInputEventArgs&>(e));
            }
            else if (r == &GotFocusEvent)
            {
                GotFocus(*this, static_cast<GotFocusEventArgs&>(e));
            }
            else if (r == &LostFocusEvent)
            {
                LostFocus(*this, static_cast<FocusChangedEventArgs&>(e));
            }
        }

        void InputElement::OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change)
        {
            Interactive::OnPropertyChanged(change);
            if ((&change.Property == &IsEnabledProperty || &change.Property == &IsVisibleProperty) && _root != nullptr)
            {
                _root->ElementStateChanged(*this);
            }
        }

        void InputElement::OnDetachedFromVisualTree()
        {
            if (_root != nullptr)
            {
                _root->ElementDetached(*this);
            }
            _isPointerOver = false;
            _isFocused = false;
            _isKeyboardFocusWithin = false;
            Interactive::OnDetachedFromVisualTree();
        }

        Point PointerEventArgs::GetPosition(const Visual* relativeTo) const
        {
            if (relativeTo == nullptr || _root == nullptr)
            {
                return _rootPosition;
            }
            const std::optional<Point> p = _root->TranslatePoint(_rootPosition, relativeTo);
            return p.value_or(_rootPosition);
        }

        PointerPoint PointerEventArgs::GetCurrentPoint(const Visual* relativeTo) const
        {
            PointerPoint point;
            point.Pointer = Pointer;
            point.Position = GetPosition(relativeTo);
            point.Properties.IsLeftButtonPressed = (_modifiers & RawInputModifiers::LeftMouseButton) != RawInputModifiers::None;
            point.Properties.IsRightButtonPressed = (_modifiers & RawInputModifiers::RightMouseButton) != RawInputModifiers::None;
            point.Properties.IsMiddleButtonPressed = (_modifiers & RawInputModifiers::MiddleMouseButton) != RawInputModifiers::None;
            point.Properties.PointerUpdateKind = UpdateKind;
            return point;
        }

        void IPointer::Capture(IInputElement* element)
        {
            if (_captured == element)
            {
                return;
            }
            IInputElement* old = _captured;
            _captured = element;
            if (old != nullptr)
            {
                PointerCaptureLostEventArgs lost(&InputElement::PointerCaptureLostEvent, this);
                old->AsInteractive().RaiseEvent(lost);
            }
        }
    }

    // ----------------------------------------------------------- Control

    namespace Controls
    {
        StyledProperty<std::any>& Control::TagProperty = Register<Control, std::any>("Tag", std::any{});

        void Control::OnAttachedToVisualTree()
        {
            InputElement::OnAttachedToVisualTree();
            AttachedToVisualTree(*this);
            OnLoaded();
            Loaded(*this);
        }

        void Control::OnDetachedFromVisualTree()
        {
            InputElement::OnDetachedFromVisualTree();
            DetachedFromVisualTree(*this);
        }

        void Control::ArrangeCore(Rect finalRect)
        {
            const Size before = Bounds().GetSize();
            InputElement::ArrangeCore(finalRect);
            const Size after = Bounds().GetSize();
            if (before != after)
            {
                OnSizeChanged(after);
                SizeChanged(*this, after);
            }
        }

        StyledProperty<Media::FontFamilyPtr>& TextElement::FontFamilyProperty
            = Register<TextElement, Media::FontFamilyPtr>("FontFamily", Media::FontFamily::Default(), true);
        StyledProperty<double>& TextElement::FontSizeProperty = Register<TextElement, double>("FontSize", 14.0, true);
        StyledProperty<Media::FontWeight>& TextElement::FontWeightProperty
            = Register<TextElement, Media::FontWeight>("FontWeight", Media::FontWeight::Normal, true);
        StyledProperty<Media::FontStyle>& TextElement::FontStyleProperty
            = Register<TextElement, Media::FontStyle>("FontStyle", Media::FontStyle::Normal, true);
        StyledProperty<Media::IBrushPtr>& TextElement::ForegroundProperty
            = Register<TextElement, Media::IBrushPtr>("Foreground", Media::Brushes::White(), true);

        StyledProperty<Media::IBrushPtr>& TemplatedControl::BackgroundProperty
            = Register<TemplatedControl, Media::IBrushPtr>("Background", nullptr);
        StyledProperty<Media::IBrushPtr>& TemplatedControl::BorderBrushProperty
            = Register<TemplatedControl, Media::IBrushPtr>("BorderBrush", nullptr);
        StyledProperty<Thickness>& TemplatedControl::BorderThicknessProperty
            = Register<TemplatedControl, Thickness>("BorderThickness", Thickness{});
        StyledProperty<Avalonia::CornerRadius>& TemplatedControl::CornerRadiusProperty
            = Register<TemplatedControl, Avalonia::CornerRadius>("CornerRadius", Avalonia::CornerRadius{});
        StyledProperty<Thickness>& TemplatedControl::PaddingProperty
            = Register<TemplatedControl, Thickness>("Padding", Thickness{});
    }
}
