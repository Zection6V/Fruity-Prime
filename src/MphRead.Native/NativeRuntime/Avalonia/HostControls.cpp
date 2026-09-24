#include "HostControls.hpp"

#include "../Gui/Host.hpp"
#include "../Gui/Text.hpp"
#include "../Stb/Image.hpp"
#include "../System/IO.hpp"
#include "../System/Runtime.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

namespace MphRead::NativeRuntime::Avalonia
{
    namespace
    {
        [[nodiscard]] Launcher::GuiColor ColorOf(Launcher::TrackedTextBrush brush)
        {
            const auto* const value = static_cast<const Launcher::GuiBrush*>(brush.Native);
            return value != nullptr ? value->Color : Launcher::GuiTheme::Text;
        }

        [[nodiscard]] std::u16string UpperInvariant(std::u16string_view text)
        {
            std::u16string result(text);
            for (char16_t& c : result)
            {
                if (c >= u'a' && c <= u'z')
                {
                    c = static_cast<char16_t>(c - u'a' + u'A');
                }
            }
            return result;
        }

        // --- MenuEntry --------------------------------------------------

        class MenuEntryContext final : public Launcher::MenuEntryDrawingContext
        {
        public:
            explicit MenuEntryContext(const Surface& surface) noexcept
                : _surface(surface)
            {
            }

            Launcher::TrackedTextFormattedText CreateFormattedText(
                std::u16string_view text, Launcher::TrackedTextCulture culture,
                Launcher::TrackedTextFlowDirection flowDirection,
                Launcher::TrackedTextFace face, double fontSize,
                Launcher::TrackedTextBrush brush) override
            {
                (void)culture;
                (void)flowDirection;
                return MakeFormattedText(text,
                    face == Launcher::TrackedTextFace::FaceTrue, fontSize,
                    ColorOf(brush));
            }

            void DrawText(const Launcher::TrackedTextFormattedText& text,
                Launcher::TrackedTextPoint point) override
            {
                _surface.DrawFormatted(text, point.X, point.Y);
            }

            void FillRectangle(
                Launcher::MenuEntryBrush brush, Launcher::GuiRect rect) override
            {
                if (brush.Kind == Launcher::MenuEntryBrushKind::Transparent)
                {
                    return;
                }
                _surface.FillRect(rect, brush.Color);
            }

            void DrawRectangle(Launcher::MenuEntryBrush brush,
                std::optional<Launcher::MenuEntryPen> pen,
                Launcher::GuiRoundedRect rect) override
            {
                (void)pen;
                if (brush.Kind == Launcher::MenuEntryBrushKind::Transparent)
                {
                    return;
                }
                _surface.FillRounded(rect, brush.Color);
            }

        private:
            Surface _surface;
        };

        // --- UpdateBadge ------------------------------------------------

        class UpdateBadgeContext final : public Launcher::UpdateBadgeDrawingContext
        {
        public:
            explicit UpdateBadgeContext(const Surface& surface) noexcept
                : _surface(surface)
            {
            }

            Launcher::TrackedTextFormattedText CreateFormattedText(
                std::u16string_view text, Launcher::TrackedTextCulture culture,
                Launcher::TrackedTextFlowDirection flowDirection,
                Launcher::TrackedTextFace face, double fontSize,
                Launcher::TrackedTextBrush brush) override
            {
                (void)culture;
                (void)flowDirection;
                return MakeFormattedText(text,
                    face == Launcher::TrackedTextFace::FaceTrue, fontSize,
                    ColorOf(brush));
            }

            void DrawText(const Launcher::TrackedTextFormattedText& text,
                Launcher::TrackedTextPoint point) override
            {
                _surface.DrawFormatted(text, point.X, point.Y);
            }

            void DrawRectangle(const Launcher::GuiBrush& brush,
                std::optional<Launcher::UpdateBadgePen> pen,
                Launcher::GuiRoundedRect rect) override
            {
                (void)pen;
                _surface.FillRounded(rect, brush.Color);
            }

        private:
            Surface _surface;
        };

        // --- ProgressRow ------------------------------------------------

