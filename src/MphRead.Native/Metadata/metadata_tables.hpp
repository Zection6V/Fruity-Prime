#pragma once

#include "Metadata/metadata.hpp"

#include <algorithm>
#include <cctype>
#include <limits>

namespace fruityprime::metadata::detail {

[[nodiscard]] inline bool equal_ascii_insensitive(std::string_view left,
                                            std::string_view right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        const auto left_char = static_cast<unsigned char>(left[i]);
        const auto right_char = static_cast<unsigned char>(right[i]);
        if (std::tolower(left_char) != std::tolower(right_char)) {
            return false;
        }
    }
    return true;
}

constexpr std::array<HunterInfo, HunterCount> HunterTable{{
    {Hunter::Samus, "Samus", "Samus", "Samus_lod0_Model.bin",
     "SamusAlt_lod0_Model.bin", "Samus", "Samus", 1, "SamusGun_Model.bin"},
    {Hunter::Kanden, "Kanden", "Kanden", "Kanden_lod0_Model.bin",
     "KandenAlt_lod0_Model.bin", "Kanden", "Kanden", 2, "KandenGun_Model.bin"},
    {Hunter::Trace, "Trace", "Trace", "Trace_lod0_Model.bin",
     "TraceAlt_lod0_Model.bin", "Trace", "Trace", 4, "TraceGun_Model.bin"},
    {Hunter::Sylux, "Sylux", "Sylux", "Sylux_lod0_Model.bin",
     "SyluxAlt_lod0_Model.bin", "Sylux", "Sylux", 7, "SyluxGun_Model.bin"},
    {Hunter::Noxus, "Noxus", "Nox", "Nox_lod0_Model.bin",
     "NoxAlt_lod0_Model.bin", "Nox", "Nox", 5, "NoxGun_Model.bin"},
    {Hunter::Spire, "Spire", "Spire", "Spire_lod0_Model.bin",
     "SpireAlt_lod0_Model.bin", "Spire", "Spire", 6, "SpireGun_Model.bin"},
    {Hunter::Weavel, "Weavel", "Weavel", "Weavel_lod0_Model.bin",
     "WeavelAlt_lod0_Model.bin", "Weavel", "Weavel", 3, "WeavelGun_Model.bin"},
    // Guardian reuses Samus's alt-form model and recolor resources in the
    // managed Metadata.HunterModels table.
    {Hunter::Guardian, "Guardian", "Guardian", "Guardian_lod0_Model.bin",
     "SamusAlt_lod0_Model.bin", "Guardian", "Samus", 0, "SamusGun_Model.bin"}
}};

constexpr std::array<WeaponInfo, WeaponCount> WeaponTable{{
    {0, "POWER", "Power Beam", 6, 24.0F, 2.0F, 5, true, true, false, 0, 0},
    {1, "MISSILE", "Missile", 32, 10.0F, 2.0F, 20, false, true, true, 1, 10},
    {2, "VOLT", "Volt Driver", 14, 40.0F, 2.0F, 5, false, true, false, 0, 5},
    {3, "HAMMER", "Battlehammer", 12, 9.0F, 1.0F, 10, true, false, false, 0, 4},
    {4, "IMPERIAL", "Imperialist", 72, 160.0F, 0.2F, 60, true, false, true, 0, 20},
    {5, "JUDICATOR", "Judicator", 20, 28.0F, 2.0F, 10, false, true, false, 0, 5},
    {6, "MAGMAUL", "Magmaul", 30, 24.0F, 2.0F, 10, true, true, false, 0, 5},
    {7, "SHOCK", "Shock Coil", 16, 32.0F, 2.0F, 5, false, false, false, 0, 0},
    {8, "OMEGA", "Omega Cannon", 200, 200.0F, 0.25F, 60, false, false, false, 0, 0}
}};

