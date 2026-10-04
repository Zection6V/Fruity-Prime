#include "AndroidQuick.hpp"
#include "MainActivity.hpp"
#include "../MphRead.Native.Qt/Shell/QmlTypes.hpp"
#include "../MphRead.Native.Qt/Shell/ShellBridge.hpp"
#include "../MphRead.Native.Qt/Shell/FocusNav.hpp"
#include "../MphRead.Native/Mods/Input/GamepadManager.hpp"
#include "../MphRead.Native/Mods/Input/GamepadUiRouter.hpp"
#include "../MphRead.Native/NativeRuntime/System/Runtime.hpp"
#include "../MphRead.Native/GameState.hpp"
#include "../MphRead.Native/Mods/GameSettings.hpp"
#include "../MphRead.Native/Mods/InputSettings.hpp"
#include "../MphRead.Native/Mods/ThumbnailGenerator.hpp"
#include "../MphRead.Native/Mods/Launcher/Portable/GameFiles.hpp"
#include "../MphRead.Native/Mods/Launcher/Portable/LauncherPrefs.hpp"

#include <QtCore/QMetaObject>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtQuick/QQuickWindow>

#include <memory>

namespace
{
    using Bridge = MphRead::Qt::ShellBridge;
    std::unique_ptr<Bridge> bridge;
    std::function<void()> resume;
    std::function<void()> leave;
    std::function<void()> quit;

    void OnQt(std::function<void()> action)
    {
        if (!QCoreApplication::instance()) throw std::runtime_error("Android Qt runtime is not ready");
        QMetaObject::invokeMethod(QCoreApplication::instance(), std::move(action), ::Qt::QueuedConnection);
    }
    void OnActivity(std::function<void(MphRead::Droid::MainActivity&)> action)
    {
        MphRead::Droid::GetMainActivityOwner().RunOnUiThread([action = std::move(action)]
        {
            if (auto* activity = MphRead::Droid::MainActivity::Instance()) action(*activity);
        });
    }
}

namespace MphRead::Droid
{
    void QuickFront()
    {
        OnQt([]
        {
            Mods::Launcher::LauncherPrefs::Load();
            Mods::InputSettings::Load();
            auto settings = GameState::LoadSettings();
            Mods::GameSettings::Apply(settings);
            bridge->SetSettings(std::move(settings));
            const bool ready = Mods::Launcher::GameFiles::Ready();
            if (ready) Mods::Launcher::GameFiles::ApplyPaths();
            bridge->SetRooms(ready ? Mods::ThumbnailGenerator::MultiplayerRooms() : std::vector<std::string>{}, ready);
            bridge->SetPage(QStringLiteral("front"));
        });
    }
    void QuickLobby() { OnQt([] { bridge->SetPage(QStringLiteral("front")); bridge->OpenLobby(); }); }
    void QuickPage(std::string page)
    {
        OnQt([page = std::move(page)] { bridge->SetPage(QString::fromStdString(page)); });
    }
    void QuickPause(std::function<void()> onResume, std::function<void()> onLeave, std::function<void()> onQuit)
    {
        OnQt([onResume = std::move(onResume), onLeave = std::move(onLeave), onQuit = std::move(onQuit)]
        {
            resume = onResume; leave = onLeave; quit = onQuit;
            bridge->SetPage(QStringLiteral("pause"));
        });
    }
    void QuickGoBack() { OnQt([] { emit bridge->backRequested(); }); }
    void QuickSuspendBackdrop(bool value) { OnQt([value] { bridge->SetBackdropSuspended(value); }); }
}

// QtQuickView starts Qt's GUI thread and calls this entry point. It owns the
// QML engine and the Android View, while the existing Activity owns the match.
int main(int argc, char** argv)
{
    // Composite the menus in the Activity's view tree above the game's
    // SurfaceView, including transparent pause/results pages.
    qputenv("QT_ANDROID_SURFACE_CONTAINER_TYPE", "1");
    QGuiApplication application(argc, argv);
    MphRead::Qt::RegisterQmlTypes();
    Bridge::Actions actions;
    actions.Launch = [](Bridge::LaunchPlan plan) { OnActivity([plan](auto& activity) { activity.StartMatch(plan); }); };
    actions.Quit = [] { OnActivity([](auto& activity) { activity.Finish(); }); };
    actions.Resume = [] { OnActivity([callback = resume](auto& activity) { if (callback) callback(); else activity.OnBackPressed(); }); };
    actions.Leave = [] { OnActivity([callback = leave](auto& activity) { if (callback) callback(); else activity.EndMatch(); }); };
    actions.QuitMatch = [] { OnActivity([callback = quit](auto& activity) { if (callback) callback(); else activity.Finish(); }); };
    actions.GameFilesChanged = [] { MphRead::Droid::QuickFront(); };
    bridge = std::make_unique<Bridge>(std::move(actions));
    namespace Input = MphRead::Mods::Input;
    Input::GamepadUiRouter router;
    router.Action.Add([](Input::UiAction action)
    {
        QQuickWindow* window = nullptr;
        for (QWindow* candidate : QGuiApplication::allWindows())
            if (auto* quick = qobject_cast<QQuickWindow*>(candidate); quick && quick->isVisible()) { window = quick; break; }
        if (!window) return;
        using Action = Input::UiAction;
        int key = 0;
        switch (action)
        {
        case Action::PreviousTab: bridge->StepTabs(-1); return;
        case Action::NextTab: bridge->StepTabs(1); return;
        case Action::PageUp: MphRead::Qt::FocusNav::Page(*window, false); return;
        case Action::PageDown: MphRead::Qt::FocusNav::Page(*window, true); return;
        case Action::Up: key = ::Qt::Key_Up; break;
        case Action::Down: key = ::Qt::Key_Down; break;
        case Action::Left: key = ::Qt::Key_Left; break;
        case Action::Right: key = ::Qt::Key_Right; break;
        case Action::Back: key = ::Qt::Key_Escape; break;
        case Action::Accept:
            if (MphRead::Qt::FocusNav::PadAccept(*window)) { bridge->KeyboardDriving(); return; }
            key = ::Qt::Key_Return; break;
        }
        QKeyEvent press(QEvent::KeyPress, key, ::Qt::NoModifier);
        QCoreApplication::sendEvent(window, &press);
        if (!press.isAccepted()) MphRead::Qt::FocusNav::Unhandled(*window, key);
        QKeyEvent release(QEvent::KeyRelease, key, ::Qt::NoModifier);
        QCoreApplication::sendEvent(window, &release);
        bridge->KeyboardDriving();
    });
    QTimer input;
    QObject::connect(&input, &QTimer::timeout, [&]
    {
        if (!bridge->Showing() || !Input::GamepadContexts::Focused()) { router.Reset(); return; }
        router.Update(Input::GamepadManager::Snapshot(), Input::GamepadContexts::Capturing()
            ? Input::GamepadContext::BindingCapture : Input::GamepadContext::Menu,
            MphRead::NativeRuntime::EnvironmentTickCount64());
    });
    input.start(16);
    const int result = application.exec();
    bridge.reset();
    return result;
}
