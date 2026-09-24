#include "HostViews.hpp"

#include "HostScreens.hpp"

#include "HostInputRows.hpp"
#include "HostRows.hpp"
#include "HostTask.hpp"

#include "../Gui/Host.hpp"
#include "../Gui/Text.hpp"
#include "../Stb/Image.hpp"

#include "../../Mods/Launcher/Gui/DemoPickerView.hpp"
#include "../../Mods/Launcher/Gui/MapPickerView.hpp"
#include "../../Mods/Launcher/Gui/PauseMenuView.hpp"
#include "../../Mods/Launcher/Gui/SettingsView.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

namespace MphRead::NativeRuntime::Avalonia
{
    // --- the screens, as the launcher asks for them -----------------------

    Toolkit::ElementPtr NewSettingsView(
        const std::shared_ptr<MphRead::MenuSettings>& settings, bool inGame)
    {
        return SettingsHost::Create(settings, inGame);
    }

    void SettingsViewClosed(
        const Toolkit::ElementPtr& view, std::function<void()> action)
    {
        auto subscriber = std::make_shared<Subscriber>();
        subscriber->Action = std::move(action);
        HostFor<SettingsHost>(view)->View().AddClosed(
            Launcher::SettingsViewEventHandler(subscriber,
                [](void* target, void*, const Launcher::SettingsViewEventArgs&)
                { static_cast<Subscriber*>(target)->Action(); }));
    }

    void SettingsViewGameFilesRequested(
        const Toolkit::ElementPtr& view, std::function<void()> action)
    {
        auto subscriber = std::make_shared<Subscriber>();
        subscriber->Action = std::move(action);
        HostFor<SettingsHost>(view)->View().AddGameFilesRequested(
            Launcher::SettingsViewEventHandler(subscriber,
                [](void* target, void*, const Launcher::SettingsViewEventArgs&)
                { static_cast<Subscriber*>(target)->Action(); }));
    }

    Toolkit::ElementPtr NewMapPickerView(
        const std::vector<std::string>& rooms, std::string current)
    {
        return MapPickerHost::Create(rooms, std::move(current));
    }

    void MapPickerClosed(const Toolkit::ElementPtr& view, std::function<void()> action)
    {
        auto subscriber = std::make_shared<Subscriber>();
        subscriber->Action = std::move(action);
        HostFor<MapPickerHost>(view)->View().AddClosed(
            Launcher::MapPickerEventHandler(subscriber.get(),
                [](void* context, void*, const Launcher::MapPickerEventArgs&)
                { static_cast<Subscriber*>(context)->Action(); },
                subscriber));
    }

    std::optional<std::string> MapPickerRoomKey(const Toolkit::ElementPtr& view)
    {
        const Launcher::MapPickerStringRef key
            = HostFor<MapPickerHost>(view)->View().RoomKey();
        return key == nullptr ? std::nullopt : std::optional<std::string>(*key);
    }

    Toolkit::ElementPtr NewDemoPickerView(
        std::shared_ptr<const std::vector<MphRead::Mods::Network::DemoRecording>> demos,
        std::string directory)
    {
        return DemoPickerHost::Create(std::move(demos), std::move(directory));
    }

    void DemoPickerClosed(const Toolkit::ElementPtr& view, std::function<void()> action)
    {
        auto subscriber = std::make_shared<Subscriber>();
        subscriber->Action = std::move(action);
        HostFor<DemoPickerHost>(view)->View().AddClosed(
            Launcher::DemoPickerViewEventHandler(subscriber,
                [](void* target, void*, const Launcher::DemoPickerViewEventArgs&)
                { static_cast<Subscriber*>(target)->Action(); }));
    }

    std::optional<std::string> DemoPickerPath(const Toolkit::ElementPtr& view)
    {
        const std::shared_ptr<const std::string> path
            = HostFor<DemoPickerHost>(view)->View().Path();
        return path == nullptr ? std::nullopt : std::optional<std::string>(*path);
    }

    bool DemoPickerImportRequested(const Toolkit::ElementPtr& view)
    {
        return HostFor<DemoPickerHost>(view)->View().ImportRequested();
    }

    Toolkit::ElementPtr NewPauseMenuView(bool offerWindowMode)
    {
        return PauseMenuHost::Create(offerWindowMode);
    }

    namespace
    {
        // The pause menu's events all have the same shape.
        template <typename Add>
        void PauseSubscribe(const Toolkit::ElementPtr& view, Add add,
            std::function<void()> action)
        {
            auto subscriber = std::make_shared<Subscriber>();
            subscriber->Action = std::move(action);
            (HostFor<PauseMenuHost>(view)->View().*add)(
                Launcher::PauseMenuViewEventHandler(subscriber.get(),
                    [](void* context, void*, const Launcher::PauseMenuViewEventArgs&)
                    { static_cast<Subscriber*>(context)->Action(); },
                    subscriber));
        }
    }

    void PauseResumed(const Toolkit::ElementPtr& view, std::function<void()> action)
    {
        PauseSubscribe(view, &Launcher::PauseMenuView::AddResumed, std::move(action));
    }

    void PauseSettingsRequested(
        const Toolkit::ElementPtr& view, std::function<void()> action)
    {
        PauseSubscribe(
            view, &Launcher::PauseMenuView::AddSettingsRequested, std::move(action));
    }

    void PauseLeaveRequested(
        const Toolkit::ElementPtr& view, std::function<void()> action)
    {
        PauseSubscribe(
            view, &Launcher::PauseMenuView::AddLeaveRequested, std::move(action));
    }

    void PauseQuitRequested(
        const Toolkit::ElementPtr& view, std::function<void()> action)
    {
        PauseSubscribe(
            view, &Launcher::PauseMenuView::AddQuitRequested, std::move(action));
    }

    void PauseSpectateRequested(
        const Toolkit::ElementPtr& view, std::function<void()> action)
    {
        PauseSubscribe(
            view, &Launcher::PauseMenuView::AddSpectateRequested, std::move(action));
    }

    void PauseRejoinRequested(
        const Toolkit::ElementPtr& view, std::function<void()> action)
    {
        PauseSubscribe(
            view, &Launcher::PauseMenuView::AddRejoinRequested, std::move(action));
    }

    void PauseRecordToggleRequested(
        const Toolkit::ElementPtr& view, std::function<void()> action)
    {
        PauseSubscribe(view, &Launcher::PauseMenuView::AddRecordToggleRequested,
            std::move(action));
    }

    void PauseFocusResume(const Toolkit::ElementPtr& view)
    {
        HostFor<PauseMenuHost>(view)->View().FocusResume();
    }
}
