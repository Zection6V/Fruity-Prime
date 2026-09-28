#include "ShellBridge.hpp"

#include "../../MphRead.Native/Menu.hpp"
#include "../../MphRead.Native/Metadata/Metadata.hpp"
#include "../../MphRead.Native/Mods/Branding.hpp"
#include "../../MphRead.Native/Mods/Chat/ChatBox.hpp"
#include "../../MphRead.Native/Mods/Credits.hpp"
#include "../../MphRead.Native/Mods/EndScreen.hpp"
#include "../../MphRead.Native/Mods/HunterSuits.hpp"
#include "../../MphRead.Native/Mods/MapPick.hpp"
#include "../../MphRead.Native/Mods/Launcher/Portable/LauncherPrefs.hpp"
#include "../../MphRead.Native/Mods/Network/DemoPlayback.hpp"
#include "../../MphRead.Native/Mods/Network/DemoRecorder.hpp"
#include "../../MphRead.Native/Mods/Network/MapVote.hpp"
#include "../../MphRead.Native/Mods/Network/NetSession.hpp"
#include "../../MphRead.Native/Mods/PauseMenu.hpp"
#include "../../MphRead.Native/Mods/SpectatorMode.hpp"
#include "../../MphRead.Native/Mods/ThumbnailGenerator.hpp"
#include "../../MphRead.Native/Mods/Update/BuildVersion.hpp"
#include "../../MphRead.Native/Mods/Update/Updater.hpp"
#include "../../MphRead.Native/Mods/WindowMode.hpp"
#include "../../MphRead.Native/NativeRuntime/System/IO.hpp"
#include "../../MphRead.Native/NativeRuntime/System/HashCode.hpp"
#include "../../MphRead.Native/NativeRuntime/System/Managed.hpp"

#include <climits>

#include <QtCore/QFileInfo>
#include <QtCore/QUrl>

#include <algorithm>
#include <iostream>
#include <utility>

namespace MphRead::Qt
{
    namespace
    {
        ShellBridge* g_current = nullptr;
        namespace Network = ::MphRead::Mods::Network;
    }

    ShellBridge::ShellBridge(Actions actions) : _actions(std::move(actions))
    {
        g_current = this;
    }

    ShellBridge::~ShellBridge()
    {
        if (g_current == this)
        {
            g_current = nullptr;
        }
    }