// Metadata.Weapons.WeaponsMP.  These are the fields consumed by
// Mods/Network/MechanicsDump.cs; the compact WeaponTable above remains the
// simulation-facing profile.  Values are cartridge units and preserve the
// managed plain entries followed by the affinity entries.
constexpr std::array<MultiplayerWeaponInfo, MultiplayerWeaponCount>
    MultiplayerWeaponTable{{
    {formats::BeamType::PowerBeam, "Power Beam MP", 0x40000700U,
     0, 0, 0, 5, 0, {0, 0}, 18, 30, 0, 0, 0,
     6, 6, 36, 8, 8, 48},
    {formats::BeamType::VoltDriver, "Volt Driver MP", 0x40040200U,
     0, 56, 56, 5, 0, {0, 0}, 15, 60, 5, 25, 25,
     14, 56, 56, 21, 56, 56},
    {formats::BeamType::Missile, "Missile MP", 0x40000200U,
     24, 32, 32, 20, 1, {0, 0}, 15, 45, 10, 15, 15,
     32, 48, 48, 32, 48, 48},
    {formats::BeamType::Battlehammer, "Battlehammer MP", 0x40000400U,
     8, 8, 8, 10, 0, {0, 0}, 15, 60, 4, 4, 4,
     12, 12, 12, 12, 12, 12},
    {formats::BeamType::Imperialist, "Imperialist MP", 0x40000C00U,
     0, 0, 0, 60, 0, {0, 0}, 10, 90, 20, 20, 20,
     72, 72, 72, 200, 200, 200},
    {formats::BeamType::Judicator, "Judicator MP", 0x40008200U,
     12, 10, 10, 15, 0, {0, 0}, 15, 60, 5, 25, 25,
     24, 24, 24, 32, 32, 32},
    {formats::BeamType::Magmaul, "Magmaul MP", 0x40029200U,
     16, 28, 28, 20, 0, {0, 0}, 15, 60, 10, 20, 20,
     32, 56, 56, 32, 56, 56},
    {formats::BeamType::ShockCoil, "Shock Coil MP", 0x40200400U,
     0, 0, 0, 0, 0, {0, 0}, 15, 45, 10, 10, 10,
     10, 10, 10, 10, 10, 10},
    {formats::BeamType::OmegaCannon, "Omega Cannon MP", 0x40000000U,
     200, 200, 200, 60, 0, {0, 0}, 15, 300, 0, 0, 0,
     200, 200, 200, 200, 200, 200},
    {formats::BeamType::PowerBeam, "Power Beam MP Affinity", 0x40000700U,
     0, 0, 0, 4, 0, {0, 0}, 18, 30, 0, 0, 0,
     6, 6, 40, 8, 8, 52},
    {formats::BeamType::VoltDriver, "Volt Driver MP Affinity", 0x40040200U,
     0, 56, 56, 5, 0, {0, 2}, 15, 60, 5, 25, 25,
     14, 56, 56, 21, 56, 56},
    {formats::BeamType::Missile, "Missile MP Affinity", 0x40000200U,
     24, 32, 32, 20, 1, {0, 0}, 15, 45, 10, 15, 15,
     32, 48, 48, 32, 48, 48},
    {formats::BeamType::Battlehammer, "Battlehammer MP Affinity", 0x40000400U,
     12, 12, 12, 15, 0, {0, 0}, 15, 60, 5, 5, 5,
     18, 18, 18, 18, 18, 18},
    {formats::BeamType::Imperialist, "Imperialist MP Affinity", 0x40000C00U,
     0, 0, 0, 60, 0, {0, 0}, 10, 90, 20, 20, 20,
     72, 72, 72, 200, 200, 200},
    {formats::BeamType::Judicator, "Judicator MP Affinity", 0x40108200U,
     12, 0, 0, 15, 0, {0, 1}, 15, 60, 5, 25, 25,
     24, 12, 12, 32, 12, 12},
    {formats::BeamType::Magmaul, "Magmaul MP Affinity", 0x40029200U,
     16, 18, 18, 20, 0, {0, 4}, 15, 60, 10, 20, 20,
     32, 48, 48, 32, 48, 48},
    {formats::BeamType::ShockCoil, "Shock Coil MP Affinity", 0x50200400U,
     0, 0, 0, 0, 0, {0, 0}, 15, 45, 10, 10, 10,
     10, 10, 10, 10, 10, 10},
    {formats::BeamType::OmegaCannon, "Omega Cannon MP Affinity", 0x40000000U,
     60, 60, 60, 60, 0, {0, 0}, 15, 300, 0, 0, 0,
     60, 60, 60, 60, 60, 60}
}};

