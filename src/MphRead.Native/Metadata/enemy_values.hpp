#pragma once

// Per-enemy tuning records from Entities/Enemies/*.cs and the tables that
// fill them in Metadata/Enemies.cs.  Transliterated from the managed sources:
// these are several hundred cartridge tuning numbers that must not be retyped.

#include "Formats/Types.hpp"

#include <array>
#include <cstdint>

namespace fruityprime::metadata {

// 10_BarbedWarWasp.cs
struct Enemy10Values {
    std::uint16_t HealthMax{};
    std::uint16_t BeamDamage{};
    std::uint16_t SplashDamage{};
    std::uint16_t ContactDamage{};
    std::int32_t StepDistance1{};
    std::int32_t StepDistance2{};
    std::int32_t StepDistance3{};
    std::int32_t CircleIncrement{};
    std::int32_t Unknown18{};
    std::int16_t MinShots{};
    std::int16_t MaxShots{};
    std::int32_t ScanId{};
    std::int32_t Effectiveness{};
};

// Metadata.Enemy10Values
inline constexpr std::array<Enemy10Values, 3> Enemy10ValuesTable{{
    {50, 3, 0, 15, 1024, 819, 2457, 6144, 0x3C001E, 1, 2, 215, 0xEABA},
    {120, 10, 2, 10, 1433, 1638, 2457, 6144, 0x3C001E, 1, 3, 191, 0xCEAA},
    {120, 8, 0, 10, 614, 409, 1024, 6144, 0x3C001E, 1, 1, 192, 0xF2AA}
}};

// 18_AlimbicTurret.cs
struct Enemy18Values {
    std::uint16_t HealthMax{};
    std::uint16_t BeamDamage{};
    std::uint16_t SplashDamage{};
    std::uint16_t ContactDamage{};
    std::int32_t MinAngleY{};
    std::int32_t MaxAngleY{};
    std::int32_t AngleIncY{};
    std::int32_t MinAngleX{};
    std::int32_t MaxAngleX{};
    std::int32_t AngleIncX{};
    std::uint16_t ShotCooldown{};
    std::uint16_t DelayTime{};
    std::uint16_t MinShots{};
    std::uint16_t MaxShots{};
    std::int32_t Unused28{};
    std::uint16_t Unused2C{};
    std::uint16_t Unused2E{};
    std::int32_t ShotOffset{};
    std::int32_t ScanId{};
    std::int32_t Effectiveness{};
};

// Metadata.Enemy18Values
inline constexpr std::array<Enemy18Values, 3> Enemy18ValuesTable{{
    {24, 2, 0, 5, -184320, 184320, 4096, 0, 245760, 4096, 5, 40, 1, 2, 214, 4090, 0, 4096, 219, 0xEAAA},
    {80, 4, 2, 5, -184320, 184320, 4096, 0, 245760, 4096, 3, 30, 3, 5, 214, 4090, 0, 4096, 196, 0xEABA},
    {120, 50, 0, 5, -184320, 184320, 4096, 0, 245760, 4096, 3, 90, 1, 1, 214, 4090, 0, 4096, 197, 0xEABA}
}};

// 19_Cretaphid.cs
struct Enemy19Values {
    std::uint16_t CrystalHealth{};
    std::uint16_t PhaseFlashTime{};
    std::uint16_t Phase0CrystalHealth{};
    std::uint16_t Phase1CrystalHealth{};
    std::uint16_t Phase2CrystalHealth{};
    std::uint16_t Phase0CrystalShotTime{};
    std::uint16_t Phase1CrystalShotTime{};
    std::uint16_t Phase2CrystalShotTime{};
    std::uint16_t Phase0CrystalShotDelay{};
    std::uint16_t Phase1CrystalShotDelay{};
    std::uint16_t Phase2CrystalShotDelay{};
    std::uint16_t Phase0CrystalUpTime{};
    std::uint16_t Phase1CrystalUpTime{};
    std::uint16_t Phase2CrystalUpTime{};
    std::array<std::uint16_t, 3> CrystalBeamDamage{};
    std::array<std::uint16_t, 3> EyeBeamDamage{};
    std::array<std::uint16_t, 3> EyeSplashDamage{};
    std::array<std::uint16_t, 3> EyeContactDamage{};
    std::int32_t Unused34{};
    std::int32_t Unused38{};
    std::int32_t Unused3C{};
    std::int32_t Seg0AngleStep{};
    std::int32_t Seg1AngleStep{};
    std::int32_t Seg2AngleStep{};
    std::int32_t Seg0BeamStartAngle{};
    std::int32_t Seg1BeamStartAngle{};
    std::int32_t Seg2BeamStartAngle{};
    std::int32_t Seg0BeamAngleMin{};
    std::int32_t Seg1BeamAngleMin{};
    std::int32_t Seg2BeamAngleMin{};
    std::int32_t Seg0BeamAngleMax{};
    std::int32_t Seg1BeamAngleMax{};
    std::int32_t Seg2BeamAngleMax{};
    std::int32_t Seg0BeamAngleStep{};
    std::int32_t Seg1BeamAngleStep{};
    std::int32_t Seg2BeamAngleStep{};
    std::uint16_t EyeHealth{};
    std::uint8_t ItemChanceHealth{};
    std::uint8_t ItemChanceMissile{};
    std::uint8_t ItemChanceUa{};
    std::uint8_t ItemChanceNone{};
    std::array<std::uint8_t, 12> Phase0EyeState{};
    std::array<std::uint8_t, 12> Phase0BeamType{};
    std::array<std::uint8_t, 12> Phase0BeamSpawnMin{};
    std::array<std::uint8_t, 12> Phase0BeamSpawnMax{};
    std::array<std::uint16_t, 12> Phase0BeamCooldown{};
    std::array<std::uint16_t, 12> Phase0EyeStateTimer0{};
    std::array<std::uint16_t, 12> Phase0EyeStateTimer1{};
    std::array<std::uint16_t, 12> Phase0EyeStateTimer2{};
    std::array<std::uint16_t, 12> Phase0EyeStateTimer3{};
    std::array<std::uint8_t, 12> Phase1EyeState{};
    std::array<std::uint8_t, 12> Phase1BeamType{};
    std::array<std::uint8_t, 12> Phase1BeamSpawnMin{};
    std::array<std::uint8_t, 12> Phase1BeamSpawnMax{};
    std::array<std::uint16_t, 12> Phase1BeamCooldown{};
    std::array<std::uint16_t, 12> Phase1EyeStateTimer0{};
    std::array<std::uint16_t, 12> Phase1EyeStateTimer1{};
    std::array<std::uint16_t, 12> Phase1EyeStateTimer2{};
    std::array<std::uint16_t, 12> Phase1EyeStateTimer3{};
    std::array<std::uint8_t, 12> Phase2EyeState{};
    std::array<std::uint8_t, 12> Phase2BeamType{};
    std::array<std::uint8_t, 12> Phase2BeamSpawnMin{};
    std::array<std::uint8_t, 12> Phase2BeamSpawnMax{};
    std::array<std::uint16_t, 12> Phase2BeamCooldown{};
    std::array<std::uint16_t, 12> Phase2EyeStateTimer0{};
    std::array<std::uint16_t, 12> Phase2EyeStateTimer1{};
    std::array<std::uint16_t, 12> Phase2EyeStateTimer2{};
    std::array<std::uint16_t, 12> Phase2EyeStateTimer3{};
    std::uint8_t ItemChanceA{};
    std::uint8_t ItemChanceB{};
    std::uint8_t ItemChanceC{};
    std::uint8_t ItemChanceD{};
    std::uint16_t Padding27E{};
    std::int32_t CollisionRadius{};
    std::uint16_t ScanId{};
    std::uint16_t CrystalScanId{};
    std::uint32_t CrystalEffectiveness{};
    std::int32_t EyeScanId{};
    std::uint32_t EyeEffectiveness{};
};

// Metadata.Enemy19Values
inline constexpr std::array<Enemy19Values, 4> Enemy19ValuesTable{{
    {490, 60, 360, 200, 0, 30, 20, 13, 1, 1, 1, 150, 120, 90, {6, 6, 6}, {3, 3, 3}, {0, 0, 0}, {3, 3, 3}, 3547, 3547, 3547, 2457, 2048, 3686, 163840, 143360, 73728, 122880, 81920, -40960, 327680, 307200, 225280, 3481, 3072, 4096, 12, 8, 5, 0, 87, {5, 5, 5, 2, 2, 2, 2, 2, 2, 2, 2, 2}, {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}, {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5}, {30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30}, {30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30}, {200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200}, {20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20}, {2, 2, 2, 5, 5, 5, 5, 2, 2, 2, 2, 2}, {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}, {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5}, {30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30}, {30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30}, {280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280}, {20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20}, {2, 2, 2, 2, 2, 2, 2, 5, 5, 5, 5, 5}, {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}, {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5}, {30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30}, {30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30}, {360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360}, {20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20}, 0, 0, 0, 0, 0, 3276, 0, 226, 0xAAAA, 0, 0xAA9A},
    {540, 60, 400, 230, 0, 15, 12, 9, 10, 10, 10, 150, 120, 90, {10, 10, 10}, {5, 5, 5}, {0, 0, 0}, {3, 3, 3}, 3547, 3547, 3547, 2457, 2048, 3686, 163840, 143360, 73728, 122880, 81920, 40960, 245760, 204800, 143360, 3072, 4096, 4096, 4, 10, 5, 5, 80, {1, 1, 1, 0, 0, 0, 0, 4, 4, 4, 4, 4}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10}, {130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130}, {50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50}, {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10}, {50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50}, {0, 0, 0, 4, 4, 4, 4, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10}, {130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130}, {45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45}, {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10}, {45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45}, {1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10}, {60, 90, 30, 60, 90, 120, 30, 60, 90, 120, 90, 30}, {45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45}, {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10}, {45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45}, 0, 0, 0, 0, 0, 3276, 0, 198, 0xD5EA, 0, 0xAAAA},
    {570, 60, 420, 230, 0, 15, 12, 9, 10, 10, 10, 150, 120, 90, {12, 12, 12}, {4, 4, 4}, {1, 1, 1}, {8, 8, 8}, 3547, 3547, 3547, 2457, 2048, 3686, 163840, 143360, 73728, 122880, 81920, -40960, 327680, 307200, 225280, 3481, 3072, 4096, 4, 8, 5, 5, 82, {0, 0, 0, 1, 1, 1, 1, 4, 4, 4, 4, 4}, {2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {5, 5, 5, 10, 10, 10, 10, 10, 10, 10, 10, 10}, {30, 30, 30, 130, 130, 130, 130, 130, 130, 130, 130, 130}, {30, 30, 30, 50, 50, 50, 50, 50, 50, 50, 50, 50}, {180, 180, 180, 10, 10, 10, 10, 10, 10, 10, 10, 10}, {20, 20, 20, 50, 50, 50, 50, 50, 50, 50, 50, 50}, {1, 1, 1, 0, 0, 0, 0, 4, 4, 4, 4, 4}, {1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {10, 10, 10, 5, 5, 5, 5, 10, 10, 10, 10, 10}, {130, 130, 130, 30, 30, 30, 30, 130, 130, 130, 130, 130}, {45, 45, 45, 30, 30, 30, 30, 45, 45, 45, 45, 45}, {10, 10, 10, 220, 220, 220, 220, 10, 10, 10, 10, 10}, {45, 45, 45, 20, 20, 20, 20, 45, 45, 45, 45, 45}, {1, 1, 1, 4, 4, 4, 4, 0, 0, 0, 0, 0}, {1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {10, 10, 10, 10, 10, 10, 10, 5, 5, 5, 5, 5}, {40, 40, 40, 40, 40, 40, 40, 30, 30, 30, 30, 30}, {45, 45, 45, 45, 45, 45, 45, 30, 30, 30, 30, 30}, {10, 10, 10, 10, 10, 10, 10, 260, 260, 260, 260, 260}, {45, 45, 45, 45, 45, 45, 45, 20, 20, 20, 20, 20}, 0, 0, 0, 0, 0, 3276, 0, 199, 0xD5EA, 0, 0xAAAA},
    {550, 60, 385, 220, 0, 12, 10, 8, 10, 10, 10, 150, 120, 90, {18, 18, 18}, {4, 4, 4}, {1, 1, 1}, {10, 10, 10}, 3547, 3547, 3547, 2457, 2048, 3686, 163840, 143360, 73728, 122880, 81920, -40960, 327680, 307200, 225280, 3481, 3072, 4096, 4, 8, 5, 5, 82, {0, 0, 0, 1, 1, 1, 1, 4, 4, 4, 4, 4}, {2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {5, 5, 5, 10, 10, 10, 10, 10, 10, 10, 10, 10}, {30, 30, 30, 130, 130, 130, 130, 130, 130, 130, 130, 130}, {30, 30, 30, 50, 50, 50, 50, 50, 50, 50, 50, 50}, {180, 180, 180, 10, 10, 10, 10, 10, 10, 10, 10, 10}, {20, 20, 20, 50, 50, 50, 50, 50, 50, 50, 50, 50}, {1, 1, 1, 0, 0, 0, 0, 4, 4, 4, 4, 4}, {0, 0, 0, 2, 2, 2, 2, 0, 0, 0, 0, 0}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {10, 10, 10, 5, 5, 5, 5, 10, 10, 10, 10, 10}, {130, 130, 130, 30, 30, 30, 30, 130, 130, 130, 130, 130}, {45, 45, 45, 30, 30, 30, 30, 45, 45, 45, 45, 45}, {10, 10, 10, 220, 220, 220, 220, 10, 10, 10, 10, 10}, {45, 45, 45, 20, 20, 20, 20, 45, 45, 45, 45, 45}, {1, 1, 1, 4, 4, 4, 4, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {10, 10, 10, 10, 10, 10, 10, 5, 5, 5, 5, 5}, {130, 130, 130, 130, 130, 130, 130, 30, 30, 30, 30, 30}, {45, 45, 45, 45, 45, 45, 45, 30, 30, 30, 30, 30}, {10, 10, 10, 10, 10, 10, 10, 260, 260, 260, 260, 260}, {45, 45, 45, 45, 45, 45, 45, 20, 20, 20, 20, 20}, 0, 0, 0, 0, 0, 3276, 0, 200, 0xD5E6, 0, 0xAAAA}
}};

// 23_PsychoBit.cs
struct Enemy23Values {
    std::uint16_t HealthMax{};
    std::uint16_t BeamDamage{};
    std::uint16_t SplashDamage{};
    std::uint16_t ContactDamage{};
    std::int32_t MinSpeedFactor1{};
    std::int32_t MaxSpeedFactor1{};
    std::int32_t MinSpeedFactor2{};
    std::int32_t MaxSpeedFactor2{};
    std::int32_t RangeMaxCosine{};
    std::int32_t Unknown1C{};
    std::int32_t Unused20{};
    std::uint16_t DelayTime{};
    std::uint16_t ShotTime{};
    std::int32_t Unused28{};
    std::uint16_t MinShots{};
    std::uint16_t MaxShots{};
    std::uint16_t DoubleSpeedSteps{};
    std::uint16_t AimSteps{};
    std::uint16_t SpeedSteps{};
    std::int16_t ScanId{};
    std::int32_t Effectiveness{};
};

// Metadata.Enemy23Values
inline constexpr std::array<Enemy23Values, 5> Enemy23ValuesTable{{
    {11, 1, 0, 10, 409, 918, 1638, 2252, -4096, 2457, 40960, 40, 15, 1638400, 1, 2, 50, 4, 26, 216, 0xEAAA},
    {24, 2, 1, 10, 614, 1228, 2867, 3686, -4096, 3276, 61440, 25, 8, 1638400, 1, 3, 60, 4, 30, 216, 0xEABA},
    {120, 10, 2, 10, 614, 1228, 2457, 3276, -4096, 4096, 40960, 25, 40, 1638400, 1, 1, 60, 4, 30, 208, 0xEAF2},
    {120, 8, 2, 10, 614, 1228, 1638, 2048, -4096, 2048, 40960, 25, 30, 1638400, 1, 2, 20, 30, 10, 209, 0xCEF9},
    {120, 8, 0, 10, 614, 1228, 1638, 2048, -4096, 4096, 61440, 25, 40, 1638400, 1, 1, 20, 30, 10, 207, 0xF2F9}
}};

// 36_Voldrum.cs
struct Enemy36Values {
    std::uint16_t HealthMax{};
    std::uint16_t BeamDamage{};
    std::uint16_t SplashDamage{};
    std::uint16_t ContactDamage{};
    std::int32_t MinSpeedFactor{};
    std::int32_t MaxSpeedFactor{};
    std::uint16_t DoubleSpeedSteps{};
    std::uint16_t AimSteps{};
    std::uint16_t DelayTime{};
    std::uint16_t ShotTime{};
    std::uint16_t MinShots{};
    std::uint16_t MaxShots{};
    std::int32_t JumpSpeed{};
    std::int32_t RangeMaxCosine{};
    std::uint16_t SpeedSteps{};
    std::int16_t ScanId{};
    std::int32_t Effectiveness{};
};

// Metadata.Enemy36Values
inline constexpr std::array<Enemy36Values, 5> Enemy36ValuesTable{{
    {55, 2, 0, 7, 819, 1433, 15, 10, 40, 15, 1, 2, 819, -4096, 7, 217, 0xEABA},
    {100, 5, 1, 7, 819, 1433, 15, 10, 25, 8, 2, 3, 819, -4096, 7, 217, 0xEABA},
    {150, 10, 2, 7, 819, 1433, 15, 10, 30, 25, 1, 1, 819, -4096, 7, 193, 0xEAB2},
    {150, 8, 2, 7, 1228, 2048, 15, 10, 25, 20, 1, 2, 819, -4096, 7, 194, 0xCEBA},
    {152, 8, 0, 7, 409, 1024, 15, 10, 25, 40, 1, 2, 819, -4096, 7, 195, 0xF2BA}
}};

// 39_FireSpawn.cs
struct Enemy39Values {
    std::uint16_t HealthMax{};
    std::uint16_t BeamDamage{};
    std::uint16_t SplashDamage{};
    std::uint16_t ContactDamage{};
    std::int16_t Unused8{};
    std::uint16_t AttackDelay{};
    std::uint16_t AttackCountMin{};
    std::uint16_t AttackCountMax{};
    std::uint16_t DiveTimerMin{};
    std::uint16_t DiveTimerMax{};
    std::int32_t Unused14{};
    std::int16_t Unused18{};
    std::int16_t ScanId{};
    std::int32_t Effectiveness{};
};

// Metadata.Enemy39Values
inline constexpr std::array<Enemy39Values, 2> Enemy39ValuesTable{{
    {600, 30, 15, 12, 600, 0, 3, 6, 1, 40, 0x100010, 50, 222, 0x8955},
    {600, 30, 0, 12, 600, 0, 2, 5, 1, 50, 0x100010, 50, 240, 0xB155}
}};

// 41_Slench.cs
struct Enemy41Values {
    std::uint16_t ScanId1{};
    std::uint16_t ScanId2{};
    std::int32_t AngleIncrement1{};
    std::int32_t Health{};
    std::int32_t AngleIncrement2{};
    std::uint16_t MinStaticShotTimer{};
    std::uint16_t MaxStaticShotTimer{};
    std::int16_t StaticShotCooldown{};
    std::uint8_t StaticShotCount{};
    std::uint8_t Padding17{};
    std::int32_t AngleIncrement3{};
    std::int32_t MoveIncrement1{};
    std::int32_t MoveIncrement2{};
    std::int32_t AngleIncrement4{};
    std::int32_t RoamTime{};
    std::int32_t MoveIncrement3{};
    std::int32_t RollTime{};
    std::int32_t FloatingAngleInc{};
    std::int32_t RollingAngleInc{};
    std::int32_t FloatingSpeed{};
    std::int32_t RollingSpeed{};
    std::int32_t AngleIncrement5{};
    std::int32_t SlamRange{};
    std::int32_t MoveIncrement4{};
    std::int32_t MoveIncrement5{};
    std::uint16_t SlamDelay{};
    std::uint8_t WobbleCycles{};
    std::uint8_t WobbleRotInc{};
    std::int32_t MaxWobbleDist{};
    std::uint16_t Magic{};
    std::uint16_t Padding5E{};
};

// Metadata.Enemy41Values
inline constexpr std::array<Enemy41Values, 12> Enemy41ValuesTable{{
    {227, 201, 16384, 200, 4096, 60, 120, 30, 1, 0, 4096, 1024, 614, 8192, 600, 410, 81920, 10240, 0, 12288, 0, 0, 0, 0, 0, 0, 2, 64, 2048, 0xBEEF, 0},
    {0, 0, 16384, 0, 4096, 60, 120, 30, 2, 0, 4096, 1024, 614, 16384, 450, 410, 81920, 18432, 0, 15565, 0, 0, 0, 0, 0, 0, 2, 64, 2048, 0xBEEF, 0},
    {0, 0, 16384, 0, 4096, 60, 120, 20, 2, 0, 4096, 1024, 614, 16384, 300, 410, 81920, 26624, 0, 13517, 0, 0, 0, 0, 0, 0, 2, 64, 2048, 0xBEEF, 0},
    {202, 203, 16384, 1200, 4096, 60, 120, 25, 1, 0, 4096, 1024, 614, 16384, 600, 410, 81920, 14336, 0, 14336, 0, 0, 0, 0, 0, 0, 2, 64, 2048, 0xBEEF, 0},
    {0, 0, 16384, 0, 4096, 60, 120, 25, 2, 0, 4096, 1024, 614, 16384, 540, 819, 81920, 18432, 0, 16384, 0, 0, 0, 0, 0, 0, 2, 64, 2048, 0xBEEF, 0},
    {0, 0, 16384, 0, 4096, 60, 120, 25, 3, 0, 4096, 1024, 614, 16384, 450, 1229, 81920, 24576, 0, 16384, 0, 0, 0, 0, 0, 0, 2, 64, 2048, 0xBEEF, 0},
    {204, 205, 32768, 900, 4096, 60, 120, 25, 2, 0, 4096, 1024, 614, 16384, 750, 819, 81920, 14336, 0, 14336, 0, 16384, 40960, 4096, 3277, 150, 10, 32, 2048, 0xBEEF, 0},
    {0, 0, 32768, 0, 4096, 60, 120, 25, 3, 0, 4096, 1024, 614, 16384, 660, 819, 81920, 18432, 0, 16384, 0, 32768, 40960, 4915, 3277, 120, 10, 48, 2048, 0xBEEF, 0},
    {0, 0, 32768, 0, 4096, 60, 120, 20, 3, 0, 4096, 1024, 614, 16384, 600, 819, 81920, 24576, 0, 16384, 0, 32768, 40960, 6144, 3277, 120, 12, 55, 2048, 0xBEEF, 0},
    {206, 223, 32768, 800, 4096, 60, 120, 20, 2, 0, 4096, 1024, 614, 16384, 1500, 819, 600, 14336, 12288, 14336, 22528, 32768, 40960, 4096, 3277, 150, 10, 50, 2048, 0xBEEF, 0},
    {0, 0, 32768, 0, 4096, 60, 120, 20, 3, 0, 4096, 1024, 614, 16384, 1500, 600, 819, 18432, 14336, 16384, 24576, 32768, 40960, 4915, 3277, 120, 10, 55, 2048, 0xBEEF, 0},
    {0, 0, 32768, 0, 4096, 60, 120, 20, 4, 0, 4096, 1024, 614, 16384, 1800, 900, 819, 26624, 16384, 16384, 24576, 32768, 49152, 6144, 3277, 120, 12, 65, 2048, 0xBEEF, 0}
}};

// 44_SlenchSynapse.cs
struct Enemy44Values {
    std::uint16_t ScanId{};
    std::uint16_t Health{};
    std::uint16_t HealTimer{};
    std::uint16_t ReappearTimer{};
    std::int32_t ColRadius{};
    std::uint16_t Magic{};
    std::uint16_t PaddingE{};
};

// Metadata.Enemy44Values
inline constexpr std::array<Enemy44Values, 12> Enemy44ValuesTable{{
    {0, 36, 120, 360, 6144, 0xBEEF, 0},
    {0, 36, 120, 300, 6144, 0xBEEF, 0},
    {0, 36, 120, 240, 6144, 0xBEEF, 0},
    {0, 72, 120, 600, 6144, 0xBEEF, 0},
    {0, 72, 120, 480, 6144, 0xBEEF, 0},
    {0, 96, 120, 360, 6144, 0xBEEF, 0},
    {0, 48, 120, 420, 6144, 0xBEEF, 0},
    {0, 48, 120, 360, 6144, 0xBEEF, 0},
    {0, 48, 120, 300, 6144, 0xBEEF, 0},
    {0, 90, 120, 600, 6144, 0xBEEF, 0},
    {0, 90, 120, 540, 6144, 0xBEEF, 0},
    {0, 90, 120, 480, 6144, 0xBEEF, 0}
}};

// 45_SlenchTurret.cs
struct Enemy45Values {
    std::uint16_t Health{};
    std::uint16_t Damage{};
    std::uint16_t Unused4{};
    std::uint16_t ContactDamage{};
    std::uint16_t ShotCooldown{};
    std::uint16_t SalvoCooldown{};
    std::uint16_t MinShots{};
    std::uint16_t MaxShots{};
    std::int32_t ScanId{};
    std::int32_t Effectiveness{};
};

// Metadata.Enemy45Values
inline constexpr std::array<Enemy45Values, 4> Enemy45ValuesTable{{
    {30, 2, 0, 10, 20, 90, 1, 1, 190, 0xAAAA},
    {30, 2, 1, 10, 20, 90, 1, 1, 190, 0xAAAA},
    {30, 2, 1, 10, 20, 250, 1, 1, 190, 0xAAAA},
    {30, 2, 1, 5, 20, 250, 1, 1, 190, 0xAAAA}
}};

} // namespace fruityprime::metadata