    ShellBridge* ShellBridge::Current() noexcept
    {
        return g_current;
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

    void ShellBridge::SetRooms(const std::vector<std::string>& rooms, bool gameFilesReady)
    {
        _rooms.clear();
        for (const std::string& room : rooms)
        {
            const auto [meta, ignored] = ::MphRead::Metadata::GetRoomByName(room);
            (void)ignored;
            QVariantMap entry;
            entry.insert(QStringLiteral("key"), QString::fromStdString(room));
            // MapCardFactory: the tag is the key's first word, the blurb the
            // name the game gives the room.
            const std::size_t space = room.find(' ');
            entry.insert(QStringLiteral("code"), QString::fromStdString(
                space == std::string::npos ? room : room.substr(0, space)));
            entry.insert(QStringLiteral("name"), QString::fromStdString(
                meta != nullptr ? meta->InGameName.value_or("") : ""));
            _rooms.push_back(entry);
        }
        _gameFilesReady = gameFilesReady;
        emit roomsChanged();
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
        // StartScreen.RefreshVersionLine.
        const auto& current = ::MphRead::Mods::Update::BuildVersion::Current();
        const QString number = current.has_value() ? QString::fromStdString(current->ToString(3))
                                                   : QStringLiteral("a local build");
        if (::MphRead::Mods::Update::Updater::Available().has_value())
        {
            return number + QStringLiteral(" -- update available, click here");
        }
        return number;
    }

    QColor ShellBridge::VersionColour() const
    {
        if (::MphRead::Mods::Update::Updater::Available().has_value())
        {
            return QColor(255, 179, 71);
        }
        return ::MphRead::Mods::Update::BuildVersion::IsRelease() && ::MphRead::Mods::Update::Updater::Checked()
            ? QColor(0x5f, 0x9e, 0x72) : QColor(138, 147, 166);
    }

    QString ShellBridge::Brand() const
    {
        return QString::fromUtf8(::MphRead::Mods::Branding::Name.data(),
            static_cast<qsizetype>(::MphRead::Mods::Branding::Name.size()));
    }

    QString ShellBridge::FontOverride() const
    {
        const QString path = qEnvironmentVariable("FP_QT_FONT");
        return path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString();
    }

    void ShellBridge::SetSettings(std::shared_ptr<::MphRead::MenuSettings> settings)
    {
        _settings = std::move(settings);
    }

    QString ShellBridge::RoomKey() const
    {
        return _settings != nullptr ? QString::fromStdString(_settings->RoomKey) : QString();
    }

    void ShellBridge::SetRoomKey(const QString& key)
    {
        if (_settings != nullptr)
        {
            _settings->RoomKey = key.toStdString();
        }
    }

    void ShellBridge::Launch(LaunchPlan plan)
    {
        if (Network::NetSession::Active() && Network::NetSession::PersistentLobby())
        {
            _lobbyPlan = std::move(plan);
            emit lobbyOpened();
            return;
        }
        if (_actions.Launch)
        {
            _actions.Launch(std::move(plan));
        }
    }

    void ShellBridge::RequestScreen(const QString& url, const QVariantMap& props)
    {
        emit screenRequested(url, props);
    }

    QString ShellBridge::WindowLabel() const
    {
        // PauseMenuView.WindowLabel.
        return ::MphRead::Mods::WindowMode::IsFullscreen() ? QStringLiteral("Windowed")
                                                           : QStringLiteral("Fullscreen");
    }

    void ShellBridge::openSupport()
    {
        (void)::MphRead::Mods::Update::Updater::OpenLink(
            std::string(::MphRead::Mods::Credits::SupportUrl));
    }

    void ShellBridge::toggleFullscreen()
    {
        if (_actions.ToggleFullscreen)
        {
            _actions.ToggleFullscreen();
        }
        emit windowChanged();
    }

    QString ShellBridge::mapShot(const QString& room) const
    {
        if (room.isEmpty())
        {
            return {};
        }
        try
        {
            const QString path = QString::fromStdString(
                ::MphRead::Mods::ThumbnailGenerator::PathFor(room.toStdString()));
            return QFileInfo::exists(path) ? QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()).toString()
                                           : QString();
        }
        catch (const std::exception&)
        {
            return {};
        }
    }

    QString ShellBridge::roomName(const QString& room) const
    {
        const auto [meta, ignored] = ::MphRead::Metadata::GetRoomByName(room.toStdString());
        (void)ignored;
        return meta != nullptr && meta->InGameName.has_value() ? QString::fromStdString(*meta->InGameName) : room;
    }

    double ShellBridge::roomPhase(const QString& room) const
    {
        const std::int32_t hash = ::MphRead::NativeRuntime::StringGetHashCode(room.toStdString());
        const std::uint32_t magnitude = hash == INT32_MIN ? 0U : static_cast<std::uint32_t>(hash < 0 ? -hash : hash);
        return static_cast<double>(magnitude % 1000U) / 1000.0;
    }

    QVariantMap ShellBridge::pauseState() const
    {
        const bool demo = Network::DemoPlayback::IsActive();
        QVariantMap state;
        state.insert(QStringLiteral("vote"), Network::MapVote::Active() && !Network::MapVote::Answered() && !demo);
        state.insert(QStringLiteral("net"), !demo && Network::NetSession::Active());
        state.insert(QStringLiteral("spectating"), !demo && ::MphRead::Mods::SpectatorMode::IsSpectating());
        state.insert(QStringLiteral("canSpectate"), !demo && !::MphRead::Mods::SpectatorMode::IsSpectating()
            && ::MphRead::Mods::SpectatorMode::CanSpectate());
        state.insert(QStringLiteral("recording"), Network::DemoRecorder::IsRecording());
        return state;
    }

    void ShellBridge::answerVote(bool yes)
    {
        Network::MapVote::Cast(yes);
        resume();
    }

