#pragma once

#include "GameState.hpp"

namespace fruityprime::net {

namespace detail {

// The managed GameState is process-wide. Native keeps the state object
// explicit elsewhere, so the frontend binds that one object once and the
// public operation retains the C# one-argument contract.
void BindGameState(game::State& state) noexcept;

} // namespace detail

class NetScoreboard final {
public:
    static void ForgetSlot(int slot) noexcept;
};

} // namespace fruityprime::net
