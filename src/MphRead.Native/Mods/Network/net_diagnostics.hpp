#pragma once

#include "Mods/Network/net_protocol.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace fruityprime::game {
struct State;
}

namespace fruityprime::gameplay {
class Session;
}

namespace fruityprime::net {

// Renderer-independent counterpart of MphRead/Mods/Network/NetDiagnostics.cs.
// The managed diagnostic reads private PlayerEntity fields directly. Native
// callers provide the equivalent Session and wire-state views explicitly so
// the report cannot inspect a second, stale network aggregate.
class NetDiagnostics final {
public:
    struct Context {
        const game::State* game_state = nullptr;
        const gameplay::Session* session = nullptr;
        const MatchStatePacket* server_match = nullptr;
        std::string_view role;
        bool active = false;
        int local_slot = -1;
        std::array<bool, NetConfig::SlotCapacity> occupied{};
        std::array<bool, NetConfig::SlotCapacity> state_valid{};
        std::array<bool, NetConfig::SlotCapacity> intent_valid{};
    };

    NetDiagnostics() noexcept;

    // ModEntry.cs sets Network.NetDiagnostics.Enabled after the process-wide
    // diagnostics object may already have been constructed by a platform
    // host.  Keep that managed static state separate from each host instance.
    static void set_process_enabled(bool enabled) noexcept;
    [[nodiscard]] static bool process_enabled() noexcept;

    [[nodiscard]] bool enabled() const noexcept;
    void set_enabled(bool enabled) noexcept;
    void reset() noexcept;
    void report(double time, const Context& context) noexcept;

private:
    static bool read_enabled() noexcept;

    double last_report_ = 0.0;
    bool enabled_ = false;
    bool checked_ = false;
};

} // namespace fruityprime::net
