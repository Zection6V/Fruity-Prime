#pragma once

#include <cstdint>
#include <string>

namespace MphRead::Mods::Diagnostics
{
    // Runs real bomb targeting, enemy damage and pooled-bomb lifecycle with
    // cartridge assets, without a graphics window.
    class LockjawEnemyCheck final
    {
    public:
        static std::int32_t Run(const std::string& room);
    };
}
