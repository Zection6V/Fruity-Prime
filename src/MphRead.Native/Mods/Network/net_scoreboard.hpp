#pragma once

#include "GameState.hpp"

namespace fruityprime::net {

// Clear the score fields owned by one network slot.  A slot can be reused by
// a different peer without rebuilding the room, so this is deliberately a
// state operation rather than a constructor-time default.
void forget_scoreboard_slot(game::State& state, int slot) noexcept;

class NetScoreboard final {
public:
    static void BindRuntime(game::State* state) noexcept;
    static void ForgetSlot(int slot) noexcept;
    static void ForgetSlot(game::State& state, int slot) noexcept;

private:
    inline static game::State* state_ = nullptr;
};

} // namespace fruityprime::net

namespace MphReadNative::Mods::Network {
using NetScoreboard = ::fruityprime::net::NetScoreboard;
}
