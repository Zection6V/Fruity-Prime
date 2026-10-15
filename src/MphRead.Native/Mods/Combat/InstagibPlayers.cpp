#include "InstagibRules.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Entities/Players/PlayerAi.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

using MphRead::NativeRuntime::ManagedAt;
using MphRead::NativeRuntime::RequireReference;

// Private-state adapters follow the existing PlayerEntity extension pattern.
// Their declarations/call sites are the only additions to the ported bodies.
namespace MphRead::Entities
{
    void PlayerEntity::ModApplyInstagibLoadout()
    {
        // Instagib is a multiplayer match rule; a story hunter or a bot of
        // the adventure keeps whatever the game gave it.
        if (!Mods::Combat::InstagibRules::Active())
        {
            return;
        }
        constexpr MphRead::BeamType beam = MphRead::BeamType::Imperialist;
        MphRead::WeaponInfo& info = RequireReference(ManagedAt(
            RequireReference(MphRead::Weapons::Current), static_cast<std::int32_t>(beam)));
        _availableWeapons.ClearAll();
        _availableCharges.ClearAll();
        _availableWeapons[beam] = true;
        _availableCharges[beam] = true;
        _weaponSlots[0] = MphRead::BeamType::None;
        _weaponSlots[1] = MphRead::BeamType::None;
        _weaponSlots[2] = beam;
        _ammo[UA] = _ammo[Missiles] = 0;
        ManagedAt(_ammo, static_cast<std::int32_t>(info.AmmoType))
            = ManagedAt(_ammoMax, static_cast<std::int32_t>(info.AmmoType));
        RequireReference(_equipInfo).InfiniteAmmo = true;
        _previousWeapon = beam;
        static_cast<void>(TryEquipWeapon(beam, true));
    }

    void PlayerEntity::PlayerAiData::ModApplyInstagibWeapons()
    {
        // Instagib: the Imperialist is the only weapon a hunter holds. The
        // personality scripts are told it is the Power Beam: their Imperialist
        // branches are a sniper's (zoom, hold a vantage point), which left the
        // bots standing still, while a Power Beam bot keeps moving. The shot
        // itself is the Imperialist's (Func2143A40). No charge and no zoom, and
        // no weapon to go and find, since nothing on the map can be picked up.
        if (!Mods::Combat::InstagibRules::Active())
        {
            return;
        }
        _weapon1 = 0;
        _weapon2 = 0;
        _findWeaponIndex = 4;
        Flags4 &= ~(AiFlags4::Bit1 | AiFlags4::Bit2);
    }
}
