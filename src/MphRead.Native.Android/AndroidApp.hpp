#pragma once
#include <exception>
#include <functional>
#include <memory>
namespace MphRead::Droid
{
    using AndroidUnhandledExceptionHandler = std::function<void(std::exception_ptr)>;
    class AndroidLauncherPage final
    {
    public:
        bool GoBack();
        void SuspendLobby();
        void ResumeLobby();
        void Reset();
        void ShowPauseMenu(std::function<void()> resume, std::function<void()> leave, std::function<void()> quit);
    };
    class AndroidApp final
    {
    public:
        static std::shared_ptr<AndroidLauncherPage> Home();
        static void SuspendBackdrop(bool value);
        void Initialize();
        void OnFrameworkInitializationCompleted();
    };
}
