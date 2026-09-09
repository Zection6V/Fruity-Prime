#include "enemy_catalog.hpp"

#include "Metadata/entity_metadata.hpp"

#include <algorithm>
#include <cmath>

namespace fruityprime::enemy {
namespace {

constexpr ProfileFlags LinkedParts = ProfileFlags::HasLinkedParts;
constexpr ProfileFlags HitZone = ProfileFlags::UsesHitZone
    | ProfileFlags::StaticCollision;

constexpr std::array<Profile, metadata::EnemyCount> ProfileTable{{
    {0, "00_WarWasp.cs", "Enemy00Entity", BehaviorFamily::Flying,
     ProfileFlags::None},
    {1, "01_Zoomer.cs", "Enemy01Entity", BehaviorFamily::Ground,
     ProfileFlags::None},
    {2, "02_Temroid.cs", "Enemy02Entity", BehaviorFamily::Ground,
     ProfileFlags::None},
    {3, "03_Petrasyl1.cs", "Enemy03Entity", BehaviorFamily::Flying,
     ProfileFlags::None},
    {4, "04_Petrasyl2.cs", "Enemy04Entity", BehaviorFamily::Flying,
     ProfileFlags::None},
    {5, "05_Petrasyl3.cs", "Enemy05Entity", BehaviorFamily::Flying,
     ProfileFlags::None},
    {6, "06_Petrasyl4.cs", "Enemy06Entity", BehaviorFamily::Flying,
     ProfileFlags::None},
    {7, {}, {}, BehaviorFamily::Unknown, ProfileFlags::None},
    {8, {}, {}, BehaviorFamily::Unknown, ProfileFlags::None},
    {9, {}, {}, BehaviorFamily::Unknown, ProfileFlags::None},
    {10, "10_BarbedWarWasp.cs", "Enemy10Entity", BehaviorFamily::Flying,
     ProfileFlags::None},
    {11, "11_Shriekbat.cs", "Enemy11Entity", BehaviorFamily::Flying,
     ProfileFlags::None},
    {12, "12_Geemer.cs", "Enemy12Entity", BehaviorFamily::Ground,
     ProfileFlags::None},
    {13, {}, {}, BehaviorFamily::Unknown, ProfileFlags::None},
    {14, {}, {}, BehaviorFamily::Unknown, ProfileFlags::None},
    {15, {}, {}, BehaviorFamily::Unknown, ProfileFlags::None},
    {16, "16_Blastcap.cs", "Enemy16Entity", BehaviorFamily::Stationary,
     ProfileFlags::StaticCollision},
    {17, {}, {}, BehaviorFamily::Unknown, ProfileFlags::None},
    {18, "18_AlimbicTurret.cs", "Enemy18Entity",
     BehaviorFamily::Stationary, ProfileFlags::UsesTurretMessages},
    {19, "19_Cretaphid.cs", "Enemy19Entity", BehaviorFamily::Boss,
     LinkedParts},
    {20, "20_CretaphidEye.cs", "Enemy20Entity", BehaviorFamily::BossPart,
     LinkedParts},
    {21, "21_CretaphidCrystal.cs", "Enemy21Entity",
     BehaviorFamily::BossPart, LinkedParts},
    {22, {}, {}, BehaviorFamily::Unknown, ProfileFlags::None},
    {23, "23_PsychoBit.cs", "Enemy23Entity", BehaviorFamily::Flying,
     LinkedParts},
    {24, "24_Gorea1A.cs", "Enemy24Entity", BehaviorFamily::Boss,
     LinkedParts},
    {25, "25_GoreaHead.cs", "Enemy25Entity", BehaviorFamily::BossPart,
     LinkedParts},
    {26, "26_GoreaArm.cs", "Enemy26Entity", BehaviorFamily::BossPart,
     LinkedParts},
    {27, "27_GoreaLeg.cs", "Enemy27Entity", BehaviorFamily::BossPart,
     LinkedParts},
    {28, "28_Gorea1B.cs", "Enemy28Entity", BehaviorFamily::Boss,
     LinkedParts},
    {29, "29_GoreaSealSphere1.cs", "Enemy29Entity",
     BehaviorFamily::BossPart, LinkedParts},
    {30, "30_Trocra.cs", "Enemy30Entity", BehaviorFamily::BossPart,
     LinkedParts},
    {31, "31_Gorea2.cs", "Enemy31Entity", BehaviorFamily::Boss,
     LinkedParts},
    {32, "32_GoreaSealSphere2.cs", "Enemy32Entity",
     BehaviorFamily::BossPart, LinkedParts},
    {33, "33_GoreaMeteor.cs", "Enemy33Entity", BehaviorFamily::BossPart,
     LinkedParts},
    {34, {}, {}, BehaviorFamily::Unknown, ProfileFlags::None},
    {35, "35_Voldrum.cs", "Enemy35Entity", BehaviorFamily::Ground,
     ProfileFlags::None},
    {36, "36_Voldrum.cs", "Enemy36Entity", BehaviorFamily::Ground,
     ProfileFlags::None},
    {37, "37_Quadtroid.cs", "Enemy37Entity", BehaviorFamily::Ground,
     ProfileFlags::None},
    {38, "38_CrashPillar.cs", "Enemy38Entity", BehaviorFamily::Stationary,
     ProfileFlags::StaticCollision},
    {39, "39_FireSpawn.cs", "Enemy39Entity", BehaviorFamily::Flying,
     HitZone},
    {40, "40_EnemySpawner.cs", "Enemy40Entity", BehaviorFamily::Spawner,
     ProfileFlags::None},
    {41, "41_Slench.cs", "Enemy41Entity", BehaviorFamily::Boss,
     LinkedParts},
    {42, "42_SlenchShield.cs", "Enemy42Entity", BehaviorFamily::BossPart,
     LinkedParts},
    {43, "43_SlenchNest.cs", "Enemy43Entity", BehaviorFamily::BossPart,
     LinkedParts},
    {44, "44_SlenchSynapse.cs", "Enemy44Entity", BehaviorFamily::BossPart,
     ProfileFlags::UsesTurretMessages | ProfileFlags::HasLinkedParts},
    {45, "45_SlenchTurret.cs", "Enemy45Entity", BehaviorFamily::BossPart,
     ProfileFlags::UsesTurretMessages},
    {46, "46_LesserIthrak.cs", "Enemy46Entity", BehaviorFamily::Ground,
     HitZone},
    {47, "47_GreaterIthrak.cs", "Enemy47Entity", BehaviorFamily::Ground,
     HitZone},
    {48, {}, {}, BehaviorFamily::Hunter, ProfileFlags::None},
    {49, "49_ForceFieldLock.cs", "Enemy49Entity",
     BehaviorFamily::Stationary, ProfileFlags::StaticCollision},
    {50, "50_HitZone.cs", "Enemy50Entity", BehaviorFamily::Stationary,
     ProfileFlags::StaticCollision},
    {51, "51_CarnivorousPlant.cs", "Enemy51Entity", BehaviorFamily::Plant,
     ProfileFlags::IgnoresPlayerRange}
}};

constexpr std::array<std::string_view, metadata::EnemyCount> ModelNameTable{{
    "warwasp_lod0", "zoomer", "Temroid_lod0", "Chomtroid", "Chomtroid",
    "Chomtroid", "Chomtroid", {}, {}, {}, "BarbedWarWasp", "shriekbat",
    "geemer", {}, {}, {}, "blastcap", {}, "Alimbic_Turret", "CylinderBoss",
    "CylinderBossEye", {}, {}, "PsychoBit", "Gorea1A_lod0", {}, {}, {},
    "Gorea1B_lod0", {}, "PowerBomb", "Gorea2_lod0", {}, "goreaMeteor",
    "PsychoBit", "GuardBot2_lod0", "GuardBot1", "DripStank_lod0",
    "AlimbicStatue_lod0", "LavaDemon", {}, "BigEyeBall", {}, "BigEyeNest",
    {}, "BigEyeTurret", "SphinkTick_lod0", "SphinkTick_lod0", {}, {}, {}, {}
}};

} // namespace

const std::array<Profile, metadata::EnemyCount>& profiles() noexcept {
    return ProfileTable;
}

const Profile& profile(std::uint8_t id) noexcept {
    const std::size_t index = std::min<std::size_t>(
        id, ProfileTable.size() - 1);
    return ProfileTable[index];
}

const std::array<std::string_view, metadata::EnemyCount>& model_names() noexcept {
    return ModelNameTable;
}

std::string_view model_name(std::uint8_t id) noexcept {
    return id < ModelNameTable.size() ? ModelNameTable[id]
                                      : std::string_view{};
}

CombatTuning combat_tuning(std::uint8_t id) noexcept {
    const Profile& enemy = profile(id);
    CombatTuning result;
    switch (enemy.family) {
    case BehaviorFamily::Flying:
        result = {2.25F, 35.0F, 1.35F, 0.85F, 10, 20, 0.55F, true, false};
        break;
    case BehaviorFamily::Ground:
        result = {1.75F, 35.0F, 1.30F, 1.0F, 12, 24, 0.65F, false, false};
        break;
    case BehaviorFamily::Stationary:
        result = {0.0F, 35.0F, 1.25F, 1.25F, 14, 30, 0.75F, false, true};
        break;
    case BehaviorFamily::Boss:
        result = {0.9F, 45.0F, 1.75F, 0.75F, 18, 160, 1.15F, false, true};
        break;
    case BehaviorFamily::BossPart:
        result = {0.0F, 45.0F, 1.50F, 1.0F, 16, 60, 0.85F, false, true};
        break;
    case BehaviorFamily::Plant:
        result = {0.0F, 45.0F, 1.35F, 1.0F, 16, 80, 0.85F, false, true};
        break;
    case BehaviorFamily::Spawner:
        result = {0.0F, 35.0F, 1.0F, 1.0F, 0, 100, 0.9F, false, false};
        break;
    case BehaviorFamily::Hunter:
        result = {2.0F, 45.0F, 1.35F, 0.9F, 12, 100, 0.65F, false, true};
        break;
    case BehaviorFamily::Unknown:
        result = {0.0F, 35.0F, 1.25F, 1.0F, 8, 20, 0.65F, false, false};
        break;
    }

    // A few authored classes have a materially different combat role from
    // their broad family. These overrides are deliberately data-only; when
    // the matching individual enemy class is ported, that class can replace
    // the generic controller without changing the room/spawner contract.
    switch (id) {
    case 0: // War Wasp
        result.move_speed = 2.75F;
        result.contact_damage = 25;
        result.attack_cooldown = 0.65F;
        break;
    case 1: // Zoomer: the class follows room surfaces rather than a target.
        result.move_speed = 0.0F;
        result.detection_radius = 0.0F;
        result.attack_radius = 1.25F;
        result.attack_cooldown = 0.5F;
        result.contact_damage = 15;
        result.health = 12;
        result.body_radius = 0.25F;
        result.airborne = false;
        result.ranged = false;
        break;
    case 2: // Temroid: contact becomes an attached drain state.
        result.move_speed = 3.0F;
        result.detection_radius = 10.0F;
        result.attack_radius = 1.25F;
        result.attack_cooldown = 0.5F;
        result.contact_damage = 15;
        result.health = 20;
        result.body_radius = 1.0F;
        result.airborne = false;
        result.ranged = false;
        break;
    case 3: // Petrasyl1
    case 4: // Petrasyl2
    case 5: // Petrasyl3
    case 6: // Petrasyl4
        result.move_speed = 0.0F;
        result.attack_radius = 1.25F;
        result.attack_cooldown = 0.5F;
        result.contact_damage = 12;
        result.health = 8;
        result.body_radius = 0.5F;
        result.airborne = true;
        result.ranged = false;
        break;
    case 12: // Geemer shares Zoomer's surface-following S00 layout.
        result.move_speed = 0.0F;
        result.detection_radius = 0.0F;
        result.attack_radius = 1.25F;
        result.attack_cooldown = 0.5F;
        result.contact_damage = 15;
        result.health = 12;
        result.body_radius = 0.25F;
        result.airborne = false;
        result.ranged = false;
        break;
    case 11: // Shriekbat
        result.move_speed = 3.0F;
        result.attack_radius = 1.6F;
        break;
    case 18: // Alimbic Turret
        result.move_speed = 0.0F;
        result.attack_radius = 1.25F;
        result.attack_cooldown = 1.5F;
        result.contact_damage = 5;
        result.health = 120;
        result.body_radius = 1.0F;
        break;
    case 16: // Blastcap: its cloud is armed when its body is destroyed.
        result.move_speed = 0.0F;
        result.attack_radius = 0.0F;
        result.attack_cooldown = 0.0F;
        result.contact_damage = 0;
        result.ranged = false;
        result.health = 12;
        result.body_radius = 1.0F;
        break;
    case 35: // Voldrum2: rolling guard bot with a ram attack.
        result.move_speed = 6.0F;
        result.detection_radius = 18.0F;
        result.attack_radius = 1.25F;
        result.attack_cooldown = 0.5F;
        result.contact_damage = 2;
        result.health = 42;
        result.body_radius = 0.5F;
        result.airborne = false;
        result.ranged = false;
        break;
    case 36: // Voldrum1: ranged rolling guard bot.
        result.move_speed = 8.0F;
        result.detection_radius = 35.0F;
        result.attack_radius = 1.25F;
        result.attack_cooldown = 0.5F;
        result.contact_damage = 7;
        result.health = 55;
        result.body_radius = 0.5F;
        result.airborne = false;
        result.ranged = true;
        break;
    case 23: // PsychoBit: S06 flying ranged enemy.
        result.move_speed = 2.25F;
        result.detection_radius = 35.0F;
        result.attack_radius = 1.25F;
        result.attack_cooldown = 0.5F;
        result.contact_damage = 10;
        result.health = 11;
        result.body_radius = 1.0F;
        result.airborne = true;
        result.ranged = true;
        break;
    case 39: // Fire Spawn
        result.move_speed = 0.0F;
        result.detection_radius = 0.0F;
        result.attack_radius = 1.0F;
        result.attack_cooldown = 0.5F;
        result.contact_damage = 12;
        result.health = 600;
        result.body_radius = 1.0F;
        result.airborne = true;
        result.ranged = true;
        break;
    case 45: // Slench turret: a stationary linked boss part.
        result.move_speed = 0.0F;
        result.detection_radius = 45.0F;
        result.attack_radius = 1.25F;
        result.attack_cooldown = 1.0F;
        result.contact_damage = 10;
        result.health = 30;
        result.body_radius = 1.0F;
        result.airborne = false;
        result.ranged = true;
        break;
    case 46: // Lesser Ithrak / Hanging Terror.
    case 47: // Greater Ithrak shares the controller with a different S05 layout.
        result.move_speed = 4.5F;
        result.detection_radius = 45.0F;
        result.attack_radius = 1.5F;
        result.attack_cooldown = 0.5F;
        result.contact_damage = 15;
        result.health = 85;
        result.body_radius = 0.5F;
        result.airborne = false;
        result.ranged = false;
        break;
    case 37: // Quadtroid / Dripstank follows room surfaces and grabs alt form.
        result.move_speed = 3.0F;
        result.detection_radius = 10.0F;
        result.attack_radius = 1.25F;
        result.attack_cooldown = 0.5F;
        result.contact_damage = 3;
        result.health = 120;
        result.body_radius = 0.25F;
        result.airborne = false;
        result.ranged = false;
        break;
    case 38: // CrashPillar / Alimbic Statue jumps inside its authored volumes.
        result.move_speed = 0.0F;
        result.detection_radius = 35.0F;
        result.attack_radius = 1.25F;
        result.attack_cooldown = 0.5F;
        result.contact_damage = 10;
        result.health = 150;
        result.body_radius = 0.5F;
        result.airborne = false;
        result.ranged = false;
        break;
    case 40: // Enemy spawner placeholder
        result.ranged = false;
        result.health = 100;
        break;
    case 49: // Force-field lock
    case 50: // Hit zone
        result.move_speed = 0.0F;
        result.contact_damage = 0;
        result.ranged = false;
        break;
    case 51: // Carnivorous plant
        result.attack_radius = 3.0F;
        result.contact_damage = 22;
        result.health = 120;
        break;
    default:
        break;
    }
    return result;
}

} // namespace fruityprime::enemy

