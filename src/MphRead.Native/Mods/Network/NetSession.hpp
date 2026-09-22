#pragma once

#include "../../Entities/Players/PlayerEntity.hpp"
#include "NetProtocol.hpp"
#include "NetTransport.hpp"

#include <array>
#include <cstdint>
#include <functional>
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
        [[nodiscard]] static const std::optional<std::string>& LastError() noexcept { return _lastError; }

        inline static std::array<PlayerState, Entities::PlayerEntity::SlotCapacity> RemoteStates{};
        inline static std::array<bool, Entities::PlayerEntity::SlotCapacity> RemoteStateValid{};
        inline static std::array<IntentPacket, Entities::PlayerEntity::SlotCapacity> RemoteIntents{};
        inline static std::array<bool, Entities::PlayerEntity::SlotCapacity> RemoteIntentValid{};
        inline static std::array<std::uint32_t, Entities::PlayerEntity::SlotCapacity> RemoteIntentArrived{};

        [[nodiscard]] static std::uint32_t RemoteIntentAge(std::int32_t slot) noexcept;

        [[nodiscard]] static std::int64_t SnapshotsReceived() noexcept { return _snapshotsReceived; }
        [[nodiscard]] static std::int64_t SnapshotsSent() noexcept { return _snapshotsSent; }
        [[nodiscard]] static std::int64_t StatesApplied() noexcept { return _statesApplied; }
        [[nodiscard]] static std::int64_t IntentsReceived() noexcept { return _intentsReceived; }
        static void NoteStatesApplied() noexcept;

        static void StartServerAuthority(SnapshotSink sink, std::function<void()> matchEnded);
        static void StartHost(std::int32_t port = NetConfig::DefaultPort);
        static void StartClient(const std::string& address,
            std::int32_t port = NetConfig::DefaultPort);
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
        [[nodiscard]] static std::int32_t ReAnnouncements() noexcept { return _reAnnouncements; }
        [[nodiscard]] static double LongestServerSilence() noexcept { return _longestServerSilence; }
        [[nodiscard]] static std::int32_t AuthorityStandDowns() noexcept { return _authorityStandDowns; }
        [[nodiscard]] static std::int64_t AuthorityFrames() noexcept { return _authorityFrames; }
        [[nodiscard]] static bool Refused() noexcept { return _refused; }
        [[nodiscard]] static RefusedPacket RefusedReason() noexcept { return _refusedReason; }

        static void SendChat(const std::string& text);
        static void SendVote(std::uint8_t kind, const std::string& roomKey);

        static void AcceptSlotIntent(std::int32_t slot, IntentPacket intent);
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

        [[nodiscard]] static std::int32_t SnapshotStreamResets() noexcept
        {
            return _snapshotStreamResets;
        }
        [[nodiscard]] static std::int64_t SnapshotsOutOfOrder() noexcept
        {
            return _snapshotsOutOfOrder;
        }
        [[nodiscard]] static std::int64_t IntentsOutOfOrder() noexcept
        {
            return _intentsOutOfOrder;
        }

        static void SendIntent(IntentPacket intent);
        static void SendMatchEnd();
        static void BroadcastSnapshot();

    private:
        static std::array<std::uint8_t, 4> ResolveIPv4(const std::string& address);
        [[nodiscard]] static std::uint32_t NewClientId();
        static void SendHello();
        static void Handle(ReceivedPacket packet, double time);
        static void HandleChat(ReceivedPacket packet, double time);
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

        static constexpr double SilenceBeforeRejoin = 5.0;
        static constexpr std::uint32_t IntentResetGap = 600;
        static constexpr std::uint32_t SnapshotResetGap = 600;
        static constexpr std::int32_t LateSnapshotsBeforeReset = 12;

        static std::unique_ptr<NetTransport> _transport;
        static std::vector<std::shared_ptr<RemotePeer>> _peers;
        static std::shared_ptr<System::Net::IPEndPoint> _hostEndPoint;
        static std::array<std::uint8_t, NetConfig::MaxPacketSize> _scratch;

        static NetRole _role;
        static std::int32_t _localSlot;
        static std::uint32_t _netFrame;
        static std::optional<std::string> _lastError;

        static std::int64_t _snapshotsReceived;
        static std::int64_t _snapshotsSent;
        static std::int64_t _statesApplied;
        static std::int64_t _intentsReceived;

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

        static std::array<std::uint32_t, Entities::PlayerEntity::SlotCapacity>
            _lastSlotIntentFrame;
        static std::int64_t _intentsOutOfOrder;

        static std::optional<MatchStatePacket> _serverMatch;
        static bool _isAuthority;
        static bool _authorityNeedsStateApply;
        static std::string _playerName;

        static std::uint32_t _lastSnapshotFrame;
        static std::int32_t _lateSnapshotRun;
        static std::int32_t _snapshotStreamResets;
        static std::int64_t _snapshotsOutOfOrder;
    };
}
