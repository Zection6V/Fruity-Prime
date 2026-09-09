#pragma once

// Native counterpart of the flat tables in Metadata/Enemies.cs,
// Metadata/FrontendMeta.cs and Metadata/SoundMeta.cs.  Transliterated from the
// managed source so the values cannot drift.

#include "Formats/Types.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace fruityprime::metadata {

// Metadata.EnemyAudioRangeIndices
inline constexpr std::array<int, 52> EnemyAudioRangeIndices{{
            8, 9, 10, 11, 11, 11, 11, 4, 4, 8,
            8, 12, 9, 4, 4, 4, 13, 18, 18, 15,
            15, 19, 15, 16, 22, 22, 22, 22, 22, 22,
            22, 22, 22, 34, 16, 20, 20, 9, 4, 4,
            24, 25, 25, 25, 25, 25, 30, 30, 4, 4,
            4, 4
}};

// Metadata.DamageMultipliers
inline constexpr std::array<float, 4> DamageMultipliers{{
 0, 0.5F, 1, 2
}};

// Metadata.SlenchEffectiveness
inline constexpr std::array<int, 4> SlenchEffectiveness{{
            0x2AAAA, 0x2AAAA, 0x155F5, 0x16566
}};

// Metadata.SlenchSynapseEffectiveness
inline constexpr std::array<int, 4> SlenchSynapseEffectiveness{{
            0x2AAAA, 0x800, 0x80, 0x2000
}};

// Metadata.GoreaEffectiveness
inline constexpr std::array<int, 6> GoreaEffectiveness{{
            5, 0x81, 0x401, 0x2001, 0x4001, 0x301
}};

// Metadata.EnemyEffectiveness
inline constexpr std::array<int, 52> EnemyEffectiveness{{
            /*  0 */ 0x2AAAA, 
            /*  1 */ 0x2AAAA, 
            /*  2 */ 0x2AAAA, 
            /*  3 */ 0x2AAAA, 
            /*  4 */ 0x2AAAA, 
            /*  5 */ 0x2AAAA, 
            /*  6 */ 0x2AAAA, 
            /*  7 */ 0x2AAAA, 
            /*  8 */ 0x2AAAA, 
            /*  9 */ 0x2AAAA, 
            /* 10 */ 0x2AAAA, 
            /* 11 */ 0x2AAAA, 
            /* 12 */ 0x2AAA8, 
            /* 13 */ 0x00000, 
            /* 14 */ 0x2AAAA, 
            /* 15 */ 0x2AAAA, 
            /* 16 */ 0x2AAAA, 
            /* 17 */ 0x2AAAA, 
            /* 18 */ 0x2AAAA, 
            /* 19 */ 0x2AAAA, 
            /* 20 */ 0x2AAAA, 
            /* 21 */ 0x2AAAA, 
            /* 22 */ 0x2AAAA, 
            /* 23 */ 0x2AAAA, 
            /* 24 */ 0x2AAAA, 
            /* 25 */ 0x2AAAA, 
            /* 26 */ 0x2AAAA, 
            /* 27 */ 0x2AAAA, 
            /* 28 */ 0x2AAAA, 
            /* 29 */ 0x2AAAA, 
            /* 30 */ 0x2AAAA, 
            /* 31 */ 0x2AAAA, 
            /* 32 */ 0x20000, 
            /* 33 */ 0x2AAAA, 
            /* 34 */ 0x2AAAA, 
            /* 35 */ 0x2AABA, 
            /* 36 */ 0x2AABA, 
            /* 37 */ 0x2AAAA, 
            /* 38 */ 0x2EAFA, 
            /* 39 */ 0x24D55, 
            /* 40 */ 0x2AAAA, 
            /* 41 */ 0x2AA99, 
            /* 42 */ 0x2AA99, 
            /* 43 */ 0x2AA99, 
            /* 44 */ 0x2AA99, 
            /* 45 */ 0x2AA99, 
            /* 46 */ 0x2AAAA, 
            /* 47 */ 0x2AAAA, 
            /* 48 */ 0x2AAAA, 
            /* 49 */ 0x2AAAA, 
            /* 50 */ 0x2AAAA, 
            /* 51 */ 0x2AAAA
}};

// Metadata.EnemyScanIds
inline constexpr std::array<int, 52> EnemyScanIds{{
            214, 210, 0, 224, 224, 224, 224, 0, 0, 0,
            215, 213, 211, 0, 0, 0, 212, 219, 219, 0,
            0, 226, 0, 216, 243, 0, 241, 0, 244, 242,
            467, 0, 466, 0, 0, 217, 217, 218, 221, 222,
            0, 227, 227, 227, 227, 227, 220, 245, 0, 0,
            0, 0
}};

