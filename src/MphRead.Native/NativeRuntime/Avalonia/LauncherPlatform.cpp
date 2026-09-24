// What Avalonia is to the launcher on the managed side: the application
// object, the window the home screen is shown in, and the dispatcher frames a
// modal wait is made of. GuiLauncher owns every decision; this only carries
// them to NativeRuntime/Gui.

#include "HomeViewHost.hpp"

#include "../Gui/Host.hpp"

#include "../../Mods/Branding.hpp"
#include "../../Mods/Launcher/Gui/GuiLauncher.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace MphRead::NativeRuntime::Avalonia
{
    namespace
    {
        // A window with no toolkit under it cannot be opened, and saying so is
        // what sends the launcher to its text screen.
        class NoToolkit final : public std::runtime_error
        {
        public:
            NoToolkit()
                : std::runtime_error("No windowing backend is available.")
            {
            }
        };

        class AppBuilder final : public Launcher::GuiLauncherAppBuilder
        {
        public:
            explicit AppBuilder(Launcher::GuiLauncherPlatform& platform) noexcept
                : _platform(platform)
            {
            }

            Launcher::GuiLauncherAppBuilder& UsePlatformDetect() override
            {
                return *this;
            }

            Launcher::GuiLauncherAppBuilder& WithInterFont() override
            {
                return *this;
            }

            void SetupWithoutStarting() override
            {
                if (!Toolkit::Window::Available())
                {
                    throw NoToolkit();
                }
                // The application outlives the call, as Avalonia's does.
                static Launcher::LauncherApp application;
                application.Initialize(_platform);
            }

        private:
            Launcher::GuiLauncherPlatform& _platform;
        };

        // GuiTheme's application icon, on a window.
        void ApplyAppIcon(Toolkit::Window& window)
        {
            const std::optional<Launcher::GuiWindowIcon>& icon
                = Launcher::GuiTheme::AppIcon.Value();
            if (!icon.has_value())
            {
                return;
            }
            const auto* const pixels
                = static_cast<const IconPixels*>(icon->Native.get());
            if (pixels == nullptr)
            {
                return;
            }
            window.Icon(pixels->Width, pixels->Height, pixels->Rgba.data());
        }

        class HomeWindowHost final : public Launcher::HomeWindowAdapter
        {
        public:
            HomeViewHandle ConstructHomeView(Launcher::HomeWindowMenuSettingsRef settings,
                Launcher::HomeWindowRoomsRef rooms) override
            {
                const auto* const names
                    = static_cast<const std::vector<std::string>*>(rooms.Native);
                // The settings object belongs to GuiLauncher for the length of
                // the window, so this reference does not own it.
                const std::shared_ptr<MphRead::MenuSettings> shared(
                    static_cast<MphRead::MenuSettings*>(settings.Native),
                    [](MphRead::MenuSettings*) {});
                _view = HomeViewHost::Create(shared,
                    names != nullptr ? *names : std::vector<std::string>{});
                _window.Content(_view);
                return _view.get();
            }

            void ConnectHomeViewDone(
                HomeViewHandle view, void* context, DoneHandler handler) override
            {
                (void)view;
                auto target = std::make_shared<Done>();
                target->Context = context;
                target->Handler = handler;
                _done = target;
                Host()->View().AddDone(Launcher::HomeViewEventHandler(target,
                    [](void* self, void*, ::MphRead::Mods::Launcher::LaunchPlan)
                    {
                        Done& done = *static_cast<Done*>(self);
                        if (done.Handler != nullptr)
                        {
                            done.Handler(done.Context);
                        }
                    }));
            }

            [[nodiscard]] ::MphRead::Mods::Launcher::LaunchPlan GetHomeViewPlan(
                HomeViewHandle view) const override
            {
                (void)view;
                return Host()->View().Plan();
            }

            void Close() override { _window.Close(); }

            void SetTitle(std::string_view title) override { _window.Title(title); }

            void SetIcon(Launcher::HomeWindowIcon icon) override
            {
                (void)icon;
                ApplyAppIcon(_window);
            }

            void SetWidth(double width) override { _width = width; }
            void SetHeight(double height) override { _height = height; }

            void SetMinWidth(double minWidth) override { _minWidth = minWidth; }
            void SetMinHeight(double minHeight) override { _minHeight = minHeight; }

            void SetWindowStartupLocation(
                Launcher::HomeWindowStartupLocation location) override
            {
                (void)location;
                _center = true;
            }

            void SetBackground(Launcher::HomeWindowBrush brush) override
            {
                (void)brush;
                _window.Background(ToColor(Launcher::GuiTheme::Panel));
            }

            void SetRequestedThemeVariant(
                Launcher::HomeWindowThemeVariant variant) override
            {
                // The launcher's colours are its own; there is one variant.
                (void)variant;
            }

            void SetContent(HomeViewHandle content) override
            {
                (void)content;
                _window.Content(_view);
            }

            void Show()
            {
                _window.ClientSize(_width, _height);
                _window.MinimumSize(_minWidth, _minHeight);
                if (_center)
                {
                    _window.CenterOnScreen();
                }
                // Escape goes back a screen, and closes the window at the top.
                _window.KeyDown(
                    [this](std::int32_t code, bool& handled)
                    {
                        Launcher::HomeViewKeyEventArgs e;
                        e.Key = code == 256 ? Launcher::HomeViewKey::Escape
                                            : Launcher::HomeViewKey::Other;
                        Host()->View().OnKeyDown(e);
                        handled = e.Handled;
                    });
                _window.Show();
            }

            void Closed(std::function<void()> handler)
            {
                _window.Closed(std::move(handler));
            }

        private:
            struct Done final
            {
                void* Context = nullptr;
                DoneHandler Handler = nullptr;
            };

            [[nodiscard]] HomeViewHost* Host() const
            {
                return static_cast<HomeViewHost*>(_view->Tag.get());
            }

            Toolkit::Window _window;
            Toolkit::ElementPtr _view;
            std::shared_ptr<Done> _done;
            double _width = 940.0;
            double _height = 560.0;
            double _minWidth = 780.0;
            double _minHeight = 480.0;
            bool _center = false;
        };

        class Platform final : public Launcher::GuiLauncherPlatform
        {
        public:
            std::shared_ptr<Launcher::GuiLauncherAppBuilder>
                ConfigureLauncherApp() override
            {
                return std::make_shared<AppBuilder>(*this);
            }

            void AddFluentTheme(Launcher::LauncherApp& application) override
            {
                // The launcher paints itself from GuiTheme, so there is no
                // style dictionary to add.
                (void)application;
            }

            void SetRequestedThemeVariantDark(
                Launcher::LauncherApp& application) override
            {
                (void)application;
            }

            void BaseInitialize(Launcher::LauncherApp& application) override
            {
                (void)application;
            }

            std::shared_ptr<Launcher::HomeWindowAdapter>
                CreateHomeWindowAdapter() override
            {
                return std::make_shared<HomeWindowHost>();
            }

            void AddHomeWindowClosed(Launcher::HomeWindowAdapter& window,
                Launcher::GuiLauncherCallback handler) override
            {
                static_cast<HomeWindowHost&>(window).Closed(
                    [handler]() { handler.Invoke(); });
            }

            void ShowHomeWindow(Launcher::HomeWindowAdapter& window) override
            {
                static_cast<HomeWindowHost&>(window).Show();
            }

            void Post(Launcher::GuiLauncherCallback callback,
                Launcher::GuiLauncherDispatcherPriority priority) override
            {
                (void)priority;
                Toolkit::Dispatcher::Instance().Post(
                    [callback]() { callback.Invoke(); },
                    Toolkit::Priority::Background);
            }

            void PushFrame(Launcher::GuiLauncherDispatcherFrame& frame) override
            {
                Toolkit::Dispatcher::Instance().PushFrame(
                    [&frame]() { return frame.Continue(); });
            }

            std::string ExceptionMessage(std::exception_ptr exception) override
            {
                if (exception == nullptr)
                {
                    return std::string();
                }
                try
                {
                    std::rethrow_exception(exception);
                }
                catch (const std::exception& failure)
                {
                    return failure.what();
                }
                catch (...)
                {
                    return "Exception of type 'System.Object' was thrown.";
                }
            }

            std::optional<std::string> ExceptionStackTrace(
                std::exception_ptr exception) override
            {
                // A C++ exception carries no stack, and saying so is better
                // than inventing one.
                (void)exception;
                return std::nullopt;
            }
        };
    }
}

namespace MphRead::Mods::Launcher::Gui::Detail
{
    GuiLauncherPlatform& GuiLauncherPlatformInstance() noexcept
    {
        static ::MphRead::NativeRuntime::Avalonia::Platform platform;
        return platform;
    }
}
