#pragma once

// The launcher's screens as Avalonia controls: settings, the map picker, the
// demo picker and the pause menu, each an adapter over NativeRuntime/Gui's
// element tree.
//
// They are here rather than inside one translation unit because the in-match
// pause window is another window over the same three adapters.

#include "HostControls.hpp"
#include "HostInputRows.hpp"
#include "HostRows.hpp"

#include "../Gui/Host.hpp"
#include "../Stb/Image.hpp"

#include "../../Mods/Launcher/Gui/DemoPickerView.hpp"
#include "../../Mods/Launcher/Gui/MapPickerView.hpp"
#include "../../Mods/Launcher/Gui/PauseMenuView.hpp"
#include "../../Mods/Launcher/Gui/SettingsView.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MphRead
{
    class MenuSettings;
}

namespace MphRead::NativeRuntime::Avalonia
{
    [[nodiscard]] inline Toolkit::ElementPtr Ptr(const std::shared_ptr<void>& handle)
    {
        return std::static_pointer_cast<Toolkit::Element>(handle);
    }

    [[nodiscard]] inline Toolkit::Element* El(const std::shared_ptr<void>& handle)
    {
        return static_cast<Toolkit::Element*>(handle.get());
    }

    [[nodiscard]] inline Toolkit::Element* El(void* handle)
    {
        return static_cast<Toolkit::Element*>(handle);
    }

    // Every screen is a control whose one child fills it, and which says
    // so when its size changes.
    class ContentRoot final : public Toolkit::CustomBehaviour
    {
    public:
        std::vector<std::function<void(double, double)>> SizeChanged;

        [[nodiscard]] Toolkit::Size Measure(
            Toolkit::Element& element, Toolkit::Size available) override
        {
            if (available.Width != _width || available.Height != _height)
            {
                _width = available.Width;
                _height = available.Height;
                const std::vector<std::function<void(double, double)>> handlers
                    = SizeChanged;
                for (const std::function<void(double, double)>& handler : handlers)
                {
                    handler(_width, _height);
                }
            }
            for (const Toolkit::ElementPtr& child : element.Children())
            {
                child->Measure(available);
            }
            return available;
        }

        void Arrange(Toolkit::Element& element, Toolkit::Rect bounds) override
        {
            for (const Toolkit::ElementPtr& child : element.Children())
            {
                child->Arrange(bounds);
            }
        }

    private:
        double _width = -1.0;
        double _height = -1.0;
    };

    // The root every screen host builds itself on.
    class ScreenHost
    {
    public:
        [[nodiscard]] Toolkit::ElementPtr Root() const { return _root; }

        // Public because a window host builds its screen's root itself.
        void MakeRoot(const std::shared_ptr<void>& host)
        {
            _root = Toolkit::Element::Create(Toolkit::ElementKind::Custom);
            _behaviour = std::make_shared<ContentRoot>();
            _root->Behaviour = _behaviour;
            _root->Tag = host;
        }

        void Content(const Toolkit::ElementPtr& content)
        {
            _root->ClearChildren();
            if (content != nullptr)
            {
                _root->AddChild(content);
            }
        }

        void WhenResized(std::function<void(double, double)> handler)
        {
            _behaviour->SizeChanged.push_back(std::move(handler));
        }

        void Focus(const Toolkit::ElementPtr& element)
        {
            if (element == nullptr)
            {
                return;
            }
            if (Toolkit::Window* const window = Toolkit::Window::Of(*element))
            {
                window->Focus(element.get());
            }
        }

    protected:
        Toolkit::ElementPtr _root;
        std::shared_ptr<ContentRoot> _behaviour;
        // Elements handed out as raw handles have to be kept alive here.
        std::vector<Toolkit::ElementPtr> _owned;
    };

    // --- the pause menu ---------------------------------------------

    class PauseMenuHost : public ScreenHost,
                                public Launcher::PauseMenuViewAdapter
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create(bool offerWindowMode)
        {
            auto host = std::make_shared<PauseMenuHost>();
            host->MakeRoot(host);
            host->_view = std::make_unique<Launcher::PauseMenuView>(
                *host, offerWindowMode);
            return host->Root();
        }

        [[nodiscard]] Launcher::PauseMenuView& View() const noexcept
        {
            return *_view;
        }

        Launcher::PauseMenuViewControlHandle ConstructStackPanel() override
        {
            return Toolkit::Element::Create(Toolkit::ElementKind::StackPanel);
        }

        void SetStackPanelSpacing(
            const Launcher::PauseMenuViewControlHandle& panel,
            double spacing) override
        {
            El(panel)->Spacing = spacing;
        }

        Launcher::PauseMenuViewControlRef<Launcher::Caption> ConstructCaption(
            std::optional<std::u16string> text) override
        {
            Toolkit::ElementPtr element = CaptionHost::Create(
                text.has_value() ? *text : std::u16string());
            auto* const host = HostOf<CaptionHost>(element);
            return Launcher::PauseMenuViewControlRef<Launcher::Caption>{element,
                std::shared_ptr<Launcher::Caption>(element, &host->Caption())};
        }

        Launcher::PauseMenuViewControlRef<Launcher::MenuEntry> ConstructMenuEntry(
            std::optional<std::u16string> title,
            std::optional<std::u16string> subtitle, double titleSize) override
        {
            Toolkit::ElementPtr element = MenuEntryHost::Create(
                std::move(title), std::move(subtitle), titleSize);
            auto* const host = HostOf<MenuEntryHost>(element);
            return Launcher::PauseMenuViewControlRef<Launcher::MenuEntry>{element,
                std::shared_ptr<Launcher::MenuEntry>(element, &host->Entry())};
        }

        void AddPanelChild(const Launcher::PauseMenuViewControlHandle& panel,
            const Launcher::PauseMenuViewControlHandle& child) override
        {
            El(panel)->AddChild(Ptr(child));
        }

        double GetControlHeight(
            const Launcher::PauseMenuViewControlHandle& control) const override
        {
            return El(control)->Height.value_or(El(control)->Desired().Height);
        }

        Launcher::PauseMenuViewControlHandle ConstructBorder() override
        {
            return Toolkit::Element::Create(Toolkit::ElementKind::Border);
        }

        void SetBorderBackground(
            const Launcher::PauseMenuViewControlHandle& border,
            const Launcher::GuiBrush& brush) override
        {
            El(border)->Background = ToColor(brush);
        }

        void SetBorderBrush(const Launcher::PauseMenuViewControlHandle& border,
            const Launcher::GuiBrush& brush) override
        {
            El(border)->BorderColor = ToColor(brush);
        }

