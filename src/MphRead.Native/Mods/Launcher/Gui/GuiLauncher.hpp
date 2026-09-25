#pragma once

#include "../../../NativeRuntime/System/Exceptions.hpp"
#include "HomeWindow.hpp"

#include <atomic>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace MphRead
{
    class MenuSettings;
}

namespace MphRead::Mods::Detail
{
    [[nodiscard]] bool PauseMenuGuiEnsureSetup();
    void PauseMenuGuiPump();
}

namespace MphRead::Mods::Launcher::Gui
{
    class LauncherApp;

    using GuiLauncherNullReferenceException = ::System::NullReferenceException;

    enum class GuiLauncherDispatcherPriority : unsigned char
    {
        Background
    };

    class GuiLauncherDispatcherFrame final
    {
    public:
        explicit GuiLauncherDispatcherFrame(bool exitWhenRequested = true) noexcept;

        [[nodiscard]] bool Continue() const noexcept;
        void Continue(bool value) noexcept;
        [[nodiscard]] bool ExitWhenRequested() const noexcept;

    private:
        bool _continue = true;
        bool _exitWhenRequested = true;
    };

    class GuiLauncherCallback final
    {
    public:
        using Function = void (*)(void* context);

        GuiLauncherCallback() = default;
        GuiLauncherCallback(void* context, Function function,
            std::shared_ptr<void> keepAlive = {});

        void Invoke() const;
        [[nodiscard]] explicit operator bool() const noexcept;

    private:
        void* _context = nullptr;
        Function _function = nullptr;
        std::shared_ptr<void> _keepAlive;
    };

    class GuiLauncherAppBuilder
    {
    public:
        virtual ~GuiLauncherAppBuilder() = default;

        virtual GuiLauncherAppBuilder& UsePlatformDetect() = 0;
        virtual GuiLauncherAppBuilder& WithInterFont() = 0;
        virtual void SetupWithoutStarting() = 0;
    };

    class GuiLauncherPlatform
    {
    public:
        virtual ~GuiLauncherPlatform() = default;

        // AppBuilder.Configure<LauncherApp>(). The returned builder owns the
        // unavoidable native binding for the fluent AppBuilder chain, retains
        // the application for the process lifetime after setup, and invokes
        // LauncherApp::Initialize at Avalonia's framework-defined point.
        [[nodiscard]] virtual std::shared_ptr<GuiLauncherAppBuilder>
            ConfigureLauncherApp() = 0;

        // LauncherApp.Initialize's three observable operations, in source order.
        virtual void AddFluentTheme(LauncherApp& application) = 0;
        virtual void SetRequestedThemeVariantDark(LauncherApp& application) = 0;
        virtual void BaseInitialize(LauncherApp& application) = 0;

        // Avalonia HomeWindow construction/event/show boundary.
        [[nodiscard]] virtual std::shared_ptr<HomeWindowAdapter>
            CreateHomeWindowAdapter() = 0;
        virtual void AddHomeWindowClosed(
            HomeWindowAdapter& window, GuiLauncherCallback handler) = 0;
        virtual void ShowHomeWindow(HomeWindowAdapter& window) = 0;

        // Dispatcher.UIThread operations.
        virtual void Post(GuiLauncherCallback callback,
            GuiLauncherDispatcherPriority priority) = 0;
        virtual void PushFrame(GuiLauncherDispatcherFrame& frame) = 0;

        // Managed Exception.Message and Exception.StackTrace boundary for
        // exceptions originating in the platform/toolkit layer.
        [[nodiscard]] virtual std::string ExceptionMessage(
            std::exception_ptr exception) = 0;
        [[nodiscard]] virtual std::optional<std::string> ExceptionStackTrace(
            std::exception_ptr exception) = 0;
    };

    class GuiLauncher final
    {
    public:
        GuiLauncher() = delete;

        [[nodiscard]] static bool TryRun();

        // C# internal: assembly-visible to PauseMenu and other native peers.
        [[nodiscard]] static bool EnsureSetup();
        static void Pump();

    private:
        static void SayWhyOnLinux();
        [[nodiscard]] static bool Probe();
        static void Run();
        [[nodiscard]] static LaunchPlan Ask(
            const std::shared_ptr<MphRead::MenuSettings>& settings,
            const std::vector<std::string>& rooms);

        static std::atomic_bool _setUp;
        static std::atomic_bool _failed;
    };

    class LauncherApp final
    {
    public:
        LauncherApp() = default;
        LauncherApp(const LauncherApp&) = delete;
        LauncherApp& operator=(const LauncherApp&) = delete;
        LauncherApp(LauncherApp&&) = delete;
        LauncherApp& operator=(LauncherApp&&) = delete;

        void Initialize(GuiLauncherPlatform& platform);
    };

    namespace Detail
    {
        // Avalonia/platform binding supplied by the native GUI host. This is
        // the unavoidable platform boundary; GuiLauncher owns all policy.
        [[nodiscard]] GuiLauncherPlatform& GuiLauncherPlatformInstance() noexcept;
    }
}
