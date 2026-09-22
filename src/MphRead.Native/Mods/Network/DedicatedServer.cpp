#include "DedicatedServer.hpp"

#include "../../GameState.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/DateTime.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../Update/ServerUpdate.hpp"
#include "NetMaster.hpp"
#include "NetSession.hpp"
#include "ServerSim.hpp"

#include "MapRotation.hpp"
#include "NetTransport.hpp"
#include "../../Metadata/Metadata.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace MphRead::Mods::Network
{
    DedicatedServer::DedicatedServer(std::int32_t port, std::int32_t maxPlayers,
        std::shared_ptr<MapRotation> rotation)
        : _voteMode(GameMode::Battle),
          _port(port),
          _maxPlayers(std::clamp(maxPlayers, 2, Entities::PlayerEntity::SlotCapacity)),
          _rotation(rotation != nullptr ? std::move(rotation) : std::make_shared<MapRotation>()),
          _boundPort(port),
          _serverName(NativeRuntime::EnvironmentMachineName())
    {
    }

    DedicatedServer::~DedicatedServer() = default;

    bool DedicatedServer::AllowMapVotes() const noexcept
    {
        return _allowMapVotes;
    }

    void DedicatedServer::AllowMapVotes(bool value) noexcept
    {
        _allowMapVotes = value;
    }

    const std::string& DedicatedServer::ServerName() const noexcept
    {
        return _serverName;
    }

    void DedicatedServer::ServerName(std::string value)
    {
        _serverName = std::move(value);
    }

    std::int32_t DedicatedServer::PeerCount() const noexcept
    {
        return _peerCount.load();
    }

    bool DedicatedServer::EverOccupied() const noexcept
    {
        return _everOccupied.load();
    }

    bool DedicatedServer::Listening() const noexcept
    {
        return _listening.load();
    }

    std::int32_t DedicatedServer::BoundPort() const noexcept
    {
        return _boundPort.load();
    }

    std::shared_ptr<MasterReporter> DedicatedServer::Reporter() const noexcept
    {
        return _reporter;
    }

    void DedicatedServer::Reporter(std::shared_ptr<MasterReporter> value) noexcept
    {
        _reporter = std::move(value);
    }

    bool DedicatedServer::FriendlyFire() const noexcept
    {
        return _friendlyFire;
    }

    void DedicatedServer::FriendlyFire(bool value) noexcept
    {
        _friendlyFire = value;
    }

    bool DedicatedServer::ShadowFreeze() const noexcept
    {
        return _shadowFreeze;
    }

    void DedicatedServer::ShadowFreeze(bool value) noexcept
    {
        _shadowFreeze = value;
    }

    bool DedicatedServer::AutoUpdate() const noexcept
    {
        return _autoUpdate;
    }

    void DedicatedServer::AutoUpdate(bool value) noexcept
    {
        _autoUpdate = value;
    }

    bool DedicatedServer::Simulate() const noexcept
    {
        return _simulate;
    }

    void DedicatedServer::Simulate(bool value) noexcept
    {
        _simulate = value;
    }

    bool DedicatedServer::Simulating() const
    {
        return _sim != nullptr && _sim->Running();
    }

    double DedicatedServer::EndSequenceFor() const
    {
        bool all = true;
        std::int32_t counted = 0;
        for (std::size_t i = 0; i < _peers.size(); i++)
        {
            counted++;
            if (!_peers[i]->Ready)
            {
                all = false;
                break;
            }
        }
        const double wait = counted == 0 || all ? AllReadySeconds : ReadyWaitSeconds;
        const double endSequenceSeconds = 3.0 + GameState::MatchEndingSeconds + 1.0;
        return std::max(endSequenceSeconds, wait);
    }

    void DedicatedServer::Run(std::stop_token cancel)
    {
        _transport = std::make_unique<NetTransport>(_port);
        _boundPort.store(_transport->LocalPort());
        _listening.store(true);
        _running.store(true);
        Log("listening on UDP "
            + NativeRuntime::Int32ToString((_transport->LocalPort()))
            + ", up to " + NativeRuntime::Int32ToString(_maxPlayers)
            + " players");
        StartSimulation();
        Log(Simulating()
            ? "authority mode: this server simulates the match itself"
            : "relay mode: the first client to connect is the simulation authority");
        Log("rotation: "
            + NativeRuntime::Int32ToString((static_cast<std::int32_t>(_rotation->Entries().size())))
            + " map(s), starting on " + _rotation->Current()->ToString());

        const std::uint16_t listenPort = static_cast<std::uint16_t>(_transport->LocalPort());
        const auto clock = std::chrono::steady_clock::now();
        double lastReport = 0.0;
        double lastStateBroadcast = 0.0;
        _matchStarted = 0.0;

        try
        {
            while (_running.load() && !cancel.stop_requested())
            {
                const double now = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - clock).count();
                _now = now;
                for (ReceivedPacket packet : _transport->Drain())
                {
                    Handle(packet, now);
                }
                DropTimedOut(now);
                if (_sim != nullptr)
                {
                    _sim->Advance(now);
                }

                const float limit = _rotation->Current()->TimeLimit;
                if (_matchEndedAt < 0.0 && limit > 0.0F && !_peers.empty()
                    && now - _matchStarted >= limit)
                {
                    EndMatch(now, "time limit");
                }
                else if (_matchEndedAt >= 0.0 && now - _matchEndedAt >= EndSequenceFor())
                {
                    AdvanceMap(now);
                }

                if (now - lastStateBroadcast >= 1.0)
                {
                    lastStateBroadcast = now;
                    PingPeers(now);
                    BroadcastMatchState(now);
                    BroadcastRoster();
                    Tally(now);
                    BroadcastVoteState(now);
                    if (_authority != nullptr && !Simulating())
                    {
                        NotifyAuthority(_authority);
                    }
                    if (_reporter != nullptr)
                    {
                        _reporter->Beat(now, _serverName,
                            listenPort, static_cast<std::uint8_t>(_peers.size()),
                            static_cast<std::uint8_t>(_maxPlayers),
                            static_cast<std::uint8_t>(_rotation->Current()->Mode),
                            _rotation->Current()->RoomKey);
                    }
                }

                if (_autoUpdate
                    && Update::ServerUpdate::ShouldRestart(
                        static_cast<std::int32_t>(_peers.size())))
                {
                    Log("shutting down to come back on the new build");
                    _running.store(false);
                    break;
                }

                if (now - lastReport >= 30.0)
                {
                    lastReport = now;
                    std::string authority;
                    if (Simulating())
                    {
                        authority = ", authority = this server";
                    }
                    else if (_authority != nullptr)
                    {
                        authority = ", authority = slot "
                            + NativeRuntime::Int32ToString(_authority->SlotIndex);
                    }
                    else
                    {
                        authority = ", no authority";
                    }
                    std::string line = NativeRuntime::Int32ToString((static_cast<std::int32_t>(_peers.size())))
                        + " peer(s) connected" + authority
                        + ", map " + _rotation->Current()->RoomKey;
                    if (limit > 0.0F)
                    {
                        line += ", " + NativeRuntime::DoubleToStringNoDecimals((std::max(0.0, static_cast<double>(limit) - (now - _matchStarted))))
                            + " s left";
                    }
                    if (_transport != nullptr && _transport->PacketsDropped() > 0)
                    {
                        line += ", " + NativeRuntime::Int64ToString((_transport->PacketsDropped())) + " packet(s) dropped";
                    }
                    Log(line);
                    if (_sim != nullptr)
                    {
                        Log("sim: " + _sim->Describe());
                        Log("sim: " + _sim->DescribeUnlagged());
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(
                    _peers.empty() && !Simulating() ? 20 : 1));
            }
        }
        catch (...)
        {
            Shutdown(listenPort);
            throw;
        }
        Shutdown(listenPort);
    }

    void DedicatedServer::Shutdown(std::uint16_t listenPort)
    {
        Log("shutting down");
        _running.store(false);
        try
        {
            if (_reporter != nullptr)
            {
                _reporter->Farewell(listenPort);
            }
        }
        catch (...)
        {
        }
        if (_reporter != nullptr)
        {
            _reporter->Dispose();
        }
        _reporter.reset();
        if (_transport != nullptr)
        {
            _transport->Dispose();
        }
        _transport.reset();
        _boundPort.store(_port);
        _listening.store(false);
        if (_sim != nullptr)
        {
            _sim->Stop();
        }
        _sim.reset();
    }

    void DedicatedServer::EndMatch(double now, const std::string& reason)
    {
        if (_matchEndedAt >= 0.0)
        {
            return;
        }
        _matchEndedAt = now;
        Log("match over on " + _rotation->Current()->RoomKey + " (" + reason + "); "
            + _rotation->Next()->RoomKey + " in "
            + NativeRuntime::DoubleToStringNoDecimals((EndSequenceFor())) + " s");
        BroadcastMatchState(now);
    }

    void DedicatedServer::AdvanceMap(double now)
    {
        const std::shared_ptr<const RotationEntry> entry = _rotation->Advance();
        _matchStarted = now;
        _matchEndedAt = -1.0;
        _matchId++;
        if (_voteRunning)
        {
            _voteRunning = false;
            _voteResolvedAt = now;
            _voteResult = VoteStatePacket::StateFailed;
            for (std::size_t i = 0; i < _peers.size(); i++)
            {
                _peers[i]->Ballot = 0;
            }
        }
        for (std::size_t i = 0; i < _peers.size(); i++)
        {
            _peers[i]->Ready = false;
        }
        Log("rotating to " + entry->ToString());
        MatchStatePacket state = BuildState(now);
        if (_sim != nullptr)
        {
            NetSession::ApplyMatchState(state, true);
        }
        state.Write(_scratch);
        for (std::size_t i = 0; i < _peers.size(); i++)
        {
            if (_transport != nullptr)
            {
                _transport->Send(_peers[i]->EndPoint, PacketType::MapChange,
                    std::span<const std::uint8_t>(_scratch.data(), MatchStatePacket::Size));
            }
        }
    }

    MatchStatePacket DedicatedServer::BuildState(double now) const
    {
        const std::shared_ptr<const RotationEntry> entry = _rotation->Current();
        const float elapsed = static_cast<float>(now - _matchStarted);
        const bool ending = _matchEndedAt >= 0.0;
        MatchStatePacket state{};
        state.Mode = static_cast<std::uint8_t>(entry->Mode);
        state.TimeRemaining = ending || entry->TimeLimit <= 0.0F
            ? 0.0F
            : std::max(0.0F, entry->TimeLimit - elapsed);
        state.TimeElapsed = elapsed;
        state.PlayerCount = static_cast<std::uint8_t>(_peers.size());
        state.Flags = static_cast<std::uint8_t>(
            (ending ? MatchStatePacket::FlagEnding : MatchStatePacket::FlagInProgress)
            | (_friendlyFire ? MatchStatePacket::FlagFriendlyFire : 0)
            | (_shadowFreeze ? 0 : MatchStatePacket::FlagNoShadowFreeze));
        state.PointGoal = static_cast<std::uint16_t>(
            std::clamp(entry->PointGoal, 0, static_cast<std::int32_t>(std::numeric_limits<std::uint16_t>::max())));
        state.MatchId = _matchId;
        state.RoomKey = entry->RoomKey;
        state.NextRoomKey = _rotation->Next()->RoomKey;
        return state;
    }

    void DedicatedServer::BroadcastMatchState(double now)
    {
        MatchStatePacket state = BuildState(now);
        if (_sim != nullptr)
        {
            NetSession::ApplyMatchState(state, false);
        }
        if (_peers.empty())
        {
            return;
        }
        state.Write(_scratch);
        for (std::size_t i = 0; i < _peers.size(); i++)
        {
            if (_transport != nullptr)
            {
                _transport->Send(_peers[i]->EndPoint, PacketType::MatchState,
                    std::span<const std::uint8_t>(_scratch.data(), MatchStatePacket::Size));
            }
        }
    }

    void DedicatedServer::StartSimulation()
    {
        if (!_simulate)
        {
            return;
        }
        std::string why;
        if (!ServerSim::Available(why))
        {
            Log("-simulate asked for, but " + why);
            Log("carrying on as a relay; the first client to connect will be the authority");
            return;
        }
        std::shared_ptr<ServerSim> sim = std::make_shared<ServerSim>();
        const std::shared_ptr<const RotationEntry> entry = _rotation->Current();
        if (!sim->Start(entry->RoomKey, entry->Mode,
            _maxPlayers,
            [this](std::span<const std::uint8_t> payload) { SendSnapshot(payload); },
            [this]() { EndMatch(_now, "score"); }))
        {
            Log("carrying on as a relay; the first client to connect will be the authority");
            return;
        }
        _sim = std::move(sim);
        SyncSimulationState(_now);
    }

    void DedicatedServer::SendSnapshot(std::span<const std::uint8_t> payload)
    {
        _lastSnapshot = std::make_shared<std::vector<std::uint8_t>>(payload.begin(), payload.end());
        for (std::size_t i = 0; i < _peers.size(); i++)
        {
            if (_transport != nullptr)
            {
                _transport->Send(_peers[i]->EndPoint, PacketType::Snapshot, payload);
            }
        }
    }

    void DedicatedServer::SyncSimulationState(double now)
    {
        if (_sim == nullptr)
        {
            return;
        }
        NetSession::ApplyRoster(BuildRoster());
        NetSession::ApplyMatchState(BuildState(now), false);
    }

    void DedicatedServer::Stop() noexcept
    {
        _running.store(false);
    }

    void DedicatedServer::Handle(const ReceivedPacket& packet, double now)
    {
        switch (packet.Type())
        {
        case PacketType::Hello:
            HandleHello(packet, now);
            break;
        case PacketType::Intent:
            HandleIntent(packet, now);
            break;
        case PacketType::Snapshot:
            HandleSnapshot(packet, now);
            break;
        case PacketType::Bye:
            HandleBye(packet);
            break;
        case PacketType::Identify:
            HandleIdentify(packet, now);
            break;
        case PacketType::Ping:
            if (_transport != nullptr)
            {
                _transport->Send(packet.Sender, PacketType::Pong, std::span<const std::uint8_t>{});
            }
            break;
        case PacketType::Pong:
            HandlePong(packet, now);
            break;
        case PacketType::StatusQuery:
            SendStatus(packet.Sender, now);
            break;
        case PacketType::MatchEnd:
            HandleMatchEnd(packet, now);
            break;
        case PacketType::Chat:
            HandleChat(packet, now);
            break;
        case PacketType::Vote:
            HandleVote(packet, now);
            break;
        default:
            break;
        }
    }

    void DedicatedServer::HandleVote(const ReceivedPacket& packet, double now)
    {
        std::shared_ptr<Peer> peer = Find(packet.Sender);
        const std::span<const std::uint8_t> payload = packet.Payload();
        if (peer == nullptr || peer->SlotIndex < 0
            || payload.size() < static_cast<std::size_t>(VotePacket::Size))
        {
            return;
        }
        peer->LastSeen = now;
        if (!_allowMapVotes)
        {
            return;
        }
        const VotePacket vote = VotePacket::Read(payload);
        if (vote.Kind == VotePacket::KindPropose)
        {
            StartVote(peer, vote.RoomKey.value_or(std::string{}), now);
            return;
        }
        if (!_voteRunning
            || (vote.Kind != VotePacket::KindYes && vote.Kind != VotePacket::KindNo))
        {
            return;
        }
        if (peer->Ballot != 0)
        {
            return;
        }
        peer->Ballot = vote.Kind;
        BroadcastVoteState(now);
        Tally(now);
    }

    void DedicatedServer::StartVote(const std::shared_ptr<Peer>& peer,
        const std::string& roomKey, double now)
    {
        if (_voteRunning)
        {
            Tell(peer, "a vote is already running");
            return;
        }
        if (static_cast<std::int32_t>(_peers.size()) < VoteMinimumPlayers)
        {
            Tell(peer, "not enough players to hold a vote");
            return;
        }
        const double sinceVote = now - _voteResolvedAt;
        if (sinceVote < VoteCooldownSeconds)
        {
            Tell(peer, "another vote may be called in "
                + NativeRuntime::DoubleToStringNoDecimals((VoteCooldownSeconds - sinceVote))
                + " s");
            return;
        }
        const double sinceMine = now - peer->LastProposal;
        if (sinceMine < ProposalCooldownSeconds)
        {
            Tell(peer, "you may propose again in "
                + NativeRuntime::DoubleToStringNoDecimals((ProposalCooldownSeconds - sinceMine))
                + " s");
            return;
        }
        const std::optional<std::string> resolved = ResolveRoomKey(roomKey);
        if (!resolved.has_value())
        {
            Tell(peer, "no map called \"" + roomKey + "\"");
            return;
        }
        if (NativeRuntime::StringEqualsOrdinalIgnoreCase((*resolved), (_rotation->Current()->RoomKey)))
        {
            Tell(peer, "that is the map you are on");
            return;
        }
        _voteRunning = true;
        _voteRoom = *resolved;
        _voteMode = ModeForRoom(*resolved);
        _voteProposer = !peer->Name.empty()
            ? peer->Name
            : "Player" + NativeRuntime::Int32ToString((peer->SlotIndex + 1));
        _voteProposerSlot = peer->SlotIndex;
        _voteStartedAt = now;
        _voteResult = VoteStatePacket::StateIdle;
        peer->LastProposal = now;
        for (std::size_t i = 0; i < _peers.size(); i++)
        {
            _peers[i]->Ballot = 0;
        }
        peer->Ballot = VotePacket::KindYes;
        Announce(_voteProposer + " proposes " + *resolved
            + " -- F1 to accept, F2 to deny");
        Log("vote started by slot "
            + NativeRuntime::Int32ToString(peer->SlotIndex)
            + " for " + *resolved + " ("
            + ::MphRead::ToString(_voteMode) + ")");
        BroadcastVoteState(now);
        Tally(now);
    }

    void DedicatedServer::Tally(double now)
    {
        if (!_voteRunning)
        {
            return;
        }
        const auto [yes, no, eligible, needed] = CountVotes();
        if (yes >= needed)
        {
            ResolveVote(now, true,
                NativeRuntime::Int32ToString(yes) + " of "
                    + NativeRuntime::Int32ToString(eligible));
            return;
        }
        if (eligible - no < needed)
        {
            ResolveVote(now, false,
                NativeRuntime::Int32ToString(yes) + " of "
                    + NativeRuntime::Int32ToString(eligible));
            return;
        }
        if (now - _voteStartedAt >= VoteSeconds)
        {
            ResolveVote(now, false,
                NativeRuntime::Int32ToString(yes) + " of "
                    + NativeRuntime::Int32ToString(eligible));
        }
    }

    std::tuple<std::int32_t, std::int32_t, std::int32_t, std::int32_t>
        DedicatedServer::CountVotes() const
    {
        std::int32_t yes = 0;
        std::int32_t no = 0;
        for (std::size_t i = 0; i < _peers.size(); i++)
        {
            if (_peers[i]->Ballot == VotePacket::KindYes)
            {
                yes++;
            }
            else if (_peers[i]->Ballot == VotePacket::KindNo)
            {
                no++;
            }
        }
        const std::int32_t eligible = static_cast<std::int32_t>(_peers.size());
        const std::int32_t needed = std::max(1,
            static_cast<std::int32_t>(std::ceil(eligible * VoteThreshold)));
        return {yes, no, eligible, needed};
    }

    void DedicatedServer::ResolveVote(double now, bool passed, const std::string& count)
    {
        const std::string room = _voteRoom;
        const GameMode mode = _voteMode;
        _voteRunning = false;
        _voteResolvedAt = now;
        _voteResult = passed ? VoteStatePacket::StatePassed : VoteStatePacket::StateFailed;
        for (std::size_t i = 0; i < _peers.size(); i++)
        {
            _peers[i]->Ballot = 0;
        }
        if (passed)
        {
            Announce("vote passed (" + count + ") -- changing to " + room);
            Log("vote passed (" + count + ") for " + room);
            _rotation->PlayNext(room, mode);
            AdvanceMap(now);
        }
        else
        {
            Announce("vote failed (" + count + ") -- staying on "
                + _rotation->Current()->RoomKey);
            Log("vote failed (" + count + ") for " + room);
        }
        BroadcastVoteState(now);
    }

    void DedicatedServer::ReviewVote(double now)
    {
        if (!_voteRunning)
        {
            return;
        }
        if (static_cast<std::int32_t>(_peers.size()) < VoteMinimumPlayers)
        {
            ResolveVote(now, false, "not enough players");
            return;
        }
        Tally(now);
    }

    std::optional<std::string> DedicatedServer::ResolveRoomKey(const std::string& roomKey)
    {
        if (NativeRuntime::StringIsNullOrWhiteSpace(roomKey))
        {
            return std::nullopt;
        }
        const std::string wanted = NativeRuntime::StringTrim(std::string(roomKey));
        for (const auto& entry : Metadata::RoomMetadata)
        {
            if (entry.second == nullptr)
            {
                throw std::runtime_error("Metadata.RoomMetadata contains a null value.");
            }
            if (entry.second->Multiplayer
                && NativeRuntime::StringEqualsOrdinalIgnoreCase(entry.first, wanted))
            {
                return entry.first;
            }
        }
        return std::nullopt;
    }

    GameMode DedicatedServer::ModeForRoom(const std::string& roomKey) const
    {
        for (const std::shared_ptr<const RotationEntry>& entry : _rotation->Entries())
        {
            if (NativeRuntime::StringEqualsOrdinalIgnoreCase(entry->RoomKey, roomKey))
            {
                return entry->Mode;
            }
        }
        return _rotation->Current()->Mode;
    }

    void DedicatedServer::BroadcastVoteState(double now)
    {
        if (_peers.empty())
        {
            return;
        }
        VoteStatePacket state{};
        state.State = _voteRunning ? VoteStatePacket::StateRunning : _voteResult;
        state.RoomKey = _voteRunning ? _voteRoom : std::string{};
        state.Proposer = _voteRunning ? _voteProposer : std::string{};
        if (_voteRunning)
        {
            const auto [yes, no, eligible, needed] = CountVotes();
            state.Yes = static_cast<std::uint8_t>(yes);
            state.No = static_cast<std::uint8_t>(no);
            state.Eligible = static_cast<std::uint8_t>(eligible);
            state.Needed = static_cast<std::uint8_t>(needed);
            state.Seconds = static_cast<std::uint16_t>(
                std::max(0.0, VoteSeconds - (now - _voteStartedAt)));
        }
        else if (_allowMapVotes)
        {
            const double wait = VoteCooldownSeconds - (now - _voteResolvedAt);
            state.Seconds = static_cast<std::uint16_t>(
                std::clamp(wait, 0.0, static_cast<double>(std::numeric_limits<std::uint16_t>::max())));
        }
        else
        {
            state.Seconds = std::numeric_limits<std::uint16_t>::max();
        }
        state.Write(_scratch);
        for (std::size_t i = 0; i < _peers.size(); i++)
        {
            if (_transport != nullptr)
            {
                _transport->Send(_peers[i]->EndPoint, PacketType::VoteState,
                    std::span<const std::uint8_t>(_scratch.data(), VoteStatePacket::Size));
            }
        }
    }

    void DedicatedServer::Tell(const std::shared_ptr<Peer>& peer, const std::string& text)
    {
        ChatPacket chat{};
        chat.Slot = 0xFF;
        chat.Kind = ChatPacket::KindSystem;
        chat.Name = std::string{};
        chat.Text = text;
        chat.Write(_scratch);
        if (_transport != nullptr)
        {
            _transport->Send(peer->EndPoint, PacketType::Chat,
                std::span<const std::uint8_t>(_scratch.data(), ChatPacket::Size));
        }
    }

    void DedicatedServer::HandleChat(const ReceivedPacket& packet, double now)
    {
        std::shared_ptr<Peer> peer = Find(packet.Sender);
        const std::span<const std::uint8_t> payload = packet.Payload();
        if (peer == nullptr || peer->SlotIndex < 0
            || payload.size() < static_cast<std::size_t>(ChatPacket::Size))
        {
            return;
        }
        peer->LastSeen = now;
        ChatPacket chat = ChatPacket::Read(payload);
        const std::string text = chat.Text.value_or(std::string{});
        if (text.empty())
        {
            return;
        }
        peer->ChatCredit = std::min(ChatBurst,
            peer->ChatCredit + (now - peer->ChatCreditAt) * ChatRatePerSecond);
        peer->ChatCreditAt = now;
        if (peer->ChatCredit < 1.0)
        {
            const std::int32_t oldDropped = peer->ChatDropped;
            peer->ChatDropped = static_cast<std::int32_t>(
                static_cast<std::uint32_t>(peer->ChatDropped) + 1U);
            if (oldDropped == 0)
            {
                Log("chat from slot "
                    + NativeRuntime::Int32ToString(peer->SlotIndex)
                    + " (" + peer->EndPoint->ToString() + ") dropped: too fast");
            }
            return;
        }
        peer->ChatCredit -= 1.0;
        peer->ChatDropped = 0;
        chat.Slot = static_cast<std::uint8_t>(peer->SlotIndex);
        chat.Name = !peer->Name.empty()
            ? peer->Name
            : "Player" + NativeRuntime::Int32ToString(peer->SlotIndex);
        chat.Kind = ChatPacket::KindSay;
        chat.Write(_scratch);
        for (std::size_t i = 0; i < _peers.size(); i++)
        {
            if (_peers[i] != peer && _transport != nullptr)
            {
                _transport->Send(_peers[i]->EndPoint, PacketType::Chat,
                    std::span<const std::uint8_t>(_scratch.data(), ChatPacket::Size));
            }
        }
        Log("chat " + chat.Name.value_or(std::string{}) + ": " + text);
    }

    void DedicatedServer::Announce(const std::string& text)
    {
        if (_peers.empty())
        {
            return;
        }
        ChatPacket chat{};
        chat.Slot = 0xFF;
        chat.Kind = ChatPacket::KindSystem;
        chat.Name = std::string{};
        chat.Text = text;
        chat.Write(_scratch);
        for (std::size_t i = 0; i < _peers.size(); i++)
        {
            if (_transport != nullptr)
            {
                _transport->Send(_peers[i]->EndPoint, PacketType::Chat,
                    std::span<const std::uint8_t>(_scratch.data(), ChatPacket::Size));
            }
        }
    }

    void DedicatedServer::SendStatus(const std::shared_ptr<System::Net::IPEndPoint>& sender,
        double now)
    {
        ServerStatusPacket status{};
        status.Match = BuildState(now);
        status.MaxPlayers = static_cast<std::uint8_t>(_maxPlayers);
        status.Protocol = static_cast<std::uint8_t>(NetConfig::ProtocolVersion);
        status.ServerName = _serverName;
        status.Write(_scratch);
        if (_transport != nullptr)
        {
            _transport->Send(sender, PacketType::StatusReply,
                std::span<const std::uint8_t>(_scratch.data(), ServerStatusPacket::Size));
        }
    }

    void DedicatedServer::HandleHello(const ReceivedPacket& packet, double now)
    {
        const std::span<const std::uint8_t> payload = packet.Payload();
        if (payload.size() < 1 || payload[0] != NetConfig::ProtocolVersion)
        {
            Log("rejected " + packet.Sender->ToString() + ": protocol mismatch");
            SendRefusal(packet.Sender, RefusedPacket::ReasonProtocol);
            return;
        }
        const std::uint32_t clientId = payload.size() >= 6
            ? static_cast<std::uint32_t>(payload[2])
                | (static_cast<std::uint32_t>(payload[3]) << 8)
                | (static_cast<std::uint32_t>(payload[4]) << 16)
                | (static_cast<std::uint32_t>(payload[5]) << 24)
            : 0U;
        std::shared_ptr<Peer> peer = Find(packet.Sender);
        if (peer == nullptr && clientId != 0)
        {
            for (std::size_t i = 0; i < _peers.size(); i++)
            {
                if (_peers[i]->ClientId == clientId)
                {
                    peer = _peers[i];
                    Log("slot " + NativeRuntime::Int32ToString(peer->SlotIndex)
                        + " (" + peer->Name + ") came back on " + packet.Sender->ToString()
                        + ", was " + peer->EndPoint->ToString());
                    peer->EndPoint = packet.Sender;
                    break;
                }
            }
        }
        if (peer == nullptr)
        {
            std::int32_t slot = -1;
            if (payload.size() >= 2 && payload[1] != 0xFF
                && payload[1] < _maxPlayers && SlotFree(payload[1]))
            {
                slot = payload[1];
            }
            if (slot < 0)
            {
                slot = NextFreeSlot();
            }
            if (slot < 0)
            {
                Log("rejected " + packet.Sender->ToString() + ": session full");
                SendRefusal(packet.Sender, RefusedPacket::ReasonFull);
                return;
            }
            if (_peers.empty())
            {
                _matchStarted = now;
                _matchEndedAt = -1.0;
                _matchId++;
            }
            peer = std::make_shared<Peer>();
            peer->EndPoint = packet.Sender;
            peer->SlotIndex = slot;
            _peers.push_back(peer);
            _peerCount.store(static_cast<std::int32_t>(_peers.size()));
            _everOccupied.store(true);
            if (_authority == nullptr && !Simulating())
            {
                _authority = peer;
                Log(packet.Sender->ToString() + " joined as slot "
                    + NativeRuntime::Int32ToString(slot) + " (authority)");
                NotifyAuthority(peer);
            }
            else
            {
                Log(packet.Sender->ToString() + " joined as slot "
                    + NativeRuntime::Int32ToString(slot));
                if (Simulating() && _lastSnapshot != nullptr && _transport != nullptr)
                {
                    _transport->Send(peer->EndPoint, PacketType::Snapshot, *_lastSnapshot);
                }
            }
        }
        peer->ClientId = clientId;
        peer->LastSeen = now;
        _scratch[0] = static_cast<std::uint8_t>(peer->SlotIndex);
        if (_transport != nullptr)
        {
            _transport->Send(peer->EndPoint, PacketType::Welcome,
                std::span<const std::uint8_t>(_scratch.data(), 1));
        }
        MatchStatePacket state = BuildState(now);
        state.Write(_scratch);
        if (_transport != nullptr)
        {
            _transport->Send(peer->EndPoint, PacketType::MatchState,
                std::span<const std::uint8_t>(_scratch.data(), MatchStatePacket::Size));
        }
        BroadcastRoster();
    }

    void DedicatedServer::HandleMatchEnd(const ReceivedPacket& packet, double now)
    {
        std::shared_ptr<Peer> peer = Find(packet.Sender);
        if (peer == nullptr)
        {
            return;
        }
        peer->LastSeen = now;
        if (Simulating() || peer != _authority)
        {
            return;
        }
        EndMatch(now, "a player reached the goal");
    }

    void DedicatedServer::SendRefusal(const std::shared_ptr<System::Net::IPEndPoint>& to,
        std::uint8_t reason)
    {
        RefusedPacket refusal{};
        refusal.Reason = reason;
        refusal.Players = static_cast<std::uint8_t>(_peers.size());
        refusal.MaxPlayers = static_cast<std::uint8_t>(_maxPlayers);
        refusal.Write(_scratch);
        if (_transport != nullptr)
        {
            _transport->Send(to, PacketType::Refused,
                std::span<const std::uint8_t>(_scratch.data(), RefusedPacket::Size));
        }
    }

    void DedicatedServer::HandleIdentify(const ReceivedPacket& packet, double now)
    {
        std::shared_ptr<Peer> peer = Find(packet.Sender);
        if (peer == nullptr)
        {
            return;
        }
        peer->LastSeen = now;
        const std::span<const std::uint8_t> payload = packet.Payload();
        if (payload.size() < 1)
        {
            return;
        }
        if (payload.size() < 2)
        {
            return;
        }
        const std::uint8_t hunter = payload[0];
        const std::uint8_t color = payload[1];
        std::string name = NativeRuntime::AsciiGetString(payload.subspan(2));
        while (!name.empty() && name.back() == '\0')
        {
            name.pop_back();
        }
        name = NativeRuntime::StringTrim(std::string(name));
        if (name.empty())
        {
            return;
        }
        if (name.size() > static_cast<std::size_t>(RosterPacket::MaxNameBytes))
        {
            name.resize(RosterPacket::MaxNameBytes);
        }
        if (peer->Name == name && peer->Hunter == hunter && peer->Color == color)
        {
            return;
        }
        const bool firstName = peer->Name.empty();
        peer->Name = name;
        peer->Hunter = hunter;
        peer->Color = color;
        Log("slot " + NativeRuntime::Int32ToString(peer->SlotIndex)
            + " is \"" + name + "\" playing "
            + ::MphRead::ToString(static_cast<MphRead::Hunter>(hunter))
            + " in suit " + NativeRuntime::Int32ToString((color + 1)));
        if (firstName)
        {
            Announce(name + " joined");
        }
        BroadcastRoster();
    }

    void DedicatedServer::NotifyAuthority(const std::shared_ptr<Peer>& peer)
    {
        if (_lastSnapshot != nullptr && _transport != nullptr)
        {
            _transport->Send(peer->EndPoint, PacketType::Snapshot, *_lastSnapshot);
        }
        _scratch[0] = 1;
        if (_transport != nullptr)
        {
            _transport->Send(peer->EndPoint, PacketType::Authority,
                std::span<const std::uint8_t>(_scratch.data(), 1));
        }
    }

    void DedicatedServer::PingPeers(double now)
    {
        for (std::size_t i = 0; i < _peers.size(); i++)
        {
            const std::shared_ptr<Peer>& peer = _peers[i];
            if (peer->PingPending && now - peer->PingSentAt < 5.0)
            {
                continue;
            }
            peer->PingId++;
            peer->PingSentAt = now;
            peer->PingPending = true;
            _scratch[0] = peer->PingId;
            if (_transport != nullptr)
            {
                _transport->Send(peer->EndPoint, PacketType::Ping,
                    std::span<const std::uint8_t>(_scratch.data(), 1));
            }
        }
    }

    void DedicatedServer::HandlePong(const ReceivedPacket& packet, double now)
    {
        std::shared_ptr<Peer> peer = Find(packet.Sender);
        if (peer == nullptr || !peer->PingPending)
        {
            return;
        }
        const std::span<const std::uint8_t> payload = packet.Payload();
        if (payload.size() < 1 || payload[0] != peer->PingId)
        {
            return;
        }
        peer->PingPending = false;
        peer->LastSeen = now;
        std::int32_t rtt = NativeRuntime::MathRoundToInt32(((now - peer->PingSentAt) * 1000.0));
        rtt = std::clamp(rtt, 0, 9999);
        peer->Ping = peer->Ping == 0 ? rtt : (peer->Ping * 2 + rtt) / 3;
    }

    RosterPacket DedicatedServer::BuildRoster() const
    {
        RosterPacket roster = RosterPacket::Create();
        for (std::size_t i = 0;
            i < _peers.size() && i < static_cast<std::size_t>(RosterPacket::MaxSlots); i++)
        {
            (*roster.Slots)[roster.Count] = static_cast<std::uint8_t>(_peers[i]->SlotIndex);
            (*roster.Hunters)[roster.Count] = _peers[i]->Hunter;
            (*roster.Colors)[roster.Count] = _peers[i]->Color;
            (*roster.Pings)[roster.Count] = static_cast<std::uint16_t>(
                std::clamp(_peers[i]->Ping, 0, 9999));
            (*roster.Names)[roster.Count] = !_peers[i]->Name.empty()
                ? _peers[i]->Name
                : "Player" + NativeRuntime::Int32ToString((_peers[i]->SlotIndex + 1));
            roster.Count++;
        }
        return roster;
    }

    void DedicatedServer::BroadcastRoster()
    {
        RosterPacket roster = BuildRoster();
        if (_sim != nullptr)
        {
            NetSession::ApplyRoster(roster);
        }
        if (_peers.empty())
        {
            return;
        }
        roster.Write(_scratch);
        for (std::size_t i = 0; i < _peers.size(); i++)
        {
            if (_transport != nullptr)
            {
                _transport->Send(_peers[i]->EndPoint, PacketType::Roster,
                    std::span<const std::uint8_t>(_scratch.data(), RosterPacket::Size));
            }
        }
    }

    void DedicatedServer::HandleIntent(const ReceivedPacket& packet, double now)
    {
        std::shared_ptr<Peer> peer = Find(packet.Sender);
        if (peer == nullptr || (_authority == nullptr && !Simulating()))
        {
            return;
        }
        peer->LastSeen = now;
        const std::span<const std::uint8_t> payload = packet.Payload();
        if (payload.size() >= static_cast<std::size_t>(IntentPacket::Size))
        {
            const IntentPacket intent = IntentPacket::Read(payload);
            if (_sim != nullptr)
            {
                NetSession::AcceptSlotIntent(peer->SlotIndex, intent);
            }
            if (peer->LastIntentFrame != 0 && intent.Frame <= peer->LastIntentFrame
                && peer->LastIntentFrame - intent.Frame < IntentResetGap)
            {
                return;
            }
            peer->LastIntentFrame = intent.Frame;
            peer->Ready = (intent.Buttons & IntentButtons::ReadyState) == IntentButtons::ReadyState;
        }
        _scratch[0] = static_cast<std::uint8_t>(peer->SlotIndex);
        if (payload.size() > _scratch.size() - 1)
        {
            throw std::length_error("Intent payload does not fit the SlotIntent scratch buffer.");
        }
        std::copy(payload.begin(), payload.end(), _scratch.begin() + 1);
        for (std::size_t i = 0; i < _peers.size(); i++)
        {
            if (_peers[i] != peer && _transport != nullptr)
            {
                _transport->Send(_peers[i]->EndPoint, PacketType::SlotIntent,
                    std::span<const std::uint8_t>(_scratch.data(), payload.size() + 1));
            }
        }
    }

    void DedicatedServer::HandleSnapshot(const ReceivedPacket& packet, double now)
    {
        std::shared_ptr<Peer> peer = Find(packet.Sender);
        if (peer == nullptr)
        {
            return;
        }
        peer->LastSeen = now;
        if (Simulating() || peer != _authority)
        {
            return;
        }
        const std::span<const std::uint8_t> payload = packet.Payload();
        _lastSnapshot = std::make_shared<std::vector<std::uint8_t>>(payload.begin(), payload.end());
        for (std::size_t i = 0; i < _peers.size(); i++)
        {
            if (_peers[i] != peer && _transport != nullptr)
            {
                _transport->Send(_peers[i]->EndPoint, PacketType::Snapshot, payload);
            }
        }
    }

    void DedicatedServer::HandleBye(const ReceivedPacket& packet)
    {
        std::shared_ptr<Peer> peer = Find(packet.Sender);
        if (peer != nullptr)
        {
            Remove(peer, "left");
        }
    }

    void DedicatedServer::DropTimedOut(double now)
    {
        for (std::int32_t i = static_cast<std::int32_t>(_peers.size()) - 1; i >= 0; i--)
        {
            if (now - _peers[static_cast<std::size_t>(i)]->LastSeen > NetConfig::TimeoutSeconds)
            {
                Remove(_peers[static_cast<std::size_t>(i)], "timed out");
            }
        }
    }

    void DedicatedServer::Remove(const std::shared_ptr<Peer>& peer, const std::string& reason)
    {
        const auto it = std::find(_peers.begin(), _peers.end(), peer);
        if (it != _peers.end())
        {
            _peers.erase(it);
            _peerCount.store(static_cast<std::int32_t>(_peers.size()));
        }
        BroadcastRoster();
        ReviewVote(_now);
        Log(peer->EndPoint->ToString() + " " + reason + " (slot "
            + NativeRuntime::Int32ToString(peer->SlotIndex) + ")");
        if (!peer->Name.empty())
        {
            Announce(peer->Name + " " + reason);
        }
        if (Simulating() || _authority != peer)
        {
            return;
        }
        _authority = !_peers.empty() ? _peers[0] : nullptr;
        Log(_authority != nullptr
            ? "authority moved to slot "
                + NativeRuntime::Int32ToString(_authority->SlotIndex)
            : "no peers left; waiting for a new authority");
        if (_authority != nullptr)
        {
            NotifyAuthority(_authority);
        }
    }

    std::shared_ptr<DedicatedServer::Peer> DedicatedServer::Find(
        const std::shared_ptr<System::Net::IPEndPoint>& endPoint) const
    {
        for (std::size_t i = 0; i < _peers.size(); i++)
        {
            if (_peers[i]->EndPoint->Equals(*endPoint))
            {
                return _peers[i];
            }
        }
        return nullptr;
    }

    bool DedicatedServer::SlotFree(std::int32_t slot) const
    {
        for (std::size_t i = 0; i < _peers.size(); i++)
        {
            if (_peers[i]->SlotIndex == slot)
            {
                return false;
            }
        }
        return true;
    }

    std::int32_t DedicatedServer::NextFreeSlot() const
    {
        for (std::int32_t slot = 0; slot < _maxPlayers; slot++)
        {
            bool taken = false;
            for (std::size_t i = 0; i < _peers.size(); i++)
            {
                if (_peers[i]->SlotIndex == slot)
                {
                    taken = true;
                    break;
                }
            }
            if (!taken)
            {
                return slot;
            }
        }
        return -1;
    }

    void DedicatedServer::Log(const std::string& message)
    {
        NativeRuntime::ConsoleWriteLine(("["
            + NativeRuntime::DateTimeToString(NativeRuntime::DateTimeNow(), "HH:mm:ss")
            + "] [server] " + message));
    }
}