inline constexpr WeaponVisualInfo make_weapon_visual(
    std::array<std::uint8_t, 2> draw_func_ids,
    std::array<std::uint16_t, 2> colors,
    std::array<std::uint8_t, 2> collision_effects,
    std::array<std::uint8_t, 2> muzzle_effects,
    std::array<std::uint8_t, 2> damage_dir_types,
    std::array<std::uint8_t, 2> damage_interpolations) noexcept {
    return {draw_func_ids, colors, collision_effects, muzzle_effects,
            damage_dir_types, damage_interpolations};
}

// Metadata.Weapons.Weapons1P / WeaponsMP.  The native session currently uses
// the uncharged column; the charged column is retained so the visual state is
// data-driven when the charge/projectile-count path is expanded.
constexpr std::array<WeaponVisualInfo, WeaponCount> WeaponVisualTable{{
    make_weapon_visual({0, 0}, {9055, 21407}, {4, 95}, {65, 65},
                       {0, 0}, {0, 0}),
    make_weapon_visual({1, 2}, {32767, 32767}, {89, 174}, {60, 60},
                       {0, 0}, {2, 2}),
    make_weapon_visual({7, 7}, {32140, 32140}, {8, 193}, {65, 65},
                       {2, 2}, {2, 2}),
    make_weapon_visual({10, 10}, {16367, 16367}, {176, 176}, {63, 63},
                       {2, 2}, {0, 0}),
    make_weapon_visual({8, 8}, {32767, 32767}, {31, 31}, {66, 66},
                       {0, 0}, {3, 3}),
    make_weapon_visual({3, 3}, {32404, 32404}, {10, 10}, {62, 62},
                       {3, 3}, {0, 0}),
    make_weapon_visual({4, 5}, {15711, 15711}, {9, 194}, {64, 64},
                       {2, 2}, {2, 2}),
    make_weapon_visual({9, 9}, {32767, 32767}, {255, 255}, {62, 62},
                       {3, 3}, {3, 3}),
    make_weapon_visual({11, 11}, {32767, 32767}, {248, 248}, {60, 60},
                       {0, 0}, {3, 3})
}};

