#include "NetCheckClient.hpp"

#include "DemoClip.hpp"
#include "DemoRecorder.hpp"
#include "MapVote.hpp"
#include "NetFeatureCheck.hpp"
#include "NetHitPrediction.hpp"
#include "NetLag.hpp"
#include "NetLaunch.hpp"
#include "NetLog.hpp"
#include "NetProtocol.hpp"
#include "NetSession.hpp"
#include "NetTestScript.hpp"
#include "NetTransport.hpp"
#include "NetUnlagged.hpp"
#include "../SpectatorMode.hpp"
#include "../ScreenCapture.hpp"
#include "../Chat/ChatBox.hpp"
#include "../../GameState.hpp"
#include "../../Metadata/Metadata.hpp"
#include "../../Scene.hpp"

#include <OpenTK/Graphics/OpenGL/GL.hpp>
#include <OpenTK/Windowing/Common/ContextFlags.hpp>
#include <OpenTK/Windowing/Common/ContextProfile.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace
{
    using MphRead::GameMode;
    using MphRead::Hunter;
    using MphRead::Entities::LoadFlags;
    using MphRead::Entities::PlayerFlags2;
    using MphRead::Mods::Network::TestPhase;
    using OpenTK::Mathematics::Vector3;

    template <typename TEnum>
    [[nodiscard]] bool TestFlag(TEnum value, TEnum flag) noexcept
    {
        using Underlying = std::underlying_type_t<TEnum>;
        return (static_cast<Underlying>(value) & static_cast<Underlying>(flag)) != 0;
    }

    [[nodiscard]] float Length(Vector3 value) noexcept
    {
        return std::sqrt(
            value.X * value.X + value.Y * value.Y + value.Z * value.Z);
    }

    [[nodiscard]] std::string Fixed(double value, std::int32_t digits)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(digits) << value;
        return stream.str();
    }

    [[nodiscard]] std::string TwoDigits(std::int32_t value)
    {
        std::ostringstream stream;
        stream << std::setw(2) << std::setfill('0') << value;
        return stream.str();
    }

    [[nodiscard]] std::string PathCombine(
        const std::string& directory, const std::string& filename)
    {
        return (std::filesystem::path(directory) / std::filesystem::path(filename)).string();
    }

    [[nodiscard]] const char* BoolText(bool value) noexcept
    {
        return value ? "True" : "False";
    }

    [[nodiscard]] std::string HunterName(Hunter hunter)
    {
        switch (hunter)
        {
            case Hunter::Samus: return "Samus";
            case Hunter::Kanden: return "Kanden";
            case Hunter::Trace: return "Trace";
            case Hunter::Sylux: return "Sylux";
            case Hunter::Noxus: return "Noxus";
            case Hunter::Spire: return "Spire";
            case Hunter::Weavel: return "Weavel";
            case Hunter::Guardian: return "Guardian";
            case Hunter::Random: return "Random";
        }
        return std::to_string(static_cast<std::uint32_t>(hunter));
    }

    [[nodiscard]] std::string GameModeName(GameMode mode)
    {
        switch (mode)
        {
            case GameMode::None: return "None";
            case GameMode::SinglePlayer: return "SinglePlayer";
            case GameMode::Battle: return "Battle";
            case GameMode::BattleTeams: return "BattleTeams";
            case GameMode::Survival: return "Survival";
            case GameMode::SurvivalTeams: return "SurvivalTeams";
            case GameMode::Bounty: return "Bounty";
            case GameMode::BountyTeams: return "BountyTeams";
            case GameMode::Capture: return "Capture";
            case GameMode::Defender: return "Defender";
            case GameMode::DefenderTeams: return "DefenderTeams";
            case GameMode::Nodes: return "Nodes";
            case GameMode::NodesTeams: return "NodesTeams";
            case GameMode::PrimeHunter: return "PrimeHunter";
        }
        return std::to_string(static_cast<std::int32_t>(mode));
    }

    [[nodiscard]] std::string TestPhaseName(TestPhase phase)
    {
        switch (phase)
        {
            case TestPhase::Idle: return "Idle";
            case TestPhase::Walk: return "Walk";
            case TestPhase::Jump: return "Jump";
            case TestPhase::Turn: return "Turn";
            case TestPhase::Shoot: return "Shoot";
            case TestPhase::SwitchWeapons: return "SwitchWeapons";
            case TestPhase::Charge: return "Charge";
            case TestPhase::MorphA: return "MorphA";
            case TestPhase::AltAttackA: return "AltAttackA";
            case TestPhase::MorphB: return "MorphB";
            case TestPhase::AltAttackB: return "AltAttackB";
            case TestPhase::Unmorph: return "Unmorph";
            case TestPhase::Zoom: return "Zoom";
            case TestPhase::Afflict: return "Afflict";
            case TestPhase::Duel: return "Duel";
        }
        return std::to_string(static_cast<std::int32_t>(phase));
    }

    [[nodiscard]] std::size_t Utf16Length(std::string_view value) noexcept
    {
        std::size_t length = 0;
        for (std::size_t index = 0; index < value.size();)
        {
            const unsigned char lead = static_cast<unsigned char>(value[index]);
            std::uint32_t codePoint = 0;
            std::size_t count = 1;
            if ((lead & 0x80U) == 0)
            {
                codePoint = lead;
            }
            else if ((lead & 0xE0U) == 0xC0U && index + 1 < value.size())
            {
                codePoint = lead & 0x1FU;
                count = 2;
            }
            else if ((lead & 0xF0U) == 0xE0U && index + 2 < value.size())
            {
                codePoint = lead & 0x0FU;
                count = 3;
            }
            else if ((lead & 0xF8U) == 0xF0U && index + 3 < value.size())
            {
                codePoint = lead & 0x07U;
                count = 4;
            }
            else
            {
                ++length;
                ++index;
                continue;
            }

            bool valid = true;
            for (std::size_t offset = 1; offset < count; ++offset)
            {
                const unsigned char next = static_cast<unsigned char>(value[index + offset]);
                if ((next & 0xC0U) != 0x80U)
                {
                    valid = false;
                    break;
                }
                codePoint = (codePoint << 6U) | (next & 0x3FU);
            }
            if (!valid)
            {
                ++length;
                ++index;
                continue;
            }
            length += codePoint > 0xFFFFU ? 2U : 1U;
            index += count;
        }
        return length;
    }

    [[nodiscard]] std::string PadRightManaged(std::string value, std::size_t width)
    {
        const std::size_t length = Utf16Length(value);
        if (length < width)
        {
            value.append(width - length, ' ');
        }
        return value;
    }

    [[nodiscard]] std::optional<std::string> EnvironmentVariable(const char* name)
    {
        const char* value = std::getenv(name);
        if (value == nullptr)
        {
            return std::nullopt;
        }
        return std::string(value);
    }

    [[nodiscard]] bool TryParseInvariantDouble(const std::string& input, double& value)
    {
        std::size_t first = 0;
        while (first < input.size()
            && std::isspace(static_cast<unsigned char>(input[first])) != 0)
        {
            ++first;
        }
        std::size_t last = input.size();
        while (last > first
            && std::isspace(static_cast<unsigned char>(input[last - 1])) != 0)
        {
            --last;
        }
        if (first == last)
        {
            value = 0.0;
            return false;
        }

        std::string text(input.substr(first, last - first));
        if (text == "NaN")
        {
            value = std::numeric_limits<double>::quiet_NaN();
            return true;
        }
        if (text == "Infinity" || text == "+Infinity")
        {
            value = std::numeric_limits<double>::infinity();
            return true;
        }
        if (text == "-Infinity")
        {
            value = -std::numeric_limits<double>::infinity();
            return true;
        }

        // Double.TryParse(string, InvariantCulture, out ...) uses the default
        // floating-point NumberStyles: sign, decimal point, exponent and
        // thousands separators. Group separators belong to the integral
        // significand; they are not stripped from the fractional or exponent
        // portions, where doing so would accept strings the managed parser
        // rejects (for example, an exponent containing a comma).
        std::size_t index = 0;
        if (index < text.size() && (text[index] == '+' || text[index] == '-'))
        {
            ++index;
        }
        bool haveDigits = false;
        bool previousWasGroup = false;
        std::string normalized;
        normalized.reserve(text.size());
        if (index > 0)
        {
            normalized.push_back(text[0]);
        }
        while (index < text.size())
        {
            const char ch = text[index];
            if (ch >= '0' && ch <= '9')
            {
                haveDigits = true;
                previousWasGroup = false;
                normalized.push_back(ch);
                ++index;
                continue;
            }
            if (ch == ',')
            {
                if (!haveDigits || previousWasGroup || index + 1 >= text.size()
                    || text[index + 1] < '0' || text[index + 1] > '9')
                {
                    value = 0.0;
                    return false;
                }
                previousWasGroup = true;
                ++index;
                continue;
            }
            break;
        }
        if (index < text.size() && text[index] == '.')
        {
            normalized.push_back('.');
            ++index;
            while (index < text.size() && text[index] >= '0' && text[index] <= '9')
            {
                haveDigits = true;
                normalized.push_back(text[index]);
                ++index;
            }
        }
        if (!haveDigits)
        {
            value = 0.0;
            return false;
        }
        if (index < text.size() && (text[index] == 'e' || text[index] == 'E'))
        {
            normalized.push_back(text[index]);
            ++index;
            if (index < text.size() && (text[index] == '+' || text[index] == '-'))
            {
                normalized.push_back(text[index]);
                ++index;
            }
            const std::size_t exponentStart = index;
            while (index < text.size() && text[index] >= '0' && text[index] <= '9')
            {
                normalized.push_back(text[index]);
                ++index;
            }
            if (index == exponentStart)
            {
                value = 0.0;
                return false;
            }
        }
        if (index != text.size())
        {
            value = 0.0;
            return false;
        }

        char* end = nullptr;
        value = std::strtod(normalized.c_str(), &end);
        if (end == normalized.c_str() || end == nullptr || *end != '\0')
        {
            value = 0.0;
            return false;
        }
        return true;
    }

    [[nodiscard]] std::string OptionalInterpolation(
        const std::optional<std::string>& value)
    {
        return value.has_value() ? *value : std::string();
    }
}

