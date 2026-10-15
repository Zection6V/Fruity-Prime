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

    void InstagibRules::ApplyDamage(BeamType beam, std::int32_t& damage,
        std::int32_t& headshotDamage, std::int32_t& splashDamage)
    {
        if (Active() && beam == BeamType::Imperialist)
        {
            // Within the wire's range, lethal scoped or unscoped.
            damage = 9999;
            headshotDamage = 9999;
            splashDamage = 0;
        }
    }
}
