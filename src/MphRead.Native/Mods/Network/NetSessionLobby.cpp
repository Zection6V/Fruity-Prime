#include "NetSession.hpp"

#include "DemoPlayback.hpp"
#include "NetDamage.hpp"
#include "NetHealthSync.hpp"
#include "NetHitClaims.hpp"
#include "NetHitPrediction.hpp"
#include "NetLifecycleTracker.hpp"
#include "NetMatchEnd.hpp"
#include "NetMatchSync.hpp"
#include "NetPlayerBridge.hpp"
#include "NetPlayerLifecycle.hpp"
#include "NetPlayerSetup.hpp"
#include "NetRoomChange.hpp"
#include "NetSlotManager.hpp"
#include "NetSmoothing.hpp"
#include "NetUnlagged.hpp"
#include "PlayerColors.hpp"

#include "../../GameState.hpp"
#include "../SpectatorMode.hpp"
#include "../Chat/NetChat.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../NativeRuntime/System/Stopwatch.hpp"

#include <algorithm>

namespace MphRead::Mods::Network
{
    namespace Runtime = ::MphRead::NativeRuntime;

    std::optional<SessionStatePacket> NetSession::_serverSession{};
    std::string NetSession::_lobbyMessage{};
    Runtime::Guid NetSession::_ownerToken{};
    std::uint32_t NetSession::_nextCommandId = 0;
    std::optional<std::uint16_t> NetSession::_loadedMatch{};
    std::uint16_t NetSession::_rosterSessionRevision = 0;
    double NetSession::_lastLoadAck = 0;
    double NetSession::_lastIdentity = 0;
    std::map<std::uint32_t, NetSession::PendingLobbyCommand> NetSession::_pendingLobby{};

    SessionPhase NetSession::CurrentSessionPhase() noexcept
    {
        return _serverSession.has_value() ? _serverSession->Phase : SessionPhase::InMatch;
    }

    std::uint16_t NetSession::SessionRevision() noexcept
    {
        return _serverSession.has_value() ? _serverSession->Revision : std::uint16_t{0};
    }

    std::optional<MatchDefinition> NetSession::ActiveMatchDefinition()
    {
        return _serverSession.has_value() ? std::optional<MatchDefinition>(_serverSession->Match) : std::nullopt;
    }

    bool NetSession::LocalIsLobbyOwner() noexcept
    {
        return _localSlot >= 0 && _serverSession.has_value() && _serverSession->OwnerSlot == _localSlot;
    }

    bool NetSession::IsInLobby() noexcept { return CurrentSessionPhase() == SessionPhase::Lobby; }
    bool NetSession::IsStarting() noexcept { return CurrentSessionPhase() == SessionPhase::Starting; }
    bool NetSession::IsPlaying() noexcept { return CurrentSessionPhase() == SessionPhase::InMatch; }
    bool NetSession::IsPostMatch() noexcept { return CurrentSessionPhase() == SessionPhase::PostMatch; }
    bool NetSession::CanEditLobby() noexcept { return IsInLobby() && LocalIsLobbyOwner(); }

    bool NetSession::PersistentLobby() noexcept
    {
        return _serverSession.has_value() && _serverSession->Policy == ServerSessionPolicy::Lobby;
    }

    bool NetSession::FreezeGameplay() noexcept
    {
        return PersistentLobby()
            && (CurrentSessionPhase() == SessionPhase::Lobby || CurrentSessionPhase() == SessionPhase::Starting);
    }

    bool NetSession::ShouldLoadMatch() noexcept
    {
        if (!_serverSession.has_value())
        {
            return false;
        }
        const SessionStatePacket& session = *_serverSession;
        return session.Phase == SessionPhase::InMatch || (session.Phase == SessionPhase::Starting
            && _localSlot >= 0 && (session.ExpectedParticipants & (1 << _localSlot)) != 0);
    }

    std::int32_t NetSession::ConnectionPort()
    {
        return _transport != nullptr ? _transport->LocalPort() : -1;
    }

    bool NetSession::SessionTimedOut()
    {
        return IsClient() && !DemoPlayback::IsActive() && _hostEndPoint != nullptr
            && Clock() - _lastServerPacket > NetConfig::TimeoutSeconds;
    }

    double NetSession::Clock()
    {
        return static_cast<double>(Runtime::StopwatchGetTimestamp()) / static_cast<double>(Runtime::StopwatchFrequency());
    }

    void NetSession::Pump(double time)
    {
        Update(time);
    }

