#pragma once

// Native counterpart of the standalone tables in Metadata/Metadata.cs.  The
// values are transliterated from the managed source rather than retyped, so
// the two trees cannot drift.

#include "Formats/Types.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace fruityprime::metadata {

// Metadata.AdpcmTable
inline constexpr std::array<int, 89> AdpcmTable{{
            7, 8, 9, 10, 11, 12, 13, 14,
            16, 17, 19, 21, 23, 25, 28, 31,
            34, 37, 41, 45, 50, 55, 60, 66,
            73, 80, 88, 97, 107, 118, 130, 143,
            157, 173, 190, 209, 230, 253, 279, 307,
            337, 371, 408, 449, 494, 544, 598, 658,
            724, 796, 876, 963, 1060, 1166, 1282, 1411,
            1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
            3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
            7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
            15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
            32767
}};

// Metadata.BeamRadiusValues
inline constexpr std::array<float, 4> BeamRadiusValues{{
            0.15F, 0.25F, 0.5F, 0.75F
}};

// Metadata.BeamDrawEffects
inline constexpr std::array<int, 23> BeamDrawEffects{{
    0, 237, 137, 0, 211, 130, 0, 0, 0, 0, 134, 209,
    64, 0, 102, 94, 96, 0, 116, 138, 183, 238, 246
}};

// Metadata.DamageLevels
inline constexpr std::array<float, 3> DamageLevels{{
            0.75F, 1, 1.25F
}};

// Metadata.MusicSeqs
inline constexpr std::array<std::string_view, 60> MusicSeqs{{
            "SEQ_BRINSTAR",
            "SEQ_MP1",
            "SEQ_MP2",
            "SEQ_PARASITE",
            "SEQ_SHIP",
            "SEQ_YELLOW",
            "SEQ_RESULTS",
            "SEQ_TIMEOUT",
            "SEQ_WIN",
            "SEQ_GARLIC",
            "SEQ_MP2_X",
            "SEQ_PARASITE_X",
            "SEQ_RED",
            "SEQ_BLUE",
            "SEQ_AMBIENT_1",
            "SEQ_TELEPORT",
            "SEQ_DRONE",
            "SEQ_MENU1",
            "SEQ_GREY",
            "SEQ_SAFFRON",
            "SEQ_GUMBO",
            "SEQ_INTRO_SYLUX",
            "SEQ_INTRO_TRACE",
            "SEQ_INTRO_NOXUS",
            "SEQ_INTRO_WEAVEL",
            "SEQ_INTRO_KANDEN",
            "SEQ_INTRO_SPIRE",
            "SEQ_FLY_IN_2",
            "SEQ_FLY_IN_1",
            "SEQ_FLY_IN_3",
            "SEQ_FLY_IN_4",
            "SEQ_SHIP_LAND1",
            "SEQ_SHIP_LAND2",
            "SEQ_SHIP_LAND3",
            "SEQ_SHIP_LAND4",
            "SEQ_GET_WEAPON",
            "SEQ_GET_OCTOLITH",
            "SEQ_NEW_GAME",
            "SEQ_BEAT_HUNTER1",
            "SEQ_INTRO_GUARDIAN",
            "SEQ_GUARDIAN",
            "SEQ_BEAT_CYLBOSS1",
            "SEQ_GREEN",
            "SEQ_CHUTNEY",
            "SEQ_DILL",
            "SEQ_GOREA_1",
            "SEQ_ENEMY_1",
            "SEQ_GOREA_2",
            "SEQ_PEPPER",
            "SEQ_SINGLE_CART_MENU",
            "SEQ_SINGLE_CART_INGAME",
            "SEQ_SINGLE_CART_TIMEOUT",
            "SEQ_OREGANO",
            "SEQ_ENEMY_2",
            "SEQ_WHITE",
            "SEQ_ENERGY_TIMER",
            "SEQ_BLACK",
            "SEQ_INDIGO",
            "SEQ_CREDITS",
            "SEQ_FLY_IN_GOREA"
}};

// Metadata.ObjectVisPosOffsets
inline constexpr std::array<formats::Vector3, 54> ObjectVisPosOffsets{{
            {},
            {},
            {},
            {},
            {},
            {},
            {},
            {},
            {},
            {},
            {},
            {},
            {-0.05F, 1.5F, -0.4F},
            {-0.05F, 1.5F, -0.4F},
            {},
            {},
            {},
            {-0.05F, 1.5F, 0},
            {0.01F, 1.5F, 0},
            {},
            {},
            {},
            {-0.03F, 1.5F, -0.3F},
            {0, 1.5F, -0.3F},
            {},
            {},
            {},
            {0, 1.5F, -0.2F},
            {0, 1.5F, -0.2F},
            {},
            {},
            {},
            {0, 1.4F, 0},
            {0, 1.4F, 0},
            {},
            {},
            {},
            {},
            {},
            {},
            {},
            {},
            {},
            {},
            {},
            {0, 1.75F, 0},
            {},
            {},
            {},
            {},
            {},
            {},
            {},
            {}
}};

// Metadata.SyluxBombEffects
inline constexpr std::array<int, 6> SyluxBombEffects{{
            113, 152, 151, 153, 150, 149
}};

// Metadata.TeamColors
inline constexpr std::array<formats::ColorRgb, 2> TeamColors{{
            {31, 19, 0}, 
            {0, 31, 0}
}};

// Metadata.WeaponMessageIds
inline constexpr std::array<int, 11> WeaponMessageIds{{
            0, 109, 0, 110, 111, 112, 113, 114, 115, 0, 0
}};

// Metadata.WeaponNames
inline constexpr std::array<std::string_view, 11> WeaponNames{{
            "Power Beam", "Volt Driver", "Missiles", "Battlehammer", "Imperialist",
            "Judicator", "Magmaul", "Shock Coil", "Omega Cannon", "Platform", "Enemy"
}};

// Metadata.WeaponNamesUpper
inline constexpr std::array<std::string_view, 11> WeaponNamesUpper{{
            "POWER BEAM", "VOLT DRIVER", "MISSILES", "BATTLEHAMMER", "IMPERIALIST",
            "JUDICATOR", "MAGMAUL", "SHOCK COIL", "OMEGA CANNON", "PLATFORM", "ENEMY"
}};

inline constexpr formats::Vector3 OctolithLight1Vector{0, 0.3005371F, -0.5F};
inline constexpr formats::Vector3 OctolithLight2Vector{0, 0, -0.5F};
inline constexpr formats::Vector3 OctolithLightColor{1, 1, 1};
inline constexpr formats::Vector4 RedPalette{189 / 255.0F, 66 / 255.0F, 0.0F, 1.0F};
inline constexpr formats::Vector4 WhitePalette{1.0F, 1.0F, 1.0F, 1.0F};
inline constexpr formats::Vector3 EmissionOrange{0.5161290322580645F, 0.22580645161290322F, 0.16129032258064516F};
inline constexpr formats::Vector3 EmissionGreen{0.16129032258064516F, 0.3548387096774194F, 0.16129032258064516F};
inline constexpr formats::Vector3 EmissionGray{0.41935483870967744F, 0.41935483870967744F, 0.41935483870967744F};


} // namespace fruityprime::metadata