        [[nodiscard]] Launcher::GuiColor ColorOf(Launcher::ProgressRowBrush brush) noexcept
        {
            switch (brush)
            {
            case Launcher::ProgressRowBrush::TextDimBrush:
                return Launcher::GuiTheme::TextDim;
            case Launcher::ProgressRowBrush::TextBrush:
                return Launcher::GuiTheme::Text;
            case Launcher::ProgressRowBrush::Ink:
                return Launcher::GuiTheme::Ink;
            case Launcher::ProgressRowBrush::AccentBrush:
                return Launcher::GuiTheme::Accent;
            }
            return Launcher::GuiTheme::Text;
        }

        class ProgressRowContext final : public Launcher::ProgressRowDrawingContext
        {
        public:
            explicit ProgressRowContext(const Surface& surface) noexcept
                : _surface(surface)
            {
            }

            Launcher::ProgressRowFormattedText CreateFormattedText(
                std::string_view text, Launcher::ProgressRowCulture culture,
                Launcher::ProgressRowFlowDirection flowDirection,
                Launcher::ProgressRowFace face, double fontSize,
                Launcher::ProgressRowBrush brush) override
            {
                (void)culture;
                (void)flowDirection;
                const Launcher::TrackedTextFormattedText made = MakeFormattedText(
                    Utf16(text), face == Launcher::ProgressRowFace::FaceTrue, fontSize,
                    ColorOf(brush));
                return Launcher::ProgressRowFormattedText{
                    made.Native, made.Width, made.Height};
            }

            void DrawText(const Launcher::ProgressRowFormattedText& text,
                Launcher::ProgressRowPoint point) override
            {
                _surface.DrawFormatted(
                    Launcher::TrackedTextFormattedText{
                        text.Native, text.Width, text.Height},
                    point.X, point.Y);
            }

            void DrawRectangle(Launcher::ProgressRowBrush brush,
                Launcher::ProgressRowPen pen,
                Launcher::ProgressRowRoundedRect rect) override
            {
                (void)pen;
                const Launcher::GuiRect body{
                    rect.Rect.X, rect.Rect.Y, rect.Rect.Width, rect.Rect.Height};
                _surface.FillRounded(
                    Launcher::GuiTheme::Round(body, rect.Radius), ColorOf(brush));
            }

        private:
            Surface _surface;
        };

        // --- SplashView -------------------------------------------------

        [[nodiscard]] std::shared_ptr<Launcher::SplashBitmap> DecodeBitmap(
            const std::vector<std::uint8_t>& bytes)
        {
            const std::shared_ptr<HostImage> image = DecodeImage(bytes);
            if (image == nullptr)
            {
                return nullptr;
            }
            auto bitmap = std::make_shared<Launcher::SplashBitmap>();
            bitmap->Width = image->Width();
            bitmap->Height = image->Height();
            bitmap->Native = std::move(image);
            return bitmap;
        }

        class SplashContext final : public Launcher::SplashViewDrawingContext
        {
        public:
            explicit SplashContext(const Surface& surface) noexcept
                : _surface(surface)
            {
            }

            Launcher::TrackedTextFormattedText CreateFormattedText(
                std::u16string_view text, Launcher::TrackedTextCulture culture,
                Launcher::TrackedTextFlowDirection flowDirection,
                Launcher::TrackedTextFace face, double fontSize,
                Launcher::TrackedTextBrush brush) override
            {
                (void)culture;
                (void)flowDirection;
                return MakeFormattedText(text,
                    face == Launcher::TrackedTextFace::FaceTrue, fontSize,
                    ColorOf(brush));
            }

            void DrawText(const Launcher::TrackedTextFormattedText& text,
                Launcher::TrackedTextPoint point) override
            {
                _surface.DrawFormatted(text, point.X, point.Y);
            }

            void FillRectangle(
                const Launcher::GuiBrush& brush, Launcher::GuiRect rect) override
            {
                Launcher::GuiColor color = brush.Color;
                color.A = static_cast<std::uint8_t>(
                    std::clamp(color.A * brush.Opacity, 0.0, 255.0));
                _surface.FillRect(rect, color);
            }

