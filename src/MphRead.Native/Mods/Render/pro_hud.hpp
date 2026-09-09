#pragma once

#include "GameState.hpp"
#include "Entities/gameplay.hpp"

#include <cstdint>
#include <string>

namespace MphReadNative::Hud {
struct PlayerState;
}

namespace fruityprime::mods::render::pro_hud {

// The managed Pro HUD uses one three-level vocabulary for both readouts.
// Keeping the state renderer-neutral makes the thresholds testable without a
// window and prevents a frontend from inventing different colours.
enum class Tone : std::uint8_t {
    Good,
    Warning,
    Danger
};

struct Color {
    float red = 1.0F;
    float green = 1.0F;
    float blue = 1.0F;
};

struct Frame {
    std::string health_text;
    float health_fraction = 0.0F;
    Tone health_tone = Tone::Good;

    bool ammo_visible = false;
    std::uint8_t weapon = 0;
    std::string ammo_text;
    float ammo_fraction = 0.0F;
    Tone ammo_tone = Tone::Good;

    int score_message_id = 212;
    std::string score_label;
    std::string score_text;
    float score_y = 12.0F;
};

// Build the values drawn by PlayerEntityProHud.DrawProHud.  `energy_tank` is
// the selected hunter's authored tank size; multiplayer measures the bar
// against EnergyTank - 1, while story mode measures it against the current
// inventory maximum.  The chat-adjusted score Y is calculated here, at the
// same ModChatClearance call site as the managed partial.
[[nodiscard]] Frame build(const MphReadNative::Hud::PlayerState& player,
                          const gameplay::InventoryState& inventory,
                          const game::State& state, std::uint8_t slot,
                          std::uint16_t energy_tank);

[[nodiscard]] Color tone_color(Tone tone) noexcept;

} // namespace fruityprime::mods::render::pro_hud
