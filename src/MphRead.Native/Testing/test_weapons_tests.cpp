#include "Testing/test_weapons.hpp"

#include <cassert>
#include <cstdint>
#include <sstream>
#include <string>

int main() {
    using namespace fruityprime::formats;
    using namespace fruityprime::testing::weapons;

    static_assert(sizeof(RawWeaponInfo) == 0xf0);
    const auto ricochets = get_ricochets();
    const auto weapons_1p = get_1p_weapons();
    const auto weapons_mp = get_mp_weapons();
    const auto enemies = get_enemy_weapons();
    const auto platforms = get_platform_weapons();

    assert(ricochets.size() == 4);
    assert(weapons_1p.size() == 18);
    assert(weapons_mp.size() == 18);
    assert(enemies.size() == 11);
    assert(platforms.size() == 4);

    const RawWeaponInfo& power = weapons_1p.front();
    assert(power.beam == BeamType::PowerBeam);
    assert(power.beam_kind == BeamType::PowerBeam);
    assert(power.colors[0] == 9055);
    assert(power.colors[1] == 21407);
    assert(power.priority() == 1);
    assert(static_cast<std::uint32_t>(power.flags())
           == 0x40000700U);
    assert(power.uncharged_damage == 6);
    assert(power.charged_damage == 36);
    assert(power.zoom_fov == 40960);
    assert(power.ricochet_weapon_ptr[0] == 0);
    assert(power.smoke_charge_amount == 75);

    const RawWeaponInfo& judicator = ricochets.front();
    assert(judicator.beam == BeamType::Judicator);
    assert(judicator.priority() == 2);
    assert(judicator.uncharged_damage == 24);
    assert(judicator.flags() == WeaponFlags::SurfaceCollision
           || static_cast<std::uint32_t>(judicator.flags())
                  == 0x40001200U);

    const std::string dumped = dump_weapon_info(power);
    assert(dumped.find("description: \"\",") != std::string::npos);
    assert(dumped.find("beam: BeamType.PowerBeam") != std::string::npos);
    assert(dumped.find("unchargedDamage: 6") != std::string::npos);
    assert(dumped.find("ricochetWeapon: new uint[]") != std::string::npos);

    std::ostringstream report;
    test_weapon_info(report);
    assert(report.str().find("PowerBeam") != std::string::npos);
    assert(report.str().find("1P Nrm:") != std::string::npos);
    return 0;
}
