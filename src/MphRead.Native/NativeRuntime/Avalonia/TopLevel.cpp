#include "TopLevel.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::NativeRuntime::Avalonia
{
    namespace
    {
        std::int32_t NextPointerId()
        {
            static std::int32_t next = 0;
            return ++next;
        }

        [[nodiscard]] bool IsAncestorOrSelf(const Visual* ancestor, const Visual* v)
        {
            for (; v != nullptr; v = v->GetVisualParent())
            {
                if (v == ancestor)
                {
                    return true;
                }
            }
            return false;
        }

        // Children in drawing order: by ZIndex, then as added.
        [[nodiscard]] std::vector<Visual*> Ordered(const Visual& visual)
        {
            std::vector<Visual*> children;
            for (const std::shared_ptr<Visual>& child : visual.VisualChildren())
            {
                children.push_back(child.get());
            }
            std::stable_sort(children.begin(), children.end(),
                [](const Visual* a, const Visual* b) { return a->ZIndex() < b->ZIndex(); });
            return children;
        }
    }

    Input::IInputElement* Input::FocusManager::GetFocusedElement() const
    {
        return _owner.FocusedElement();
    }

    void Input::FocusManager::ClearFocus()
    {
        _owner.SetFocusedElement(nullptr, NavigationMethod::Unspecified, KeyModifiers::None);
    }

    TopLevel::TopLevel()
        : _mouse(std::make_unique<Input::IPointer>(NextPointerId(), Input::PointerType::Mouse, true))
    {
        _root = this;
        OnAttachedToVisualTree();
    }

    TopLevel::~TopLevel()
    {
        // Children go first, while the root they point at still exists.
        Content(Controls::ControlPtr{});
    }

    TopLevel* TopLevel::GetTopLevel(const Visual* visual)
    {
        return visual == nullptr ? nullptr : visual->GetVisualRoot();
    }

    void TopLevel::SetClientSize(Size size)
    {
        if (_clientSize == size)
        {
            return;
        }
        _clientSize = size;
        InvalidateMeasure();
        _layoutDirty = true;
        _renderDirty = true;
    }

    void TopLevel::Prepare()
    {
        ExecuteLayoutPass();
    }

    void TopLevel::StartRendering()
    {
        _rendering = true;
        _renderDirty = true;
    }

    void TopLevel::ExecuteLayoutPass()
    {
        // Measure and arrange until nothing asks again, as the layout manager
        // does, with a limit against a control that never settles.
        for (int pass = 0; pass < 10; pass++)
        {
            _layoutDirty = false;
            Measure(_clientSize);
            Arrange(Rect(_clientSize));
            if (!_layoutDirty && IsMeasureValid() && IsArrangeValid())
            {
                break;
            }
        }
        _layoutDirty = false;
        LayoutUpdated(*this);
    }

    void TopLevel::RenderVisual(Visual& visual, Media::DrawingContext& context, bool isRoot)
    {
        if (!visual.IsVisible() || visual.Opacity() <= 0)
        {
            return;
        }
        std::optional<Media::DrawingContext::PushedState> transform;
        if (!isRoot)
        {
            transform.emplace(context.PushTransform(visual.LocalTransform()));
        }
        std::optional<Media::DrawingContext::PushedState> opacity;
        if (visual.Opacity() < 1)
        {
            opacity.emplace(context.PushOpacity(visual.Opacity()));
        }
        std::optional<Media::DrawingContext::PushedState> clip;
        if (visual.ClipToBounds())
        {
            const Avalonia::CornerRadius radius = visual.ClipCornerRadius();
            const Rect bounds(visual.Bounds().GetSize());
            clip.emplace(radius.IsZero() ? context.PushClip(bounds) : context.PushClip(bounds, radius));
        }
        const std::size_t before = context.DrawCount;
        visual.Render(context);
        visual.RenderedContent = context.DrawCount != before;
        for (Visual* child : Ordered(visual))
        {
            RenderVisual(*child, context, false);
        }
        visual.RenderOverlay(context);
    }

    bool TopLevel::Render()
    {
        if (_layoutDirty || !IsMeasureValid() || !IsArrangeValid())
        {
            ExecuteLayoutPass();
        }
        if (!_rendering || !_renderDirty)
        {
            return false;
        }
        _renderDirty = false;
        const auto width = static_cast<std::int32_t>(std::max(1.0, std::round(_clientSize.Width)));
        const auto height = static_cast<std::int32_t>(std::max(1.0, std::round(_clientSize.Height)));
        if (_pixels.Width() != width || _pixels.Height() != height)
        {
            _pixels.Resize(width, height);
        }
        else
        {
            _pixels.Clear();
        }
        Skia::Canvas canvas(_pixels);
        Media::DrawingContext context(canvas);
        RenderVisual(*this, context, true);
        _drawn++;
        if (Painted)
        {
            Painted();
        }
        return true;
    }

    // ----------------------------------------------------------- hit test

    Input::InputElement* TopLevel::InputHitTest(Point point) const
    {
        // Depth first, front to back: the deepest visual that drew
        // something under the point, then the nearest input element above it.
        std::function<const Visual*(const Visual&, Point)> hit = [&](const Visual& v, Point local) -> const Visual*
        {
            if (!v.IsVisible())
            {
                return nullptr;
            }
            if (const auto* element = dynamic_cast<const Input::InputElement*>(&v))
            {
                if (!element->IsHitTestVisible() || !element->IsEnabled())
                {
                    return nullptr;
                }
            }
            const Rect bounds(v.Bounds().GetSize());
            if (v.ClipToBounds() && !bounds.Contains(local))
            {
                return nullptr;
            }
            const std::vector<Visual*> children = Ordered(v);
            for (auto it = children.rbegin(); it != children.rend(); ++it)
            {
                const std::optional<Matrix> inverse = (*it)->LocalTransform().TryInvert();
                if (!inverse.has_value())
                {
                    continue;
                }
                if (const Visual* found = hit(**it, inverse->Transform(local)))
                {
                    return found;
                }
            }
            if (v.RenderedContent && bounds.Contains(local))
            {
                return &v;
            }
            return nullptr;
        };
        const Visual* found = hit(*this, point);
        for (const Visual* v = found; v != nullptr; v = v->GetVisualParent())
        {
            if (auto* element = dynamic_cast<const Input::InputElement*>(v))
            {
                return const_cast<Input::InputElement*>(element);
            }
        }
        return nullptr;
    }

    Input::InputElement* TopLevel::PointerTarget(Input::IPointer& pointer, Point point) const
    {
        if (Input::IInputElement* captured = pointer.Captured())
        {
            return dynamic_cast<Input::InputElement*>(&captured->AsInteractive());
        }
        return InputHitTest(point);
    }

    std::uint64_t TopLevel::Timestamp() const
    {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _start).count());
    }

    void TopLevel::UpdatePointerOver(Input::IPointer& pointer, Point point, Input::RawInputModifiers modifiers)
    {
        // Under capture only the captured element's own subtree can be over.
        Input::InputElement* hitElement = InputHitTest(point);
        if (Input::IInputElement* captured = pointer.Captured())
        {
            const Visual* capturedVisual = &captured->AsInteractive();
            if (hitElement != nullptr && !IsAncestorOrSelf(capturedVisual, hitElement))
            {
                hitElement = nullptr;
            }
        }
        std::vector<Input::InputElement*> chain;
        for (Visual* v = hitElement; v != nullptr; v = v->GetVisualParent())
        {
            if (auto* e = dynamic_cast<Input::InputElement*>(v))
            {
                chain.push_back(e);
            }
        }
        // Left: in the old chain and not the new, innermost first.
        for (Input::InputElement* old : _pointerOver)
        {
            if (std::find(chain.begin(), chain.end(), old) == chain.end())
            {
                old->_isPointerOver = false;
                Input::PointerEventArgs e(&Input::InputElement::PointerExitedEvent, &pointer, point, this, modifiers,
                    Timestamp());
                old->RaiseEvent(e);
                old->InvalidateVisual();
            }
        }
        // Entered: outermost first.
        for (auto it = chain.rbegin(); it != chain.rend(); ++it)
        {
            if (std::find(_pointerOver.begin(), _pointerOver.end(), *it) == _pointerOver.end())
            {
                (*it)->_isPointerOver = true;
                Input::PointerEventArgs e(&Input::InputElement::PointerEnteredEvent, &pointer, point, this, modifiers,
                    Timestamp());
                (*it)->RaiseEvent(e);
                (*it)->InvalidateVisual();
            }
        }
        _pointerOver = std::move(chain);
        _cursor = Input::StandardCursorType::Arrow;
        for (Input::InputElement* e : _pointerOver)
        {
            if (const std::shared_ptr<Input::Cursor> cursor = e->Cursor())
            {
                _cursor = cursor->Type;
                break;
            }
        }
    }

    void TopLevel::RaisePointer(Input::IPointer& pointer, const Interactivity::RoutedEvent& routedEvent, Point point,
        Input::RawInputModifiers modifiers, Input::PointerUpdateKind kind)
    {
        Input::InputElement* target = PointerTarget(pointer, point);
        if (target == nullptr)
        {
            return;
        }
        if (&routedEvent == &Input::InputElement::PointerPressedEvent)
        {
            Input::PointerPressedEventArgs e(&routedEvent, &pointer, point, this, modifiers, Timestamp());
            e.UpdateKind = kind;
            e.ClickCount = _clickCount;
            target->RaiseEvent(e);
        }
        else if (&routedEvent == &Input::InputElement::PointerReleasedEvent)
        {
            Input::PointerReleasedEventArgs e(&routedEvent, &pointer, point, this, modifiers, Timestamp());
            e.UpdateKind = kind;
            target->RaiseEvent(e);
        }
        else
        {
            Input::PointerEventArgs e(&routedEvent, &pointer, point, this, modifiers, Timestamp());
            e.UpdateKind = kind;
            target->RaiseEvent(e);
        }
    }

    void TopLevel::MouseMove(Point point, Input::RawInputModifiers modifiers)
    {
        UpdatePointerOver(*_mouse, point, modifiers);
        RaisePointer(*_mouse, Input::InputElement::PointerMovedEvent, point, modifiers, Input::PointerUpdateKind::Other);
    }

    void TopLevel::MouseDown(Point point, Input::MouseButton button, Input::RawInputModifiers modifiers)
    {
        UpdatePointerOver(*_mouse, point, modifiers);
        const auto now = std::chrono::steady_clock::now();
        // The platform's double-click time and distance.
        if (now - _lastPress < std::chrono::milliseconds(500) && std::abs(point.X - _lastPressPoint.X) <= 4
            && std::abs(point.Y - _lastPressPoint.Y) <= 4)
        {
            _clickCount++;
        }
        else
        {
            _clickCount = 1;
        }
        _lastPress = now;
        _lastPressPoint = point;
        const Input::PointerUpdateKind kind = button == Input::MouseButton::Right ? Input::PointerUpdateKind::RightButtonPressed
            : button == Input::MouseButton::Middle ? Input::PointerUpdateKind::MiddleButtonPressed
                                                   : Input::PointerUpdateKind::LeftButtonPressed;
        Input::RawInputModifiers held = modifiers;
        held |= button == Input::MouseButton::Right ? Input::RawInputModifiers::RightMouseButton
            : button == Input::MouseButton::Middle ? Input::RawInputModifiers::MiddleMouseButton
                                                   : Input::RawInputModifiers::LeftMouseButton;
        RaisePointer(*_mouse, Input::InputElement::PointerPressedEvent, point, held, kind);
    }

    void TopLevel::MouseUp(Point point, Input::MouseButton button, Input::RawInputModifiers modifiers)
    {
        const Input::PointerUpdateKind kind = button == Input::MouseButton::Right ? Input::PointerUpdateKind::RightButtonReleased
            : button == Input::MouseButton::Middle ? Input::PointerUpdateKind::MiddleButtonReleased
                                                   : Input::PointerUpdateKind::LeftButtonReleased;
        RaisePointer(*_mouse, Input::InputElement::PointerReleasedEvent, point, modifiers, kind);
        // A button that was captured to something lets go of it.
        _mouse->Capture(nullptr);
        UpdatePointerOver(*_mouse, point, modifiers);
    }

    void TopLevel::MouseWheel(Point point, Vector delta, Input::RawInputModifiers modifiers)
    {
        UpdatePointerOver(*_mouse, point, modifiers);
        Input::InputElement* target = PointerTarget(*_mouse, point);
        if (target == nullptr)
        {
            return;
        }
        Input::PointerWheelEventArgs e(&Input::InputElement::PointerWheelChangedEvent, _mouse.get(), point, this,
            modifiers, Timestamp(), delta);
        target->RaiseEvent(e);
    }

    void TopLevel::TouchBegin(Point point, std::int64_t id)
    {
        // Settle the layout first, so the press resolves against what is
        // on screen now.
        if (_layoutDirty)
        {
            ExecuteLayoutPass();
        }
        auto& touch = _touches[id];
        touch = std::make_unique<Input::IPointer>(NextPointerId(), Input::PointerType::Touch, _touches.size() == 1);
        _clickCount = 1;
        UpdatePointerOver(*touch, point, Input::RawInputModifiers::LeftMouseButton);
        RaisePointer(*touch, Input::InputElement::PointerPressedEvent, point, Input::RawInputModifiers::LeftMouseButton,
            Input::PointerUpdateKind::LeftButtonPressed);
    }

    void TopLevel::TouchUpdate(Point point, std::int64_t id)
    {
        const auto found = _touches.find(id);
        if (found == _touches.end())
        {
            return;
        }
        RaisePointer(*found->second, Input::InputElement::PointerMovedEvent, point,
            Input::RawInputModifiers::LeftMouseButton, Input::PointerUpdateKind::Other);
    }

    void TopLevel::TouchEnd(Point point, std::int64_t id)
    {
        const auto found = _touches.find(id);
        if (found == _touches.end())
        {
            return;
        }
        std::unique_ptr<Input::IPointer> touch = std::move(found->second);
        _touches.erase(found);
        RaisePointer(*touch, Input::InputElement::PointerReleasedEvent, point, Input::RawInputModifiers::None,
            Input::PointerUpdateKind::LeftButtonReleased);
        touch->Capture(nullptr);
        // A finger hovers nothing: it leaves everything as it lifts.
        for (Input::InputElement* old : _pointerOver)
        {
            old->_isPointerOver = false;
            Input::PointerEventArgs e(&Input::InputElement::PointerExitedEvent, touch.get(), point, this,
                Input::RawInputModifiers::None, Timestamp());
            old->RaiseEvent(e);
            old->InvalidateVisual();
        }
        _pointerOver.clear();
    }

    void TopLevel::RaiseKey(const Interactivity::RoutedEvent& routedEvent, Input::Key key,
        Input::RawInputModifiers modifiers, std::optional<std::string> keySymbol)
    {
        Input::KeyEventArgs e(&routedEvent);
        e.Key = key;
        e.KeyModifiers = static_cast<Input::KeyModifiers>(static_cast<std::int32_t>(modifiers)
            & static_cast<std::int32_t>(Input::RawInputModifiers::KeyboardMask));
        e.KeySymbol = std::move(keySymbol);
        Interactivity::Interactive* target = _focused != nullptr ? static_cast<Interactivity::Interactive*>(_focused)
                                                                 : static_cast<Interactivity::Interactive*>(this);
        target->RaiseEvent(e);
    }

    void TopLevel::KeyPress(Input::Key key, Input::RawInputModifiers modifiers, std::optional<std::string> keySymbol)
    {
        RaiseKey(Input::InputElement::KeyDownEvent, key, modifiers, std::move(keySymbol));
    }

    void TopLevel::KeyRelease(Input::Key key, Input::RawInputModifiers modifiers, std::optional<std::string> keySymbol)
    {
        RaiseKey(Input::InputElement::KeyUpEvent, key, modifiers, std::move(keySymbol));
    }

    void TopLevel::TextInput(const std::string& text)
    {
        Input::TextInputEventArgs e(&Input::InputElement::TextInputEvent);
        e.Text = text;
        Interactivity::Interactive* target = _focused != nullptr ? static_cast<Interactivity::Interactive*>(_focused)
                                                                 : static_cast<Interactivity::Interactive*>(this);
        target->RaiseEvent(e);
    }

    // -------------------------------------------------------------- focus

    void TopLevel::SetFocusedElement(Input::InputElement* element, Input::NavigationMethod method,
        Input::KeyModifiers modifiers)
    {
        if (_focused == element)
        {
            return;
        }
        Input::InputElement* old = _focused;
        _focused = element;
        const auto updateWithin = [](Input::InputElement* start, bool value)
        {
            for (Visual* v = start; v != nullptr; v = v->GetVisualParent())
            {
                if (auto* e = dynamic_cast<Input::InputElement*>(v))
                {
                    e->_isKeyboardFocusWithin = value;
                }
            }
        };
        if (old != nullptr)
        {
            old->_isFocused = false;
            updateWithin(old, false);
            Input::FocusChangedEventArgs lost(&Input::InputElement::LostFocusEvent);
            lost.NavigationMethod = method;
            lost.KeyModifiers = modifiers;
            old->RaiseEvent(lost);
            old->InvalidateVisual();
        }
        if (element != nullptr)
        {
            element->_isFocused = true;
            updateWithin(element, true);
            Input::GotFocusEventArgs got(&Input::InputElement::GotFocusEvent);
            got.NavigationMethod = method;
            got.KeyModifiers = modifiers;
            element->RaiseEvent(got);
            element->InvalidateVisual();
        }
    }

    void TopLevel::ElementStateChanged(Input::InputElement& element)
    {
        if (_focused != nullptr && IsAncestorOrSelf(&element, _focused)
            && (!_focused->IsEffectivelyEnabled() || !_focused->IsEffectivelyVisible()))
        {
            SetFocusedElement(nullptr, Input::NavigationMethod::Unspecified, Input::KeyModifiers::None);
        }
    }

    void TopLevel::ElementDetached(Input::InputElement& element)
    {
        if (_focused == &element)
        {
            // Quietly: a control leaving the tree hears no LostFocus.
            _focused->_isFocused = false;
            _focused = nullptr;
        }
        std::erase(_pointerOver, &element);
        if (_mouse->Captured() == &element)
        {
            _mouse->Capture(nullptr);
        }
        for (auto& [id, touch] : _touches)
        {
            (void)id;
            if (touch->Captured() == &element)
            {
                touch->Capture(nullptr);
            }
        }
    }

    // ------------------------------------------------- RenderTargetBitmap

    Media::Imaging::RenderTargetBitmap::RenderTargetBitmap(Avalonia::PixelSize size, Vector dpi)
        : Bitmap(std::make_shared<Skia::Bitmap>(size.Width, size.Height))
    {
        (void)dpi;
    }

    void Media::Imaging::RenderTargetBitmap::Render(Visual& visual)
    {
        Skia::Canvas canvas(*_pixels);
        Media::DrawingContext context(canvas);
        TopLevel::RenderVisual(visual, context, true);
    }
}