// Metadata.EnemyDeathEffects
inline constexpr std::array<int, 52> EnemyDeathEffects{{
            193, 221, 219, 219, 219, 219, 219, 76, 76, 76,
            193, 108, 221, 76, 76, 76, 76, 6, 6, 76,
            77, 76, 76, 77, 76, 76, 76, 76, 76, 76,
            77, 76, 76, 76, 77, 77, 77, 220, 222, 0,
            6, 76, 76, 76, 76, 76, 223, 223, 76, 77,
            0, 220
}};

// Metadata.EnemyModelNames
inline constexpr std::array<std::string_view, 52> EnemyModelNames{{
            /*  0 */ "warwasp_lod0",
            /*  1 */ "zoomer",
            /*  2 */ "Temroid_lod0",
            /*  3 */ "Chomtroid",
            /*  4 */ "Chomtroid",
            /*  5 */ "Chomtroid",
            /*  6 */ "Chomtroid",
            /*  7 */ "",
            /*  8 */ "",
            /*  9 */ "",
            /* 10 */ "BarbedWarWasp",
            /* 11 */ "shriekbat",
            /* 12 */ "geemer",
            /* 13 */ "",
            /* 14 */ "",
            /* 15 */ "",
            /* 16 */ "blastcap",
            /* 17 */ "",
            /* 18 */ "Alimbic_Turret",
            /* 19 */ "CylinderBoss",
            /* 20 */ "CylinderBossEye",
            /* 21 */ "",
            /* 22 */ "",
            /* 23 */ "PsychoBit",
            /* 24 */ "Gorea1A_lod0",
            /* 25 */ "",
            /* 26 */ "",
            /* 27 */ "",
            /* 28 */ "Gorea1B_lod0",
            /* 29 */ "",
            /* 30 */ "PowerBomb",
            /* 31 */ "Gorea2_lod0",
            /* 32 */ "",
            /* 33 */ "goreaMeteor",
            /* 34 */ "PsychoBit",
            /* 35 */ "GuardBot2_lod0",
            /* 36 */ "GuardBot1",
            /* 37 */ "DripStank_lod0",
            /* 38 */ "AlimbicStatue_lod0",
            /* 39 */ "LavaDemon",
            /* 40 */ "",
            /* 41 */ "BigEyeBall",
            /* 42 */ "",
            /* 43 */ "BigEyeNest",
            /* 44 */ "",
            /* 45 */ "BigEyeTurret",
            /* 46 */ "SphinkTick_lod0",
            /* 47 */ "SphinkTick_lod0",
            /* 48 */ "",
            /* 49 */ "",
            /* 50 */ "",
            /* 51 */ ""
}};

// Metadata.Enemy24Colors: ambient/diffuse pairs per Gorea phase
inline constexpr formats::ColorRgb Enemy24Colors[6][4] = {
            {
                {31, 31, 31}, 
                {0, 31, 0}, 
                {9, 7, 0}, 
                {31, 31, 31} 
            },
            {
                {31, 31, 31},
                {31, 24, 0},
                {0, 5, 0},
                {31, 31, 31}
            },
            {
                {31, 31, 31},
                {31, 15, 0},
                {3, 2, 4},
                {31, 31, 31}
            },
            {
                {31, 31, 31},
                {20, 10, 31},
                {9, 3, 0},
                {31, 31, 31}
            },
            {
                {31, 31, 31},
                {31, 0, 0},
                {0, 2, 4},
                {31, 31, 31}
            },
            {
                {31, 31, 31},
                {0, 13, 31},
                {9, 0, 0},
                {31, 31, 31}
            }
};

// FrontendMeta.NavMapModelNames
inline constexpr std::array<std::string_view, 7> NavMapModelNames{{
            "unit1_1nav",
            "unit1_2nav",
            "unit2_1nav",
            "unit2_2nav",
            "unit3_1nav",
            "unit3_2nav",
            "unit4_1nav"
}};

// FrontendMeta.MovieDisplayInfo
inline constexpr std::array<std::string_view, 36> MovieDisplayInfo{{
            "Opening (01_top/01_bot) - 0",
            "Story Intro (02_top/02_bot) - 1",
            "Good Ending (03_top/03_bot) - 2",
            "Alinos Landing (04_top) - 3",
            "Alinos Takeoff (05_top) - 4",
            "Celestial Archives Landing (06_top) - 5",
            "Celestial Archives Takeoff (07_top) - 6",
            "Arcterra Landing (08_top) - 7",
            "Arcterra Takeoff (09_top) - 8",
            "VDO Landing (10_top) - 9",
            "VDO Takeoff (11_top) - 10",
            "Oubliette Unlock (12_top/12_bot) - 11",
            "Oubliette Landing (13_top) - 12",
            "Unused (14_top/14_bot) - 13",
            "Octolith Obtained(15_top/15_bot) - 14",
            "Cretaphid V1 Intro (16_top/16_bot) - 15",
            "Cretaphid V1 Defeat (17_top/17_bot) - 16",
            "Cretaphid V2 Intro (18_top/18_bot) - 17",
            "Cretaphid V2 Defeat (19_top/19_bot) - 18",
            "Cretaphid V3 Intro (20_top/20_bot) - 19",
            "Cretaphid V3 Defeat (21_top/21_bot) - 20",
            "Cretaphid V4 Intro (22_top/22_bot) - 21",
            "Cretaphid V4 Defeat (23_top/23_bot) - 22",
            "Slench 1 Intro (24_top/24_bot) - 23",
            "Slench 1 Defeat (25_top/25_bot) - 24",
            "Slench 2 Intro (26_top/26_bot) - 25",
            "Slench 2 Defeat (27_top/27_bot) - 26",
            "Slench 3 Intro (28_top/28_bot) - 27",
            "Slench 3 Defeat (29_top/29_bot) - 28",
            "Slench 4 Intro (30_top/30_bot) - 29",
            "Slench 4 Defeat (31_top/31_bot) - 30",
            "Gorea Intro (32_top/32_bot) - 31",
            "Bad Ending Part 1 (33_top/33_bot) - 32",
            "Gorea 2 Intro (34_top/34_bot) - 33",
            "Unused (35_top/35_bot) - 34",
            "Bad Ending Part 2 (36_top/36_bot) - 35"
}};

