#include "Mods/Launcher/Portable/match_start.hpp"

#include "GameState.hpp"
#include "Metadata/metadata.hpp"
#include "Mods/Network/net_protocol.hpp"

#include <algorithm>
#include <cmath>
#include <exception>

namespace fruityprime::launcher {

PreparedMatch MatchStart::prepare(
    const LaunchPlan& plan, MatchStartOptions options,
    std::string_view authoritative_room,
    std::optional<std::uint8_t> authoritative_mode) {
    PreparedMatch result;
    result.kind = plan.kind;

    if (plan.kind == LaunchKind::None) {
        result.error = "no launch kind was selected";
        return result;
    }
    if (plan.kind == LaunchKind::Demo) {
        result.error = "demo launch needs the replay boundary";
        return result;
    }
    if (plan.hunter == metadata::Hunter::Random
        || static_cast<std::uint8_t>(plan.hunter)
            >= metadata::PlayableHunterCount) {
        result.error = "launch hunter is not a playable hunter";
        return result;
    }
    if (options.local_slot >= net::NetConfig::SlotCapacity) {
        result.error = "local player slot is outside the session capacity";
        return result;
    }

    result.room_key = std::string(authoritative_room.empty()
                                      ? std::string_view(plan.room_key)
                                      : authoritative_room);
    if (result.room_key.empty() || result.room_key == "none") {
        result.error = "launch room is empty";
        return result;
    }

    // Story launch is local and is selected from a save slot.  Do not let a
    // stale/default multiplayer mode in a caller override that boundary.
    result.mode = plan.kind == LaunchKind::Adventure
        ? static_cast<std::uint8_t>(game::Mode::Story)
        : authoritative_mode.value_or(plan.mode);
    const auto mode_defaults = match::defaults_for_mode(result.mode);
    const float time_limit = options.time_limit_seconds.value_or(
        mode_defaults.time_limit_seconds);
    const std::uint16_t point_goal = options.point_goal.value_or(
        mode_defaults.point_goal);
    if (!std::isfinite(time_limit) || time_limit < 0.0F) {
        result.error = "match time limit must be finite and non-negative";
        return result;
    }

    const bool team_mode = options.team_play || match::is_team_mode(result.mode);
    result.rules.mode = result.mode;
    result.rules.time_limit_seconds = time_limit;
    result.rules.point_goal = point_goal;
    result.rules.team_mode = team_mode;
    result.rules.room_key = result.room_key;

    result.gameplay.mode = result.mode;
    result.gameplay.point_goal = point_goal;
    result.gameplay.team_mode = team_mode;
    result.gameplay.friendly_fire = options.friendly_fire;
    result.gameplay.survival_mode = result.mode == 5 || result.mode == 6;
    result.gameplay.survival_lives = point_goal;
    result.gameplay.objective_authority = options.objective_authority;

    result.players.push_back(PlayerSlot{
        options.local_slot, plan.hunter,
        static_cast<std::uint8_t>(options.local_slot & 1), 0, false
    });
    const int requested_bots = std::clamp(
        plan.kind == LaunchKind::Offline ? plan.bots : 0,
        0, static_cast<int>(net::NetConfig::SlotCapacity) - 1);
    const int bot_level = std::clamp(plan.bot_level, 0, 2);
    int added_bots = 0;
    for (std::uint8_t slot = 0;
         slot < net::NetConfig::SlotCapacity && added_bots < requested_bots;
         ++slot) {
        if (slot == options.local_slot) {
            continue;
        }
        ++added_bots;
        const auto hunter = static_cast<metadata::Hunter>(
            (static_cast<std::uint8_t>(plan.hunter) + added_bots)
            % metadata::PlayableHunterCount);
        result.players.push_back(PlayerSlot{
            slot, hunter,
            team_mode ? static_cast<std::uint8_t>(slot & 1)
                      : static_cast<std::uint8_t>(0),
            bot_level, true
        });
    }
    result.ok = true;
    return result;
}

bool MatchStart::populate(gameplay::Session& session,
                          const PreparedMatch& prepared,
                          std::vector<std::uint8_t>* bot_slots,
                          std::string* error) {
    if (!prepared.ok) {
        if (error != nullptr) {
            *error = prepared.error;
        }
        return false;
    }
    if (bot_slots != nullptr) {
        bot_slots->clear();
    }
    try {
        for (const auto& player : prepared.players) {
            static_cast<void>(session.add_player(
                player.slot, static_cast<std::uint8_t>(player.hunter)));
            if (player.bot && bot_slots != nullptr) {
                bot_slots->push_back(player.slot);
            }
        }
        return true;
    } catch (const std::exception& exception) {
        if (error != nullptr) {
            *error = exception.what();
        }
        return false;
    }
}

} // namespace fruityprime::launcher
