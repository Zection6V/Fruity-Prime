#include "AndroidApp.hpp"

#include "../MphRead.Native/GameState.hpp"
#include "../MphRead.Native/Mods/DebugLog.hpp"
#include "../MphRead.Native/Mods/GameSettings.hpp"
#include "../MphRead.Native/Mods/InputSettings.hpp"
#include "../MphRead.Native/Mods/ThumbnailGenerator.hpp"
#include "../MphRead.Native/Mods/Launcher/Portable/GameFiles.hpp"
#include "../MphRead.Native/Mods/Launcher/Portable/LauncherPrefs.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using MphRead::Mods::Launcher::Gui::HomeViewAdapter;
    using MphRead::Mods::Launcher::Gui::HomeViewRoomEnumerator;
    using MphRead::Mods::Launcher::Gui::HomeViewRoomList;

    class AndroidAppRoomEnumerator final : public HomeViewRoomEnumerator
    {
    public:
        explicit AndroidAppRoomEnumerator(
            const std::vector<std::string>& rooms) noexcept
            : _rooms(rooms)
        {
        }

        [[nodiscard]] bool MoveNext() override
        {
            if (_next >= _rooms.size())
            {
                _current = nullptr;
                return false;
            }
            _current = &_rooms[_next];
            ++_next;
            return true;
        }

        [[nodiscard]] std::string Current() const override
        {
            return *_current;
        }

        void Dispose() override
        {
        }

    private:
        const std::vector<std::string>& _rooms;
        std::size_t _next = 0;
        const std::string* _current = nullptr;
    };

    class AndroidAppRoomList final : public HomeViewRoomList
    {
    public:
        explicit AndroidAppRoomList(std::vector<std::string> rooms)
            : _rooms(std::move(rooms))
        {
        }

        [[nodiscard]] std::shared_ptr<HomeViewRoomEnumerator>
            GetEnumerator() const override
        {
            return std::make_shared<AndroidAppRoomEnumerator>(_rooms);
        }

    private:
        std::vector<std::string> _rooms;
    };

    struct AndroidAppDoneTarget final
    {
        // HomeView stores a reference to its adapter. The managed HomeView owns
        // its Avalonia control state intrinsically; retaining the adapter here
        // gives the native peer the same lifetime without adding policy.
        std::shared_ptr<HomeViewAdapter> Adapter{};
    };

    void OnHomeDone(
        void* target,
        void* sender,
        MphRead::Mods::Launcher::LaunchPlan plan)
    {
        (void)target;
        (void)sender;
        MphRead::Droid::AndroidAppOwner& owner =
            MphRead::Droid::GetAndroidAppOwner();
        if (plan.Kind() == MphRead::Mods::Launcher::LaunchKind::None)
        {
            owner.FinishMainActivityIfPresent();
            return;
        }
        owner.StartMatchIfMainActivityPresent(plan);
    }
}

namespace MphRead::Droid
{
    std::shared_ptr<MphRead::Mods::Launcher::Gui::HomeView>
        AndroidApp::_home{};

    std::shared_ptr<MphRead::Mods::Launcher::Gui::HomeView>
        AndroidApp::Home() noexcept
    {
        return _home;
    }

    void AndroidApp::Initialize()
    {
        AndroidAppOwner& owner = GetAndroidAppOwner();
        owner.AddFluentTheme(*this);
        owner.SetRequestedThemeVariantDark(*this);
        owner.BaseInitialize(*this);
    }

    void AndroidApp::OnFrameworkInitializationCompleted()
    {
        AndroidAppOwner& owner = GetAndroidAppOwner();
        AndroidSingleViewLifetime single =
            owner.SingleViewApplicationLifetime(*this);
        if (single)
        {
            std::shared_ptr<MphRead::Mods::Launcher::Gui::HomeViewAdapter>
                adapter;
            std::shared_ptr<MphRead::Mods::Launcher::Gui::HomeView> home =
                BuildHome(owner, adapter);

            // C#: single.MainView = Home = BuildHome();
            // Home is assigned before the framework MainView setter runs,
            // including when that setter subsequently throws.
            _home = home;
            owner.SetSingleViewMainView(single, *home, *adapter);
        }
        owner.BaseOnFrameworkInitializationCompleted(*this);
    }

    std::shared_ptr<MphRead::Mods::Launcher::Gui::HomeView>
        AndroidApp::BuildHome(
            AndroidAppOwner& owner,
            std::shared_ptr<
                MphRead::Mods::Launcher::Gui::HomeViewAdapter>& adapter)
    {
        MphRead::Mods::Launcher::LauncherPrefs::Load();
        MphRead::Mods::InputSettings::Load();
        MphRead::Mods::DebugLog::Attach();

        std::shared_ptr<MphRead::MenuSettings> settings =
            MphRead::GameState::LoadSettings();
        MphRead::Mods::GameSettings::Apply(settings);

        std::vector<std::string> rooms;
        if (MphRead::Mods::Launcher::GameFiles::Ready())
        {
            MphRead::Mods::Launcher::GameFiles::ApplyPaths();
            rooms = MphRead::Mods::ThumbnailGenerator::MultiplayerRooms();
        }

        std::shared_ptr<const
            MphRead::Mods::Launcher::Gui::HomeViewRoomList> roomList =
            std::make_shared<AndroidAppRoomList>(std::move(rooms));

        adapter = owner.CreateHomeViewAdapter();
        auto home = std::make_shared<
            MphRead::Mods::Launcher::Gui::HomeView>(
                *adapter, settings, roomList);

        auto target = std::make_shared<AndroidAppDoneTarget>();
        target->Adapter = adapter;
        home->AddDone(
            MphRead::Mods::Launcher::Gui::HomeViewEventHandler(
                target, &OnHomeDone));
        return home;
    }
}