// SoundMeta.SequenceFiles
inline constexpr std::array<std::string_view, 60> SequenceFiles{{
            "0000 - SEQ_BRINSTAR.minincsf",
            "0001 - SEQ_MP1.minincsf",
            "0002 - SEQ_MP2.minincsf",
            "0003 - SEQ_PARASITE.minincsf",
            "0004 - SEQ_SHIP.minincsf",
            "0005 - SEQ_YELLOW.minincsf",
            "0006 - SEQ_RESULTS.minincsf",
            "0007 - SEQ_TIMEOUT.minincsf",
            "0008 - SEQ_WIN.minincsf",
            "0009 - SEQ_GARLIC.minincsf",
            "000A - SEQ_MP2_X.minincsf",
            "000B - SEQ_PARASITE_X.minincsf",
            "000C - SEQ_RED.minincsf",
            "000D - SEQ_BLUE.minincsf",
            "000E - SEQ_AMBIENT_1.minincsf",
            "000F - SEQ_TELEPORT.minincsf",
            "0010 - SEQ_DRONE.minincsf",
            "0011 - SEQ_MENU1.minincsf",
            "0012 - SEQ_GREY.minincsf",
            "0013 - SEQ_SAFFRON.minincsf",
            "0014 - SEQ_GUMBO.minincsf",
            "0015 - SEQ_INTRO_SYLUX.minincsf",
            "0016 - SEQ_INTRO_TRACE.minincsf",
            "0017 - SEQ_INTRO_NOXUS.minincsf",
            "0018 - SEQ_INTRO_WEAVEL.minincsf",
            "0019 - SEQ_INTRO_KANDEN.minincsf",
            "001A - SEQ_INTRO_SPIRE.minincsf",
            "001B - SEQ_FLY_IN_2.minincsf",
            "001C - SEQ_FLY_IN_1.minincsf",
            "001D - SEQ_FLY_IN_3.minincsf",
            "001E - SEQ_FLY_IN_4.minincsf",
            "001F - SEQ_SHIP_LAND1.minincsf",
            "0020 - SEQ_SHIP_LAND2.minincsf",
            "0021 - SEQ_SHIP_LAND3.minincsf",
            "0022 - SEQ_SHIP_LAND4.minincsf",
            "0023 - SEQ_GET_WEAPON.minincsf",
            "0024 - SEQ_GET_OCTOLITH.minincsf",
            "0025 - SEQ_NEW_GAME.minincsf",
            "0026 - SEQ_BEAT_HUNTER1.minincsf",
            "0027 - SEQ_INTRO_GUARDIAN.minincsf",
            "0028 - SEQ_GUARDIAN.minincsf",
            "0029 - SEQ_BEAT_CYLBOSS1.minincsf",
            "002A - SEQ_GREEN.minincsf",
            "002B - SEQ_CHUTNEY.minincsf",
            "002C - SEQ_DILL.minincsf",
            "002D - SEQ_GOREA_1.minincsf",
            "002E - SEQ_ENEMY_1.minincsf",
            "002F - SEQ_GOREA_2.minincsf",
            "0030 - SEQ_PEPPER.minincsf",
            "0031 - SEQ_SINGLE_CART_MENU.minincsf",
            "0032 - SEQ_SINGLE_CART_INGAME.minincsf",
            "0033 - SEQ_SINGLE_CART_TIMEOUT.minincsf",
            "0034 - SEQ_OREGANO.minincsf",
            "0035 - SEQ_ENEMY_2.minincsf",
            "0036 - SEQ_WHITE.minincsf",
            "0037 - SEQ_ENERGY_TIMER.minincsf",
            "0038 - SEQ_BLACK.minincsf",
            "0039 - SEQ_INDIGO.minincsf",
            "003A - SEQ_CREDITS.minincsf",
            "003B - SEQ_FLY_IN_GOREA.minincsf"
}};


} // namespace fruityprime::metadata
