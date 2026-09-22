#pragma once

#include "NetProtocol.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <tuple>
#include <vector>

namespace System::Net
{
    class IPEndPoint;
}

namespace MphRead
{
    enum class GameMode : std::uint8_t;
}

namespace MphRead::Mods::Network
{
    class MapRotation;
    class MasterReporter;
    class NetTransport;
    class ServerSim;
    struct MatchStatePacket;
    struct ReceivedPacket;
    struct RosterPacket;

    class DedicatedServer final
    {
    private:
        static constexpr double VoteThreshold = 0.70;
        static constexpr double VoteSeconds = 30.0;
        static constexpr double VoteCooldownSeconds = 90.0;
        static constexpr double ProposalCooldownSeconds = 180.0;
        static constexpr std::int32_t VoteMinimumPlayers = 2;
        static constexpr double ReadyWaitSeconds = 30.0;
        static constexpr double AllReadySeconds = 5.0;
        static constexpr double ChatRatePerSecond = 0.5;
        static constexpr double ChatBurst = 3.0;
        static constexpr std::uint32_t IntentResetGap = 600;

        class Peer final
        {
        public:
            std::shared_ptr<System::Net::IPEndPoint> EndPoint{};
            std::int32_t SlotIndex = -1;
            double LastSeen = 0.0;
            std::uint32_t LastIntentFrame = 0;
            std::string Name{};
            std::uint8_t Hunter = 0;
            std::uint8_t Color = 0;
            std::int32_t Ping = 0;
            double PingSentAt = 0.0;
            std::uint8_t PingId = 0;
            bool PingPending = false;
            double ChatCredit = ChatBurst;
            double ChatCreditAt = 0.0;
            std::int32_t ChatDropped = 0;
            bool Ready = false;
            std::uint8_t Ballot = 0;
            std::uint32_t ClientId = 0;
            double LastProposal = -std::numeric_limits<double>::infinity();
        };

    public:
        explicit DedicatedServer(std::int32_t port = NetConfig::DefaultPort,
            std::int32_t maxPlayers = 4,
            std::shared_ptr<MapRotation> rotation = nullptr);

        DedicatedServer(const DedicatedServer&) = delete;
        DedicatedServer(DedicatedServer&&) = delete;
        DedicatedServer& operator=(const DedicatedServer&) = delete;
        DedicatedServer& operator=(DedicatedServer&&) = delete;
        ~DedicatedServer();

        [[nodiscard]] bool AllowMapVotes() const noexcept;
        void AllowMapVotes(bool value) noexcept;

        [[nodiscard]] const std::string& ServerName() const noexcept;
        void ServerName(std::string value);

        [[nodiscard]] std::int32_t PeerCount() const noexcept;
        [[nodiscard]] bool EverOccupied() const noexcept;
        [[nodiscard]] bool Listening() const noexcept;
        [[nodiscard]] std::int32_t BoundPort() const noexcept;

        [[nodiscard]] std::shared_ptr<MasterReporter> Reporter() const noexcept;
        void Reporter(std::shared_ptr<MasterReporter> value) noexcept;

        [[nodiscard]] bool FriendlyFire() const noexcept;
        void FriendlyFire(bool value) noexcept;

        [[nodiscard]] bool ShadowFreeze() const noexcept;
        void ShadowFreeze(bool value) noexcept;

        [[nodiscard]] bool AutoUpdate() const noexcept;
        void AutoUpdate(bool value) noexcept;

        [[nodiscard]] bool Simulate() const noexcept;
        void Simulate(bool value) noexcept;

        [[nodiscard]] bool Simulating() const;

        void Run(std::stop_token cancel = {});
        void Stop() noexcept;

    private:
        double _now = 0.0;
        bool _voteRunning = false;
        std::string _voteRoom{};
        GameMode _voteMode;
        std::string _voteProposer{};
        std::int32_t _voteProposerSlot = -1;
        double _voteStartedAt = 0.0;
        double _voteResolvedAt = -std::numeric_limits<double>::infinity();
        std::uint8_t _voteResult = VoteStatePacket::StateIdle;