            void FillRectangle(const Launcher::SplashLinearGradientBrush& brush,
                Launcher::GuiRect rect) override
            {
                // No gradient primitive: the two stops are laid down as bands
                // along the brush's own axis, which is vertical everywhere the
                // launcher uses one.
                const Launcher::GuiColor from = brush.GradientStops[0].Color;
                const Launcher::GuiColor to = brush.GradientStops[1].Color;
                const bool horizontal
                    = std::abs(brush.EndPoint.Point.X - brush.StartPoint.Point.X)
                    > std::abs(brush.EndPoint.Point.Y - brush.StartPoint.Point.Y);
                const double span = horizontal ? rect.Width : rect.Height;
                const std::int32_t bands = std::clamp(
                    static_cast<std::int32_t>(span / 2.0), 1, 128);
                for (std::int32_t i = 0; i < bands; ++i)
                {
                    const double t = (static_cast<double>(i) + 0.5) / bands;
                    Launcher::GuiColor color;
                    color.A = static_cast<std::uint8_t>(std::clamp(
                        (from.A + (to.A - from.A) * t) * brush.Opacity, 0.0, 255.0));
                    color.R = static_cast<std::uint8_t>(from.R + (to.R - from.R) * t);
                    color.G = static_cast<std::uint8_t>(from.G + (to.G - from.G) * t);
                    color.B = static_cast<std::uint8_t>(from.B + (to.B - from.B) * t);
                    const double start = span * i / bands;
                    const double size = span / bands;
                    _surface.FillRect(horizontal
                            ? Launcher::GuiRect{rect.X + start, rect.Y, size, rect.Height}
                            : Launcher::GuiRect{rect.X, rect.Y + start, rect.Width, size},
                        color);
                }
            }

            void DrawImage(const Launcher::SplashBitmap& image,
                Launcher::GuiRect source, Launcher::GuiRect destination) override
            {
                auto* const native = static_cast<HostImage*>(image.Native.get());
                if (native == nullptr || image.Width <= 0.0 || image.Height <= 0.0)
                {
                    return;
                }
                const Toolkit::TextureHandle texture = native->Texture();
                if (texture == 0)
                {
                    return;
                }
                _surface.Renderer().DrawImage(texture, _surface.Map(destination),
                    Toolkit::Rect{source.X / image.Width, source.Y / image.Height,
                        source.Width / image.Width, source.Height / image.Height},
                    1.0);
            }

        private:
            Surface _surface;
        };
    }

    // --- HostControl ------------------------------------------------------

    Toolkit::ElementPtr HostControl::Bind(const std::shared_ptr<HostControl>& host)
    {
        Toolkit::ElementPtr element
            = Toolkit::Element::Create(Toolkit::ElementKind::Custom);
        host->_visual = element.get();
        element->Behaviour = host;
        element->Tag = host;
        return element;
    }

    Launcher::GuiRect HostControl::ControlBounds() const noexcept
    {
        if (_visual == nullptr)
        {
            return Launcher::GuiRect{0.0, 0.0, 0.0, 0.0};
        }
        const Toolkit::Rect bounds = _visual->Bounds();
        return Launcher::GuiRect{0.0, 0.0, bounds.Width, bounds.Height};
    }

    void HostControl::SetFocusHere()
    {
        if (Toolkit::Window* const window = Toolkit::Window::Of(*_visual))
        {
            window->Focus(_visual);
        }
    }

    void HostControl::CaptureHere()
    {
        if (Toolkit::Window* const window = Toolkit::Window::Of(*_visual))
        {
            window->Capture(_visual);
        }
    }

    void HostControl::ReleaseCapture()
    {
        if (Toolkit::Window* const window = Toolkit::Window::Of(*_visual))
        {
            window->Capture(nullptr);
        }
    }

    // --- MenuEntryHost ----------------------------------------------------

    void MenuEntryHost::Wire(const Toolkit::ElementPtr& element, MenuEntryHost* host)
    {
        element->PointerEntered = [host](Toolkit::PointerEvent& e)
        {
            Launcher::MenuEntryPointerEventArgs args{&e, host};
            host->_entry->OnPointerEntered(args);
        };
        element->PointerExited = [host](Toolkit::PointerEvent& e)
        {
            Launcher::MenuEntryPointerEventArgs args{&e, host};
            host->_entry->OnPointerExited(args);
        };
        element->PointerPressed = [host](Toolkit::PointerEvent& e)
        {
            Launcher::MenuEntryPointerEventArgs args{&e, host};
            host->_entry->OnPointerPressed(args);
            e.Handled = true;
        };
        element->PointerReleased = [host](Toolkit::PointerEvent& e)
        {
            Launcher::MenuEntryPointerEventArgs args{&e, host};
            host->_entry->OnPointerReleased(args);
            e.Handled = true;
        };
        element->PointerCaptureLost = [host]()
        {
            Launcher::MenuEntryPointerCaptureLostEventArgs args{host};
            host->_entry->OnPointerCaptureLost(args);
        };
        element->GotFocus = [host]()
        {
            Launcher::MenuEntryGotFocusEventArgs args{host};
            host->_entry->OnGotFocus(args);
        };
        element->LostFocus = [host]()
        {
            Launcher::MenuEntryRoutedEventArgs args{host};
            host->_entry->OnLostFocus(args);
        };
        element->KeyDown = [host](Toolkit::KeyEvent& e)
        {
            Launcher::MenuEntryKeyEventArgs args;
            args.Native = &e;
            args.Key = e.Which == Toolkit::Key::Enter ? Launcher::MenuEntryKey::Enter
                : e.Which == Toolkit::Key::Space      ? Launcher::MenuEntryKey::Space
                                                      : Launcher::MenuEntryKey::Other;
            host->_entry->OnKeyDown(args);
            e.Handled = args.Handled;
        };
    }

