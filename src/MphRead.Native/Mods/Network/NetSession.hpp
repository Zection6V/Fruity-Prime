#pragma once

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Guid.hpp"
#include "ContinuousWeaponPhase.hpp"
#include "MatchDefinition.hpp"
#include "NetProtocol.hpp"
#include "NetTransport.hpp"
#include "SessionProtocol.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace MphRead::Mods::Network
{
    enum class NetRole : std::int32_t
    {
        Offline = 0,
        Host = 1,
        Client = 2,
        Server = 3
    };

    // NetRole.ToString().
    [[nodiscard]] std::string ToString(NetRole value);

    using SnapshotSink = std::function<void(std::span<const std::uint8_t>)>;

    class RemotePeer final
    {
    public:
        std::shared_ptr<System::Net::IPEndPoint> EndPoint{};
        std::int32_t SlotIndex = -1;
        IntentPacket LatestIntent{};
        std::uint32_t LastIntentFrame = 0;
        double LastSeenTime = 0.0;
        std::uint32_t ClientId = 0;
    };

    // NetSession.cs and NetSessionLobby.cs: one partial class, one header.
    class NetSession final
    {
    public:
        using MapChangedHandler = std::function<void(MatchStatePacket)>;

        NetSession() = delete;
        NetSession(const NetSession&) = delete;
        NetSession(NetSession&&) = delete;
        NetSession& operator=(const NetSession&) = delete;
        NetSession& operator=(NetSession&&) = delete;

        [[nodiscard]] static NetRole Role() noexcept { return _role; }
        [[nodiscard]] static bool Active() noexcept { return _role != NetRole::Offline; }
        [[nodiscard]] static bool IsHost() noexcept { return _role == NetRole::Host; }
        [[nodiscard]] static bool IsClient() noexcept { return _role == NetRole::Client; }
        [[nodiscard]] static bool IsServer() noexcept { return _role == NetRole::Server; }

        [[nodiscard]] static std::int32_t LocalSlot() noexcept { return _localSlot; }
        [[nodiscard]] static std::uint32_t NetFrame() noexcept { return _netFrame; }
        [[nodiscard]] static std::uint32_t LastSnapshotFrame() noexcept { return _lastSnapshotFrame; }
        [[nodiscard]] static std::uint32_t SnapshotArrived() noexcept { return _snapshotArrived; }
        [[nodiscard]] static std::uint32_t SnapshotAge() noexcept;
        [[nodiscard]] static const std::optional<std::string>& LastError() noexcept { return _lastError; }

        inline static std::array<PlayerState, Entities::PlayerEntity::SlotCapacity> RemoteStates{};
        inline static std::array<bool, Entities::PlayerEntity::SlotCapacity> RemoteStateValid{};
        inline static std::array<IntentPacket, Entities::PlayerEntity::SlotCapacity> RemoteIntents{};
        inline static std::array<bool, Entities::PlayerEntity::SlotCapacity> RemoteIntentValid{};
        inline static ContinuousWeaponPhase ContinuousPhase{Entities::PlayerEntity::SlotCapacity};
        inline static std::array<std::uint32_t, Entities::PlayerEntity::SlotCapacity> RemoteIntentArrived{};

        [[nodiscard]] static std::uint32_t RemoteIntentAge(std::int32_t slot) noexcept;

        [[nodiscard]] static std::int64_t SnapshotsReceived() noexcept { return _snapshotsReceived; }
        [[nodiscard]] static std::int64_t SnapshotsSent() noexcept { return _snapshotsSent; }
        [[nodiscard]] static std::int64_t StatesApplied() noexcept { return _statesApplied; }
        [[nodiscard]] static std::int64_t IntentsReceived() noexcept { return _intentsReceived; }
        static void NoteStatesApplied() noexcept;
        [[nodiscard]] static std::uint32_t AppliedSnapshotFrame() noexcept { return _appliedSnapshotFrame; }

        static void StartServerAuthority(SnapshotSink sink, std::function<void()> matchEnded);
        static void StartHost(std::int32_t port = NetConfig::DefaultPort);
        static void StartClient(const std::string& address,
            std::int32_t port = NetConfig::DefaultPort,
            ::MphRead::NativeRuntime::Guid ownerToken = {});
        static void StartPlayback();
        static void RewindPlayback();
        static void InjectPlaybackPacket(
            const std::shared_ptr<std::vector<std::uint8_t>>& data, std::int32_t length);
        static void Stop();

        static void SendIdentify();

        [[nodiscard]] static Hunter LocalHunter() noexcept { return _localHunter; }
        static void SetLocalHunter(Hunter value) noexcept { _localHunter = value; }
        [[nodiscard]] static std::int32_t LocalColor() noexcept { return _localColor; }
        static void SetLocalColor(std::int32_t value) noexcept { _localColor = value; }

        inline static std::array<Hunter, Entities::PlayerEntity::SlotCapacity> SlotHunter{};
        inline static std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity> SlotPing{};

        static const std::uint32_t ClientId;

        static void RebindSocket();
        static void Update(double time);

        [[nodiscard]] static bool ConnectionLost() noexcept { return _connectionLost; }
        static void PumpMapTransfer();
        [[nodiscard]] static std::int32_t ReAnnouncements() noexcept { return _reAnnouncements; }
        [[nodiscard]] static double LongestServerSilence() noexcept { return _longestServerSilence; }
        [[nodiscard]] static std::int32_t AuthorityStandDowns() noexcept { return _authorityStandDowns; }
        [[nodiscard]] static std::int64_t AuthorityFrames() noexcept { return _authorityFrames; }
        [[nodiscard]] static bool Refused() noexcept { return _refused; }
        [[nodiscard]] static RefusedPacket RefusedReason() noexcept { return _refusedReason; }

        static void SendChat(const std::string& text);
        static void SendVote(std::uint8_t kind, const std::string& roomKey);
        static void SendMapPick(const std::string& roomKey);

        static void AcceptSlotIntent(std::int32_t slot, IntentPacket intent);
        [[nodiscard]] static std::int64_t IntentsOutOfOrder() noexcept { return _intentsOutOfOrder; }
        static void ForgetSlot(std::int32_t slot);

        [[nodiscard]] static std::optional<MatchStatePacket> ServerMatch() { return _serverMatch; }
        [[nodiscard]] static bool IsAuthority() noexcept { return _isAuthority; }
        [[nodiscard]] static bool ConsumeAuthorityStateSync() noexcept;
        [[nodiscard]] static std::int32_t ServerPlayerCount() noexcept;

        [[nodiscard]] static const std::string& PlayerName() noexcept { return _playerName; }
        static void SetPlayerName(std::string value) { _playerName = std::move(value); }

        inline static std::vector<MapChangedHandler> MapChanged{};
        inline static std::array<bool, Entities::PlayerEntity::SlotCapacity> SlotOccupied{};

        static void ApplyRoster(RosterPacket roster);
        static void ApplyMatchState(MatchStatePacket state, bool rotated);

        [[nodiscard]] static std::uint16_t CurrentMatchId() noexcept;
        [[nodiscard]] static std::uint64_t AuthorityEpoch() noexcept;
        [[nodiscard]] static std::int32_t SnapshotStreamResets() noexcept { return _snapshotStreamResets; }
        [[nodiscard]] static bool MatchesStream(std::uint16_t match, std::uint64_t epoch);
        [[nodiscard]] static std::int64_t SnapshotsOutOfOrder() noexcept { return _snapshotsOutOfOrder; }

        static void SendIntent(IntentPacket intent);
        static void SendMatchEnd();
        static void BroadcastSnapshot();

        // NetSessionLobby.cs
        [[nodiscard]] static const std::optional<SessionStatePacket>& ServerSession() noexcept { return _serverSession; }
        [[nodiscard]] static SessionPhase CurrentSessionPhase() noexcept;
        [[nodiscard]] static std::uint16_t SessionRevision() noexcept;
        [[nodiscard]] static std::optional<MatchDefinition> ActiveMatchDefinition();
        inline static std::array<std::int8_t, Entities::PlayerEntity::SlotCapacity> SlotTeamIndex{};
        inline static std::array<bool, Entities::PlayerEntity::SlotCapacity> SlotLobbyReady{};
        [[nodiscard]] static bool LocalIsLobbyOwner() noexcept;
        [[nodiscard]] static bool IsInLobby() noexcept;
        [[nodiscard]] static bool IsStarting() noexcept;
        [[nodiscard]] static bool IsPlaying() noexcept;
        [[nodiscard]] static bool IsPostMatch() noexcept;
        [[nodiscard]] static bool CanEditLobby() noexcept;
        [[nodiscard]] static bool PersistentLobby() noexcept;
        [[nodiscard]] static bool FreezeGameplay() noexcept;
        [[nodiscard]] static bool ShouldLoadMatch() noexcept;
        [[nodiscard]] static const std::string& LobbyMessage() noexcept { return _lobbyMessage; }
        [[nodiscard]] static bool LobbyCommandPending() noexcept { return !_pendingLobby.empty(); }
        [[nodiscard]] static std::int32_t ConnectionPort();
        [[nodiscard]] static bool SessionTimedOut();
        [[nodiscard]] static double Clock();

        static void Pump(double time = 0);
        static bool SendLobbyCommand(LobbyCommandType type, std::uint8_t targetSlot = 255,
            std::int8_t team = -1, bool ready = false,
            std::optional<SessionStatePacket> configuration = std::nullopt);
        static void ApplySessionState(SessionStatePacket state);
        static void MarkMatchLoaded();
        static void ReportMatchLoadFailed(const std::string& reason);
        static void ResetMatchState();
        [[nodiscard]] static RosterPacket LobbyRoster();

    private:
        static std::array<std::uint8_t, 4> ResolveIPv4(const std::string& address);
        [[nodiscard]] static std::uint32_t NewClientId();
        static void SendHello();
        static void Handle(ReceivedPacket packet, double time);
        static void HandleHitClaim(ReceivedPacket packet);
        static void SendVerdicts(std::int32_t slot,
            std::span<const std::pair<std::uint16_t, std::uint8_t>> verdicts);
        static void HandleChat(ReceivedPacket packet, double time);
        static void BroadcastHostControl();
        static void HandleHello(ReceivedPacket packet, double time);
        static void HandleIntent(ReceivedPacket packet, double time);
        static void HandleSlotIntent(ReceivedPacket packet);
        static void HandleRoster(ReceivedPacket packet);
        static void HandleMatchState(ReceivedPacket packet, bool rotated);
        static void HandleSnapshot(ReceivedPacket packet);
        static void HandleBye(ReceivedPacket packet);
        static void DropTimedOutPeers(double time);
        [[nodiscard]] static std::shared_ptr<RemotePeer> FindPeer(
            const std::shared_ptr<System::Net::IPEndPoint>& endPoint);
        [[nodiscard]] static std::int32_t NextFreeSlot();

        // NetSessionLobby.cs
        static void SendLobbyPacket(const LobbyCommandPacket& command);
        static void PumpLobby(double now);
        static void ApplyLobbyResult(const LobbyCommandResultPacket& result);
        static void ResetLobbySession();

        struct PendingLobbyCommand final
        {
            LobbyCommandPacket Packet{};
            double SentAt = 0;
            std::int32_t Attempts = 0;
        };

        static constexpr double SilenceBeforeRejoin = 5.0;

        static std::unique_ptr<NetTransport> _transport;
        static bool _playback;
        static std::vector<std::shared_ptr<RemotePeer>> _peers;
        static std::shared_ptr<System::Net::IPEndPoint> _hostEndPoint;
        static std::array<std::uint8_t, NetConfig::MaxPacketSize> _scratch;

        static NetRole _role;
        static std::int32_t _localSlot;
        static std::uint32_t _netFrame;
        static std::uint32_t _snapshotArrived;
        static std::optional<std::string> _lastError;

        static std::int64_t _snapshotsReceived;
        static std::int64_t _snapshotsSent;
        static std::int64_t _statesApplied;
        static std::int64_t _intentsReceived;
        static std::uint32_t _appliedSnapshotFrame;

        static SnapshotSink _snapshotSink;
        static std::function<void()> _serverMatchEnded;

        static Hunter _localHunter;
        static std::int32_t _localColor;

        static bool _connectionLost;
        static double _lastServerPacket;
        static std::int32_t _reAnnouncements;
        static double _longestServerSilence;
        static std::int32_t _authorityStandDowns;
        static bool _reAnnounced;
        static std::int64_t _authorityFrames;
        static bool _refused;
        static RefusedPacket _refusedReason;

        static std::array<std::uint16_t, Entities::PlayerEntity::SlotCapacity> _hostGenerations;
        static std::array<std::uint32_t, Entities::PlayerEntity::SlotCapacity> _lastSlotIntentFrame;
        static std::int64_t _intentsOutOfOrder;

        static std::optional<MatchStatePacket> _serverMatch;
        static bool _isAuthority;
        static bool _authorityNeedsStateApply;
        static std::string _playerName;

        static std::uint32_t _lastSnapshotFrame;
        static bool _hasSnapshot;
        static std::uint32_t _rosterRevision;
        static bool _hasRoster;
        static std::int32_t _snapshotStreamResets;
        static std::int64_t _snapshotsOutOfOrder;
        static std::array<PlayerState, Entities::PlayerEntity::SlotCapacity> _snapshotScratch;

        // NetSessionLobby.cs
        static std::optional<SessionStatePacket> _serverSession;
        static std::string _lobbyMessage;
        static ::MphRead::NativeRuntime::Guid _ownerToken;
        static std::uint32_t _nextCommandId;
        static std::optional<std::uint16_t> _loadedMatch;
        static std::uint16_t _rosterSessionRevision;
        static double _lastLoadAck;
        static double _lastIdentity;
        static std::map<std::uint32_t, PendingLobbyCommand> _pendingLobby;
    };
}
