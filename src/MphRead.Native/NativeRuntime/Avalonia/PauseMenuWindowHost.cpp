// The pause menu's own window, and the two dialogs it opens: what Avalonia is
// to PauseMenuWindow on the managed side. The screens themselves are the same
// adapters the launcher uses.

#include "HostScreens.hpp"

#include "../Gui/Host.hpp"

#include "../../Mods/Launcher/Gui/PauseMenuWindow.hpp"
#include "../../Mods/Launcher/Gui/SettingsWindow.hpp"

#include <memory>
#include <string>
#include <utility>

namespace MphRead::NativeRuntime::Avalonia
{
    namespace
    {
        // A window and the element tree one of the screen hosts builds in it.
        class ScreenWindow
        {
        public:
            void Open(const Toolkit::ElementPtr& content)
            {
                _window.Content(content);
                _window.ClientSize(_width, _height);
                _window.MinimumSize(_minWidth, _minHeight);
                _window.Decorated(_decorated);
                _window.Topmost(_topmost);
                if (_center)
                {
                    _window.CenterOnScreen();
                }
                else if (_placed)
                {
                    _window.Position(_x, _y);
                }
                _window.Show();
            }

            void Place(std::int32_t x, std::int32_t y)
            {
                _x = x;
                _y = y;
                _placed = true;
                _center = false;
                _window.Position(x, y);
            }

            [[nodiscard]] Toolkit::Window& Window() noexcept { return _window; }

            double _width = 720.0;
            double _height = 520.0;
            double _minWidth = 0.0;
            double _minHeight = 0.0;
            bool _center = false;
            bool _decorated = true;
            bool _topmost = false;
            bool _placed = false;
            std::int32_t _x = 0;
            std::int32_t _y = 0;

        private:
            Toolkit::Window _window;
        };

        // The two dialogs the pause menu opens, in one window as the adapter
        // they share is one object.
        class ChildWindowHost final : public Launcher::PauseMenuWindowChildAdapter,
                                      public SettingsHost,
                                      public MapPickerHost
        {
        public:
            ChildWindowHost()
            {
                // Both screens are built into this one window; whichever is
                // shown first makes its own root.
                SettingsHost::MakeRoot(std::shared_ptr<void>(this, [](void*) {}));
                MapPickerHost::MakeRoot(std::shared_ptr<void>(this, [](void*) {}));
            }

            // --- the window itself
            void Close() override { _window.Window().Close(); }

            void SetIcon(const std::optional<Launcher::GuiWindowIcon>& icon) override
            {
                (void)icon;
            }

            void SetBackground(const Launcher::GuiBrush& brush) override
            {
                _window.Window().Background(ToColor(brush));
            }

            void SetWidth(double width) override { _window._width = width; }
            void SetHeight(double height) override { _window._height = height; }
            void SetMinWidth(double minWidth) override { _window._minWidth = minWidth; }

            void SetMinHeight(double minHeight) override
            {
                _window._minHeight = minHeight;
            }

            // --- the cover target
            void SetWindowStartupLocation(
                Launcher::PauseMenuWindowStartupLocation location) override
            {
                _window._center
                    = location == Launcher::PauseMenuWindowStartupLocation::CenterScreen;
            }

            [[nodiscard]] Launcher::PauseMenuWindowPixelPoint Position() const override
            {
                return _position;
            }

            void SetPosition(Launcher::PauseMenuWindowPixelPoint position) override
            {
                _position = position;
                _window.Place(position.X, position.Y);
            }

            [[nodiscard]] double Width() const override { return _window._width; }
            [[nodiscard]] double Height() const override { return _window._height; }

            [[nodiscard]] std::optional<double> ScreenScalingFromPoint(
                Launcher::PauseMenuWindowPixelPoint point) const override
            {
                (void)point;
                return 1.0;
            }

            [[nodiscard]] double RenderScaling() const override { return 1.0; }

            // --- the settings dialog
            void SetTitle(std::string title) override
            {
                _window.Window().Title(title);
            }

            void SetRequestedThemeVariant(
                Launcher::SettingsWindowThemeVariant variant) override
            {
                (void)variant;
            }

            void SetCanResize(bool canResize) override { (void)canResize; }

            void SetSystemDecorations(
                Launcher::SettingsWindowSystemDecorations decorations) override
            {
                (void)decorations;
            }

            void SetTopmost(bool topmost) override { (void)topmost; }

            void SetShowInTaskbar(bool showInTaskbar) override
            {
                (void)showInTaskbar;
            }

            void SetWindowStartupLocation(
                Launcher::SettingsWindowStartupLocation location) override
            {
                (void)location;
                _window._center = true;
            }

            void SetContent(const std::shared_ptr<Launcher::SettingsView>& view) override
            {
                (void)view;
                _content = SettingsHost::Root();
            }

            void BaseOnOpened(Launcher::SettingsWindowOpenedEventArgs& e) override
            {
                (void)e;
            }

            // --- the map picker dialog
            void SetTitle(std::u16string_view title) override
            {
                _window.Window().Title(Utf8(title));
            }

            void SetRequestedThemeVariant(
                Launcher::MapPickerThemeVariant variant) override
            {
                (void)variant;
            }

            void SetWindowStartupLocation(
                Launcher::MapPickerWindowStartupLocation location) override
            {
                (void)location;
                _window._center = true;
            }

            void SetContent(Launcher::MapPickerView& view) override
            {
                (void)view;
                _content = MapPickerHost::Root();
            }

            // --- showing one
            void ShowDialog(Launcher::PauseMenuWindowAdapter& owner,
                const std::shared_ptr<Launcher::SettingsWindow>& window,
                Launcher::PauseMenuWindowDialogCompletion completion) override
            {
                (void)owner;
                (void)window;
                Run(std::move(completion));
            }

            void ShowDialog(Launcher::PauseMenuWindowAdapter& owner,
                const std::shared_ptr<Launcher::MapPickerWindow>& window,
                Launcher::PauseMenuWindowDialogCompletion completion) override
            {
                (void)owner;
                (void)window;
                Run(std::move(completion));
            }

        private:
            void Run(Launcher::PauseMenuWindowDialogCompletion completion)
            {
                _window.Open(_content);
                // A modal dialog is a nested frame, as it is on the managed
                // side: the call returns when the window closes.
                bool open = true;
                _window.Window().Closed([&open]() { open = false; });
                Toolkit::Dispatcher::Instance().PushFrame(
                    [&open]() { return open; });
                completion.Invoke();
            }

            ScreenWindow _window;
            Toolkit::ElementPtr _content;
            Launcher::PauseMenuWindowPixelPoint _position{};
        };