    Toolkit::ElementPtr MenuEntryHost::Create(std::optional<std::u16string> title,
        std::optional<std::u16string> subtitle, double titleSize)
    {
        auto host = std::make_shared<MenuEntryHost>();
        Toolkit::ElementPtr element = Bind(host);
        host->_owned = std::make_unique<Launcher::MenuEntry>(
            *host, std::move(title), std::move(subtitle), titleSize);
        host->_entry = host->_owned.get();
        Wire(element, host.get());
        return element;
    }

    Toolkit::ElementPtr MenuEntryHost::CreateBare()
    {
        auto host = std::make_shared<MenuEntryHost>();
        return Bind(host);
    }

    void MenuEntryHost::Attach(Launcher::MenuEntry& entry)
    {
        _entry = &entry;
        Wire(_visual->shared_from_this(), this);
    }

    void MenuEntryHost::Click(std::function<void()> action)
    {
        _click = std::move(action);
        _entry->AddClick(Launcher::MenuEntryEventHandler(this,
            [](void* context, void*, const Launcher::MenuEntryEventArgs&)
            {
                MenuEntryHost* const self = static_cast<MenuEntryHost*>(context);
                if (self->_click)
                {
                    self->_click();
                }
            }));
    }

    Toolkit::Size MenuEntryHost::Measure(
        Toolkit::Element& element, Toolkit::Size available)
    {
        (void)element;
        const Launcher::MenuEntrySize size
            = _entry->MeasureOverride(Launcher::MenuEntrySize{
                available.Width, available.Height});
        return Toolkit::Size{size.Width, size.Height};
    }

    void MenuEntryHost::Render(Toolkit::Element& element, Toolkit::Renderer& renderer)
    {
        MenuEntryContext context(Surface(renderer, element.Bounds()));
        _entry->Render(context);
    }

    Launcher::MenuEntrySize MenuEntryHost::BaseMeasureOverride(
        Launcher::MenuEntrySize availableSize)
    {
        // Avalonia's Control.MeasureOverride for a control with no children.
        (void)availableSize;
        return Launcher::MenuEntrySize{0.0, 0.0};
    }

    void MenuEntryHost::BaseOnPropertyChanged(Launcher::MenuEntryProperty property)
    {
        (void)property;
    }

    void MenuEntryHost::SetHeight(double height)
    {
        _visual->Height = height;
    }

    double MenuEntryHost::GetHeight() const
    {
        return _visual->Height.value_or(0.0);
    }

    void MenuEntryHost::SetFocusable(bool focusable)
    {
        _visual->Focusable = focusable;
    }

    void MenuEntryHost::SetHandCursor()
    {
        _visual->CursorKind = Toolkit::Cursor::Hand;
    }

    void MenuEntryHost::Focus()
    {
        SetFocusHere();
    }

    bool MenuEntryHost::GetIsEnabled() const
    {
        return _visual->IsEnabled;
    }

    void MenuEntryHost::SetIsEnabled(bool isEnabled)
    {
        _visual->IsEnabled = isEnabled;
    }

    bool MenuEntryHost::IsPointerOver() const
    {
        return _visual->IsPointerOver;
    }

    bool MenuEntryHost::IsFocused() const
    {
        return _visual->IsFocused;
    }

    Launcher::GuiRect MenuEntryHost::Bounds() const
    {
        return ControlBounds();
    }