// Metadata.Effects.  Keeping the complete ID/name table in native code is
// important because effect IDs are stored in room/entity records and the
// native host now preloads the complete effect resource catalog.
constexpr std::array<EffectInfo, EffectCount> EffectTable{{
    {0, "", ""},
    {1, "powerBeam", "effects"},
    {2, "powerBeamNoSplat", "effects"},
    {3, "blastCapHit", ""},
    {4, "blastCapBlow", ""},
    {5, "missile1", "effects"},
    {6, "mortar1", "effects"},
    {7, "shotGunCol", "effects"},
    {8, "shotGunShrapnel", "effects"},
    {9, "bombStart", ""},
    {10, "ballDeath", ""},
    {11, "jackHammerCol", "effects"},
    {12, "effectiveHitPB", ""},
    {13, "effectiveHitElectric", ""},
    {14, "effectiveHitMsl", ""},
    {15, "effectiveHitJack", ""},
    {16, "effectiveHitSniper", ""},
    {17, "effectiveHitIce", ""},
    {18, "effectiveHitMortar", ""},
    {19, "effectiveHitGhost", ""},
    {20, "sprEffectivePB", ""},
    {21, "sprEffectiveElectric", ""},
    {22, "sprEffectiveMsl", ""},
    {23, "sprEffectiveJack", ""},
    {24, "sprEffectiveSniper", ""},
    {25, "sprEffectiveIce", ""},
    {26, "sprEffectiveMortar", ""},
    {27, "sprEffectiveGhost", ""},
    {28, "sniperCol", "effects"},
    {29, "shriekBatTrail", ""},
    {30, "samusFurl", ""},
    {31, "spawnEffect", ""},
    {32, "test", ""},
    {33, "spawnEffectMP", ""},
    {34, "burstFlame", ""},
    {35, "gunSmoke", ""},
    {36, "jetFlame", ""},
    {37, "spireAltSlam", ""},
    {38, "steamBurst", ""},
    {39, "steamSamusShip", ""},
    {40, "steamDoorway", ""},
    {41, "goreaArmChargeUp", ""},
    {42, "goreaBallExplode", ""},
    {43, "goreaShoulderDamageLoop", ""},
    {44, "goreaShoulderHits", ""},
    {45, "goreaShoulderKill", ""},
    {46, "goreaChargeElc", ""},
    {47, "goreaChargeIce", ""},
    {48, "goreaChargeJak", ""},
    {49, "goreaChargeMrt", ""},
    {50, "goreaChargeSnp", ""},
    {51, "goreaFireElc", ""},
    {52, "goreaFireGst", ""},
    {53, "goreaFireIce", ""},
    {54, "goreaFireJak", ""},
    {55, "goreaFireMrt", ""},
    {56, "goreaFireSnp", ""},
    {57, "muzzleElc", ""},
    {58, "muzzleGst", ""},
    {59, "muzzleIce", ""},
    {60, "muzzleJak", ""},
    {61, "muzzleMrt", ""},
    {62, "muzzlePB", ""},
    {63, "muzzleSnp", ""},
    {64, "tear", ""},
    {65, "cylCrystalCharge", ""},
    {66, "cylCrystalKill", ""},
    {67, "cylCrystalShot", ""},
    {68, "tearSplat", ""},
    {69, "eyeShieldCharge", ""},
    {70, "eyeShieldHit", ""},
    {71, "goreaSlam", ""},
    {72, "goreaBallExplode2", ""},
    {73, "cylCrystalKill2", ""},
    {74, "cylCrystalKill3", ""},
    {75, "goreaCrystalExplode", ""},
    {76, "DeathBio1", ""},
    {77, "DeathMech1", ""},
    {78, "iceWave", ""},
    {79, "goreaMeteor", ""},
    {80, "goreaTeleport", ""},
    {81, "tearChargeUp", ""},
    {82, "eyeShield", ""},
    {83, "eyeShieldDefeat", ""},
    {84, "grateSparks", ""},
    {85, "electroCharge", ""},
    {86, "electroHit", ""},
    {87, "torch", ""},
    {88, "jetFlameBlue", ""},
    {89, "lavaBurstLarge", ""},
    {90, "lavaBurstSmall", ""},
    {91, "ember", ""},
    {92, "powerBeamCharge", ""},
    {93, "lavaDemonDive", ""},
    {94, "lavaDemonHurl", ""},
    {95, "lavaDemonRise", ""},
    {96, "iceDemonHurl", ""},
    {97, "lavaBurstExtraLarge", ""},
    {98, "powerBeamChargeNoSplat", ""},
    {99, "powerBeamHolo", ""},
    {100, "powerBeamLava", ""},
    {101, "hangingDrip", ""},
    {102, "hangingSpit", ""},
    {103, "hangingSplash", ""},
    {104, "goreaEyeFlash", ""},
    {105, "smokeBurst", ""},
    {106, "sparks", ""},
    {107, "sparksFall", ""},
    {108, "shriekBatCol", ""},
    {109, "eyeTurretCharge", ""},
    {110, "lavaDemonSplat", ""},
    {111, "tearDrips", ""},
    {112, "syluxShipExhaust", ""},
    {113, "bombStartSylux", ""},
    {114, "lockDefeat", ""},
    {115, "ineffectivePsycho", ""},
    {116, "cylCrystalProjectile", ""},
    {117, "cylWeakSpotShot", ""},
    {118, "eyeLaser", ""},
    {119, "bombStartMP", ""},
    {120, "enemyMslCol", ""},
    {121, "powerBeamHoloBG", ""},
    {122, "powerBeamHoloB", ""},
    {123, "powerBeamIce", ""},
    {124, "powerBeamRock", ""},
    {125, "powerBeamSand", ""},
    {126, "powerBeamSnow", ""},
    {127, "bubblesRising", ""},
    {128, "bombKanden", ""},
    {129, "collapsingStreaks", ""},
    {130, "fireProjectile", ""},
    {131, "iceDemonSplat", ""},
    {132, "iceDemonRise", ""},
    {133, "iceDemonDive", ""},
    {134, "hammerProjectile", ""},
    {135, "synapseKill", ""},
    {136, "samusDash", ""},
    {137, "electroProjectile", ""},
    {138, "cylHomingProjectile", ""},
    {139, "cylHomingKill", ""},
    {140, "energyRippleB", ""},
    {141, "energyRippleBG", ""},
    {142, "energyRippleO", ""},
    {143, "columnCrash", ""},
    {144, "artifactKeyEffect", ""},
    {145, "bombBlue", ""},
    {146, "bombSylux", ""},
    {147, "columnBreak", ""},
    {148, "grappleEnd", ""},
    {149, "bombStartSyluxG", ""},
    {150, "bombStartSyluxO", ""},
    {151, "bombStartSyluxP", ""},
    {152, "bombStartSyluxR", ""},
    {153, "bombStartSyluxW", ""},
    {154, "mpEffectivePB", ""},
    {155, "mpEffectiveElectric", ""},
    {156, "mpEffectiveMsl", ""},
    {157, "mpEffectiveJack", ""},
    {158, "mpEffectiveSniper", ""},
    {159, "mpEffectiveIce", ""},
    {160, "mpEffectiveMortar", ""},
    {161, "mpEffectiveGhost", ""},
    {162, "pipeTricity", ""},
    {163, "breakableExplode", ""},
    {164, "goreaCrystalHit", ""},
    {165, "chargeElc", ""},
    {166, "chargeIce", ""},
    {167, "chargeJak", ""},
    {168, "chargeMrt", ""},
    {169, "chargePB", ""},
    {170, "chargeMsl", ""},
    {171, "electroChargeNA", ""},
    {172, "mortarSecondary", ""},
    {173, "jackHammerColNA", "effects"},
    {174, "goreaMeteorLaunch", ""},
    {175, "goreaReveal", ""},
    {176, "goreaMeteorDamage", ""},
    {177, "goreaMeteorDestroy", ""},
    {178, "goreaMeteorHit", ""},
    {179, "goreaGrappleDamage", ""},
    {180, "goreaGrappleDie", ""},
    {181, "deathBall", ""},
    {182, "nozzleJet", ""},
    {183, "syluxMissile", ""},
    {184, "syluxMissileCol", ""},
    {185, "syluxMissileFlash", ""},
    {186, "sphereTricity", ""},
    {187, "flamingAltForm", ""},
    {188, "flamingGun", ""},
    {189, "flamingHunter", ""},
    {190, "missileCharged", "effects"},
    {191, "mortarCharged", "effects"},
    {192, "mortarChargedAffinity", "effects"},
    {193, "DeathBio2", ""},
    {194, "chargeLoopElc", ""},
    {195, "chargeLoopIce", ""},
    {196, "chargeLoopMrt", ""},
    {197, "chargeLoopMsl", ""},
    {198, "chargeLoopPB", ""},
    {199, "sphereTricitySmall", ""},
    {200, "generatorExplosion", ""},
    {201, "eyeDamageLoop", ""},
    {202, "eyeHit", ""},
    {203, "eyelKill", ""},
    {204, "eyeKill2", ""},
    {205, "eyeKill3", ""},
    {206, "eyeFinalKill", ""},
    {207, "chargeTurret", ""},
    {208, "flashTurret", ""},
    {209, "ultimateProjectile", ""},
    {210, "goreaLaserCharge", ""},
    {211, "mortarProjectile", ""},
    {212, "fallingSnow", ""},
    {213, "fallingDust", ""},
    {214, "fallingRock", ""},
    {215, "DeathMech2", ""},
    {216, "deathAlt", ""},
    {217, "iceDemonDeath", ""},
    {218, "lavaDemonDeath", ""},
    {219, "DeathBio3", ""},
    {220, "DeathBio4", ""},
    {221, "DeathBio5", ""},
    {222, "DeathStatue", ""},
    {223, "DeathTick", ""},
    {224, "goreaLaserCol", ""},
    {225, "goreaHurt", ""},
    {226, "explosionAbove", ""},
    {227, "fireFlurry", ""},
    {228, "snowFlurry", ""},
    {229, "enemySpawn", ""},
    {230, "teleporter", ""},
    {231, "iceShatter", "effects"},
    {232, "sphereTricityDeath", ""},
    {233, "greenFlurry", ""},
    {234, "pmagAbsorb", ""},
    {235, "noxHit", ""},
    {236, "spireBurst", ""},
    {237, "electroProjectileUncharged", ""},
    {238, "enemyProjectile1", ""},
    {239, "enemyCol1", ""},
    {240, "psychoCharge", ""},
    {241, "hammerProjectileSml", ""},
    {242, "nozzleJetOff", ""},
    {243, "powerBeamChargeNoSplatMP", ""},
    {244, "doubleDamageGun", ""},
    {245, "ultimateCol", ""},
    {246, "enemyMortarProjectile", ""}
}};

