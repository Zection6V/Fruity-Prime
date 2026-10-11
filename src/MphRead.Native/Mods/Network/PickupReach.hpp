#pragma once

#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

namespace MphRead::Mods::Network
{
    // Whether a player whose position is `player` could have touched an item
    // at `item`. The game's own test (PlayerEntity::PickUpItems) reaches about
    // a unit across and a little over one up; this allows twice that, for
    // the alt form's sphere and for where two machines put the same player.
    [[nodiscard]] inline bool WithinPickupReach(OpenTK::Mathematics::Vector3 player,
        OpenTK::Mathematics::Vector3 item) noexcept
    {
        constexpr float Lateral = 2.0F;
        constexpr float Below = -2.0F;
        constexpr float Above = 2.5F;
        const float dx = item.X - player.X;
        const float dz = item.Z - player.Z;
        const float dy = item.Y - player.Y;
        return dx * dx + dz * dz <= Lateral * Lateral && dy >= Below && dy <= Above;
    }
}