    bool NetSession::SendLobbyCommand(LobbyCommandType type, std::uint8_t targetSlot,
        std::int8_t team, bool ready, std::optional<SessionStatePacket> configuration)
    {
        if (!Active() || !_serverSession.has_value() || !_pendingLobby.empty())
        {
            return false;
        }
        std::uint32_t id = ++_nextCommandId;
        if (id == 0)
        {
            id = ++_nextCommandId;
        }
        LobbyCommandPacket command{};
        command.CommandId = id;
        command.ExpectedRevision = SessionRevision();
        command.Type = type;
        command.TargetSlot = targetSlot;
        command.TeamIndex = team;
        command.Ready = ready;
        command.Configuration = configuration.value_or(*_serverSession);
        PendingLobbyCommand pending{};
        pending.Packet = command;
        pending.SentAt = Clock();
        pending.Attempts = 0;
        _pendingLobby.emplace(id, pending);
        _lobbyMessage = "Waiting for server...";
        SendLobbyPacket(command);
        return true;
    }

    void NetSession::SendLobbyPacket(const LobbyCommandPacket& command)
    {
        command.Write(_scratch);
        if (_hostEndPoint != nullptr && _transport != nullptr)
        {
            _transport->Send(_hostEndPoint, PacketType::LobbyCommand,
                std::span<const std::uint8_t>(_scratch).first(LobbyCommandPacket::Size));
        }
    }

    void NetSession::PumpLobby(double now)
    {
        for (auto it = _pendingLobby.begin(); it != _pendingLobby.end(); ++it)
        {
            PendingLobbyCommand& pending = it->second;
            if (now - pending.SentAt < std::min(1.0, 0.25 * (pending.Attempts + 1)))
            {
                continue;
            }
            if (pending.Attempts >= 4)
            {
                _lobbyMessage = "The server did not acknowledge the command. Check the current lobby and try again.";
                _pendingLobby.erase(it);
                break;
            }
            pending.Attempts++;
            pending.SentAt = now;
            SendLobbyPacket(pending.Packet);
        }
        if (_loadedMatch.has_value() && _serverSession.has_value() && *_loadedMatch == _serverSession->MatchId
            && IsStarting() && now - _lastLoadAck >= 0.25)
        {
            MarkMatchLoaded();
        }
        // Identity updates are also eventually reliable, without a second identity protocol.
        if (now - _lastIdentity >= 1)
        {
            _lastIdentity = now;
            SendIdentify();
        }
    }

    void NetSession::ApplySessionState(SessionStatePacket state)
    {
        // Match control can arrive before its session packet. Check that
        // stream too, before a stale lobby packet resets the running world.
        if (state.MatchId == 0 || state.AuthorityEpoch == 0)
        {
            return;
        }
        if (_serverMatch.has_value())
        {
            const MatchStatePacket& match = *_serverMatch;
            if (state.AuthorityEpoch < match.AuthorityEpoch
                || (state.AuthorityEpoch == match.AuthorityEpoch && state.MatchId != match.MatchId
                    && !NetLifecycleTracker::Newer(state.MatchId, match.MatchId)))
            {
                return;
            }
        }
        if (_serverSession.has_value())
        {
            const SessionStatePacket& old = *_serverSession;
            if (state.AuthorityEpoch != old.AuthorityEpoch
                && !NetLifecycleTracker::Newer(state.AuthorityEpoch, old.AuthorityEpoch))
            {
                return;
            }
            if (state.AuthorityEpoch == old.AuthorityEpoch && state.Revision != old.Revision
                && !SessionStatePacket::IsNewer(state.Revision, old.Revision))
            {
                return;
            }
        }
        const bool newMatch = !_serverSession.has_value() || _serverSession->MatchId != state.MatchId;
        if (state.Policy == ServerSessionPolicy::Lobby
            && (newMatch || (state.Phase == SessionPhase::Lobby && !IsInLobby())))
        {
            ResetMatchState();
        }
        _serverSession = state;
        if (!_serverMatch.has_value() || _serverMatch->MatchId != state.MatchId
            || _serverMatch->AuthorityEpoch != state.AuthorityEpoch)
        {
            MatchStatePacket match{};
            match.RoomKey = state.Match.RoomKey;
            match.Mode = static_cast<std::uint8_t>(state.Match.Mode);
            match.AuthorityEpoch = state.AuthorityEpoch;
            match.PointGoal = state.Match.PointGoal;
            match.TimeRemaining = state.Match.TimeLimitSeconds;
            match.MatchId = state.MatchId;
            match.Flags = static_cast<std::uint8_t>(MatchStatePacket::FlagInProgress
                | (state.Match.FriendlyFire ? MatchStatePacket::FlagFriendlyFire : 0)
                | (state.Match.ShadowFreeze ? 0 : MatchStatePacket::FlagNoShadowFreeze)
                | MatchStatePacket::RuleFlags(1, state.Match.AffinityWeapons));
            ApplyMatchState(match, false);
        }
        if (newMatch)
        {
            _loadedMatch.reset();
        }
    }

    void NetSession::ApplyLobbyResult(const LobbyCommandResultPacket& result)
    {
        if (_pendingLobby.erase(result.CommandId) == 0)
        {
            return;
        }
        _lobbyMessage = result.ResultCode == LobbyResultCode::Ok ? std::string() : result.Reason;
    }

