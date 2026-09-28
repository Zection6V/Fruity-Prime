#include "ShellBridge.hpp"

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