constexpr std::array<ItemInfo, ItemTableCount> ItemTable{{
    {-1, "NONE", "", -1},
    {0, "HEALTH MEDIUM", "pick_health_B", -1},
    {1, "HEALTH SMALL", "pick_health_A", -1},
    {2, "HEALTH BIG", "pick_health_C", -1},
    {3, "DOUBLE DAMAGE", "pick_dblDamage", -1},
    {4, "ENERGY TANK", "PickUp_EnergyExp", -1},
    {5, "VOLT DRIVER", "pick_wpn_electro", 2},
    {6, "MISSILE EXPANSION", "PickUp_MissileExp", -1},
    {7, "BATTLEHAMMER", "pick_wpn_jackhammer", 3},
    {8, "IMPERIALIST", "pick_wpn_snipergun", 4},
    {9, "JUDICATOR", "pick_wpn_shotgun", 5},
    {10, "MAGMAUL", "pick_wpn_mortar", 6},
    {11, "SHOCK COIL", "pick_wpn_ghostbuster", 7},
    {12, "OMEGA CANNON", "pick_wpn_gorea", 8},
    {13, "UA SMALL", "pick_ammo_green", -1},
    {14, "UA BIG", "pick_ammo_green", -1},
    {15, "MISSILE SMALL", "pick_ammo_orange", -1},
    {16, "MISSILE BIG", "pick_ammo_orange", -1},
    {17, "CLOAK", "pick_invis", -1},
    {18, "UA EXPANSION", "PickUp_AmmoExp", -1},
    {19, "ARTIFACT KEY", "Artifact_Key", -1},
    {20, "DEATHALT", "pick_deathball", -1},
    {21, "AFFINITY WEAPON", "pick_wpn_all", -1},
    {22, "MISSILE WEAPON", "pick_wpn_missile", 1}
}};