    void NetSession::MarkMatchLoaded()
    {
        if (!PersistentLobby() || !_serverSession.has_value() || _hostEndPoint == nullptr)
        {
            return;
        }
        if (_loadedMatch.has_value() && *_loadedMatch == _serverSession->MatchId && Clock() - _lastLoadAck < 0.25)
        {
            return;
        }
        _loadedMatch = _serverSession->MatchId;
        _lastLoadAck = Clock();
        MatchLoadedPacket{*_loadedMatch}.Write(_scratch);
        if (_transport != nullptr)
        {
            _transport->Send(_hostEndPoint, PacketType::MatchLoaded,
                std::span<const std::uint8_t>(_scratch).first(MatchLoadedPacket::Size));
        }
    }

    void NetSession::ReportMatchLoadFailed(const std::string& reason)
    {
        if (!_serverSession.has_value() || _hostEndPoint == nullptr)
        {
            return;
        }
        MatchLoadFailedPacket{_serverSession->MatchId, reason}.Write(_scratch);
        if (_transport != nullptr)
        {
            _transport->Send(_hostEndPoint, PacketType::MatchLoadFailed,
                std::span<const std::uint8_t>(_scratch).first(MatchLoadFailedPacket::Size));
        }
    }

    // The socket, local slot, identity, authoritative roster and lobby state survive this reset.
    void NetSession::ResetMatchState()
    {
        NetHealthSync::BeginRoom();
        NetPlayerSetup::Reset();
        SpectatorMode::Reset();
        NetMatchSync::Reset();
        NetSlotManager::Reset();
        NetDamage::Reset();
        NetRoomChange::Reset();
        NetMatchEnd::Reset();
        NetPlayerBridge::Reset();
        NetUnlagged::Reset();
        NetHitPrediction::Reset();
        NetHitClaims::Reset();
        NetSmoothing::Reset();
        RemoteStateValid.fill(false);
        RemoteIntentValid.fill(false);
        RemoteIntentArrived.fill(0);
        _lastSlotIntentFrame.fill(0);
        _lastSnapshotFrame = 0;
        _snapshotArrived = 0;
        _appliedSnapshotFrame = 0;
        _hasSnapshot = false;
        ContinuousPhase.Reset();
        NetPlayerLifecycle::ResetLives();
    }

    void NetSession::ResetLobbySession()
    {
        _serverSession.reset();
        _pendingLobby.clear();
        _loadedMatch.reset();
        _rosterRevision = 0;
        _hasRoster = false;
        _ownerToken = Runtime::Guid::Empty();
        _rosterSessionRevision = 0;
        _lobbyMessage.clear();
        _lastLoadAck = _lastIdentity = 0;
        SlotTeamIndex.fill(-1);
        SlotLobbyReady.fill(false);
        Mods::Chat::NetChat::Clear();
    }

    RosterPacket NetSession::LobbyRoster()
    {
        RosterPacket roster = RosterPacket::Create();
        roster.Revision = _rosterRevision;
        roster.SessionRevision = _rosterSessionRevision;
        roster.MatchId = CurrentMatchId();
        roster.AuthorityEpoch = AuthorityEpoch();
        auto& slots = Runtime::RequireReference(roster.Slots);
        auto& teams = Runtime::RequireReference(roster.Teams);
        auto& generations = Runtime::RequireReference(roster.Generations);
        auto& ready = Runtime::RequireReference(roster.LobbyReady);
        auto& names = Runtime::RequireReference(roster.Names);
        auto& hunters = Runtime::RequireReference(roster.Hunters);
        auto& colors = Runtime::RequireReference(roster.Colors);
        auto& pings = Runtime::RequireReference(roster.Pings);
        for (std::int32_t slot = 0; slot < static_cast<std::int32_t>(SlotOccupied.size()); slot++)
        {
            const auto index = static_cast<std::size_t>(slot);
            if (!SlotOccupied[index])
            {
                continue;
            }
            const std::int32_t at = roster.Count++;
            Runtime::ManagedAt(slots, at) = static_cast<std::uint8_t>(slot);
            Runtime::ManagedAt(teams, at) = SlotTeamIndex[index];
            Runtime::ManagedAt(generations, at) = NetPlayerLifecycle::Generation(slot);
            Runtime::ManagedAt(ready, at) = SlotLobbyReady[index];
            Runtime::ManagedAt(names, at) = GameState::Nicknames()[slot];
            Runtime::ManagedAt(hunters, at) = static_cast<std::uint8_t>(SlotHunter[index]);
            Runtime::ManagedAt(colors, at) = static_cast<std::uint8_t>(PlayerColors::Choice[slot]);
            Runtime::ManagedAt(pings, at) = static_cast<std::uint16_t>(SlotPing[index]);
        }
        return roster;
    }
}
