#pragma once

#include "../../Formats/Enums.hpp"

#include <cstdint>
#include <optional>

namespace MphRead::Mods::Combat
{
    // Shared match policy for player, projectile and AI hooks. Story mode
    // keeps the cartridge's rules even if a previous match enabled Instagib.
    class InstagibRules final
    {
    public:
        InstagibRules() = delete;
        [[nodiscard]] static bool Active();
        [[nodiscard]] static bool PickupsAllowed();
        [[nodiscard]] static bool SuppressPickupTarget(std::int32_t type);
        [[nodiscard]] static BeamType AiWeapon(BeamType selected);
        // Also consumed by the authority's claim bounds, so a lethal shot is
        // validated against the same match rule that produced its damage.
        [[nodiscard]] static std::optional<std::int32_t> DirectDamage(BeamType beam);
        static void ApplyDamage(BeamType beam, std::int32_t& damage,
            std::int32_t& headshotDamage, std::int32_t& splashDamage);
    };
}