        void SetBorderThickness(const Launcher::PauseMenuViewControlHandle& border,
            Launcher::PauseMenuViewThickness thickness) override
        {
            El(border)->BorderThickness = Toolkit::Thickness{thickness.Left,
                thickness.Top, thickness.Right, thickness.Bottom};
        }

        void SetBorderPadding(const Launcher::PauseMenuViewControlHandle& border,
            Launcher::PauseMenuViewThickness padding) override
        {
            El(border)->Padding = Toolkit::Thickness{
                padding.Left, padding.Top, padding.Right, padding.Bottom};
        }

        void SetBorderChild(const Launcher::PauseMenuViewControlHandle& border,
            const Launcher::PauseMenuViewControlHandle& child) override
        {
            El(border)->ClearChildren();
            El(border)->AddChild(Ptr(child));
        }

        void SetBorderCornerRadius(
            const Launcher::PauseMenuViewControlHandle& border,
            double radius) override
        {
            El(border)->CornerRadius = radius;
        }

        void SetControlMaxWidth(
            const Launcher::PauseMenuViewControlHandle& control,
            double maxWidth) override
        {
            El(control)->MaxWidth = maxWidth;
        }

        void SetControlHorizontalAlignment(
            const Launcher::PauseMenuViewControlHandle& control,
            Launcher::PauseMenuViewHorizontalAlignment alignment) override
        {
            (void)alignment;
            El(control)->Horizontal = Toolkit::HorizontalAlignment::Center;
        }

        void SetControlVerticalAlignment(
            const Launcher::PauseMenuViewControlHandle& control,
            Launcher::PauseMenuViewVerticalAlignment alignment) override
        {
            (void)alignment;
            El(control)->Vertical = Toolkit::VerticalAlignment::Center;
        }

        Launcher::PauseMenuViewControlHandle
            ConstructLayoutTransformControl() override
        {
            return Toolkit::Element::Create(Toolkit::ElementKind::Panel);
        }

        void SetLayoutTransformChild(
            const Launcher::PauseMenuViewControlHandle& control,
            const Launcher::PauseMenuViewControlHandle& child) override
        {
            El(control)->ClearChildren();
            El(control)->AddChild(Ptr(child));
        }

        std::optional<double> GetLayoutScaleY(
            const Launcher::PauseMenuViewControlHandle& control) const override
        {
            const double scale = El(control)->LayoutScale;
            return scale == 1.0 ? std::nullopt : std::optional<double>(scale);
        }

        void SetLayoutTransform(
            const Launcher::PauseMenuViewControlHandle& control,
            std::optional<Launcher::PauseMenuViewScaleTransform> transform) override
        {
            El(control)->LayoutScale
                = transform.has_value() ? transform->ScaleY : 1.0;
        }

        Launcher::PauseMenuViewControlHandle ConstructScrollViewer() override
        {
            return Toolkit::Element::Create(Toolkit::ElementKind::ScrollViewer);
        }

        void SetScrollViewerContent(
            const Launcher::PauseMenuViewControlHandle& viewer,
            const Launcher::PauseMenuViewControlHandle& content) override
        {
            El(viewer)->ClearChildren();
            El(viewer)->AddChild(Ptr(content));
        }

        void SetScrollViewerPadding(
            const Launcher::PauseMenuViewControlHandle& viewer,
            Launcher::PauseMenuViewThickness padding) override
        {
            El(viewer)->Padding = Toolkit::Thickness{
                padding.Left, padding.Top, padding.Right, padding.Bottom};
        }

        void SetHorizontalScrollBarVisibility(
            const Launcher::PauseMenuViewControlHandle& viewer,
            Launcher::PauseMenuViewScrollBarVisibility visibility) override
        {
            El(viewer)->HorizontalScrollDisabled
                = visibility == Launcher::PauseMenuViewScrollBarVisibility::Disabled;
        }

        void SetVerticalScrollBarVisibility(
            const Launcher::PauseMenuViewControlHandle& viewer,
            Launcher::PauseMenuViewScrollBarVisibility visibility) override
        {
            (void)viewer;
            (void)visibility;
        }

        void AddSizeChanged(const Launcher::PauseMenuViewControlHandle& control,
            Launcher::PauseMenuViewSizeChangedHandler handler) override
        {
            (void)control;
            WhenResized([handler](double, double height)
                { handler.Invoke(height); });
        }

        void SetContent(
            const Launcher::PauseMenuViewControlHandle& content) override
        {
            Content(Ptr(content));
        }

        void Focus(const Launcher::PauseMenuViewControlHandle& control) override
        {
            ScreenHost::Focus(Ptr(control));
        }

        void PostUiThread(Launcher::PauseMenuViewAction action,
            Launcher::PauseMenuViewDispatcherPriority priority) override
        {
            (void)priority;
            Toolkit::Dispatcher::Instance().Post([action]() { action.Invoke(); },
                Toolkit::Priority::Background);
        }

