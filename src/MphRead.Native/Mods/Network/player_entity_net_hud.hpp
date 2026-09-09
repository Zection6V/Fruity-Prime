#pragma once

#include "GameState.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace fruityprime::net::hud {

struct ScoreColumns {
    float first = 160.0F;
    float second = 215.0F;
    float ping = 236.0F;
};

enum class PingTone : std::uint8_t {
    Unknown,
    Good,
    Warning,
    Bad,
};

struct PingLabel {
    std::string text;
    PingTone tone = PingTone::Unknown;
};

// These helpers are the renderer-neutral part of PlayerEntityNetHud.cs.
// Coordinates remain in the managed HUD's 256-wide design space; the Win32
// frontend scales them when it draws its scoreboard.
[[nodiscard]] ScoreColumns score_columns(bool networked) noexcept;
[[nodiscard]] PingLabel ping_label(int ping);
[[nodiscard]] std::array<float, 3> ping_rgb(PingTone tone) noexcept;

[[nodiscard]] std::string score_header_one(game::Mode mode);
[[nodiscard]] std::string score_header_two(game::Mode mode);
[[nodiscard]] std::string score_value_one(const game::State& state,
                                          game::Mode mode,
                                          std::size_t slot);
[[nodiscard]] std::string score_value_two(const game::State& state,
                                          game::Mode mode,
                                          std::size_t slot);

} // namespace fruityprime::net::hud
