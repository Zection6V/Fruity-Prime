#pragma once

#include <cstdint>

namespace MphRead::Mods::Network
{
    enum class FreezeStep : std::uint8_t
    {
        Keep,
        // Frozen for what the authority has left, the ice drawn as long.
        Mirror,
        // The authority's has thawed.
        End
    };

    // How a client takes the authority's word about one player's freeze. The
    // authority's frames are copied, not restarted by a local rule -- which
    // gave a second freeze inside two seconds a quarter of the time, and an
    // ice layer that ran out while the player stayed frozen. A freeze this
    // machine's own hit just put on a copy is `awaiting` the authority's,
    // which a snapshot that has not heard of the hit must not thaw.
    [[nodiscard]] constexpr FreezeStep ReconcileFreeze(std::uint8_t reported, std::uint16_t local, bool awaiting) noexcept
    {
        if (reported > 0)
        {
            return FreezeStep::Mirror;
        }
        return local > 1 && !awaiting ? FreezeStep::End : FreezeStep::Keep;
    }
}
