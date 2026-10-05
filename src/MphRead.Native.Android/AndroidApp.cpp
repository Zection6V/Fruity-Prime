#include "AndroidApp.hpp"
#include "AndroidQuick.hpp"
#include "../MphRead.Native/Mods/CrashReport.hpp"
namespace MphRead::Droid
{
    std::shared_ptr<AndroidLauncherPage> AndroidApp::Home()
    {
        static auto home = std::make_shared<AndroidLauncherPage>();
        return home;
    }
    void AndroidApp::Initialize() { MphRead::Mods::CrashReport::Install(); }
    void AndroidApp::OnFrameworkInitializationCompleted() { Home()->Reset(); }
    void AndroidApp::SuspendBackdrop(bool value) { QuickSuspendBackdrop(value); }
    bool AndroidLauncherPage::GoBack() { QuickGoBack(); return true; }
    void AndroidLauncherPage::SuspendLobby() { QuickPage(""); }
    void AndroidLauncherPage::ResumeLobby() { QuickLobby(); }
    void AndroidLauncherPage::Reset() { QuickFront(); }
    void AndroidLauncherPage::ShowPauseMenu(std::function<void()> resume, std::function<void()> leave, std::function<void()> quit)
    { QuickPause(std::move(resume), std::move(leave), std::move(quit)); }
}
