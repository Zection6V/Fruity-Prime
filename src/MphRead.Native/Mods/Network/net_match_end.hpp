#pragma once

#include "GameState.hpp"
#include "Mods/Network/net_protocol.hpp"

#include <cstdint>

namespace fruityprime::net {

struct MatchEndResult {
    bool should_report = false;
    bool recovered = false;
};

// Native counterpart of MphRead/Mods/Network/NetMatchEnd.cs.  It deliberately
// does not own a socket: the caller decides how to send the one-byte-free
// MatchEnd packet, while this class retains the retry/acknowledgement policy.
class MatchEnd final {
public:
    static constexpr std::uint32_t ReportIntervalFrames = 15;
    static constexpr std::uint32_t StrandedFrames = 60 * 12;

    void reset() noexcept;

    [[nodiscard]] MatchEndResult sync(
        std::uint32_t frame, bool network_active, bool may_end,
        game::State& local, const MatchStatePacket* server) noexcept;

    [[nodiscard]] static bool may_end_on_score(
        bool network_active, bool authority_or_host) noexcept {
        return !network_active || authority_or_host;
    }

    [[nodiscard]] static bool in_intermission(
        bool network_active, const game::State& local,
        const MatchStatePacket* server) noexcept;

    [[nodiscard]] bool acknowledged() const noexcept { return acknowledged_; }
    [[nodiscard]] bool reported() const noexcept { return reported_; }

private:
    void recover_if_stranded(std::uint32_t frame, game::State& local,
                             const MatchStatePacket* server,
                             bool server_ending,
                             MatchEndResult& result) noexcept;

    std::uint32_t last_report_frame_ = 0;
    std::uint32_t stranded_since_ = 0;
    bool reported_ = false;
    bool acknowledged_ = false;
};

} // namespace fruityprime::net