        // The window is the view's adapter on the managed side, where one
        // class is both. Here the screen is its own object and the window
        // forwards to it, because PauseMenuWindowAdapter already carries a
        // PauseMenuViewAdapter base of its own.
        class PauseWindowHost final : public Launcher::PauseMenuWindowAdapter
        {
        public:
            PauseWindowHost()
                : _screen(std::make_shared<PauseMenuHost>())
            {
                _screen->MakeRoot(_screen);
            }

        [[nodiscard]] Launcher::PauseMenuViewControlHandle ConstructStackPanel() override
        {
            return _screen->ConstructStackPanel();
        }

        void SetStackPanelSpacing(const Launcher::PauseMenuViewControlHandle& panel,
            double spacing) override
        {
            _screen->SetStackPanelSpacing(panel, spacing);
        }

        [[nodiscard]] Launcher::PauseMenuViewControlRef<Launcher::Caption> ConstructCaption(std::optional<std::u16string> text) override
        {
            return _screen->ConstructCaption(text);
        }

        [[nodiscard]] Launcher::PauseMenuViewControlRef<Launcher::MenuEntry> ConstructMenuEntry(std::optional<std::u16string> title,
            std::optional<std::u16string> subtitle, double titleSize) override
        {
            return _screen->ConstructMenuEntry(title, subtitle, titleSize);
        }

        void AddPanelChild(const Launcher::PauseMenuViewControlHandle& panel,
            const Launcher::PauseMenuViewControlHandle& child) override
        {
            _screen->AddPanelChild(panel, child);
        }

        [[nodiscard]] double GetControlHeight(const Launcher::PauseMenuViewControlHandle& control)const override
        {
            return _screen->GetControlHeight(control);
        }

        [[nodiscard]] Launcher::PauseMenuViewControlHandle ConstructBorder() override
        {
            return _screen->ConstructBorder();
        }

        void SetBorderBackground(const Launcher::PauseMenuViewControlHandle& border,
            const Launcher::GuiBrush& brush) override
        {
            _screen->SetBorderBackground(border, brush);
        }

        void SetBorderBrush(const Launcher::PauseMenuViewControlHandle& border,
            const Launcher::GuiBrush& brush) override
        {
            _screen->SetBorderBrush(border, brush);
        }

        void SetBorderThickness(const Launcher::PauseMenuViewControlHandle& border,
            Launcher::PauseMenuViewThickness thickness) override
        {
            _screen->SetBorderThickness(border, thickness);
        }

        void SetBorderPadding(const Launcher::PauseMenuViewControlHandle& border,
            Launcher::PauseMenuViewThickness padding) override
        {
            _screen->SetBorderPadding(border, padding);
        }