constexpr std::array<std::string_view, FhItemCount> FhItemTable{{
    "pick_ammo_A",
    "pick_ammo_B",
    "pick_health_A",
    "pick_health_B",
    "pick_dblDamage",
    "pick_morphball",
    "pick_wpn_electro",
    "pick_wpn_missile"
}};

constexpr std::array<GameModeInfo, 12> ModeTable{{
    {3, "BATTLE", false, false, false},
    {4, "BATTLE TEAMS", true, false, false},
    {5, "SURVIVAL", false, true, false},
    {6, "SURVIVAL TEAMS", true, true, false},
    {7, "CAPTURE", false, false, true},
    {8, "BOUNTY", false, false, true},
    {9, "BOUNTY TEAMS", true, false, true},
    {10, "NODES", false, false, true},
    {11, "NODES TEAMS", true, false, true},
    {12, "DEFENDER", false, false, true},
    {13, "DEFENDER TEAMS", true, false, true},
    {14, "PRIME HUNTER", false, false, true}
}};

constexpr std::array<std::uint16_t, EnemyCount> EnemyDeathEffects{{
    193, 221, 219, 219, 219, 219, 219, 76, 76, 76,
    193, 108, 221, 76, 76, 76, 76, 6, 6, 76,
    77, 76, 76, 77, 76, 76, 76, 76, 76, 76,
    77, 76, 76, 76, 77, 77, 77, 220, 222, 0,
    6, 76, 76, 76, 76, 76, 223, 223, 76, 77,
    0, 220
}};

