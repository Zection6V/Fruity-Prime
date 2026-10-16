#pragma once

#include <cstdint>
#include <string>

namespace MphRead::Mods::Network
{
    // Noxus's Vhoscythe against a real player on a headless authority, driven
    // by relayed intents the way a remote owner drives it: the startup count
    // follows the owner's frames across a late press, a threshold release and
    // a packet gap, and the hit lands both when the authority resolves it and
    // when the owner's claim is judged.
    class VhoscytheCombatCheck final
    {
    public:
        VhoscytheCombatCheck() = delete;
        [[nodiscard]] static std::int32_t Run(const std::string& room);
    };
}