        void SetBorderChild(const Launcher::PauseMenuViewControlHandle& border,
            const Launcher::PauseMenuViewControlHandle& child) override
        {
            _screen->SetBorderChild(border, child);
        }

        void SetBorderCornerRadius(const Launcher::PauseMenuViewControlHandle& border,
            double radius) override
        {
            _screen->SetBorderCornerRadius(border, radius);
        }

        void SetControlMaxWidth(const Launcher::PauseMenuViewControlHandle& control,
            double maxWidth) override
        {
            _screen->SetControlMaxWidth(control, maxWidth);
        }

        void SetControlHorizontalAlignment(const Launcher::PauseMenuViewControlHandle& control,
            Launcher::PauseMenuViewHorizontalAlignment alignment) override
        {
            _screen->SetControlHorizontalAlignment(control, alignment);
        }

        void SetControlVerticalAlignment(const Launcher::PauseMenuViewControlHandle& control,
            Launcher::PauseMenuViewVerticalAlignment alignment) override
        {
            _screen->SetControlVerticalAlignment(control, alignment);
        }

        [[nodiscard]] Launcher::PauseMenuViewControlHandle ConstructLayoutTransformControl() override
        {
            return _screen->ConstructLayoutTransformControl();
        }

        void SetLayoutTransformChild(const Launcher::PauseMenuViewControlHandle& control,
            const Launcher::PauseMenuViewControlHandle& child) override
        {
            _screen->SetLayoutTransformChild(control, child);
        }

        [[nodiscard]] std::optional<double> GetLayoutScaleY(const Launcher::PauseMenuViewControlHandle& control)const override
        {
            return _screen->GetLayoutScaleY(control);
        }

        void SetLayoutTransform(const Launcher::PauseMenuViewControlHandle& control,
            std::optional<Launcher::PauseMenuViewScaleTransform> transform) override
        {
            _screen->SetLayoutTransform(control, transform);
        }

        [[nodiscard]] Launcher::PauseMenuViewControlHandle ConstructScrollViewer() override
        {
            return _screen->ConstructScrollViewer();
        }

        void SetScrollViewerContent(const Launcher::PauseMenuViewControlHandle& viewer,
            const Launcher::PauseMenuViewControlHandle& content) override
        {
            _screen->SetScrollViewerContent(viewer, content);
        }

        void SetScrollViewerPadding(const Launcher::PauseMenuViewControlHandle& viewer,
            Launcher::PauseMenuViewThickness padding) override
        {
            _screen->SetScrollViewerPadding(viewer, padding);
        }

        void SetHorizontalScrollBarVisibility(const Launcher::PauseMenuViewControlHandle& viewer,
            Launcher::PauseMenuViewScrollBarVisibility visibility) override
        {
            _screen->SetHorizontalScrollBarVisibility(viewer, visibility);
        }

        void SetVerticalScrollBarVisibility(const Launcher::PauseMenuViewControlHandle& viewer,
            Launcher::PauseMenuViewScrollBarVisibility visibility) override
        {
            _screen->SetVerticalScrollBarVisibility(viewer, visibility);
        }

        void AddSizeChanged(const Launcher::PauseMenuViewControlHandle& control,
            Launcher::PauseMenuViewSizeChangedHandler handler) override
        {
            _screen->AddSizeChanged(control, handler);
        }

        void SetContent(const Launcher::PauseMenuViewControlHandle& content) override
        {
            _screen->SetContent(content);
        }

        void Focus(const Launcher::PauseMenuViewControlHandle& control) override
        {
            _screen->Focus(control);
        }

        void PostUiThread(Launcher::PauseMenuViewAction action,
            Launcher::PauseMenuViewDispatcherPriority priority) override
        {
            _screen->PostUiThread(action, priority);
        }

            void BindWindow(
                const std::shared_ptr<Launcher::PauseMenuWindow>& window) override
            {
                _owner = window;
            }

            void Show() override
            {
                _window.Open(_screen->Root());
                _window.Window().KeyDown(
                    [this](std::int32_t code, bool& handled)
                    {
                        Launcher::PauseMenuWindowKeyEventArgs e;
                        e.Key = code == 256 ? Launcher::PauseMenuWindowKey::Escape
                                            : Launcher::PauseMenuWindowKey::Other;
                        if (_owner != nullptr)
                        {
                            DispatchKeyDown(*_owner, e);
                        }
                        handled = e.Handled;
                    });
                _window.Window().Closed(
                    [this]()
                    {
                        Launcher::PauseMenuWindowEventArgs e;
                        if (_owner != nullptr)
                        {
                            DispatchClosed(*_owner, e);
                        }
                    });
                Launcher::PauseMenuWindowEventArgs opened;
                if (_owner != nullptr)
                {
                    DispatchOpened(*_owner, opened);
                }
            }