constexpr std::array<std::uint8_t, EnemyCount> EnemyAudioRanges{{
    8, 9, 10, 11, 11, 11, 11, 4, 4, 8,
    8, 12, 9, 4, 4, 4, 13, 18, 18, 15,
    15, 19, 15, 16, 22, 22, 22, 22, 22, 22,
    22, 22, 22, 34, 16, 20, 20, 9, 4, 4,
    24, 25, 25, 25, 25, 25, 30, 30, 4, 4,
    4, 4
}};

constexpr std::array<std::uint16_t, EnemyCount> EnemyScanIds{{
    214, 210, 0, 224, 224, 224, 224, 0, 0, 0,
    215, 213, 211, 0, 0, 0, 212, 219, 219, 0,
    0, 226, 0, 216, 243, 0, 241, 0, 244, 242,
    467, 0, 466, 0, 0, 217, 217, 218, 221, 222,
    0, 227, 227, 227, 227, 227, 220, 245, 0, 0,
    0, 0
}};

constexpr std::array<std::uint32_t, EnemyCount> EnemyEffectiveness{{
    0x2AAAA, 0x2AAAA, 0x2AAAA, 0x2AAAA, 0x2AAAA, 0x2AAAA, 0x2AAAA,
    0x2AAAA, 0x2AAAA, 0x2AAAA, 0x2AAAA, 0x2AAAA, 0x2AAA8, 0x00000,
    0x2AAAA, 0x2AAAA, 0x2AAAA, 0x2AAAA, 0x2AAAA, 0x2AAAA, 0x2AAAA,
    0x2AAAA, 0x2AAAA, 0x2AAAA, 0x2AAAA, 0x2AAAA, 0x2AAAA, 0x2AAAA,
    0x2AAAA, 0x2AAAA, 0x2AAAA, 0x20000, 0x2AAAA, 0x2AAAA, 0x2AABA,
    0x2AABA, 0x2AAAA, 0x2EAFA, 0x24D55, 0x2AAAA, 0x2AA99, 0x2AA99,
    0x2AA99, 0x2AA99, 0x2AA99, 0x2AAAA, 0x2AAAA, 0x2AAAA, 0x2AAAA,
    0x2AAAA, 0x2AAAA
}};

constexpr std::array<EnemyInfo, EnemyCount> EnemyTable = [] {
    std::array<EnemyInfo, EnemyCount> result{};
    for (std::size_t i = 0; i < EnemyCount; ++i) {
        result[i] = EnemyInfo{
            static_cast<formats::EnemyType>(i), EnemyDeathEffects[i],
            EnemyAudioRanges[i], EnemyScanIds[i], EnemyEffectiveness[i]
        };
    }
    return result;
}();


} // namespace fruityprime::metadata::detail