    std::u16string MenuEntryHost::ToUpperInvariant(std::u16string_view text)
    {
        return UpperInvariant(text);
    }

    Launcher::TrackedTextFormattedText MenuEntryHost::CreateFormattedText(
        std::u16string_view text, Launcher::TrackedTextCulture culture,
        Launcher::TrackedTextFlowDirection flowDirection,
        Launcher::TrackedTextFace face, double fontSize,
        Launcher::TrackedTextBrush brush)
    {
        (void)culture;
        (void)flowDirection;
        return MakeFormattedText(text, face == Launcher::TrackedTextFace::FaceTrue,
            fontSize, ColorOf(brush));
    }

    void* MenuEntryHost::Captured(void* pointer) const
    {
        (void)pointer;
        return _captured;
    }

    void MenuEntryHost::Capture(void* pointer, void* control)
    {
        (void)pointer;
        _captured = control;
        if (control != nullptr)
        {
            CaptureHere();
            return;
        }
        ReleaseCapture();
    }

    Launcher::MenuEntryPoint MenuEntryHost::GetPosition(
        const Launcher::MenuEntryPointerEventArgs& e) const
    {
        const auto* const event = static_cast<const Toolkit::PointerEvent*>(e.Native);
        if (event == nullptr)
        {
            return Launcher::MenuEntryPoint{0.0, 0.0};
        }
        const Toolkit::Rect bounds = _visual->Bounds();
        return Launcher::MenuEntryPoint{event->X - bounds.X, event->Y - bounds.Y};
    }

    void MenuEntryHost::InvalidateVisual()
    {
        // Every frame is drawn from the tree, so there is nothing to mark.
    }

    void MenuEntryHost::BaseOnPointerEntered(Launcher::MenuEntryPointerEventArgs& e)
    {
        (void)e;
    }

    void MenuEntryHost::BaseOnPointerExited(Launcher::MenuEntryPointerEventArgs& e)
    {
        (void)e;
    }

    void MenuEntryHost::BaseOnPointerPressed(Launcher::MenuEntryPointerEventArgs& e)
    {
        (void)e;
    }

    void MenuEntryHost::BaseOnPointerReleased(Launcher::MenuEntryPointerEventArgs& e)
    {
        (void)e;
    }

    void MenuEntryHost::BaseOnPointerCaptureLost(
        Launcher::MenuEntryPointerCaptureLostEventArgs& e)
    {
        (void)e;
    }

    void MenuEntryHost::BaseOnGotFocus(Launcher::MenuEntryGotFocusEventArgs& e)
    {
        (void)e;
    }

    void MenuEntryHost::BaseOnLostFocus(Launcher::MenuEntryRoutedEventArgs& e)
    {
        (void)e;
    }

    void MenuEntryHost::BaseOnKeyDown(Launcher::MenuEntryKeyEventArgs& e)
    {
        (void)e;
    }

    // --- UpdateBadgeHost --------------------------------------------------

