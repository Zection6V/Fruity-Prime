#pragma once

#if !defined(__ANDROID__)
#error "AndroidApp is only valid for the Android native target."
#endif

#include "../MphRead.Native/Mods/Launcher/Gui/HomeView.hpp"

#include <memory>

namespace MphRead::Droid
{
    class AndroidApp;

    struct AndroidSingleViewLifetime final
    {
        std::shared_ptr<void> Native{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return Native != nullptr;
        }
    };

    // AndroidApp.cs is an Avalonia Application subclass owned by
    // AvaloniaMainActivity<AndroidApp>. The native pair owns the exact
    // application policy; the actual Avalonia/Application/MainActivity owner
    // remains an external platform seam rather than a substitute lifecycle.
    class AndroidAppOwner
    {
    public:
        virtual ~AndroidAppOwner() = default;

        // AndroidApp.Initialize(), in source order.
        virtual void AddFluentTheme(AndroidApp& application) = 0;
        virtual void SetRequestedThemeVariantDark(AndroidApp& application) = 0;
        virtual void BaseInitialize(AndroidApp& application) = 0;

        // AndroidApp.OnFrameworkInitializationCompleted(). The returned token
        // is the exact pattern-matched ISingleViewApplicationLifetime instance
        // (or empty when ApplicationLifetime is another type/null).
        [[nodiscard]] virtual AndroidSingleViewLifetime
            SingleViewApplicationLifetime(AndroidApp& application) = 0;

        // HomeView's unavoidable Avalonia control binding. The adapter must be
        // non-null and represents the native host for the same HomeView object.
        [[nodiscard]] virtual std::shared_ptr<
            MphRead::Mods::Launcher::Gui::HomeViewAdapter>
            CreateHomeViewAdapter() = 0;

        virtual void SetSingleViewMainView(
            const AndroidSingleViewLifetime& lifetime,
            MphRead::Mods::Launcher::Gui::HomeView& home,
            MphRead::Mods::Launcher::Gui::HomeViewAdapter& adapter) = 0;

        virtual void BaseOnFrameworkInitializationCompleted(
            AndroidApp& application) = 0;

        // Exact MainActivity.Instance?. calls used by the Done handler.
        virtual void FinishMainActivityIfPresent() = 0;
        virtual void StartMatchIfMainActivityPresent(
            const MphRead::Mods::Launcher::LaunchPlan& plan) = 0;
    };

    // Supplied by the real Avalonia/Android application owner. This pair does
    // not invent an Android Activity/Application lifecycle.
    [[nodiscard]] AndroidAppOwner& GetAndroidAppOwner() noexcept;

    class AndroidApp final
    {
    public:
        AndroidApp() = default;
        ~AndroidApp() = default;

        AndroidApp(const AndroidApp&) = delete;
        AndroidApp& operator=(const AndroidApp&) = delete;
        AndroidApp(AndroidApp&&) = delete;
        AndroidApp& operator=(AndroidApp&&) = delete;

        [[nodiscard]] static std::shared_ptr<
            MphRead::Mods::Launcher::Gui::HomeView> Home() noexcept;

        void Initialize();
        void OnFrameworkInitializationCompleted();

    private:
        [[nodiscard]] static std::shared_ptr<
            MphRead::Mods::Launcher::Gui::HomeView> BuildHome(
                AndroidAppOwner& owner,
                std::shared_ptr<
                    MphRead::Mods::Launcher::Gui::HomeViewAdapter>& adapter);

        static std::shared_ptr<
            MphRead::Mods::Launcher::Gui::HomeView> _home;
    };
}
