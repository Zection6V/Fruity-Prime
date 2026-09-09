#pragma once

// Native counterpart of Weapons.BotWeaponValues and Weapons.BotWeapons: the
// damage a bot's shots do, by difficulty index and weapon.  A bot does not
// use the player weapon table's numbers, which is why this is its own table.

#include <array>
#include <cstdint>

namespace fruityprime::metadata {

// Weapons.BotWeaponValues
struct BotWeaponValues {
    std::uint16_t UnchargedDamage = 0;
    std::uint16_t ChargedDamage = 0;
    std::uint16_t SplashDamage = 0;
    std::uint16_t ChargedSplashDamage = 0;
};

// Weapons.BotWeapons[difficulty][weapon]
inline constexpr std::array<std::array<BotWeaponValues, 8>, 5>
    BotWeapons{{
    // index 0
    {{{1, 5, 0, 0}, {4, 10, 0, 0}, {7, 13, 0, 0}, {7, 0, 7, 0}, {20, 0, 0, 0}, {5, 10, 0, 0}, {4, 10, 4, 10}, {2, 0, 0, 0}}},
    // index 1
    {{{0, 0, 0, 0}, {3, 6, 0, 0}, {0, 0, 0, 0}, {4, 0, 4, 0}, {10, 0, 0, 0}, {7, 10, 0, 0}, {4, 6, 4, 6}, {4, 0, 0, 0}}},
    // index 2
    {{{0, 0, 0, 0}, {3, 6, 0, 0}, {0, 0, 0, 0}, {5, 0, 5, 0}, {15, 0, 0, 0}, {7, 10, 0, 0}, {4, 6, 4, 6}, {4, 0, 0, 0}}},
    // index 3
    {{{0, 0, 0, 0}, {6, 12, 0, 0}, {0, 0, 0, 0}, {8, 0, 8, 0}, {20, 0, 0, 0}, {10, 15, 0, 0}, {8, 15, 8, 15}, {7, 0, 0, 0}}},
    // index 4
    {{{0, 0, 0, 0}, {8, 18, 0, 0}, {0, 0, 0, 0}, {10, 0, 10, 0}, {25, 0, 0, 0}, {12, 18, 0, 0}, {12, 18, 12, 18}, {10, 0, 0, 0}}}
}};

} // namespace fruityprime::metadata
