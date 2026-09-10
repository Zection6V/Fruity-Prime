#pragma once

#include <chrono>
#include <string>
#include <string_view>

namespace fruityprime::net {

struct NetworkConditions;

// Value-side counterpart of NetLag.cs.  The managed type is process-wide and
// owns both the option grammar and the random source used by every transport.
class NetLag final {
public:
    // The managed type is a process-wide static class.  These accessors keep
    // the same boundary instead of making the command-line setting a
    // per-socket option.
    [[nodiscard]] static int round_trip_ms() noexcept;
    [[nodiscard]] static int jitter_ms() noexcept;
    [[nodiscard]] static double loss_percent() noexcept;
    [[nodiscard]] static bool active() noexcept;

    [[nodiscard]] static bool configure(std::string_view value) noexcept;
    [[nodiscard]] static bool configure_loss(std::string_view value) noexcept;
    [[nodiscard]] static std::string describe();

    // Counterparts of the two internal helpers in NetLag.cs.  They use one
    // process-wide generator, just as the managed static Random does; the
    // game's replicated RNG is deliberately not involved.
    [[nodiscard]] static std::chrono::steady_clock::duration hold_duration();
    [[nodiscard]] static bool drops();

    [[nodiscard]] static NetworkConditions current() noexcept;

    // Kept for callers that parse an explicit value pair in an isolated
    // native test.  The product path uses configure/configure_loss above.
    [[nodiscard]] static NetworkConditions from_options(
        std::string_view netlag, std::string_view netloss);
    [[nodiscard]] static std::string describe(
        const NetworkConditions& conditions);
};

} // namespace fruityprime::net