            void Activate() override {}

            void Close() override { _window.Window().Close(); }

            void SetTitle(std::string title) override
            {
                _window.Window().Title(title);
            }

            void SetIcon(const std::optional<Launcher::GuiWindowIcon>& icon) override
            {
                (void)icon;
            }

            void SetCanResize(bool canResize) override { (void)canResize; }

            void SetSystemDecorations(
                Launcher::PauseMenuWindowSystemDecorations decorations) override
            {
                _window._decorated
                    = decorations != Launcher::PauseMenuWindowSystemDecorations::None;
            }

            void SetTransparencyLevelHint(
                std::span<const Launcher::PauseMenuWindowTransparencyLevel> levels)
                override
            {
                (void)levels;
            }

            void SetBackground(const Launcher::GuiBrush& brush) override
            {
                _window.Window().Background(ToColor(brush));
            }

            void SetRequestedThemeVariant(
                Launcher::PauseMenuWindowThemeVariant variant) override
            {
                (void)variant;
            }

            [[nodiscard]] bool Topmost() const override { return _topmost; }

            void SetTopmost(bool topmost) override
            {
                _topmost = topmost;
                _window._topmost = topmost;
            }

            void SetShowInTaskbar(bool showInTaskbar) override
            {
                (void)showInTaskbar;
            }

            void SetWindowContent(
                const std::shared_ptr<Launcher::PauseMenuView>& view) override
            {
                // The view built itself into this adapter's own root already.
                (void)view;
            }

            // --- the cover target
            void SetWindowStartupLocation(
                Launcher::PauseMenuWindowStartupLocation location) override
            {
                _window._center
                    = location == Launcher::PauseMenuWindowStartupLocation::CenterScreen;
            }

            [[nodiscard]] Launcher::PauseMenuWindowPixelPoint Position() const override
            {
                return _position;
            }

            void SetPosition(Launcher::PauseMenuWindowPixelPoint position) override
            {
                _position = position;
                _window.Place(position.X, position.Y);
            }

            [[nodiscard]] double Width() const override { return _window._width; }
            [[nodiscard]] double Height() const override { return _window._height; }
            void SetWidth(double width) override { _window._width = width; }
            void SetHeight(double height) override { _window._height = height; }

            [[nodiscard]] std::optional<double> ScreenScalingFromPoint(
                Launcher::PauseMenuWindowPixelPoint point) const override
            {
                (void)point;
                return 1.0;
            }

            [[nodiscard]] double RenderScaling() const override { return 1.0; }

            std::shared_ptr<Launcher::PauseMenuWindowChildAdapter>
                CreateChildWindowAdapter() override
            {
                return std::make_shared<ChildWindowHost>();
            }

            std::shared_ptr<Launcher::SettingsViewAdapter>
                CreateSettingsViewAdapter() override
            {
                auto host = std::make_shared<SettingsHost>();
                host->MakeRoot(host);
                return std::shared_ptr<Launcher::SettingsViewAdapter>(host, host.get());
            }

            std::shared_ptr<Launcher::MapPickerViewAdapter>
                CreateMapPickerViewAdapter() override
            {
                auto host = std::make_shared<MapPickerHost>();
                host->MakeRoot(host);
                return std::shared_ptr<Launcher::MapPickerViewAdapter>(host, host.get());
            }

            void BaseOnOpened(Launcher::PauseMenuWindowEventArgs& e) override
            {
                (void)e;
            }

            void BaseOnKeyDown(Launcher::PauseMenuWindowKeyEventArgs& e) override
            {
                (void)e;
            }

            void BaseOnClosed(Launcher::PauseMenuWindowEventArgs& e) override
            {
                (void)e;
            }

            void PostAsyncVoidException(std::exception_ptr error) noexcept override
            {
                Toolkit::Dispatcher::Instance().Post(
                    [error]()
                    {
                        if (error != nullptr)
                        {
                            std::rethrow_exception(error);
                        }
                    });
            }

        private:
            std::shared_ptr<PauseMenuHost> _screen;
            ScreenWindow _window;
            std::shared_ptr<Launcher::PauseMenuWindow> _owner;
            Launcher::PauseMenuWindowPixelPoint _position{};
            bool _topmost = false;
        };
    }
}

namespace MphRead::Mods::Launcher::Gui::Detail
{
    std::shared_ptr<PauseMenuWindowAdapter> CreatePauseMenuWindowAdapter()
    {
        return std::make_shared<
            ::MphRead::NativeRuntime::Avalonia::PauseWindowHost>();
    }
}