namespace MphRead::Mods::Network
{
    OpenTK::Windowing::Desktop::GameWindowSettings NetCheckClient::GameSettings()
    {
        OpenTK::Windowing::Desktop::GameWindowSettings settings{};
        settings.UpdateFrequency = 60.0;
        return settings;
    }

    OpenTK::Windowing::Desktop::NativeWindowSettings NetCheckClient::WindowSettings(
        std::int32_t width, std::int32_t height)
    {
        OpenTK::Windowing::Desktop::NativeWindowSettings settings{};
        settings.ClientSize = OpenTK::Mathematics::Vector2i(width, height);
        settings.Title = "MphRead net check";
        settings.Profile = OpenTK::Windowing::Common::ContextProfile::Compatability;
        settings.Flags = OpenTK::Windowing::Common::ContextFlags::Default;
        settings.APIVersion = {3, 2};
        settings.StartVisible = false;
        return settings;
    }

    NetCheckClient::NetCheckClient(
        std::chrono::steady_clock::time_point wallClockStart,
        std::string name,
        std::string roomKey,
        MphRead::GameMode mode,
        MphRead::Hunter hunter,
        double seconds,
        std::optional<std::string> shotDirectory,
        std::int32_t width,
        std::int32_t height,
        double spectateAt,
        double rejoinAt,
        std::int32_t color)
        : GameWindow(GameSettings(), WindowSettings(width, height)),
          _name(std::move(name)),
          _shotDirectory(std::move(shotDirectory)),
          _seconds(seconds),
          _spectateAt(spectateAt),
          _rejoinAt(rejoinAt),
          _wallClockStart(wallClockStart),
          _remoteSpectatingFrames(
              static_cast<std::size_t>(Entities::PlayerEntity::MaxPlayers()), 0),
          _remotes(static_cast<std::size_t>(Entities::PlayerEntity::MaxPlayers())),
          _features(std::make_unique<NetFeatureCheck>())
    {
        for (std::unique_ptr<RemoteView>& remote : _remotes)
        {
            remote = std::make_unique<RemoteView>();
        }
        _scene = std::make_unique<MphRead::Scene>(
            Size,
            KeyboardState,
            MouseState,
            [](auto&&) {},
            [this]() { Close(); });
        NetLaunch::BuildPlayers(*_scene, hunter, color, GameState::IsTeamMode(mode));
        _scene->AddRoom(roomKey, mode, NetLaunch::RoomPlayerCount);
    }