    void ShellBridge::spectate()
    {
        ::MphRead::Mods::SpectatorMode::Start();
        resume();
    }

    void ShellBridge::rejoin()
    {
        ::MphRead::Mods::SpectatorMode::Rejoin();
        resume();
    }

    void ShellBridge::toggleRecording()
    {
        if (Network::DemoRecorder::IsRecording())
        {
            const std::optional<std::string> path = Network::DemoRecorder::CurrentPath();
            std::cout << "[demo] recording saved to " << path.value_or("") << '\n';
            Network::DemoRecorder::Stop();
        }
        else
        {
            (void)Network::DemoRecorder::Start();
        }
        resume();
    }

    QString ShellBridge::whyNotVoting() const
    {
        return QString::fromStdString(Network::MapVote::WhyNotProposing());
    }

    void ShellBridge::systemMessage(const QString& text)
    {
        ::MphRead::Mods::Chat::ChatBox::System(std::optional<std::string>(text.toStdString()));
    }

    void ShellBridge::refreshProfile()
    {
        emit profileChanged();
    }

    QVariantMap ShellBridge::endState() const
    {
        using ::MphRead::Mods::MapPick;
        using ::MphRead::Mods::EndScreen;
        const std::vector<std::string> order = MapPick::Order();
        std::int32_t best = 0;
        for (const std::string& room : order)
        {
            best = std::max(best, MapPick::VotesFor(room));
        }
        QVariantList ballot;
        for (const std::string& room : order)
        {
            const std::size_t space = room.find(' ');
            const std::int32_t votes = MapPick::VotesFor(room);
            QVariantMap tile;
            tile.insert(QStringLiteral("key"), QString::fromStdString(room));
            tile.insert(QStringLiteral("code"), QString::fromStdString(space == std::string::npos ? room : room.substr(0, space)));
            tile.insert(QStringLiteral("name"), QString::fromStdString(MapPick::NameOf(room)));
            tile.insert(QStringLiteral("votes"), votes);
            tile.insert(QStringLiteral("leader"), best > 0 && votes == best);
            tile.insert(QStringLiteral("chosen"), room == MapPick::Picked());
            ballot.push_back(tile);
        }
        QVariantMap state;
        state.insert(QStringLiteral("ballot"), ballot);
        state.insert(QStringLiteral("hunter"), std::clamp(static_cast<int>(EndScreen::Hunter()), 0, 6));
        state.insert(QStringLiteral("suit"), std::clamp(EndScreen::Suit(), 0, 3));
        state.insert(QStringLiteral("ready"), EndScreen::Ready());
        state.insert(QStringLiteral("count"), MapPick::Eligible() > 1
            ? QStringLiteral("%1 in the room").arg(MapPick::Eligible()) : QString());
        return state;
    }

    void ShellBridge::endChoose(const QString& room)
    {
        ::MphRead::Mods::MapPick::Choose(::MphRead::Mods::MapPick::IndexOf(room.toStdString()));
    }

    void ShellBridge::endPick(int hunter, int suit)
    {
        ::MphRead::Mods::EndScreen::Pick(static_cast<::MphRead::Hunter>(std::clamp(hunter, 0, 6)),
            std::clamp(suit, 0, 3));
    }

    void ShellBridge::endToggleReady()
    {
        ::MphRead::Mods::EndScreen::ToggleReady();
    }

    QColor ShellBridge::suitColour(int hunter, int suit) const
    {
        try
        {
            const ::MphRead::ColorRgba sampled = ::MphRead::Mods::HunterSuits::Color(
                static_cast<::MphRead::Hunter>(std::clamp(hunter, 0, 6)), std::clamp(suit, 0, 3));
            return QColor(sampled.Red, sampled.Green, sampled.Blue);
        }
        catch (const std::exception&)
        {
            return QColor(255, 179, 71);
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
        // InGameMenu: the pause menu asks, the frame loop leaves.
        ::MphRead::Mods::PauseMenu::RequestLeave();
        resume();
    }

    void ShellBridge::quitFromMatch()
    {
        ::MphRead::Mods::PauseMenu::RequestQuit();
        resume();
    }
}
