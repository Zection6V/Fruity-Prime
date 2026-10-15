#include "InstagibRules.hpp"

#include "../../GameState.hpp"

namespace MphRead::Mods::Combat
{
    bool InstagibRules::Active()
    {
        return GameState::Instagib() && GameState::Multiplayer();
    }

    bool InstagibRules::PickupsAllowed()
    {
        return !Active();
    }

    bool InstagibRules::SuppressPickupTarget(std::int32_t type)
    {
        // The cartridge AI's Type34-Type73 references are pickups/spawns.
        return Active() && type >= 34 && type <= 73;
    }

    BeamType InstagibRules::AiWeapon(BeamType selected)
    {
        return Active() ? BeamType::Imperialist : selected;
    }

    std::optional<std::int32_t> InstagibRules::DirectDamage(BeamType beam)
    {
        if (Active() && beam == BeamType::Imperialist)
        {
            // Within the wire's range, lethal scoped or unscoped.
            return 9999;
        }
        return std::nullopt;
    }

    void InstagibRules::ApplyDamage(BeamType beam, std::int32_t& damage,
        std::int32_t& headshotDamage, std::int32_t& splashDamage)
    {
        if (const auto override = DirectDamage(beam))
        {
            damage = *override;
            headshotDamage = *override;
            splashDamage = 0;
        }
    }
}