    NetCheckClient::~NetCheckClient() = default;

    MphRead::Scene& NetCheckClient::Scene() noexcept
    {
        return *_scene;
    }

    const MphRead::Scene& NetCheckClient::Scene() const noexcept
    {
        return *_scene;
    }

    void NetCheckClient::OnLoad()
    {
        _scene->Size = ClientSize;
        _scene->OnLoad();
        GameWindow::OnLoad();
        OpenTK::Graphics::OpenGL::GL::Viewport(0, 0, ClientSize.X, ClientSize.Y);
        _scene->OnResize();
    }

    void NetCheckClient::OnRenderFrame(OpenTK::Windowing::Common::FrameEventArgs args)
    {
        GameState::ApplyPause();
        _scene->OnUpdateFrame();
        if (!_scene->OnRenderFrame())
        {
            return;
        }
        ++_frame;
        UpdateSpectating();
        DriveVoteTest();
        DriveRebindTest();
        Observe();
        _features->Observe(*_scene);
        SampleScoreboardOnServerClock();
        if (_shotDirectory.has_value() && _frame % 120 == 0)
        {
            const std::string path = PathCombine(
                *_shotDirectory, _name + "-" + TwoDigits(_shots) + ".png");
            if (Mods::ScreenCapture::Save(*_scene, path))
            {
                ++_shots;
                _litFraction = std::max(
                    _litFraction, Mods::ScreenCapture::NonBlackFraction(*_scene));
            }
        }
        if (_shotDirectory.has_value() && _opponentInView && _duelShots < 8
            && _frame - _lastDuelShotFrame > 45)
        {
            const std::string path = PathCombine(
                *_shotDirectory, _name + "-duel-" + TwoDigits(_duelShots) + ".png");
            if (Mods::ScreenCapture::Save(*_scene, path))
            {
                ++_duelShots;
                _lastDuelShotFrame = _frame;
            }
        }
        SwapBuffers();
        _scene->AfterRenderFrame();
        GameWindow::OnRenderFrame(args);
        if (_frame >= _seconds * 60.0)
        {
            Close();
        }
    }

