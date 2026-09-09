#include "Mods/Render/pro_hud.hpp"

#include "HUD/hud.hpp"
#include "Metadata/metadata.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace fruityprime::mods::render::pro_hud {
namespace {

constexpr float HealthWarning = 60.0F / 99.0F;
constexpr float HealthDanger = 33.0F / 99.0F;

[[nodiscard]] Tone health_tone(const float fraction) noexcept {
    // These strict comparisons intentionally mirror GetCrosshairColor and
    // ProHealthColor: exactly 60/33 is already the next lower state.
    if (fraction > HealthWarning) {
        return Tone::Good;
    }
    if (fraction > HealthDanger) {
        return Tone::Warning;
    }
    return Tone::Danger;
}

[[nodiscard]] Tone ammo_tone(const int amount, const int full) noexcept {
    if (amount < 0 || amount >= full / 2) {
        return Tone::Good;
    }
    if (amount >= full / 5) {
        return Tone::Warning;
    }
    return Tone::Danger;
}

[[nodiscard]] bool point_mode(const game::Mode mode) noexcept {
    return mode == game::Mode::Battle || mode == game::Mode::BattleTeams
        || mode == game::Mode::Capture || mode == game::Mode::Nodes
        || mode == game::Mode::NodesTeams || mode == game::Mode::Bounty
        || mode == game::Mode::BountyTeams;
}

[[nodiscard]] std::size_t team_index(const game::State& state,
                                     const std::uint8_t slot) noexcept {
    if (slot >= state.player_teams.size()) {
        return 0;
    }
    const auto team = state.player_teams[slot];
    return std::min<std::size_t>(team, state.team_points.size() - 1);
}

[[nodiscard]] std::string format_time(const float seconds) {
    if (!std::isfinite(seconds)) {
        return "0:00";
    }
    const int total = std::max(0, static_cast<int>(std::floor(seconds)));
    const int minutes = total / 60;
    const int remainder = total % 60;
    std::ostringstream text;
    text << minutes << ':' << std::setfill('0') << std::setw(2) << remainder;
    return text.str();
}

[[nodiscard]] std::pair<int, const char*> score_message(
    const game::Mode mode) noexcept {
    switch (mode) {
    case game::Mode::Survival:
    case game::Mode::SurvivalTeams:
        return {213, "LIVES LEFT"};
    case game::Mode::PrimeHunter:
        return {214, "PRIME TIME"};
    case game::Mode::Bounty:
    case game::Mode::BountyTeams:
    case game::Mode::Capture:
        return {mode == game::Mode::Capture ? 216 : 215, "OCTOLITHS"};
    case game::Mode::Defender:
    case game::Mode::DefenderTeams:
        return {217, "RING TIME"};
    case game::Mode::Nodes:
    case game::Mode::NodesTeams:
        return {218, "POINTS"};
    default:
        return {212, "POINTS"};
    }
}

[[nodiscard]] std::string format_score(const game::State& state,
                                        const game::Mode mode,
                                        const std::uint8_t slot) {
    if (point_mode(mode)) {
        if (state.teams) {
            const std::size_t team = team_index(state, slot);
            return std::to_string(state.team_points[team]) + " / "
                + std::to_string(state.point_goal);
        }
        if (slot >= state.points.size()) {
            return "0 / " + std::to_string(state.point_goal);
        }
        return std::to_string(state.points[slot]) + " / "
            + std::to_string(state.point_goal);
    }
    if (mode == game::Mode::Survival || mode == game::Mode::SurvivalTeams) {
        const std::size_t team = team_index(state, slot);
        const int lives = std::max(
            static_cast<int>(state.point_goal)
                - state.team_deaths[team], 0);
        return std::to_string(lives);
    }
    if (mode == game::Mode::Defender || mode == game::Mode::DefenderTeams
        || mode == game::Mode::PrimeHunter) {
        const float time = slot < state.player_time.size()
            ? state.player_time[slot] : 0.0F;
        return format_time(time) + "/" + format_time(state.time_goal);
    }
    return " ";
}

} // namespace

Frame build(const MphReadNative::Hud::PlayerState& player,
            const gameplay::InventoryState& inventory,
            const game::State& state, const std::uint8_t slot,
            const std::uint16_t energy_tank, const float score_y) {
    Frame result;
    result.health_text = std::to_string(player.health);
    const bool multiplayer = state.mode != game::Mode::SinglePlayer;
    const int health_span = multiplayer
        ? std::max(static_cast<int>(energy_tank) - 1,
                   static_cast<int>(1))
        : std::max(static_cast<int>(inventory.health_max), 1);
    result.health_fraction = std::clamp(
        static_cast<float>(player.health) / static_cast<float>(health_span),
        0.0F, 1.0F);
    result.health_tone = health_tone(result.health_fraction);

    result.weapon = player.current_weapon;
    const auto& weapon = metadata::weapon_info(player.current_weapon);
    const std::size_t ammo_type = std::min<std::size_t>(
        weapon.ammo_type, inventory.ammo.size() - 1);
    const int amount = static_cast<int>(inventory.ammo[ammo_type]);
    result.ammo_visible = !player.alt_form && weapon.ammo_cost > 0;
    if (result.ammo_visible) {
        result.ammo_text = inventory.infinite_ammo
            ? "--"
            : std::to_string(amount / static_cast<int>(weapon.ammo_cost));
        const int full = multiplayer ? 100 : 250;
        result.ammo_fraction = inventory.infinite_ammo
            ? 1.0F
            : std::clamp(static_cast<float>(amount)
                             / static_cast<float>(full),
                         0.0F, 1.0F);
        result.ammo_tone = inventory.infinite_ammo
            ? Tone::Good : ammo_tone(amount, full);
    }

    const auto [message_id, label] = score_message(state.mode);
    result.score_message_id = message_id;
    result.score_label = label;
    result.score_text = format_score(state, state.mode, slot);
    result.score_y = score_y;
    return result;
}

Color tone_color(const Tone tone) noexcept {
    switch (tone) {
    case Tone::Good:
        return {0.24F, 0.85F, 0.32F};
    case Tone::Warning:
        return {1.0F, 0.68F, 0.10F};
    case Tone::Danger:
    default:
        return {0.95F, 0.18F, 0.18F};
    }
}

} // namespace fruityprime::mods::render::pro_hud