    protected:
        std::unique_ptr<Launcher::PauseMenuView> _view;
    };


    // --- the map picker ---------------------------------------------

    // The rooms the picker walks.
    class RoomNames final : public Launcher::MapPickerRoomList
    {
    public:
        explicit RoomNames(std::vector<std::string> rooms) noexcept
            : _rooms(std::move(rooms))
        {
        }

        [[nodiscard]] std::shared_ptr<Launcher::MapPickerRoomEnumerator>
            GetEnumerator() const override
        {
            return std::make_shared<Enumerator>(_rooms);
        }

    private:
        class Enumerator final : public Launcher::MapPickerRoomEnumerator
        {
        public:
            explicit Enumerator(const std::vector<std::string>& rooms) noexcept
                : _rooms(rooms)
            {
            }

            [[nodiscard]] bool MoveNext() override
            {
                ++_index;
                return _index < static_cast<std::ptrdiff_t>(_rooms.size());
            }

            [[nodiscard]] Launcher::MapPickerStringRef Current() const override
            {
                return std::make_shared<const std::string>(
                    _rooms[static_cast<std::size_t>(_index)]);
            }

            void Dispose() override {}

        private:
            const std::vector<std::string>& _rooms;
            std::ptrdiff_t _index = -1;
        };

        std::vector<std::string> _rooms;
    };

    class MapTileContext final : public Launcher::MapPickerDrawingContext
    {
    public:
        explicit MapTileContext(const Surface& surface) noexcept
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
            const auto* const value
                = static_cast<const Launcher::GuiBrush*>(brush.Native);
            return MakeFormattedText(text,
                face == Launcher::TrackedTextFace::FaceTrue, fontSize,
                value != nullptr ? value->Color : Launcher::GuiTheme::Text);
        }

        void DrawText(const Launcher::TrackedTextFormattedText& text,
            Launcher::TrackedTextPoint point) override
        {
            _surface.DrawFormatted(text, point.X, point.Y);
        }

        void DrawImage(const Launcher::MapPickerBitmap& image,
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
            _surface.Renderer().DrawImage(texture,
                _surface.Map(destination),
                Toolkit::Rect{source.X / image.Width, source.Y / image.Height,
                    source.Width / image.Width, source.Height / image.Height},
                1.0);
        }

        void FillRectangle(
            const Launcher::GuiBrush& brush, Launcher::GuiRect rect) override
        {
            _surface.FillRect(rect, brush.Color);
        }

        void SetFormattedTextMaxTextWidth(
            Launcher::TrackedTextFormattedText& text, double maxTextWidth) override
        {
            if (FormattedRun* const run = RunOf(text.Native))
            {
                run->MaxTextWidth = maxTextWidth;
                text.Width = std::min(text.Width, maxTextWidth);
            }
        }

        void SetFormattedTextMaxTextHeight(
            Launcher::TrackedTextFormattedText& text, double maxTextHeight) override
        {
            text.Height = std::min(text.Height, maxTextHeight);
        }

        void SetFormattedTextTrimming(Launcher::TrackedTextFormattedText& text,
            Launcher::MapPickerTextTrimming trimming) override
        {
            (void)trimming;
            if (FormattedRun* const run = RunOf(text.Native))
            {
                run->Trim = true;
            }
        }

        void DrawRectangle(const Launcher::GuiBrush* brush,
            const Launcher::MapPickerPen& pen, Launcher::GuiRect rect) override
        {
            if (brush != nullptr)
            {
                _surface.FillRect(rect, brush->Color);
            }
            if (pen.Brush != nullptr)
            {
                _surface.StrokeRounded(Launcher::GuiTheme::Round(rect, 0.0),
                    pen.Brush->Color, pen.Thickness);
            }
        }

    private:
        Surface _surface;
    };

    class MapTileHost final : public HostControl,
                              public Launcher::MapTileControlAdapter
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create()
        {
            auto host = std::make_shared<MapTileHost>();
            return Bind(host);
        }

        void Attach(const std::shared_ptr<Launcher::MapTile>& tile)
        {
            _tile = tile;
            MapTileHost* const raw = this;
            const Toolkit::ElementPtr element = _visual->shared_from_this();
            element->PointerEntered = [raw](Toolkit::PointerEvent& e)
            {
                Launcher::MapPickerPointerEventArgs args{&e};
                raw->_tile->OnPointerEntered(args);
            };
            element->PointerExited = [raw](Toolkit::PointerEvent& e)
            {
                Launcher::MapPickerPointerEventArgs args{&e};
                raw->_tile->OnPointerExited(args);
            };
            element->PointerReleased = [raw](Toolkit::PointerEvent& e)
            {
                Launcher::MapPickerPointerReleasedEventArgs args;
                args.Native = &e;
                raw->_tile->OnPointerReleased(args);
                e.Handled = true;
            };
            element->PointerPressed
                = [](Toolkit::PointerEvent& e) { e.Handled = true; };
            element->GotFocus = [raw]()
            {
                Launcher::MapPickerGotFocusEventArgs args{nullptr};
                raw->_tile->OnGotFocus(args);
            };
            element->LostFocus = [raw]()
            {
                Launcher::MapPickerRoutedEventArgs args{nullptr};
                raw->_tile->OnLostFocus(args);
            };
            element->KeyDown = [raw](Toolkit::KeyEvent& e)
            {
                Launcher::MapPickerKeyEventArgs args;
                args.Native = &e;
                args.Key = e.Which == Toolkit::Key::Enter
                    ? Launcher::MapPickerKey::Enter
                    : e.Which == Toolkit::Key::Space
                    ? Launcher::MapPickerKey::Space
                    : e.Which == Toolkit::Key::Escape
                    ? Launcher::MapPickerKey::Escape
                    : Launcher::MapPickerKey::Other;
                raw->_tile->OnKeyDown(args);
                e.Handled = args.Handled;
            };
        }

        [[nodiscard]] Toolkit::Size Measure(
            Toolkit::Element& element, Toolkit::Size available) override
        {
            (void)available;
            return Toolkit::Size{element.Width.value_or(248.0),
                element.Height.value_or(168.0)};
        }

        void Render(
            Toolkit::Element& element, Toolkit::Renderer& renderer) override
        {
            if (_tile == nullptr)
            {
                return;
            }
            MapTileContext context(Surface(renderer, element.Bounds()));
            _tile->Render(context);
        }

        void SetWidth(double width) override { _visual->Width = width; }
        void SetHeight(double height) override { _visual->Height = height; }

        void SetMargin(Launcher::MapPickerThickness margin) override
        {
            _visual->Margin = Toolkit::Thickness{
                margin.Left, margin.Top, margin.Right, margin.Bottom};
        }

        void SetFocusable(bool focusable) override
        {
            _visual->Focusable = focusable;
        }

        void SetHandCursor() override
        {
            _visual->CursorKind = Toolkit::Cursor::Hand;
        }

        void Focus() override { SetFocusHere(); }

        [[nodiscard]] Launcher::GuiRect Bounds() const override
        {
            return ControlBounds();
        }

        [[nodiscard]] Launcher::MapPickerPoint GetPosition(
            const Launcher::MapPickerPointerReleasedEventArgs& e) const override
        {
            const auto* const event
                = static_cast<const Toolkit::PointerEvent*>(e.Native);
            if (event == nullptr)
            {
                return Launcher::MapPickerPoint{0.0, 0.0};
            }
            const Toolkit::Rect bounds = _visual->Bounds();
            return Launcher::MapPickerPoint{
                event->X - bounds.X, event->Y - bounds.Y};
        }

        [[nodiscard]] std::shared_ptr<Launcher::MapPickerBitmap>
            CreateBitmapFromMemory(std::span<const std::uint8_t> bytes) override
        {
            const std::shared_ptr<HostImage> image = DecodeImage(bytes);
            if (image == nullptr)
            {
                return nullptr;
            }
            auto bitmap = std::make_shared<Launcher::MapPickerBitmap>();
            bitmap->Width = image->Width();
            bitmap->Height = image->Height();
            bitmap->Native = std::move(image);
            return bitmap;
        }

        void InvalidateVisual() override {}

        void BaseOnPointerEntered(
            Launcher::MapPickerPointerEventArgs& e) override
        {
            (void)e;
        }

        void BaseOnPointerExited(Launcher::MapPickerPointerEventArgs& e) override
        {
            (void)e;
        }

        void BaseOnPointerReleased(
            Launcher::MapPickerPointerReleasedEventArgs& e) override
        {
            (void)e;
        }

        void BaseOnKeyDown(Launcher::MapPickerKeyEventArgs& e) override
        {
            (void)e;
        }

        void BaseOnGotFocus(Launcher::MapPickerGotFocusEventArgs& e) override
        {
            (void)e;
        }

        void BaseOnLostFocus(Launcher::MapPickerRoutedEventArgs& e) override
        {
            (void)e;
        }

    private:
        std::shared_ptr<Launcher::MapTile> _tile;
    };

    class MapPickerHost : public ScreenHost,
                                public Launcher::MapPickerViewAdapter
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create(
            const std::vector<std::string>& rooms, std::string current)
        {
            auto host = std::make_shared<MapPickerHost>();
            host->MakeRoot(host);
            host->_view = std::make_unique<Launcher::MapPickerView>(*host,
                std::make_shared<const RoomNames>(rooms),
                std::make_shared<const std::string>(std::move(current)));
            return host->Root();
        }

        [[nodiscard]] Launcher::MapPickerView& View() const noexcept
        {
            return *_view;
        }

        void SetBackground(const Launcher::GuiBrush& brush) override
        {
            _root->Background = ToColor(brush);
        }

        void SetFocusable(bool focusable) override
        {
            _root->Focusable = focusable;
        }

        ElementHandle CreateWrapPanel() override
        {
            return Keep(Toolkit::Element::Create(Toolkit::ElementKind::WrapPanel));
        }

        void SetWrapPanelOrientation(
            ElementHandle panel, Launcher::MapPickerOrientation orientation) override
        {
            (void)orientation;
            El(panel)->StackOrientation = Toolkit::Orientation::Horizontal;
        }

        std::shared_ptr<Launcher::MapTileControlAdapter>
            CreateMapTileControl() override
        {
            const Toolkit::ElementPtr element = MapTileHost::Create();
            _owned.push_back(element);
            return std::shared_ptr<Launcher::MapTileControlAdapter>(
                element, HostOf<MapTileHost>(element));
        }

        ElementHandle ElementOfMapTile(
            Launcher::MapTileControlAdapter& tile) override
        {
            return &static_cast<MapTileHost&>(tile).Visual();
        }

        void AttachMapTile(Launcher::MapTileControlAdapter& control,
            const std::shared_ptr<Launcher::MapTile>& tile) override
        {
            static_cast<MapTileHost&>(control).Attach(tile);
        }

        std::shared_ptr<Launcher::MenuEntryControlAdapter>
            CreateMenuEntryControl() override
        {
            const Toolkit::ElementPtr element = MenuEntryHost::CreateBare();
            _owned.push_back(element);
            return std::shared_ptr<Launcher::MenuEntryControlAdapter>(
                element, HostOf<MenuEntryHost>(element));
        }

        ElementHandle ElementOfMenuEntry(
            Launcher::MenuEntryControlAdapter& entry) override
        {
            return &static_cast<MenuEntryHost&>(entry).Visual();
        }

        void AttachMenuEntry(Launcher::MenuEntryControlAdapter& control,
            Launcher::MenuEntry& entry) override
        {
            static_cast<MenuEntryHost&>(control).Attach(entry);
        }

        ElementHandle CreateTextBlock() override
        {
            return Keep(Toolkit::Element::Create(Toolkit::ElementKind::TextBlock));
        }

        void SetTextBlockText(ElementHandle textBlock,
            std::optional<std::u16string_view> text) override
        {
            El(textBlock)->Text = text.has_value()
                ? std::optional<std::string>(Utf16ToUtf8(*text))
                : std::nullopt;
        }

        void SetTextBlockFontFamily(
            ElementHandle textBlock, Launcher::GuiFontFamily family) override
        {
            (void)textBlock;
            (void)family;
        }

        void SetTextBlockFontSize(
            ElementHandle textBlock, double fontSize) override
        {
            El(textBlock)->FontSize = fontSize;
        }

        void SetTextBlockForeground(
            ElementHandle textBlock, const Launcher::GuiBrush& brush) override
        {
            El(textBlock)->Foreground = ToColor(brush);
        }

        ElementHandle CreateGrid() override
        {
            return Keep(Toolkit::Element::Create(Toolkit::ElementKind::Grid));
        }

        void SetGridColumnDefinitions(
            ElementHandle grid, std::u16string_view definitions) override
        {
            El(grid)->ColumnDefinitions
                = Toolkit::ParseGridDefinitions(Utf16ToUtf8(definitions));
        }

        void SetGridColumn(ElementHandle child, std::int32_t column) override
        {
            El(child)->GridColumn = column;
        }

        ElementHandle CreateBorder() override
        {
            return Keep(Toolkit::Element::Create(Toolkit::ElementKind::Border));
        }

        void SetBorderBackground(
            ElementHandle border, const Launcher::GuiBrush& brush) override
        {
            El(border)->Background = ToColor(brush);
        }

        void SetBorderPadding(
            ElementHandle border, Launcher::MapPickerThickness padding) override
        {
            El(border)->Padding = Toolkit::Thickness{
                padding.Left, padding.Top, padding.Right, padding.Bottom};
        }

        void SetBorderChild(ElementHandle border, ElementHandle child) override
        {
            El(border)->ClearChildren();
            El(border)->AddChild(Owned(child));
        }

        ElementHandle CreateScrollViewer() override
        {
            return Keep(
                Toolkit::Element::Create(Toolkit::ElementKind::ScrollViewer));
        }

        void SetScrollViewerContent(
            ElementHandle scrollViewer, ElementHandle content) override
        {
            El(scrollViewer)->ClearChildren();
            El(scrollViewer)->AddChild(Owned(content));
        }

        void SetScrollViewerPadding(ElementHandle scrollViewer,
            Launcher::MapPickerThickness padding) override
        {
            El(scrollViewer)->Padding = Toolkit::Thickness{
                padding.Left, padding.Top, padding.Right, padding.Bottom};
        }

        void SetHorizontalScrollBarVisibility(ElementHandle scrollViewer,
            Launcher::MapPickerScrollBarVisibility visibility) override
        {
            El(scrollViewer)->HorizontalScrollDisabled = visibility
                == Launcher::MapPickerScrollBarVisibility::Disabled;
        }

        ElementHandle CreateDockPanel() override
        {
            return Keep(Toolkit::Element::Create(Toolkit::ElementKind::DockPanel));
        }

        void SetDockPanelLastChildFill(ElementHandle panel, bool value) override
        {
            El(panel)->LastChildFill = value;
        }

        void SetDock(ElementHandle child, Launcher::MapPickerDock dock) override
        {
            (void)dock;
            El(child)->DockSide = Toolkit::Dock::Top;
        }

        void SetWidth(ElementHandle element, double width) override
        {
            El(element)->Width = width;
        }

        void SetHorizontalAlignment(ElementHandle element,
            Launcher::MapPickerHorizontalAlignment alignment) override
        {
            El(element)->Horizontal
                = alignment == Launcher::MapPickerHorizontalAlignment::Right
                ? Toolkit::HorizontalAlignment::Right
                : Toolkit::HorizontalAlignment::Left;
        }

        void SetVerticalAlignment(ElementHandle element,
            Launcher::MapPickerVerticalAlignment alignment) override
        {
            (void)alignment;
            El(element)->Vertical = Toolkit::VerticalAlignment::Center;
        }

        void AddChild(ElementHandle parent, ElementHandle child) override
        {
            El(parent)->AddChild(Owned(child));
        }

        void SetContent(ElementHandle content) override
        {
            Content(Owned(content));
        }

        void Post(void* context, PostedCallback callback,
            std::shared_ptr<void> keepAlive,
            Launcher::MapPickerDispatcherPriority priority) override
        {
            (void)priority;
            Toolkit::Dispatcher::Instance().Post(
                [context, callback, keepAlive]()
                {
                    if (callback != nullptr)
                    {
                        callback(context);
                    }
                },
                Toolkit::Priority::Background);
        }

        void BaseOnAttachedToVisualTree(
            Launcher::MapPickerVisualTreeAttachmentEventArgs& e) override
        {
            (void)e;
        }

        void BaseOnKeyDown(Launcher::MapPickerKeyEventArgs& e) override
        {
            (void)e;
        }

    private:
        // A raw handle is only valid while the host holds the element.
        [[nodiscard]] ElementHandle Keep(Toolkit::ElementPtr element)
        {
            ElementHandle handle = element.get();
            _owned.push_back(std::move(element));
            return handle;
        }

        [[nodiscard]] Toolkit::ElementPtr Owned(ElementHandle handle) const
        {
            for (const Toolkit::ElementPtr& element : _owned)
            {
                if (element.get() == handle)
                {
                    return element;
                }
            }
            return nullptr;
        }

        std::unique_ptr<Launcher::MapPickerView> _view;
    };

    // --- the demo picker --------------------------------------------

    // The recordings the picker walks.
    class DemoList final : public Launcher::DemoPickerViewDemoList
    {
    public:
        explicit DemoList(
            std::shared_ptr<const std::vector<
                MphRead::Mods::Network::DemoRecording>> demos) noexcept
            : _demos(std::move(demos))
        {
        }

        [[nodiscard]] std::shared_ptr<Launcher::DemoPickerViewDemoEnumerator>
            GetEnumerator() override
        {
            return std::make_shared<Enumerator>(_demos);
        }

        [[nodiscard]] std::int32_t Count() override
        {
            return _demos == nullptr
                ? 0
                : static_cast<std::int32_t>(_demos->size());
        }

    private:
        class Enumerator final : public Launcher::DemoPickerViewDemoEnumerator
        {
        public:
            explicit Enumerator(
                std::shared_ptr<const std::vector<
                    MphRead::Mods::Network::DemoRecording>> demos) noexcept
                : _demos(std::move(demos))
            {
            }

            [[nodiscard]] bool MoveNext() override
            {
                ++_index;
                return _demos != nullptr
                    && _index < static_cast<std::ptrdiff_t>(_demos->size());
            }

            [[nodiscard]] MphRead::Mods::Network::DemoRecording Current() override
            {
                return (*_demos)[static_cast<std::size_t>(_index)];
            }

            void Dispose() override {}

        private:
            std::shared_ptr<const std::vector<
                MphRead::Mods::Network::DemoRecording>> _demos;
            std::ptrdiff_t _index = -1;
        };

        std::shared_ptr<const std::vector<
            MphRead::Mods::Network::DemoRecording>> _demos;
    };

    class DemoPickerHost : public ScreenHost,
                                 public Launcher::DemoPickerViewAdapter
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create(
            std::shared_ptr<const std::vector<
                MphRead::Mods::Network::DemoRecording>> demos,
            std::string directory)
        {
            auto host = std::make_shared<DemoPickerHost>();
            host->MakeRoot(host);
            host->_view = std::make_unique<Launcher::DemoPickerView>(*host,
                std::make_shared<DemoList>(std::move(demos)),
                std::move(directory));
            return host->Root();
        }

        [[nodiscard]] Launcher::DemoPickerView& View() const noexcept
        {
            return *_view;
        }

        void SetBackground(const Launcher::GuiBrush& brush) override
        {
            _root->Background = ToColor(brush);
        }

        void SetFocusable(bool focusable) override
        {
            _root->Focusable = focusable;
        }

        ControlHandle ConstructStackPanel() override
        {
            return Toolkit::Element::Create(Toolkit::ElementKind::StackPanel);
        }

        void SetStackPanelSpacing(
            const ControlHandle& panel, double spacing) override
        {
            El(panel)->Spacing = spacing;
        }

        ControlHandle ConstructMenuEntry(
            std::string title, std::string subtitle, double titleSize) override
        {
            return MenuEntryHost::Create(
                Utf8ToUtf16(title), Utf8ToUtf16(subtitle), titleSize);
        }

        void SetMenuEntryAccent(
            const ControlHandle& entry, Launcher::GuiColor accent) override
        {
            HostOf<MenuEntryHost>(Ptr(entry))->Entry().Accent(accent);
        }

        void AddMenuEntryClick(const ControlHandle& entry,
            Launcher::DemoPickerViewAction handler) override
        {
            HostOf<MenuEntryHost>(Ptr(entry))->Click(
                [handler]()
                {
                    if (handler.Function != nullptr)
                    {
                        handler.Function(handler.Target.get());
                    }
                });
        }

        void Focus(const ControlHandle& control) override
        {
            ScreenHost::Focus(Ptr(control));
        }

        ControlHandle ConstructTextBlock() override
        {
            return Toolkit::Element::Create(Toolkit::ElementKind::TextBlock);
        }

        void SetTextBlockText(
            const ControlHandle& textBlock, std::string text) override
        {
            El(textBlock)->Text = std::move(text);
        }

        void SetTextBlockForeground(const ControlHandle& textBlock,
            const Launcher::GuiBrush& brush) override
        {
            El(textBlock)->Foreground = ToColor(brush);
        }

        void SetTextBlockFontSize(
            const ControlHandle& textBlock, double fontSize) override
        {
            El(textBlock)->FontSize = fontSize;
        }

        void SetTextBlockTextWrapping(const ControlHandle& textBlock,
            Launcher::DemoPickerViewTextWrapping wrapping) override
        {
            (void)wrapping;
            El(textBlock)->WrapText = true;
        }

        void SetTextBlockFontFamily(const ControlHandle& textBlock,
            const Launcher::GuiFontFamily& fontFamily) override
        {
            (void)textBlock;
            (void)fontFamily;
        }

        void SetControlMargin(const ControlHandle& control,
            Launcher::DemoPickerViewThickness margin) override
        {
            El(control)->Margin = Toolkit::Thickness{
                margin.Left, margin.Top, margin.Right, margin.Bottom};
        }

        void SetControlWidth(const ControlHandle& control, double width) override
        {
            El(control)->Width = width;
        }

        void SetControlHorizontalAlignment(const ControlHandle& control,
            Launcher::DemoPickerViewHorizontalAlignment alignment) override
        {
            El(control)->Horizontal
                = alignment
                    == Launcher::DemoPickerViewHorizontalAlignment::Right
                ? Toolkit::HorizontalAlignment::Right
                : Toolkit::HorizontalAlignment::Left;
        }

        void SetControlVerticalAlignment(const ControlHandle& control,
            Launcher::DemoPickerViewVerticalAlignment alignment) override
        {
            (void)alignment;
            El(control)->Vertical = Toolkit::VerticalAlignment::Center;
        }

        ControlHandle ConstructGrid() override
        {
            return Toolkit::Element::Create(Toolkit::ElementKind::Grid);
        }

        ColumnDefinitionsHandle ConstructColumnDefinitions(
            std::string_view definitions) override
        {
            return std::make_shared<std::vector<Toolkit::GridLength>>(
                Toolkit::ParseGridDefinitions(definitions));
        }

        void SetGridColumnDefinitions(const ControlHandle& grid,
            const ColumnDefinitionsHandle& definitions) override
        {
            El(grid)->ColumnDefinitions
                = *static_cast<std::vector<Toolkit::GridLength>*>(
                    definitions.get());
        }

        void SetGridColumn(
            const ControlHandle& control, std::int32_t column) override
        {
            El(control)->GridColumn = column;
        }

        ControlHandle ConstructBorder() override
        {
            return Toolkit::Element::Create(Toolkit::ElementKind::Border);
        }

        void SetBorderBackground(const ControlHandle& border,
            const Launcher::GuiBrush& brush) override
        {
            El(border)->Background = ToColor(brush);
        }

        void SetBorderPadding(const ControlHandle& border,
            Launcher::DemoPickerViewThickness padding) override
        {
            El(border)->Padding = Toolkit::Thickness{
                padding.Left, padding.Top, padding.Right, padding.Bottom};
        }

        void SetBorderChild(
            const ControlHandle& border, const ControlHandle& child) override
        {
            El(border)->ClearChildren();
            El(border)->AddChild(Ptr(child));
        }

        ControlHandle ConstructScrollViewer() override
        {
            return Toolkit::Element::Create(Toolkit::ElementKind::ScrollViewer);
        }

        void SetScrollViewerContent(
            const ControlHandle& viewer, const ControlHandle& content) override
        {
            El(viewer)->ClearChildren();
            El(viewer)->AddChild(Ptr(content));
        }

        void SetScrollViewerPadding(const ControlHandle& viewer,
            Launcher::DemoPickerViewThickness padding) override
        {
            El(viewer)->Padding = Toolkit::Thickness{
                padding.Left, padding.Top, padding.Right, padding.Bottom};
        }

        void SetScrollViewerHorizontalScrollBarVisibility(
            const ControlHandle& viewer,
            Launcher::DemoPickerViewScrollBarVisibility visibility) override
        {
            El(viewer)->HorizontalScrollDisabled = visibility
                == Launcher::DemoPickerViewScrollBarVisibility::Disabled;
        }

        ControlHandle ConstructDockPanel() override
        {
            return Toolkit::Element::Create(Toolkit::ElementKind::DockPanel);
        }

        void SetDockPanelLastChildFill(
            const ControlHandle& panel, bool lastChildFill) override
        {
            El(panel)->LastChildFill = lastChildFill;
        }

        void SetDock(
            const ControlHandle& control, Launcher::DemoPickerViewDock dock) override
        {
            (void)dock;
            El(control)->DockSide = Toolkit::Dock::Top;
        }

        void AddPanelChild(
            const ControlHandle& panel, const ControlHandle& child) override
        {
            El(panel)->AddChild(Ptr(child));
        }

        void SetContent(const ControlHandle& content) override
        {
            Content(Ptr(content));
        }

        void PostUiThread(Launcher::DemoPickerViewAction action,
            Launcher::DemoPickerViewDispatcherPriority priority) override
        {
            (void)priority;
            Toolkit::Dispatcher::Instance().Post(
                [action]()
                {
                    if (action.Function != nullptr)
                    {
                        action.Function(action.Target.get());
                    }
                },
                Toolkit::Priority::Background);
        }

        void BaseOnAttachedToVisualTree(
            Launcher::DemoPickerViewVisualTreeAttachmentEventArgs& e) override
        {
            (void)e;
        }

        void BaseOnKeyDown(Launcher::DemoPickerViewKeyEventArgs& e) override
        {
            (void)e;
        }

    private:
        std::unique_ptr<Launcher::DemoPickerView> _view;
    };

    // --- the settings screen ----------------------------------------

    class SettingsHost : public ScreenHost,
                               public Launcher::SettingsViewAdapter
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create(
            const std::shared_ptr<MphRead::MenuSettings>& settings, bool inGame)
        {
            auto host = std::make_shared<SettingsHost>();
            host->MakeRoot(host);
            host->_view = std::make_unique<Launcher::SettingsView>(
                *host, settings, inGame);
            return host->Root();
        }

        [[nodiscard]] Launcher::SettingsView& View() const noexcept
        {
            return *_view;
        }

        [[nodiscard]] bool IsAndroid() const override
        {
#if defined(ANDROID) || defined(__ANDROID__)
            return true;
#else
            return false;
#endif
        }

        [[nodiscard]] bool OrdinalIgnoreCaseEquals(
            std::optional<std::u16string_view> left,
            std::optional<std::u16string_view> right) const override
        {
            if (!left.has_value() || !right.has_value())
            {
                return left.has_value() == right.has_value();
            }
            if (left->size() != right->size())
            {
                return false;
            }
            for (std::size_t i = 0; i < left->size(); ++i)
            {
                char16_t a = (*left)[i];
                char16_t b = (*right)[i];
                if (a >= u'a' && a <= u'z')
                {
                    a = static_cast<char16_t>(a - u'a' + u'A');
                }
                if (b >= u'a' && b <= u'z')
                {
                    b = static_cast<char16_t>(b - u'a' + u'A');
                }
                if (a != b)
                {
                    return false;
                }
            }
            return true;
        }

        void SetBackground(const Launcher::GuiBrush& brush) override
        {
            _root->Background = ToColor(brush);
        }

        void SetFocusable(bool focusable) override
        {
            _root->Focusable = focusable;
        }

        Launcher::SettingsViewControlHandle ConstructStackPanel() override
        {
            return Toolkit::Element::Create(Toolkit::ElementKind::StackPanel);
        }

        Launcher::SettingsViewControlHandle ConstructWrapPanel() override
        {
            return Toolkit::Element::Create(Toolkit::ElementKind::WrapPanel);
        }

        Launcher::SettingsViewControlHandle ConstructPanel() override
        {
            return Toolkit::Element::Create(Toolkit::ElementKind::Panel);
        }

        Launcher::SettingsViewControlHandle ConstructGrid() override
        {
            return Toolkit::Element::Create(Toolkit::ElementKind::Grid);
        }

        Launcher::SettingsViewControlHandle ConstructBorder() override
        {
            return Toolkit::Element::Create(Toolkit::ElementKind::Border);
        }

        Launcher::SettingsViewControlHandle ConstructScrollViewer() override
        {
            return Toolkit::Element::Create(Toolkit::ElementKind::ScrollViewer);
        }

        Launcher::SettingsViewControlRef<Launcher::Caption> ConstructCaption(
            std::optional<std::u16string> text) override
        {
            const Toolkit::ElementPtr element = CaptionHost::Create(
                text.has_value() ? *text : std::u16string());
            auto* const host = HostOf<CaptionHost>(element);
            return Launcher::SettingsViewControlRef<Launcher::Caption>{element,
                std::shared_ptr<Launcher::Caption>(element, &host->Caption())};
        }

        Launcher::SettingsViewControlRef<Launcher::Note> ConstructNote(
            std::optional<std::u16string> text,
            std::optional<Launcher::GuiColor> color) override
        {
            const Toolkit::ElementPtr element = NoteHost::Create(
                text.has_value() ? *text : std::u16string(), color);
            auto* const host = HostOf<NoteHost>(element);
            return Launcher::SettingsViewControlRef<Launcher::Note>{element,
                std::shared_ptr<Launcher::Note>(element, &host->Note())};
        }

        Launcher::SettingsViewControlRef<Launcher::MenuEntry> ConstructMenuEntry(
            std::optional<std::u16string> title,
            std::optional<std::u16string> subtitle, double titleSize) override
        {
            const Toolkit::ElementPtr element = MenuEntryHost::Create(
                std::move(title), std::move(subtitle), titleSize);
            auto* const host = HostOf<MenuEntryHost>(element);
            return Launcher::SettingsViewControlRef<Launcher::MenuEntry>{element,
                std::shared_ptr<Launcher::MenuEntry>(element, &host->Entry())};
        }

        Launcher::SettingsViewControlRef<Launcher::ChoiceRow> ConstructChoiceRow(
            std::optional<std::u16string> label,
            Launcher::RowsStringListRef options, std::int32_t index) override
        {
            const Toolkit::ElementPtr element = ChoiceRowHost::CreateFromList(
                label.has_value() ? *label : std::u16string(),
                std::move(options), index);
            auto* const host = HostOf<ChoiceRowHost>(element);
            return Launcher::SettingsViewControlRef<Launcher::ChoiceRow>{element,
                std::shared_ptr<Launcher::ChoiceRow>(element, &host->Row())};
        }

        Launcher::SettingsViewControlRef<Launcher::ToggleRow> ConstructToggleRow(
            std::optional<std::u16string> label, bool on) override
        {
            const Toolkit::ElementPtr element = ToggleRowHost::Create(
                label.has_value() ? *label : std::u16string(), on);
            auto* const host = HostOf<ToggleRowHost>(element);
            return Launcher::SettingsViewControlRef<Launcher::ToggleRow>{element,
                std::shared_ptr<Launcher::ToggleRow>(element, &host->Row())};
        }

        Launcher::SettingsViewControlRef<Launcher::FieldRow> ConstructFieldRow(
            std::optional<std::u16string> label,
            std::optional<std::u16string> value, double boxWidth) override
        {
            const Toolkit::ElementPtr element = FieldRowHost::Create(
                label.has_value() ? *label : std::u16string(),
                value.has_value() ? *value : std::u16string(), boxWidth);
            auto* const host = HostOf<FieldRowHost>(element);
            return Launcher::SettingsViewControlRef<Launcher::FieldRow>{element,
                std::shared_ptr<Launcher::FieldRow>(element, &host->Row())};
        }

        Launcher::SettingsViewControlRef<Launcher::SliderRow> ConstructSliderRow(
            std::optional<std::u16string> label, std::int32_t value,
            SliderFormat format, double labelWidth, std::int32_t min,
            std::int32_t max, std::int32_t keyStep) override
        {
            const Toolkit::ElementPtr element = SliderRowHost::Create(
                std::move(label), value, std::move(format), labelWidth, min, max,
                keyStep);
            auto* const host = HostOf<SliderRowHost>(element);
            return Launcher::SettingsViewControlRef<Launcher::SliderRow>{element,
                std::shared_ptr<Launcher::SliderRow>(element, &host->Row())};
        }

        Launcher::SettingsViewControlRef<Launcher::KeyRow> ConstructKeyRow(
            const MphRead::Mods::InputBindingProperty* property,
            double labelWidth) override
        {
            const Toolkit::ElementPtr element
                = KeyRowHost::Create(property, labelWidth);
            auto* const host = HostOf<KeyRowHost>(element);
            return Launcher::SettingsViewControlRef<Launcher::KeyRow>{element,
                std::shared_ptr<Launcher::KeyRow>(element, &host->Row())};
        }

        Launcher::SettingsViewControlRef<Launcher::KeyRow> ConstructKeyRow(
            std::optional<std::u16string> label, Launcher::KeyRowGetHandler get,
            Launcher::KeyRowSetHandler set, double labelWidth) override
        {
            const Toolkit::ElementPtr element = KeyRowHost::Create(
                std::move(label), std::move(get), std::move(set), labelWidth);
            auto* const host = HostOf<KeyRowHost>(element);
            return Launcher::SettingsViewControlRef<Launcher::KeyRow>{element,
                std::shared_ptr<Launcher::KeyRow>(element, &host->Row())};
        }

        Launcher::SettingsViewControlRef<Launcher::PadRow> ConstructPadRow(
            MphRead::Mods::Input::PadAction action, double labelWidth) override
        {
            const Toolkit::ElementPtr element
                = PadRowHost::Create(action, labelWidth);
            auto* const host = HostOf<PadRowHost>(element);
            return Launcher::SettingsViewControlRef<Launcher::PadRow>{element,
                std::shared_ptr<Launcher::PadRow>(element, &host->Row())};
        }

        void SetStackPanelSpacing(
            const Launcher::SettingsViewControlHandle& panel,
            double spacing) override
        {
            El(panel)->Spacing = spacing;
        }

        void AddPanelChild(const Launcher::SettingsViewControlHandle& panel,
            const Launcher::SettingsViewControlHandle& child) override
        {
            El(panel)->AddChild(Ptr(child));
        }

        void ClearPanelChildren(
            const Launcher::SettingsViewControlHandle& panel) override
        {
            El(panel)->ClearChildren();
        }

        Launcher::SettingsViewControlHandle GetParent(
            const Launcher::SettingsViewControlHandle& control) const override
        {
            Toolkit::Element* const parent = El(control)->Parent();
            return parent == nullptr ? Launcher::SettingsViewControlHandle{}
                                     : parent->shared_from_this();
        }

        void SetControlMargin(const Launcher::SettingsViewControlHandle& control,
            Launcher::SettingsViewThickness margin) override
        {
            El(control)->Margin = Toolkit::Thickness{
                margin.Left, margin.Top, margin.Right, margin.Bottom};
        }

        void SetControlWidth(const Launcher::SettingsViewControlHandle& control,
            double width) override
        {
            El(control)->Width = width;
        }

        void SetControlIsVisible(
            const Launcher::SettingsViewControlHandle& control,
            bool visible) override
        {
            El(control)->Visible = visible;
        }

        void SetControlVerticalAlignment(
            const Launcher::SettingsViewControlHandle& control,
            Launcher::SettingsViewVerticalAlignment alignment) override
        {
            (void)alignment;
            El(control)->Vertical = Toolkit::VerticalAlignment::Bottom;
        }

        void Focus(const Launcher::SettingsViewControlHandle& control) override
        {
            ScreenHost::Focus(Ptr(control));
        }

        Launcher::SettingsViewDefinitionsHandle ConstructColumnDefinitions(
            std::string_view definitions) override
        {
            return std::make_shared<std::vector<Toolkit::GridLength>>(
                Toolkit::ParseGridDefinitions(definitions));
        }

        Launcher::SettingsViewDefinitionsHandle ConstructRowDefinitions(
            std::string_view definitions) override
        {
            return std::make_shared<std::vector<Toolkit::GridLength>>(
                Toolkit::ParseGridDefinitions(definitions));
        }

        void SetGridColumnDefinitions(
            const Launcher::SettingsViewControlHandle& grid,
            const Launcher::SettingsViewDefinitionsHandle& definitions) override
        {
            El(grid)->ColumnDefinitions
                = *static_cast<std::vector<Toolkit::GridLength>*>(
                    definitions.get());
        }

        void SetGridRowDefinitions(
            const Launcher::SettingsViewControlHandle& grid,
            const Launcher::SettingsViewDefinitionsHandle& definitions) override
        {
            El(grid)->RowDefinitions
                = *static_cast<std::vector<Toolkit::GridLength>*>(
                    definitions.get());
        }

        void SetGridRow(const Launcher::SettingsViewControlHandle& control,
            std::int32_t row) override
        {
            El(control)->GridRow = row;
        }

        void SetGridColumn(const Launcher::SettingsViewControlHandle& control,
            std::int32_t column) override
        {
            El(control)->GridColumn = column;
        }

        void SetGridRowSpan(const Launcher::SettingsViewControlHandle& control,
            std::int32_t rowSpan) override
        {
            El(control)->GridRowSpan = rowSpan;
        }

        void SetBorderBackground(
            const Launcher::SettingsViewControlHandle& border,
            const Launcher::GuiBrush& brush) override
        {
            El(border)->Background = ToColor(brush);
        }

        void SetBorderPadding(const Launcher::SettingsViewControlHandle& border,
            Launcher::SettingsViewThickness padding) override
        {
            El(border)->Padding = Toolkit::Thickness{
                padding.Left, padding.Top, padding.Right, padding.Bottom};
        }

        void SetBorderChild(const Launcher::SettingsViewControlHandle& border,
            const Launcher::SettingsViewControlHandle& child) override
        {
            El(border)->ClearChildren();
            El(border)->AddChild(Ptr(child));
        }

        void SetScrollViewerContent(
            const Launcher::SettingsViewControlHandle& viewer,
            const Launcher::SettingsViewControlHandle& content) override
        {
            El(viewer)->ClearChildren();
            El(viewer)->AddChild(Ptr(content));
        }

        void SetScrollViewerHorizontalScrollBarVisibility(
            const Launcher::SettingsViewControlHandle& viewer,
            Launcher::SettingsViewScrollBarVisibility visibility) override
        {
            El(viewer)->HorizontalScrollDisabled
                = visibility == Launcher::SettingsViewScrollBarVisibility::Disabled;
        }

        void SetScrollViewerVerticalScrollBarVisibility(
            const Launcher::SettingsViewControlHandle& viewer,
            Launcher::SettingsViewScrollBarVisibility visibility) override
        {
            (void)viewer;
            (void)visibility;
        }

        void AddMenuEntryClick(
            const Launcher::SettingsViewControlHandle& entry,
            Launcher::SettingsViewAction handler) override
        {
            HostOf<MenuEntryHost>(Ptr(entry))
                ->Click([handler]() { handler.Invoke(); });
        }

        void AddSizeChanged(
            Launcher::SettingsViewSizeChangedHandler handler) override
        {
            WhenResized([handler](double width, double)
                { handler.Invoke(width); });
        }

        void SetContent(
            const Launcher::SettingsViewControlHandle& content) override
        {
            Content(Ptr(content));
        }

        void PostUiThread(Launcher::SettingsViewAction action,
            Launcher::SettingsViewDispatcherPriority priority) override
        {
            (void)priority;
            Toolkit::Dispatcher::Instance().Post([action]() { action.Invoke(); },
                Toolkit::Priority::Background);
        }

        void BaseOnAttachedToVisualTree(
            Launcher::SettingsViewVisualTreeAttachmentEventArgs& e) override
        {
            (void)e;
        }

        void BaseOnKeyDown(Launcher::SettingsViewKeyEventArgs& e) override
        {
            (void)e;
        }

    protected:
        std::unique_ptr<Launcher::SettingsView> _view;
    };

    // A handler that does not own its target, for an event owned by it.
    [[nodiscard]] inline std::shared_ptr<void> Borrowed(void* target)
    {
        return std::shared_ptr<void>(target, [](void*) {});
    }

    // One subscriber, kept alive by the element it belongs to.
    struct Subscriber final
    {
        std::function<void()> Action;
    };

    template <typename Host>
    [[nodiscard]] Host* HostFor(const Toolkit::ElementPtr& view)
    {
        return static_cast<Host*>(view->Tag.get());
    }
}
