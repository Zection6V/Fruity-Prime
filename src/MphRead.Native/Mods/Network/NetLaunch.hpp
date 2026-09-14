#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace MphRead
{
    enum class GameMode : std::int32_t;
    enum class Hunter : std::uint8_t;
    class Scene;
}

namespace MphRead::Mods::Network
{
    struct NetLaunchServerRoom
    {
        std::string RoomKey;
        GameMode Mode;
    };

    class NetLaunch final
    {
    public:
        NetLaunch() = delete;

        static bool Join(const std::string& address, std::int32_t port,
            const std::string& playerName, Hunter hunter,
            std::int32_t timeoutMs = 8000, std::int32_t color = -1);

        static const std::string& LastJoinError();
        static void DisableCheatsForMatch();
        static std::optional<NetLaunchServerRoom> ServerRoom();

        static constexpr std::int32_t RoomPlayerCount = 2;

        static void BuildPlayers(Scene& scene, Hunter localHunter,
            std::int32_t localRecolor, bool teams = false,
            std::optional<std::int32_t> localSlot = std::nullopt);

    private:
        static std::string _lastJoinError;

        static std::string DescribeJoinFailure(const std::string& address,
            std::int32_t port);
    };
}
