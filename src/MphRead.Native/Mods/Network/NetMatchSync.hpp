#pragma once

#include <string>

namespace MphRead::Mods::Network
{
    class NetMatchSync final
    {
    public:
        NetMatchSync() = delete;

        static void Reset();
        [[nodiscard]] static bool Synced();
        [[nodiscard]] static float LastDrift();
        static void Apply();

    private:
        static std::string _lastRoom;
        static bool _everSynced;
        static float _lastDrift;
    };
}
