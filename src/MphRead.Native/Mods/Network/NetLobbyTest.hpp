#pragma once

#include "NetProtocol.hpp"
#include "SessionProtocol.hpp"

#include "../../NativeRuntime/System/Guid.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Network
{
    // The lobby over real UDP: protocol round trips, the load barrier, two
    // rounds on one socket, owner migration, teams, rebind and continuous
    // rotation, against an in-process DedicatedServer.
    class NetLobbyTest final
    {
    public:
        NetLobbyTest() = delete;

        [[nodiscard]] static std::int32_t Run();

    private:
        class Client;
        class Rig;

        static void Check(bool condition, std::string_view message);
        static void ProtocolChecks();
        static void DemoProtocolCheck();
        static void LayoutChecks();
        static void ClientStateChecks();
        [[nodiscard]] static std::vector<std::string> Rooms();
        static void Scenario();
        static void TeamScenario();
        static void CustomScenario();
        static void FourTeamScenario();
        static void ContinuousScenario();
        static void ClientSessionScenario();

        inline static std::int32_t _checks = 0;
    };
}
