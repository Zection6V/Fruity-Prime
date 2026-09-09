#include "Mods/Network/net_diagnostics.hpp"

#include "GameState.hpp"
#include "Entities/gameplay.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>

namespace fruityprime::net {

NetDiagnostics::NetDiagnostics() noexcept
    : enabled_(read_enabled()), checked_(true) {}

bool NetDiagnostics::read_enabled() noexcept {
    return std::getenv("MPHREAD_NET_DEBUG") != nullptr;
}

bool NetDiagnostics::enabled() const noexcept {
    return checked_ ? enabled_ : read_enabled();
}

void NetDiagnostics::set_enabled(bool enabled_value) noexcept {
    enabled_ = enabled_value;
    checked_ = true;
}

void NetDiagnostics::reset() noexcept {
    last_report_ = 0.0;
    enabled_ = read_enabled();
    checked_ = true;
}

void NetDiagnostics::report(double time, const Context& context) noexcept {
    if (!enabled() || !context.active || context.game_state == nullptr
        || time - last_report_ < 1.0) {
        return;
    }
    last_report_ = time;

    const auto& state = *context.game_state;
    std::ostringstream line;
    line << "[netdbg] role="
         << (context.role.empty() ? std::string_view{"offline"}
                                  : context.role)
         << " slot=" << context.local_slot << " slots=[";

    std::size_t active = 0;
    std::size_t created = 0;
    for (std::size_t slot = 0; slot < NetConfig::SlotCapacity; ++slot) {
        const bool has_player = context.session != nullptr
            && context.session->has_player(static_cast<std::uint8_t>(slot));
        if (!has_player) {
            line << '-';
            continue;
        }
        ++created;
        const auto& player = context.session->player(
            static_cast<std::uint8_t>(slot));
        const bool is_active = (player.flags & PlayerState::FlagActive) != 0;
        if (is_active) {
            ++active;
        }
        // Native Session has no separate PlayerAi-owned IsBot bit. An active
        // native player is therefore shown as A; reporting a fabricated B
        // would hide the exact remote-AI failure this diagnostic is meant to
        // find.
        line << (is_active ? 'A' : context.occupied[slot] ? 's' : '.');
    }
    line << "] active=" << active
         << " scoreboard=" << static_cast<unsigned>(state.active_players)
         << " created=" << created << " remoteState=[";
    for (const bool valid : context.state_valid) {
        line << (valid ? 'y' : 'n');
    }
    line << "] remoteIntent=[";
    for (const bool valid : context.intent_valid) {
        line << (valid ? 'y' : 'n');
    }
    line << ']';

    if (context.server_match != nullptr) {
        line << " serverMap=" << context.server_match->room_key
             << " serverPlayers="
             << static_cast<unsigned>(context.server_match->player_count);
    }
    std::cout << line.str() << '\n';
}

} // namespace fruityprime::net
