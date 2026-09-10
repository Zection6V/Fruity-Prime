#pragma once

#include "GameState.hpp"

namespace fruityprime::net {

class NetScoreboard final {
public:
    // C# NetScoreboard.ForgetSlot clears the GameState arrays for the
    // specified slot. The native state is explicit because there is no
    // managed static-field runtime to bind behind this call.
    static void ForgetSlot(game::State& state, int slot) noexcept;
};

} // namespace fruityprime::net

namespace MphReadNative::Mods::Network {
using NetScoreboard = ::fruityprime::net::NetScoreboard;
}
