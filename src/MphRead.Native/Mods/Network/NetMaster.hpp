#pragma once

#include "NetProtocol.hpp"
#include "NetTransport.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <limits>
#include <optional>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <vector>

namespace MphRead
{
    enum class GameMode : std::int32_t;
}

namespace MphRead::Mods::Network
{
    class DedicatedServer;
    class MapRotation;

    class NetMasterConfig final
    {
    public:
        static constexpr const char* DefaultHost = "net.livetek.fr";
        static constexpr std::uint16_t DefaultPort = 27889;
        static constexpr double HeartbeatSeconds = 15.0;
        static constexpr double ExpirySeconds = 50.0;

        [[nodiscard]] static constexpr std::int32_t EntriesPerPacket() noexcept
        {
            return (NetConfig::MaxPacketSize - 1 - 2) / MasterEntryPacket::Size;
        }

        NetMasterConfig() = delete;
    };

    class MasterReporter final
    {
    public:
        MasterReporter(std::string host, std::int32_t port);
        MasterReporter(const MasterReporter&) = delete;
        MasterReporter(MasterReporter&&) = delete;
        MasterReporter& operator=(const MasterReporter&) = delete;
        MasterReporter& operator=(MasterReporter&&) = delete;
        ~MasterReporter() = default;

        void Beat(double now, const std::string& serverName, std::uint16_t port,
            std::uint8_t players, std::uint8_t maxPlayers, std::uint8_t mode,
            const std::string& roomKey);
        void Farewell(std::uint16_t port);
        void Dispose();

    private:
        [[nodiscard]] bool Resolve(double now);
        void Complain(const std::string& message);

        const std::string _host;
        const std::int32_t _port;
        std::optional<std::uintptr_t> _socket{};
        std::shared_ptr<System::Net::IPEndPoint> _endPoint{};
        double _lastBeat = -std::numeric_limits<double>::infinity();
        double _lastResolve = -std::numeric_limits<double>::infinity();
        bool _complained = false;
        std::array<std::uint8_t, MasterHeartbeatPacket::Size> _scratch{};
    };

    class MasterServer final
    {
    private:
        class Entry;
        class Hosted;

    public:
        explicit MasterServer(std::int32_t port = NetMasterConfig::DefaultPort);
        MasterServer(const MasterServer&) = delete;
        MasterServer(MasterServer&&) = delete;
        MasterServer& operator=(const MasterServer&) = delete;
        MasterServer& operator=(MasterServer&&) = delete;
        ~MasterServer() = default;

        void SetHostPorts(std::int32_t first, std::int32_t last) noexcept;
        [[nodiscard]] bool CanHost() const noexcept;
        [[nodiscard]] bool SetPublicAddress(const std::string& host);
        void Stop() noexcept;
        void Run(std::stop_token cancel = {});

    private:
        static constexpr double PortCooldownSeconds = 5.0;
        static constexpr double HostedStartupSeconds = 180.0;
        static constexpr double HostedEmptySeconds = 45.0;

        [[nodiscard]] static std::uint32_t ToUInt32(
            const std::array<std::uint8_t, 4>& address) noexcept;
        [[nodiscard]] static bool IsLocal(const std::array<std::uint8_t, 4>& address) noexcept;
        [[nodiscard]] double ClockElapsedSeconds() const noexcept;

        void Handle(const ReceivedPacket& packet, double now);
        void HandleFarewell(const ReceivedPacket& packet);
        void HandleHostRequest(const ReceivedPacket& packet, double now);
        [[nodiscard]] HostReplyPacket StartHosted(const HostRequestPacket& request,
            const std::shared_ptr<System::Net::IPEndPoint>& asker, double now);
        [[nodiscard]] std::int32_t FreeHostPort(double now);
        void ReapHosted(double now);
        void StopHosted(const std::shared_ptr<Hosted>& entry, const std::string& why);
        void Unlist(std::int32_t port);
        void HandleHeartbeat(const ReceivedPacket& packet, double now);
        void Expire(double now);
        void SendList(const std::shared_ptr<System::Net::IPEndPoint>& sender);
        static void Log(const std::string& message);

        const std::int32_t _port;
        std::vector<std::shared_ptr<Entry>> _entries{};
        std::vector<std::shared_ptr<Hosted>> _hosted{};
        std::array<std::uint8_t, NetConfig::MaxPacketSize> _scratch{};
        std::unique_ptr<NetTransport> _transport{};
        std::atomic<bool> _running{false};
        std::chrono::steady_clock::time_point _clockStart{};
        std::uint32_t _publicAddress = 0;
        std::string _publicName{};
        std::int32_t _hostPortFirst = 0;
        std::int32_t _hostPortLast = -1;
        std::unordered_map<std::int32_t, double> _cooling{};
    };

    struct MasterListing
    {
        std::string Address{};
        std::int32_t Port = 0;
        std::string ServerName{};
        std::string RoomKey{};
        GameMode Mode{};
        std::int32_t Players = 0;
        std::int32_t MaxPlayers = 0;
        std::int32_t Protocol = 0;

        [[nodiscard]] std::string Endpoint() const;
    };

    struct MasterListResult
    {
        std::shared_ptr<const std::vector<MasterListing>> Servers{};
        bool Answered = false;
    };

    struct HostedGame
    {
        bool Started = false;
        std::string Host{};
        std::int32_t Port = 0;
        std::string Reason{};
    };

    class NetMasterClient final
    {
    public:
        [[nodiscard]] static HostedGame RequestGame(const std::string& masterHost,
            std::int32_t masterPort, const std::string& roomKey, GameMode mode,
            float timeLimit, std::int32_t pointGoal, std::int32_t maxPlayers,
            const std::string& serverName, std::int32_t timeoutMs = 6000);

        [[nodiscard]] static MasterListResult Query(const std::string& host,
            std::int32_t port = NetMasterConfig::DefaultPort,
            std::int32_t timeoutMs = 1500);

        NetMasterClient() = delete;
    };
}
