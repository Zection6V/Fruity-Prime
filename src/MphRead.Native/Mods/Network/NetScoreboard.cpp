#include "Mods/Network/net_scoreboard.hpp"

#include <algorithm>

namespace fruityprime::net {

void NetScoreboard::ForgetSlot(game::State& state, int slot) noexcept {
    if (slot < 0 || slot >= static_cast<int>(game::SlotCapacity)) {
        return;
    }
    const auto index = static_cast<std::size_t>(slot);
    state.points[index] = 0;
    state.kills[index] = 0;
    state.deaths[index] = 0;
    state.player_time[index] = 0.0F;
    state.suicides[index] = 0;
    state.friendly_kills[index] = 0;
    state.headshot_kills[index] = 0;
    state.damage_count[index] = 0;
    state.alt_damage_count[index] = 0;
    state.beam_damage_dealt[index] = 0;
    state.beam_damage_max[index] = 0;
    state.octolith_scores[index] = 0;
    state.octolith_drops[index] = 0;
    state.octolith_stops[index] = 0;
    state.nodes_captured[index] = 0;
    state.nodes_lost[index] = 0;
    state.kills_as_prime[index] = 0;
    state.primes_killed[index] = 0;
    std::fill(state.beam_kills[index].begin(), state.beam_kills[index].end(), 0);
}

} // namespace fruityprime::net
