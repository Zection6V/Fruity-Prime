#include "ShellBridge.hpp"

#include "../../MphRead.Native/Mods/Credits.hpp"
#include "../../MphRead.Native/Mods/Launcher/Portable/LauncherPrefs.hpp"
#include "../../MphRead.Native/Mods/Update/BuildVersion.hpp"
#include "../../MphRead.Native/Mods/Update/Updater.hpp"
#include "../../MphRead.Native/Mods/WindowMode.hpp"
#include "../../MphRead.Native/NativeRuntime/System/Managed.hpp"

#include <QtCore/QVariantMap>

#include <utility>

namespace MphRead::Qt
{
    namespace
    {
        [[nodiscard]] QVariantMap Choice(const char* name, int value)
        {
            QVariantMap entry;
            entry.insert(QStringLiteral("name"), QString::fromUtf8(name));
            entry.insert(QStringLiteral("value"), value);
            return entry;
        }
    }

    ShellBridge::ShellBridge(Actions actions) : _actions(std::move(actions))
    {
    }

    void ShellBridge::SetPage(QString page)
    {
        if (page == _page)
        {
            return;
        }
        _page = std::move(page);
        emit pageChanged();
    }

    void ShellBridge::SetRooms(QVariantList rooms, bool gameFilesReady)
    {
        _rooms = std::move(rooms);
        _gameFilesReady = gameFilesReady;
        emit roomsChanged();
    }

    QVariantList ShellBridge::Modes() const
    {
        // GameMode values; the multiplayer ones the launcher offers.
        return {
            Choice("Battle", 3), Choice("Battle (teams)", 4),
            Choice("Survival", 5), Choice("Survival (teams)", 6),
            Choice("Capture", 7), Choice("Bounty", 8), Choice("Bounty (teams)", 9),
            Choice("Nodes", 10), Choice("Nodes (teams)", 11),
            Choice("Defender", 12), Choice("Defender (teams)", 13),
            Choice("Prime Hunter", 14),
        };
    }

    QVariantList ShellBridge::Hunters() const
    {
        // Hunter values.
        return {
            Choice("Random", 8), Choice("Samus", 0), Choice("Kanden", 1), Choice("Trace", 2),
            Choice("Sylux", 3), Choice("Noxus", 4), Choice("Spire", 5), Choice("Weavel", 6),
        };
    }

    void ShellBridge::play(const QString& room, int mode, int hunter, int bots, int botLevel)
    {
        if (_actions.Play)
        {
            _actions.Play(room, mode, hunter, bots, botLevel);
        }
    }

    QString ShellBridge::PlayerName() const
    {
        // StartScreen.PlayerNameOrDefault.
        const QString name = QString::fromStdString(
            ::MphRead::Mods::Launcher::LauncherPrefs::PlayerName()).trimmed();
        return name.isEmpty() ? QStringLiteral("Player") : name;
    }

    QString ShellBridge::Version() const
    {
        // StartScreen.VersionNumber.
        const auto& current = ::MphRead::Mods::Update::BuildVersion::Current();
        return current.has_value() ? QString::fromStdString(current->ToString(3))
                                   : QStringLiteral("a local build");
    }

    void ShellBridge::openSupport()
    {
        (void)::MphRead::Mods::Update::Updater::OpenLink(
            std::string(::MphRead::Mods::Credits::SupportUrl));
    }

    QString ShellBridge::WindowLabel() const
    {
        // PauseMenuView.WindowLabel.
        return ::MphRead::Mods::WindowMode::IsFullscreen() ? QStringLiteral("Windowed")
                                                           : QStringLiteral("Fullscreen");
    }

    void ShellBridge::toggleFullscreen()
    {
        if (_actions.ToggleFullscreen)
        {
            _actions.ToggleFullscreen();
        }
        emit windowChanged();
    }

    void ShellBridge::openSettings()
    {
        // The settings screens are not ported to QML yet.
    }

    void ShellBridge::quit()
    {
        if (_actions.Quit)
        {
            _actions.Quit();
        }
    }

    void ShellBridge::resume()
    {
        if (_actions.Resume)
        {
            _actions.Resume();
        }
    }

    void ShellBridge::leaveMatch()
    {
        if (_actions.LeaveMatch)
        {
            _actions.LeaveMatch();
        }
    }
}