    void NetCheckClient::SampleScoreboardOnServerClock()
    {
        const std::optional<MatchStatePacket> serverMatch = NetSession::ServerMatch();
        if (!serverMatch.has_value())
        {
            return;
        }
        const float elapsed = serverMatch->TimeElapsed;
        if (_scoreboardSampleAt < 0.0F)
        {
            _scoreboardSampleAt = static_cast<float>(
                std::ceil((elapsed + 20.0F) / SampleEvery) * SampleEvery);
            return;
        }
        if (elapsed >= _scoreboardSampleAt)
        {
            _features->SampleScoreboard(static_cast<std::int32_t>(_scoreboardSampleAt));
            _scoreboardSampleAt += SampleEvery;
        }
    }

    void NetCheckClient::UpdateSpectating()
    {
        if (_spectateAt >= 0.0 && _spectateStartedFrame < 0
            && _frame >= _spectateAt * 60.0)
        {
            SpectatorMode::Start();
            if (SpectatorMode::IsSpectating())
            {
                _spectateStartedFrame = _frame;
                std::cout
                    << "[netcheck] " << _name
                    << " is spectating from frame " << _frame << '\n';
            }
            else
            {
                _spectateStartedFrame = std::numeric_limits<std::int32_t>::max();
                std::cout
                    << "[netcheck] " << _name
                    << " could not spectate (multiplayer="
                    << BoolText(GameState::Multiplayer()) << ")\n";
            }
        }
        if (_rejoinAt >= 0.0 && _rejoinedFrame < 0 && SpectatorMode::IsSpectating()
            && _frame >= _rejoinAt * 60.0)
        {
            SpectatorMode::Rejoin();
            _rejoinedFrame = _frame;
            std::cout
                << "[netcheck] " << _name
                << " rejoined the match on frame " << _frame << '\n';
        }
        if (SpectatorMode::IsSpectating())
        {
            ++_spectatingFrames;
        }
        for (std::int32_t slot = 0; slot < Entities::PlayerEntity::MaxPlayers(); ++slot)
        {
            if (slot == std::max(NetSession::LocalSlot(), 0)
                || static_cast<std::size_t>(slot) >= Entities::PlayerEntity::Players().size())
            {
                continue;
            }
            const std::shared_ptr<Entities::PlayerEntity>& player
                = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(slot));
            if (TestFlag(player->Flags2(), PlayerFlags2::Spectating))
            {
                ++_remoteSpectatingFrames.at(static_cast<std::size_t>(slot));
            }
        }
    }

    void NetCheckClient::DriveVoteTest()
    {
        const std::optional<std::string> room = EnvironmentVariable("MPHREAD_VOTE_TEST");
        if (!room.has_value())
        {
            return;
        }
        if (MapVote::Active() && !MapVote::Answered())
        {
            std::cout
                << "[votetest] " << _name
                << " sees " << MapVote::Proposer()
                << " propose " << MapVote::RoomKey()
                << " (" << MapVote::Yes() << '/' << MapVote::Needed()
                << " of " << MapVote::Eligible() << ")\n";
            MapVote::Cast(true);
            return;
        }
        if (!_votedOnce && !room->empty() && _frame == 600)
        {
            _votedOnce = true;
            std::cout << "[votetest] " << _name << " proposes " << *room << '\n';
            MapVote::Propose(*room);
        }
    }

    void NetCheckClient::DriveRebindTest()
    {
        if (_rebound)
        {
            return;
        }
        const std::optional<std::string> at = EnvironmentVariable("MPHREAD_NET_REBIND");
        double seconds = 0.0;
        if (!at.has_value() || !TryParseInvariantDouble(*at, seconds))
        {
            return;
        }
        if (_frame < seconds * 60.0)
        {
            return;
        }
        _rebound = true;
        std::cout
            << "[rebindtest] " << _name
            << " was slot " << NetSession::LocalSlot() << '\n';
        NetSession::RebindSocket();
    }

    void NetCheckClient::Observe()
    {
        _opponentInView = false;
        if (_scene->RoomId != _lastRoomId)
        {
            if (_lastRoomId != -1)
            {
                ++_roomChanges;
                _everSawSomeone |= AnyoneSeen();
                _features->Reset();
                for (std::unique_ptr<RemoteView>& remote : _remotes)
                {
                    remote = std::make_unique<RemoteView>();
                }
                _localSpawnFrame = -1;
                _lastLocalHealth = -1;
                _wasAliveLocal = false;
            }
            _lastRoomId = _scene->RoomId;
        }
        SayHello();
        const std::int32_t local = std::max(NetSession::LocalSlot(), 0);
        std::shared_ptr<Entities::PlayerEntity> me{};
        if (static_cast<std::size_t>(local) < Entities::PlayerEntity::Players().size())
        {
            me = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(local));
        }
        if (me && TestFlag(me->LoadFlags(), LoadFlags::Spawned))
        {
            if (_localSpawnFrame < 0)
            {
                _localSpawnFrame = _frame;
            }
            _minHealthSeen = std::min(_minHealthSeen, me->Health());
            if (_wasAliveLocal && me->Health() == 0)
            {
                ++_myDeaths;
            }
            if (_lastLocalHealth > 0 && me->Health() > 0 && me->Health() < _lastLocalHealth)
            {
                ++_damageTaken;
            }
            _lastLocalHealth = me->Health();
            _wasAliveLocal = me->Health() > 0;
            if (me->IsAltForm())
            {
                ++_myAltFrames;
            }
            if (me->ModDamageIndicatorActive())
            {
                ++_indicatorFrames;
            }
        }
        for (std::int32_t slot = 0; slot < Entities::PlayerEntity::MaxPlayers(); ++slot)
        {
            if (slot == local
                || static_cast<std::size_t>(slot) >= Entities::PlayerEntity::Players().size())
            {
                continue;
            }
            const std::shared_ptr<Entities::PlayerEntity>& other
                = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(slot));
            RemoteView& view = *_remotes.at(static_cast<std::size_t>(slot));
            if (!TestFlag(other->LoadFlags(), LoadFlags::Active))
            {
                continue;
            }
            ++view.FramesActive;
            if (!TestFlag(other->LoadFlags(), LoadFlags::Spawned))
            {
                continue;
            }
            ++view.FramesSpawned;
            if (view.FirstSpawnFrame < 0)
            {
                view.FirstSpawnFrame = _frame;
            }
            view.MinHealth = std::min(view.MinHealth, other->Health());
            if (view.WasAlive && other->Health() == 0)
            {
                ++view.Deaths;
            }
            if (view.LastHealth > 0 && other->Health() > 0 && other->Health() < view.LastHealth)
            {
                ++view.Hits;
            }
            view.LastHealth = other->Health();
            view.WasAlive = other->Health() > 0;
            if (view.HavePosition)
            {
                const float step = Length(other->Position - view.LastPosition);
                if (step < 5.0F)
                {
                    view.Travelled += step;
                }
                if (step > 0.05F)
                {
                    ++view.DistinctPositions;
                }
            }
            view.LastPosition = other->Position;
            view.HavePosition = true;
            view.Hunter = other->Hunter();
            if (other->IsAltForm())
            {
                ++view.AltFormFrames;
            }
            if (NetSession::RemoteStateValid.at(static_cast<std::size_t>(slot)))
            {
                const bool wanted
                    = (NetSession::RemoteStates.at(static_cast<std::size_t>(slot)).Flags
                        & PlayerState::FlagAltForm) != 0;
                if (wanted)
                {
                    ++view.AltFormWantedFrames;
                }
                if (wanted != other->IsAltForm())
                {
                    ++view.AltFormDisagreeFrames;
                }
            }
            if (me && other->Health() > 0 && !_opponentInView)
            {
                const auto [turnX, turnY] = me->ModAimDeltaTowards(other->ModAimTarget());
                const float distance = Length(other->Position - me->Position);
                _opponentInView = distance < 25.0F && std::fabs(turnX) < 18.0F
                    && std::fabs(turnY) < 18.0F;
            }
        }
    }

    void NetCheckClient::OnClosing(System::ComponentModel::CancelEventArgs& e)
    {
        _scene->DoCleanup();
        GameWindow::OnClosing(e);
    }

    void NetCheckClient::SayHello()
    {
        if (NetSession::LocalSlot() < 0 || (_frame != 300 && _frame != 1500))
        {
            return;
        }
        Mods::Chat::ChatBox::Send(
            "hello from " + _name + " at frame " + std::to_string(_frame));
    }

    bool NetCheckClient::Passed() const
    {
        return _everSawSomeone || AnyoneSeen();
    }

    bool NetCheckClient::AnyoneSeen() const
    {
        for (const std::unique_ptr<RemoteView>& remote : _remotes)
        {
            const RemoteView& view = *remote;
            if (view.FirstSpawnFrame >= 0 && view.DistinctPositions > 5
                && view.Travelled > 2.0)
            {
                return true;
            }
        }
        return false;
    }

    double NetCheckClient::ElapsedSeconds() const
    {
        return std::chrono::duration<double>(
            std::chrono::steady_clock::now() - _wallClockStart).count();
    }

    double NetCheckClient::FramesPerSecond() const
    {
        return ElapsedSeconds() > 0.0 ? _frame / ElapsedSeconds() : 0.0;
    }

    void NetCheckClient::Report()
    {
        std::cout
            << "  ran " << _frame << " frame(s) in "
            << Fixed(ElapsedSeconds(), 1) << " s -- "
            << Fixed(FramesPerSecond(), 1) << " fps\n";
        const std::int32_t local = std::max(NetSession::LocalSlot(), 0);
        std::cout << '\n';
        std::cout << "=== " << _name << ": what this client saw ===\n";
        const std::optional<std::string> lag = NetLag::Describe();
        if (lag.has_value())
        {
            std::cout
                << "  SIMULATED LINE: " << *lag << " -- these numbers "
                << "describe a reproduction, not a real connection\n";
        }
        std::cout
            << "  slot " << local
            << ", authority=" << BoolText(NetSession::IsAuthority())
            << ", frames=" << _frame << '\n';
        std::cout << "  " << NetUnlagged::Describe() << '\n';
        std::cout << "  " << NetHitPrediction::Describe() << '\n';

        const std::shared_ptr<RoomMetadata> roomMetadata
            = Metadata::GetRoomById(_scene->RoomId, true);
        const std::optional<MatchStatePacket> serverMatch = NetSession::ServerMatch();
        const std::string serverRoom = serverMatch.has_value() && serverMatch->RoomKey.has_value()
            ? *serverMatch->RoomKey
            : std::string("?");
        std::cout
            << "  room: " << (roomMetadata ? roomMetadata->Name : std::string("?"))
            << " (server says " << serverRoom << "), "
            << _roomChanges << " rotation(s) followed\n";
        std::cout
            << "  packets: snapshots sent=" << NetSession::SnapshotsSent()
            << " received=" << NetSession::SnapshotsReceived()
            << " late=" << NetSession::SnapshotsOutOfOrder()
            << " restreams=" << NetSession::SnapshotStreamResets()
            << " intents received=" << NetSession::IntentsReceived()
            << " intents late=" << NetSession::IntentsOutOfOrder()
            << " states applied=" << NetSession::StatesApplied()
            << " dropped=" << NetTransport::TotalPacketsDropped.load()
            << '\n';
        std::cout
            << "  chat: sent=" << Mods::Chat::ChatBox::Sent()
            << " received=" << Mods::Chat::ChatBox::Received() << '\n';

        std::ostringstream pings;
        for (std::int32_t slot = 0; slot < Entities::PlayerEntity::MaxPlayers(); ++slot)
        {
            if (NetSession::SlotOccupied.at(static_cast<std::size_t>(slot)))
            {
                if (pings.tellp() > 0)
                {
                    pings << "  ";
                }
                pings
                    << "slot " << slot << ' '
                    << NetSession::SlotPing.at(static_cast<std::size_t>(slot)) << " ms";
            }
        }
        if (NetSession::ReAnnouncements() > 0 || NetSession::LongestServerSilence() > 1.0)
        {
            std::cout
                << "  server silence: " << NetSession::ReAnnouncements()
                << " re-announce(s), longest gap "
                << Fixed(NetSession::LongestServerSilence(), 1)
                << " s of engine time ("
                << Fixed(
                    NetSession::LongestServerSilence() * 60.0
                        / std::max(FramesPerSecond(), 1.0),
                    1)
                << " s of wall clock at this client's "
                << Fixed(FramesPerSecond(), 0) << " fps), "
                << NetSession::AuthorityStandDowns() << " authority stand-down(s)\n";
        }
        if (pings.tellp() > 0)
        {
            std::cout << "  pings: " << pings.str() << '\n';
        }

        for (std::int32_t slot = 0; slot < Entities::PlayerEntity::MaxPlayers(); ++slot)
        {
            if (static_cast<std::size_t>(slot) >= Entities::PlayerEntity::Players().size())
            {
                continue;
            }
            const std::shared_ptr<Entities::PlayerEntity>& player
                = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(slot));
            const bool active = TestFlag(player->LoadFlags(), LoadFlags::Active);
            if (!active && !NetSession::SlotOccupied.at(static_cast<std::size_t>(slot)))
            {
                continue;
            }
            const std::string nickname = GameState::Nicknames().at(static_cast<std::size_t>(slot));
            std::cout
                << "  slot " << slot << ' '
                << PadRightManaged(nickname, 10) << ' '
                << PadRightManaged(HunterName(player->Hunter()), 8) << ' '
                << "active=" << (active ? "y" : "n") << ' '
                << "spawned="
                << (TestFlag(player->LoadFlags(), LoadFlags::Spawned) ? "y" : "n") << ' '
                << "hp=" << PadRightManaged(std::to_string(player->Health()), 4) << ' '
                << "pos=(" << Fixed(player->Position.X, 1)
                << ',' << Fixed(player->Position.Y, 1)
                << ',' << Fixed(player->Position.Z, 1) << ")\n";
        }

        std::cout << "  my player: ";
        if (static_cast<std::size_t>(local) < Entities::PlayerEntity::Players().size())
        {
            std::cout << HunterName(
                Entities::PlayerEntity::Players().at(static_cast<std::size_t>(local))->Hunter());
        }
        else
        {
            std::cout << '?';
        }
        std::cout << ", alt form on " << _myAltFrames << " frame(s)\n";
        std::cout
            << "  my player: spawned on frame " << _localSpawnFrame
            << ", lowest health "
            << (_minHealthSeen == std::numeric_limits<std::int32_t>::max()
                ? -1 : _minHealthSeen)
            << ", died " << _myDeaths << " time(s), took a hit "
            << _damageTaken << " time(s)\n";
        std::cout
            << "  HUD damage indicator lit on " << _indicatorFrames << " frame(s)\n";

        for (std::size_t slot = 0; slot < _remotes.size(); ++slot)
        {
            const RemoteView& view = *_remotes[slot];
            if (view.FramesActive == 0)
            {
                continue;
            }
            std::cout
                << "  slot " << slot << " (" << GameState::Nicknames().at(slot)
                << ") as I saw them: " << HunterName(view.Hunter)
                << ", active " << view.FramesActive << " frame(s), spawned "
                << view.FramesSpawned << ", first on frame " << view.FirstSpawnFrame
                << ", in alt form for " << view.AltFormFrames
                << " (authority said alt form on " << view.AltFormWantedFrames
                << ", disagreed on " << view.AltFormDisagreeFrames << ")\n";
            std::cout
                << "    moved " << Fixed(view.Travelled, 1) << " units over "
                << view.DistinctPositions << " distinct position(s); I saw them hit "
                << view.Hits << " time(s), killed " << view.Deaths
                << " time(s), lowest health "
                << (view.MinHealth == std::numeric_limits<std::int32_t>::max()
                    ? -1 : view.MinHealth)
                << '\n';
        }

        if (_spectateAt >= 0.0 || _spectatingFrames > 0)
        {
            std::cout
                << "  spectating: " << _spectatingFrames << " frame(s), started on frame "
                << (_spectateStartedFrame == std::numeric_limits<std::int32_t>::max()
                    ? -1 : _spectateStartedFrame)
                << ", rejoined on frame " << _rejoinedFrame
                << ", now=" << BoolText(SpectatorMode::IsSpectating()) << '\n';
        }
        for (std::size_t slot = 0; slot < _remoteSpectatingFrames.size(); ++slot)
        {
            if (_remoteSpectatingFrames[slot] > 0)
            {
                std::cout
                    << "  slot " << slot << " (" << GameState::Nicknames().at(slot)
                    << ") was spectating on " << _remoteSpectatingFrames[slot]
                    << " of my frame(s)\n";
            }
        }
        std::cout
            << "  script: " << NetTestScript::FramesOnTarget()
            << " frame(s) with somebody in its sights, phase now "
            << TestPhaseName(NetTestScript::Phase()) << '\n';
        if (_shots > 0)
        {
            std::cout
                << "  " << _shots << " screenshot(s) written to "
                << OptionalInterpolation(_shotDirectory)
                << ", of which " << _duelShots
                << " with an opponent in view, busiest frame "
                << Fixed(_litFraction * 100.0, 1) << "% lit\n";
        }
        std::int32_t featureFailures = 0;
        const bool featuresOk = _features->Report(featureFailures);
        _featureFailures = featureFailures;
        std::cout << '\n';
        if (Passed() && featuresOk)
        {
            std::cout << "  RESULT: PASS\n";
        }
        else
        {
            std::cout << "  RESULT: FAIL -- ";
            if (!Passed())
            {
                std::cout << "no other player was on the map and moving; ";
            }
            std::cout << featureFailures << " feature(s) did not cross\n";
        }
    }

    std::int32_t NetCheckClient::Run(
        std::string host,
        std::int32_t port,
        std::string name,
        MphRead::Hunter hunter,
        double seconds,
        std::optional<std::string> shotDirectory,
        std::int32_t width,
        std::int32_t height,
        bool recordDemo,
        double spectateAt,
        double rejoinAt,
        std::int32_t color)
    {
        if (!NetLaunch::Join(host, port, name, hunter, 8000, color))
        {
            std::cout << "[netcheck] " << name << " could not join\n";
            NetSession::Stop();
            return 1;
        }
        NetTestScript::Reset();
        NetTestScript::Enabled(true);

        const auto roomOptional = NetLaunch::ServerRoom();
        const auto [roomKey, roomMode] = roomOptional.value();
        std::cout
            << "[netcheck] " << name
            << " joined slot " << NetSession::LocalSlot()
            << ", loading " << roomKey << " (" << GameModeName(roomMode) << ")\n";
        if (recordDemo && DemoRecorder::Start())
        {
            std::cout
                << "[netcheck] " << name << " is recording to "
                << OptionalInterpolation(DemoRecorder::CurrentPath()) << '\n';
        }

        std::unique_ptr<NetCheckClient> window{};
        std::int32_t result = 0;
        try
        {
            window = std::unique_ptr<NetCheckClient>(new NetCheckClient(
                std::chrono::steady_clock::now(),
                name,
                roomKey,
                roomMode,
                hunter,
                seconds,
                shotDirectory,
                width,
                height,
                spectateAt,
                rejoinAt,
                NetSession::LocalColor()));
            window->GameWindow::Run();
            window->Report();
            result = window->Passed() && window->_featureFailures == 0 ? 0 : 1;
        }
        catch (const std::exception& ex)
        {
            std::cout
                << "[netcheck] " << name << " crashed: "
                << typeid(ex).name() << ": " << ex.what() << '\n';
            result = 2;
        }

        if (DemoRecorder::IsRecording())
        {
            std::cout
                << "[netcheck] " << name << " recorded "
                << OptionalInterpolation(DemoRecorder::CurrentPath()) << '\n';
            DemoRecorder::Stop();
        }
        if (EnvironmentVariable("MPHREAD_CLIP_TEST").has_value())
        {
            const double held = DemoClip::Held();
            const std::optional<std::string> first = DemoClip::Save();
            std::cout
                << "[netcheck] " << name << " clip held "
                << Fixed(held, 1) << " s, "
                << (first.has_value() ? *first : std::string("nothing saved")) << '\n';
            const std::optional<std::string> second = DemoClip::Save();
            std::cout
                << "[netcheck] " << name << " clip again -> "
                << (second.has_value() ? *second : std::string("nothing saved")) << '\n';
        }
        if (window)
        {
            window->Dispose();
        }
        SpectatorMode::Reset();
        NetTestScript::Enabled(false);
        NetSession::Stop();
        NetLog::Close();
        return result;
    }
}
