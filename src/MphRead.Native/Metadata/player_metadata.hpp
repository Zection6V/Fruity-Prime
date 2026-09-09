#pragma once

// Native counterpart of Metadata/Player.cs.  These are the cartridge's own
// per-hunter tables; the values are transliterated from the managed source so
// the two implementations cannot drift.

#include "Formats/Types.hpp"
#include "Metadata/player_values.hpp"

#include <array>
#include <cstdint>

namespace fruityprime::metadata::player {

// PlayerMetadata.SpireAltVectors
inline constexpr std::array<formats::Vector3, 16> SpireAltVectors{{
{0.06396484375F, -0.601806640625F, -0.345947265625F},
{0.1767578125F, 0.763916015625F, 0.046875F},
{0.6767578125F, -0.18798828125F, -0.075927734375F},
{-0.558837890625F, -0.1708984375F, 0.4599609375F},
{0.298828125F, -0.638916015625F, 0.18896484375F},
{-0.10986328125F, 0.532958984375F, 0.52490234375F},
{0.602783203125F, 0.298828125F, -0.23583984375F},
{-0.5458984375F, -0.2548828125F, -0.3759765625F},
{-0.370849609375F, -0.61279296875F, 0.125F},
{-0.228759765625F, 0.69287109375F, -0.23193359375F},
{0.574951171875F, 0.2177734375F, 0.310791015625F},
{-0.657958984375F, 0.2958984375F, -0.06396484375F},
{0.178955078125F, -0.31494140625F, 0.642822265625F},
{-0.1259765625F, 0.14697265625F, 0.701904296875F},
{0.161865234375F, 0.31494140625F, -0.664794921875F},
{-0.130859375F, -0.179931640625F, -0.743896484375F}
}};

// PlayerMetadata.SlipSpeedFactors
inline constexpr std::array<float, 4> SlipSpeedFactors{
0, -0.5F, 0.9F, 0.94F
};

// PlayerMetadata.TractionFactors
inline constexpr std::array<float, 4> TractionFactors{
1, 0.2F, 0.6F, 0.5F
};

// PlayerMetadata.MuzzleOffests (the managed spelling is kept so a reader can
// grep both trees for the same name).
inline constexpr std::array<formats::Vector3, 8> MuzzleOffests{{
{0.63F, -0.02F, 0},
{0.6F, 0, 0},
{1.2F, 0, -0.2F},
{0.765F, -0.05F, -0.16F},
{1.11F, 0.15F, 0},
{0.71F, 0, 0},
{0.89F, 0.15F, -0.172F},
{0, 0, 0.32F}
}};

// PlayerMetadata.MuzzleEffectIds
inline constexpr std::array<int, 9> MuzzleEffectIds{
62, 57, 62, 60, 63, 59, 61, 58, 62
};

// PlayerMetadata.ChargeEffectIds
inline constexpr std::array<int, 9> ChargeEffectIds{
169, 165, 170, 169, 169, 166, 168, 169, 169
};

// PlayerMetadata.ChargeLoopEffectIds
inline constexpr std::array<int, 9> ChargeLoopEffectIds{
198, 194, 197, 198, 198, 195, 196, 198, 198
};

// PlayerMetadata.GunAnimationIds[hunter][animation][weapon]
inline constexpr int GunAnimationIds[8][13][10] =
{
            // Samus
            {
                //   PB  VD Ms  Bh  Im  Jd  Mg  SC  OC
                { 0, -1, 3, -1, 25, 48, 17, 33, 10, -1 },   // FullCharge
                { 1, -1, 4, -1, 26, 49, 18, 34, 11, -1 },   // ChargeShot
                { 2, -1, 5, -1, 27, 50, 19, 35, 12, -1 },   // Charging
                { 24, -1, 6, -1, 28, 51, 20, 36, 13, -1 },  // Idle
                { 32, -1, 7, -1, 29, 52, 21, 37, 14, -1 },  // Switch
                { 40, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // FullChargeMissile
                { 41, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // ChargingMissile
                { 42, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileClose
                { 43, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileOpen
                { 44, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // Unknown9
                { 45, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileShot
                { 46, -1, 8, -1, 30, 53, 22, 38, 15, -1 },  // UpDown
                { 47, -1, 9, -1, 31, 54, 23, 39, 16, -1 }   // Shot
            },
            // Kanden
            {
                //   PB  VD Ms  Bh  Im  Jd  Mg  SC  OC
                { 0, -1, 3, -1, 25, 48, 17, 33, 10, -1 },   // FullCharge
                { 1, -1, 4, -1, 26, 49, 18, 34, 11, -1 },   // ChargeShot
                { 2, -1, 5, -1, 27, 50, 19, 35, 12, -1 },   // Charging
                { 24, -1, 6, -1, 28, 51, 20, 36, 13, -1 },  // Idle
                { 32, -1, 7, -1, 29, 52, 21, 37, 14, -1 },  // Switch
                { 40, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // FullChargeMissile
                { 41, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // ChargingMissile
                { 42, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileClose
                { 43, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileOpen
                { 44, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // Unknown9
                { 45, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileShot
                { 46, -1, 8, -1, 30, 53, 22, 38, 15, -1 },  // UpDown
                { 47, -1, 9, -1, 31, 54, 23, 39, 16, -1 }   // Shot
            },
            // Trace
            {
                //   PB  VD Ms  Bh  Im  Jd  Mg  SC  OC
                { 0, -1, 3, -1, 25, 48, 17, 33, 10, -1 },   // FullCharge
                { 1, -1, 4, -1, 26, 49, 18, 34, 11, -1 },   // ChargeShot
                { 2, -1, 5, -1, 27, 50, 19, 35, 12, -1 },   // Charging
                { 24, -1, 6, -1, 28, 51, 20, 36, 13, -1 },  // Idle
                { 32, -1, 7, -1, 29, 52, 21, 37, 14, -1 },  // Switch
                { 40, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // FullChargeMissile
                { 41, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // ChargingMissile
                { 42, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileClose
                { 43, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileOpen
                { 44, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // Unknown9
                { 45, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileShot
                { 46, -1, 8, -1, 30, 53, 22, 38, 15, -1 },  // UpDown
                { 47, -1, 9, -1, 31, 54, 23, 39, 16, -1 }   // Shot
            },
            // Sylux
            {
                //   PB  VD Ms  Bh  Im  Jd  Mg  SC  OC
                { 0, -1, 3, -1, 25, 48, 17, 33, 10, -1 },   // FullCharge
                { 1, -1, 4, -1, 26, 49, 18, 34, 11, -1 },   // ChargeShot
                { 2, -1, 5, -1, 27, 50, 19, 35, 12, -1 },   // Charging
                { 24, -1, 6, -1, 28, 51, 20, 36, 13, -1 },  // Idle
                { 32, -1, 7, -1, 29, 52, 21, 37, 14, -1 },  // Switch
                { 40, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // FullChargeMissile
                { 41, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // ChargingMissile
                { 42, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileClose
                { 43, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileOpen
                { 44, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // Unknown9
                { 45, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileShot
                { 46, -1, 8, -1, 30, 53, 22, 38, 15, -1 },  // UpDown
                { 47, -1, 9, -1, 31, 54, 23, 39, 16, -1 }   // Shot
            },
            // Noxus
            {
                //   PB  VD Ms  Bh  Im  Jd  Mg  SC  OC
                { 0, -1, 3, -1, 25, 48, 17, 33, 10, -1 },   // FullCharge
                { 1, -1, 4, -1, 26, 49, 18, 34, 11, -1 },   // ChargeShot
                { 2, -1, 5, -1, 27, 50, 19, 35, 12, -1 },   // Charging
                { 24, -1, 6, -1, 28, 51, 20, 36, 13, -1 },  // Idle
                { 32, -1, 7, -1, 29, 52, -1, 37, 14, -1 },  // Switch
                { 40, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // FullChargeMissile
                { 41, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // ChargingMissile
                { 42, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileClose
                { 43, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileOpen
                { 44, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // Unknown9
                { 45, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileShot
                { 46, -1, 8, -1, 30, 53, 22, 38, 15, -1 },  // UpDown
                { 47, -1, 9, -1, 31, 54, 23, 39, 16, -1 }   // Shot
            },
            // Spire
            {
                //   PB  VD Ms  Bh  Im  Jd  Mg  SC  OC
                { 0, -1, 3, -1, 25, 48, 17, 33, 10, -1 },   // FullCharge
                { 1, -1, 4, -1, 26, 49, 18, 34, 11, -1 },   // ChargeShot
                { 2, -1, 5, -1, 27, 50, 19, 35, 12, -1 },   // Charging
                { 24, -1, 6, -1, 28, 51, 20, 36, 13, -1 },  // Idle
                { 32, -1, 7, -1, 29, 52, 21, 37, 14, -1 },  // Switch
                { 40, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // FullChargeMissile
                { 41, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // ChargingMissile
                { 42, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileClose
                { 43, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileOpen
                { 44, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // Unknown9
                { 45, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileShot
                { 46, -1, 8, -1, 30, 53, 22, 38, 15, -1 },  // UpDown
                { 47, -1, 9, -1, 31, 54, 23, 39, 16, -1 }   // Shot
            },
            // Weavel
            {
                //   PB  VD Ms  Bh  Im  Jd  Mg  SC  OC
                { 0, -1, 3, -1, 25, 48, 17, 33, 10, -1 },   // FullCharge
                { 1, -1, 4, -1, 26, 49, 18, 34, 11, -1 },   // ChargeShot
                { 2, -1, 5, -1, 27, 50, 19, 35, 12, -1 },   // Charging
                { 24, -1, 6, -1, 28, 51, 20, 36, 13, -1 },  // Idle
                { 32, -1, 7, -1, 29, 52, 21, 37, 14, -1 },  // Switch
                { 40, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // FullChargeMissile
                { 41, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // ChargingMissile
                { 42, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileClose
                { 43, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileOpen
                { 44, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // Unknown9
                { 45, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileShot
                { 46, -1, 8, -1, 30, 53, 22, 38, 15, -1 },  // UpDown
                { 47, -1, 9, -1, 31, 54, 23, 39, 16, -1 }   // Shot
            },
            // Guardian
            {
                //   PB  VD Ms  Bh  Im  Jd  Mg  SC  OC
                { 0, -1, 3, -1, -1, -1, 17, -1, -1, -1 },   // FullCharge
                { 1, -1, 4, -1, -1, -1, 18, -1, -1, -1 },   // ChargeShot
                { 2, -1, 5, -1, -1, -1, 19, -1, -1, -1 },   // Charging
                { 24, -1, 6, -1, -1, -1, 20, -1, -1, -1 },  // Idle
                { 32, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // Switch
                { 40, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // FullChargeMissile
                { 41, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // ChargingMissile
                { 42, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileClose
                { 43, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileOpen
                { 44, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // Unknown9
                { 45, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // MissileShot
                { 46, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // UpDown
                { 47, -1, 9, -1, -1, -1, 23, -1, -1, -1 }   // Shot
            }
        };

} // namespace fruityprime::metadata::player

// Metadata is a partial class in the managed tree, so these tables are
// exported at the Metadata namespace level as well as in the native
// player-specific grouping above.  The references keep one storage instance
// and preserve the managed member spellings (including MuzzleOffests).
namespace fruityprime::metadata {

inline constexpr auto& SpireAltVectors = player::SpireAltVectors;
inline constexpr auto& SlipSpeedFactors = player::SlipSpeedFactors;
inline constexpr auto& TractionFactors = player::TractionFactors;
inline constexpr auto& MuzzleOffests = player::MuzzleOffests;
inline constexpr auto& MuzzleEffectIds = player::MuzzleEffectIds;
inline constexpr auto& ChargeEffectIds = player::ChargeEffectIds;
inline constexpr auto& ChargeLoopEffectIds = player::ChargeLoopEffectIds;
inline constexpr auto& GunAnimationIds = player::GunAnimationIds;
inline constexpr auto& PlayerValues = PlayerValuesTable;

} // namespace fruityprime::metadata
