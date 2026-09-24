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
        // The window itself is made when it is first shown, because whether it
        // has a frame and whether the desktop shows through it are fixed when
        // the surface is created and are not known until then.
        class ScreenWindow
        {
        public:
            void Open(const Toolkit::ElementPtr& content)
            {
                if (_window == nullptr)
                {
                    _window = std::make_unique<Toolkit::Window>(Toolkit::WindowOptions{
                        _decorated, _transparent, _topmost, _showInTaskbar});
                    _window->Title(_title);
                    _window->Background(_background);
                }
                _window->Content(content);
                _window->ClientSize(_width, _height);
                _window->MinimumSize(_minWidth, _minHeight);
                if (_center)
                {
                    _window->CenterOnScreen();
                }
                else if (_placed)
                {
                    _window->Position(_x, _y);
                }
                _window->Show();
            }

            void Place(std::int32_t x, std::int32_t y)
            {
                _x = x;
                _y = y;
                _placed = true;
                _center = false;
                if (_window != nullptr)
                {
                    _window->Position(x, y);
                }
            }

            void Title(std::string title)
            {
                _title = std::move(title);
                if (_window != nullptr)
                {
                    _window->Title(_title);
                }
            }

            void Background(Toolkit::Color color)
            {
                _background = color;
                if (_window != nullptr)
                {
                    _window->Background(color);
                }
            }

            void Close()
            {
                if (_window != nullptr)
                {
                    _window->Close();
                }
            }

            // Avalonia applies Topmost to a window that is already up; the
            // pause menu steps out of the topmost band while its settings are
            // open, and back in when they close.
            void Topmost(bool topmost)
            {
                _topmost = topmost;
                if (_window != nullptr)
                {
                    _window->Topmost(topmost);
                }
            }

            // Null until the window has been shown once.
            [[nodiscard]] Toolkit::Window* Handle() noexcept { return _window.get(); }

            double _width = 720.0;
            double _height = 520.0;
            double _minWidth = 0.0;
            double _minHeight = 0.0;
            bool _center = false;
            bool _decorated = true;
            bool _topmost = false;
            bool _transparent = false;
            bool _showInTaskbar = true;
            bool _placed = false;
            std::int32_t _x = 0;
            std::int32_t _y = 0;

        private:
            std::unique_ptr<Toolkit::Window> _window;
            std::string _title;
            Toolkit::Color _background = Toolkit::Color::FromArgb(0xFF101014);
        };

        // The two dialogs the pause menu opens, in one window as the adapter
        // they share is one object.
        //
        // The screen a dialog shows is not built into this object: the pause
        // menu builds it into the view adapter it asked the owner for first
        // (CreateSettingsViewAdapter, CreateMapPickerViewAdapter), and that is
        // the root this window has to show. The two bases here stay empty.
        class ChildWindowHost final : public Launcher::PauseMenuWindowChildAdapter,
                                      public SettingsHost,
                                      public MapPickerHost
        {
        public:
            ChildWindowHost(std::shared_ptr<SettingsHost> settings,
                std::shared_ptr<MapPickerHost> mapPicker, Toolkit::Window* owner)
                : _settings(std::move(settings)),
                  _mapPicker(std::move(mapPicker)),
                  _owner(owner)
            {
                // Both screens are built into this one window; whichever is
                // shown first makes its own root.
                SettingsHost::MakeRoot(std::shared_ptr<void>(this, [](void*) {}));
                MapPickerHost::MakeRoot(std::shared_ptr<void>(this, [](void*) {}));
            }

            // --- the window itself
            void Close() override { _window.Close(); }

            void SetIcon(const std::optional<Launcher::GuiWindowIcon>& icon) override
            {
                (void)icon;
            }

            void SetBackground(const Launcher::GuiBrush& brush) override
            {
                _window.Background(ToColor(brush));
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
                _window.Title(title);
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
                _window._decorated
                    = decorations != Launcher::SettingsWindowSystemDecorations::None;
            }

            void SetTopmost(bool topmost) override { _window.Topmost(topmost); }

            void SetShowInTaskbar(bool showInTaskbar) override
            {
                _window._showInTaskbar = showInTaskbar;
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
                _content = _settings != nullptr ? _settings->Root() : SettingsHost::Root();
            }

            void BaseOnOpened(Launcher::SettingsWindowOpenedEventArgs& e) override
            {
                (void)e;
            }

            // --- the map picker dialog
            void SetTitle(std::u16string_view title) override
            {
                _window.Title(Utf8(title));
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
                _content = _mapPicker != nullptr ? _mapPicker->Root() : MapPickerHost::Root();
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

            ~ChildWindowHost() override
            {
                *_alive = false;
            }

        private:
            // `await window.ShowDialog(this)`: the window opens and the caller
            // goes back to its frame; what follows the await runs once the
            // window has closed. The game's own frame keeps pumping this
            // window through PauseMenu.Poll, as it pumps the menu -- a nested
            // frame here would stop the match from being drawn at all, which
            // is a black game window behind the dialog.
            void Run(Launcher::PauseMenuWindowDialogCompletion completion)
            {
                _window.Open(_content);
                auto pending = std::make_shared<Launcher::PauseMenuWindowDialogCompletion>(
                    std::move(completion));
                _window.Handle()->Closed(
                    [pending]()
                    {
                        // Posted, as an await continuation is: the window that
                        // is closing, and the dialog object that closed it,
                        // are still on the stack here and the continuation
                        // releases both.
                        Toolkit::Dispatcher::Instance().Post(
                            [pending]()
                            {
                                const Launcher::PauseMenuWindowDialogCompletion done
                                    = std::exchange(*pending, {});
                                done.Invoke();
                            });
                    });
                // A dialog is owned: it goes when the window it was opened
                // over goes.
                if (_owner != nullptr)
                {
                    const std::weak_ptr<bool> alive = _alive;
                    _owner->Closed(
                        [this, alive]()
                        {
                            if (!alive.expired())
                            {
                                _window.Close();
                            }
                        });
                }
            }

            ScreenWindow _window;
            Toolkit::ElementPtr _content;
            Launcher::PauseMenuWindowPixelPoint _position{};
            std::shared_ptr<SettingsHost> _settings;
            std::shared_ptr<MapPickerHost> _mapPicker;
            Toolkit::Window* _owner = nullptr;
            std::shared_ptr<bool> _alive = std::make_shared<bool>(true);
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
                _window.Handle()->KeyDown(
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
                _window.Handle()->Closed(
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

            void Close() override { _window.Close(); }

            void SetTitle(std::string title) override
            {
                _window.Title(title);
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
                for (const Launcher::PauseMenuWindowTransparencyLevel level : levels)
                {
                    if (level == Launcher::PauseMenuWindowTransparencyLevel::Transparent)
                    {
                        _window._transparent = true;
                        break;
                    }
                }
            }

            void SetBackground(const Launcher::GuiBrush& brush) override
            {
                _window.Background(ToColor(brush));
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
                _window.Topmost(topmost);
            }

            void SetShowInTaskbar(bool showInTaskbar) override
            {
                _window._showInTaskbar = showInTaskbar;
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

            // The dialog window shows the screen the view adapter made just
            // before it was asked for: PauseMenuWindow builds the view first
            // and opens the window over it second.
            std::shared_ptr<Launcher::PauseMenuWindowChildAdapter>
                CreateChildWindowAdapter() override
            {
                return std::make_shared<ChildWindowHost>(
                    std::exchange(_settingsHost, nullptr),
                    std::exchange(_mapPickerHost, nullptr),
                    _window.Handle());
            }

            std::shared_ptr<Launcher::SettingsViewAdapter>
                CreateSettingsViewAdapter() override
            {
                auto host = std::make_shared<SettingsHost>();
                host->MakeRoot(host);
                _settingsHost = host;
                return std::shared_ptr<Launcher::SettingsViewAdapter>(host, host.get());
            }

            std::shared_ptr<Launcher::MapPickerViewAdapter>
                CreateMapPickerViewAdapter() override
            {
                auto host = std::make_shared<MapPickerHost>();
                host->MakeRoot(host);
                _mapPickerHost = host;
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

            void ReleaseAfterDispatch(std::shared_ptr<void> keepAlive) noexcept override
            {
                try
                {
                    Toolkit::Dispatcher::Instance().Post(
                        [keepAlive = std::move(keepAlive)]() mutable { keepAlive.reset(); });
                }
                catch (...)
                {
                }
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
            std::shared_ptr<SettingsHost> _settingsHost;
            std::shared_ptr<MapPickerHost> _mapPickerHost;
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
