#pragma once
#include <cstdint>
#include <string>
namespace MphRead::Mods::Network { class ServerSim; }
namespace MphRead::Entities { class PlayerEntity; }
namespace MphRead::Mods::Diagnostics
{
    class AimCheck final
    {
    public:
        static std::int32_t Run(const std::string& room);
        static void ReportClock(const Entities::PlayerEntity& player);
    private:
        static int CheckWeapons(Network::ServerSim& sim);
        static int CheckModes(Entities::PlayerEntity& player);
        static int CheckHunterWeapons(const std::string& room);
        static void Benchmark(Entities::PlayerEntity& player);
    };
}