        bool _allowMapVotes = true;
        std::vector<std::shared_ptr<Peer>> _peers{};
        std::atomic<std::int32_t> _peerCount{0};
        std::array<std::uint8_t, NetConfig::MaxPacketSize> _scratch{};
        const std::int32_t _port;
        const std::int32_t _maxPlayers;
        const std::shared_ptr<MapRotation> _rotation;
        std::unique_ptr<NetTransport> _transport{};
        std::atomic<bool> _listening{false};
        std::atomic<std::int32_t> _boundPort;
        std::shared_ptr<Peer> _authority{};
        std::shared_ptr<ServerSim> _sim{};
        std::shared_ptr<std::vector<std::uint8_t>> _lastSnapshot{};
        std::atomic<bool> _running{false};
        double _matchStarted = 0.0;
        double _matchEndedAt = -1.0;
        std::uint16_t _matchId = 1;

        std::string _serverName;
        std::atomic<bool> _everOccupied{false};
        std::shared_ptr<MasterReporter> _reporter{};
        bool _friendlyFire = false;
        bool _shadowFreeze = true;
        bool _autoUpdate = false;
        bool _simulate = false;

        [[nodiscard]] double EndSequenceFor() const;
        void Shutdown(std::uint16_t listenPort);
        void EndMatch(double now, const std::string& reason);
        void AdvanceMap(double now);
        [[nodiscard]] MatchStatePacket BuildState(double now) const;
        void BroadcastMatchState(double now);
        void StartSimulation();
        void SendSnapshot(std::span<const std::uint8_t> payload);
        void SyncSimulationState(double now);
        void Handle(const ReceivedPacket& packet, double now);
        void HandleVote(const ReceivedPacket& packet, double now);
        void StartVote(const std::shared_ptr<Peer>& peer, const std::string& roomKey, double now);
        void Tally(double now);
        [[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::int32_t, std::int32_t>
            CountVotes() const;
        void ResolveVote(double now, bool passed, const std::string& count);
        void ReviewVote(double now);
        [[nodiscard]] static std::optional<std::string> ResolveRoomKey(const std::string& roomKey);
        [[nodiscard]] GameMode ModeForRoom(const std::string& roomKey) const;
        void BroadcastVoteState(double now);
        void Tell(const std::shared_ptr<Peer>& peer, const std::string& text);
        void HandleChat(const ReceivedPacket& packet, double now);
        void Announce(const std::string& text);
        void SendStatus(const std::shared_ptr<System::Net::IPEndPoint>& sender, double now);
        void HandleHello(const ReceivedPacket& packet, double now);
        void HandleMatchEnd(const ReceivedPacket& packet, double now);
        void SendRefusal(const std::shared_ptr<System::Net::IPEndPoint>& to, std::uint8_t reason);
        void HandleIdentify(const ReceivedPacket& packet, double now);
        void NotifyAuthority(const std::shared_ptr<Peer>& peer);
        void PingPeers(double now);
        void HandlePong(const ReceivedPacket& packet, double now);
        [[nodiscard]] RosterPacket BuildRoster() const;
        void BroadcastRoster();
        void HandleIntent(const ReceivedPacket& packet, double now);
        void HandleSnapshot(const ReceivedPacket& packet, double now);
        void HandleBye(const ReceivedPacket& packet);
        void DropTimedOut(double now);
        void Remove(const std::shared_ptr<Peer>& peer, const std::string& reason);
        [[nodiscard]] std::shared_ptr<Peer> Find(
            const std::shared_ptr<System::Net::IPEndPoint>& endPoint) const;
        [[nodiscard]] bool SlotFree(std::int32_t slot) const;
        [[nodiscard]] std::int32_t NextFreeSlot() const;
        static void Log(const std::string& message);
    };
}
