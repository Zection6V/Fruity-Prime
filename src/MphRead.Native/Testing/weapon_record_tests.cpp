// The generated weapon tables against the compact ones the session reads.
//
// Weapons.generated.cpp is machine-written from Metadata/Weapons.cs, so the
// question worth asking is not whether the generator ran but whether the
// hand-written accessors still agree with it.  They were transcribed by hand
// from the same source and had drifted to twelve of the sixty-odd fields;
// this pins the ones that remain.
#include "Metadata/weapon_record.hpp"
#include "Metadata/metadata.hpp"

#include <cassert>
#include <cstdio>
#include <string>

namespace {

using namespace fruityprime;

void require(bool condition, const char* message) {
    if (!condition) {
        std::printf("native weapon record tests failed: %s\n", message);
        std::exit(1);
    }
}

void assert_table_shapes() {
    // The managed tables: eighteen 1P and MP entries (nine beams, each with
    // an affinity variant), and the enemy/boss/Gorea/platform/ricochet sets.
    require(metadata::Weapons1P().size() == 18, "1P table is not 18 entries");
    require(metadata::WeaponsMp().size() == 18, "MP table is not 18 entries");
    require(metadata::EnemyWeapons().size() == 11, "enemy table size");
    require(metadata::BossWeapons().size() == 8, "boss table size");
    require(metadata::GoreaWeapons().size() == 6, "Gorea table size");
    require(metadata::PlatformWeapons().size() == 4, "platform table size");
    require(metadata::Ricochets().size() == 6, "ricochet table size");
}

void assert_known_values() {
    // Spot values read straight out of Weapons.cs, so a generator that
    // silently mapped the wrong constructor argument is caught here rather
    // than by somebody noticing the Power Beam hits too hard.
    const auto& power = metadata::WeaponsMp()[0];
    require(power.description == "Power Beam MP", "first MP entry");
    require(power.uncharged_damage == 6, "power beam uncharged damage");
    require(power.charged_damage == 36, "power beam charged damage");
    require(power.headshot_damage == 8, "power beam headshot damage");
    require(power.min_charge == 18, "power beam min charge");
    require(power.full_charge == 30, "power beam full charge");

    const auto& missile = metadata::WeaponsMp()[2];
    require(missile.description == "Missile MP", "missile entry");
    require(missile.ammo_cost == 10, "missile costs ammo");
    require(missile.charged_splash_damage == 32,
            "missile charged splash");

    // The affinity half of the MP table is the second nine.
    require(metadata::WeaponsMp()[9].description == "Power Beam MP Affinity",
            "affinity entries follow the plain ones");
}

void assert_compact_matches_record() {
    // WeaponInfo is what the session reads.  Every field it kept has a
    // counterpart in the generated record, and they have to agree.
    for (std::size_t index = 0; index < metadata::WeaponCount; ++index) {
        const auto& compact = metadata::weapon_info(
            static_cast<std::uint8_t>(index));
        // The native slot order puts Missile before Volt Driver; the
        // cartridge lists them the other way round.
        static constexpr std::size_t MpTableIndex[] = {0, 2, 1, 3, 4, 5, 6,
                                                       7, 8};
        const auto& record = metadata::WeaponsMp()[MpTableIndex[index]];
        const std::string where = "weapon " + std::to_string(index) + ": ";
        require(compact.ammo_type == record.ammo_type,
                (where + "ammo type disagrees").c_str());
        require(compact.ammo_cost == record.ammo_cost,
                (where + "ammo cost disagrees").c_str());
        require(compact.uncharged_damage == record.uncharged_damage,
                (where + "uncharged damage disagrees").c_str());
    }
}

} // namespace

int main() {
    assert_table_shapes();
    assert_known_values();
    assert_compact_matches_record();
    std::printf("native weapon record tests passed\n");
    return 0;
}
