#pragma once

#include "Entities/gameplay.hpp"

#include <cstddef>
#include <cstdint>

namespace fruityprime::net {

struct PlayerSetupResult {
    bool applied = false;
    std::uint8_t local_slot = 0xff;
    std::size_t active_players = 0;
    std::size_t remote_players = 0;
};

// The managed version turns Scene-created bot slots into network-driven
// players. Native Session has no AI flag on PlayerState: its inputs are
// explicit, so the equivalent operation is to reassert active network flags
// after roster synchronization and report the local/remote split.
[[nodiscard]] PlayerSetupResult apply_player_setup(
    gameplay::Session& session, int local_slot) noexcept;

class NetPlayerSetup final {
public:
    static void Reset() noexcept;
    [[nodiscard]] static PlayerSetupResult ApplyOnce(
        gameplay::Session& session, int local_slot,
        bool network_active = true) noexcept;

private:
    inline static bool applied_ = false;
};

} // namespace fruityprime::net

namespace MphReadNative::Mods::Network {
using NetPlayerSetup = ::fruityprime::net::NetPlayerSetup;
}