    Toolkit::ElementPtr UpdateBadgeHost::Create()
    {
        auto host = std::make_shared<UpdateBadgeHost>();
        Toolkit::ElementPtr element = Bind(host);
        host->_badge = std::make_unique<Launcher::UpdateBadge>(*host);

        UpdateBadgeHost* const raw = host.get();
        element->PointerEntered = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::UpdateBadgePointerEventArgs args{&e, false};
            raw->_badge->OnPointerEntered(args);
        };
        element->PointerExited = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::UpdateBadgePointerEventArgs args{&e, false};
            raw->_badge->OnPointerExited(args);
        };
        element->PointerPressed = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::UpdateBadgePointerPressedEventArgs args{&e, false};
            raw->_badge->OnPointerPressed(args);
            e.Handled = true;
        };
        element->PointerReleased = [raw](Toolkit::PointerEvent& e)
        {
            Launcher::UpdateBadgePointerReleasedEventArgs args{&e, false};
            raw->_badge->OnPointerReleased(args);
            e.Handled = true;
        };
        element->GotFocus = [raw]()
        {
            Launcher::UpdateBadgeGotFocusEventArgs args{nullptr, false};
            raw->_badge->OnGotFocus(args);
        };
        element->LostFocus = [raw]()
        {
            Launcher::UpdateBadgeRoutedEventArgs args{nullptr, false};
            raw->_badge->OnLostFocus(args);
        };
        element->KeyDown = [raw](Toolkit::KeyEvent& e)
        {
            Launcher::UpdateBadgeKeyEventArgs args;
            args.Native = &e;
            args.Key = e.Which == Toolkit::Key::Enter ? Launcher::UpdateBadgeKey::Enter
                : e.Which == Toolkit::Key::Space      ? Launcher::UpdateBadgeKey::Space
                                                      : Launcher::UpdateBadgeKey::Other;
            raw->_badge->OnKeyDown(args);
            e.Handled = args.Handled;
        };
        return element;
    }

    void UpdateBadgeHost::Click(std::function<void()> action)
    {
        _click = std::move(action);
        // A handle that does not own: the badge is owned by this host, so a
        // strong target here would be a cycle.
        _badge->AddClick(Launcher::UpdateBadgeEventHandler(
            std::shared_ptr<void>(static_cast<void*>(this), [](void*) {}),
            [](void* context, void*, const Launcher::UpdateBadgeEventArgs&)
            {
                UpdateBadgeHost* const self = static_cast<UpdateBadgeHost*>(context);
                if (self->_click)
                {
                    self->_click();
                }
            }));
    }

    Toolkit::Size UpdateBadgeHost::Measure(
        Toolkit::Element& element, Toolkit::Size available)
    {
        (void)element;
        const Launcher::UpdateBadgeSize size = _badge->MeasureOverride(
            Launcher::UpdateBadgeSize{available.Width, available.Height});
        _desired = Toolkit::Size{size.Width, size.Height};
        return _desired;
    }

    void UpdateBadgeHost::Render(Toolkit::Element& element, Toolkit::Renderer& renderer)
    {
        UpdateBadgeContext context(Surface(renderer, element.Bounds()));
        _badge->Render(context);
    }

    void UpdateBadgeHost::SetFocusable(bool focusable)
    {
        _visual->Focusable = focusable;
    }

    void UpdateBadgeHost::SetHandCursor()
    {
        _visual->CursorKind = Toolkit::Cursor::Hand;
    }

    void UpdateBadgeHost::Focus()
    {
        SetFocusHere();
    }

    bool UpdateBadgeHost::GetIsVisible() const
    {
        return _visual->Visible;
    }

    void UpdateBadgeHost::SetIsVisible(bool isVisible)
    {
        _visual->Visible = isVisible;
    }

    bool UpdateBadgeHost::IsPointerOver() const
    {
        return _visual->IsPointerOver;
    }

    bool UpdateBadgeHost::IsFocused() const
    {
        return _visual->IsFocused;
    }

    Launcher::GuiRect UpdateBadgeHost::Bounds() const
    {
        return ControlBounds();
    }

    Launcher::UpdateBadgeSize UpdateBadgeHost::DesiredSize() const
    {
        return Launcher::UpdateBadgeSize{_desired.Width, _desired.Height};
    }

    std::u16string UpdateBadgeHost::ToUpperInvariant(std::u16string_view text)
    {
        return UpperInvariant(text);
    }

    Launcher::TrackedTextFormattedText UpdateBadgeHost::CreateFormattedText(
        std::u16string_view text, Launcher::TrackedTextCulture culture,
        Launcher::TrackedTextFlowDirection flowDirection,
        Launcher::TrackedTextFace face, double fontSize,
        Launcher::TrackedTextBrush brush)
    {
        (void)culture;
        (void)flowDirection;
        return MakeFormattedText(text, face == Launcher::TrackedTextFace::FaceTrue,
            fontSize, ColorOf(brush));
    }

    void UpdateBadgeHost::InvalidateMeasure()
    {
    }

    void UpdateBadgeHost::InvalidateVisual()
    {
    }

    void UpdateBadgeHost::BaseOnPointerEntered(Launcher::UpdateBadgePointerEventArgs& e)
    {
        (void)e;
    }

    void UpdateBadgeHost::BaseOnPointerExited(Launcher::UpdateBadgePointerEventArgs& e)
    {
        (void)e;
    }

    void UpdateBadgeHost::BaseOnPointerPressed(
        Launcher::UpdateBadgePointerPressedEventArgs& e)
    {
        (void)e;
    }

    void UpdateBadgeHost::BaseOnPointerReleased(
        Launcher::UpdateBadgePointerReleasedEventArgs& e)
    {
        (void)e;
    }

    void UpdateBadgeHost::BaseOnGotFocus(Launcher::UpdateBadgeGotFocusEventArgs& e)
    {
        (void)e;
    }

    void UpdateBadgeHost::BaseOnLostFocus(Launcher::UpdateBadgeRoutedEventArgs& e)
    {
        (void)e;
    }

    void UpdateBadgeHost::BaseOnKeyDown(Launcher::UpdateBadgeKeyEventArgs& e)
    {
        (void)e;
    }

    // --- ProgressRowHost --------------------------------------------------

    Toolkit::ElementPtr ProgressRowHost::Create()
    {
        auto host = std::make_shared<ProgressRowHost>();
        Toolkit::ElementPtr element = Bind(host);
        host->_row = std::make_unique<Launcher::ProgressRow>(*host);
        return element;
    }

    Toolkit::Size ProgressRowHost::Measure(
        Toolkit::Element& element, Toolkit::Size available)
    {
        (void)element;
        return Toolkit::Size{available.Width, 0.0};
    }

    void ProgressRowHost::Render(Toolkit::Element& element, Toolkit::Renderer& renderer)
    {
        const Toolkit::Rect bounds = element.Bounds();
        ProgressRowContext context(Surface(renderer, bounds));
        _row->Render(context,
            Launcher::ProgressRowBounds{bounds.Width, bounds.Height});
    }

    void ProgressRowHost::SetHeight(double height)
    {
        _visual->Height = height;
    }

    void ProgressRowHost::SetIsVisible(bool isVisible)
    {
        _visual->Visible = isVisible;
    }

    void ProgressRowHost::InvalidateVisual()
    {
    }

    // --- SplashViewHost ---------------------------------------------------

    Toolkit::ElementPtr SplashViewHost::Create()
    {
        auto host = std::make_shared<SplashViewHost>();
        Toolkit::ElementPtr element = Bind(host);
        host->_splash = std::make_unique<Launcher::SplashView>(*host);
        return element;
    }

    Toolkit::Size SplashViewHost::Measure(
        Toolkit::Element& element, Toolkit::Size available)
    {
        (void)element;
        // The splash fills whatever it is given, as a background does.
        return Toolkit::Size{available.Width, available.Height};
    }

    void SplashViewHost::Render(Toolkit::Element& element, Toolkit::Renderer& renderer)
    {
        SplashContext context(Surface(renderer, element.Bounds()));
        _splash->Render(context);
    }

    Launcher::GuiRect SplashViewHost::Bounds() const
    {
        return ControlBounds();
    }

    void SplashViewHost::InvalidateVisual()
    {
    }

    std::string SplashViewHost::AppContextBaseDirectory() const
    {
        return ::MphRead::NativeRuntime::AppContextBaseDirectory();
    }

    std::string SplashViewHost::PathCombine(
        std::string_view left, std::string_view right) const
    {
        const auto native = [](std::string_view text)
        {
            return std::filesystem::path(std::u8string(text.begin(), text.end()));
        };
        const std::u8string combined = (native(left) / native(right)).u8string();
        return std::string(combined.begin(), combined.end());
    }

    std::shared_ptr<Launcher::SplashBitmap> SplashViewHost::CreateBitmapFromMemory(
        std::span<const std::uint8_t> bytes)
    {
        return DecodeBitmap(std::vector<std::uint8_t>(bytes.begin(), bytes.end()));
    }

    std::shared_ptr<Launcher::SplashBitmap> SplashViewHost::CreateBitmapFromAsset(
        std::string_view uri)
    {
        // An avares: URI names a file the published build carries beside the
        // executable, which is where the native build keeps it.
        std::string path(uri);
        const std::size_t slash = path.rfind('/');
        if (slash != std::string::npos)
        {
            path = path.substr(slash + 1);
        }
        const std::string full = PathCombine(AppContextBaseDirectory(), path);
        if (!::MphRead::NativeRuntime::FileExists(full))
        {
            return nullptr;
        }
        const std::vector<std::uint8_t> bytes = ::MphRead::NativeRuntime::FileReadAllBytes(full);
        return DecodeBitmap(bytes);
    }

    void SplashViewHost::DisposeBitmap(Launcher::SplashBitmap& bitmap)
    {
        bitmap.Native.reset();
    }

    std::u16string SplashViewHost::ToUpperInvariant(std::u16string_view text) const
    {
        return UpperInvariant(text);
    }
}
