// Multiplayer objective rules for the native Session.
//
// This is the native counterpart of the mode-specific GameState/Entity
// objective updates. Keeping it out of gameplay.cpp makes the fixed-step
// orchestrator independent from the Node/Defender/Prime-Hunter state machine.
#include "Entities/gameplay.hpp"
#include "gameplay_helpers.hpp"

#include <algorithm>

namespace fruityprime::gameplay {
using namespace detail;

void Session::initialize_objectives() {
    objectives_ = {};
    for (const auto& entity : room_.entities()) {
        if (entity.kind != scene::EntityKind::NodeDefense) {
            continue;
        }
        const auto* data = std::get_if<scene::NodeDefenseData>(
            &entity.typed_data);
        if (data == nullptr || data->volume.kind == scene::VolumeKind::Invalid) {
            continue;
        }
        objectives_.nodes.push_back(NodeObjectiveState{
            entity.entity_id, data->volume, NeutralObjectiveTeam,
            NeutralObjectiveTeam, 0xff, 0.0F, 0.0F, false
        });
    }
    for (const auto& entity : room_.entities()) {
        if (entity.kind != scene::EntityKind::OctolithFlag) {
            continue;
        }
        const auto* data = std::get_if<scene::OctolithFlagData>(
            &entity.typed_data);
        if (data == nullptr) {
            continue;
        }
        const auto base_position = to_volume_point(entity.position);
        objectives_.flags.push_back(FlagObjectiveState{
            entity.entity_id, data->team_id, config_.mode == 8
                || config_.mode == 9, base_position, base_position,
            0xff, true, 0.0F
        });
    }
}

void Session::update_flags() {
    const bool capture = config_.mode == 7;
    const bool bounty = config_.mode == 8 || config_.mode == 9;
    if (!capture && !bounty) {
        return;
    }

    const auto own_flag_at_base = [this](std::uint8_t team) {
        return std::all_of(
            objectives_.flags.begin(), objectives_.flags.end(),
            [team](const FlagObjectiveState& flag) {
                return flag.team_id != team || flag.at_base;
            });
    };
    for (auto& flag : objectives_.flags) {
        flag.bounty = bounty;
        auto carrier = std::find_if(
            players_.begin(), players_.end(),
            [slot = flag.carrier_slot](const net::PlayerState& player) {
                return slot != 0xff && player.slot_index == slot;
            });
        if (carrier != players_.end() && !objective_player(*carrier)) {
            flag.carrier_slot = 0xff;
            flag.at_base = false;
            flag.reset_timer = 0.0F;
            carrier = players_.end();
        }

        if (carrier != players_.end()) {
            flag.position = to_volume_point(carrier->position);
            flag.position.y += 1.05F;
            for (const auto& entity : room_.entities()) {
                if (entity.kind != scene::EntityKind::FlagBase) {
                    continue;
                }
                const auto* base = std::get_if<scene::FlagBaseData>(
                    &entity.typed_data);
                if (base == nullptr
                    || !base->volume.contains(to_volume_point(
                        carrier->position))) {
                    continue;
                }
                const auto base_team = static_cast<std::uint8_t>(
                    base->team_id & 0xffu);
                if (capture && (carrier->team != base_team
                                || !own_flag_at_base(carrier->team))) {
                    continue;
                }
                increment_points(carrier->points);
                flag.carrier_slot = 0xff;
                flag.at_base = true;
                flag.position = flag.base_position;
                flag.reset_timer = 0.0F;
                break;
            }
            continue;
        }

        bool touched = false;
        for (const auto& player : players_) {
            if (!objective_player(player)) {
                continue;
            }
            const float pickup_radius = 1.0F;
            if (distance_squared(player.position, to_net(flag.position))
                > pickup_radius * pickup_radius) {
                continue;
            }
            if (capture && player.team == flag.team_id) {
                if (!flag.at_base) {
                    flag.at_base = true;
                    flag.position = flag.base_position;
                    flag.reset_timer = 0.0F;
                }
                touched = true;
                break;
            }
            flag.carrier_slot = player.slot_index;
            flag.at_base = false;
            flag.reset_timer = 0.0F;
            touched = true;
            break;
        }
        if (!touched && !flag.at_base) {
            flag.reset_timer += config_.tick_seconds;
            if (flag.reset_timer >= 20.0F) {
                flag.carrier_slot = 0xff;
                flag.at_base = true;
                flag.position = flag.base_position;
                flag.reset_timer = 0.0F;
            }
        }
    }
}

void Session::update_objectives() {
    if (!config_.objective_authority) {
        return;
    }

    update_flags();

    // GameState.ModeStateSurvival keeps a per-player survival duration and
    // the best duration for each team. It is not a scoring packet field, so
    // retain it alongside the other objective timers for the result/HUD
    // boundary. Eliminated players stop contributing, while a dead player
    // with spare lives remains eligible exactly like the managed condition.
    if (config_.mode == 5 || config_.mode == 6) {
        for (const auto& player : players_) {
            if ((player.flags & net::PlayerState::FlagActive) == 0
                || (player.flags & net::PlayerState::FlagSpectating) != 0
                || (player.health == 0
                    && player.deaths > config_.survival_lives)) {
                continue;
            }
            const std::size_t slot = player.slot_index;
            if (slot >= objectives_.player_time.size()) {
                continue;
            }
            objectives_.player_time[slot] += config_.tick_seconds;
            const std::size_t team = std::min<std::size_t>(
                player.team, objectives_.team_time.size() - 1);
            objectives_.team_time[team] = std::max(
                objectives_.team_time[team], objectives_.player_time[slot]);
        }
    }

    if (config_.mode == 14 && objectives_.prime_hunter >= 0) {
        auto found = std::find_if(
            players_.begin(), players_.end(),
            [slot = static_cast<std::uint8_t>(objectives_.prime_hunter)](
                const net::PlayerState& player) {
                return player.slot_index == slot;
            });
        if (found == players_.end() || !objective_player(*found)) {
            objectives_.prime_hunter = -1;
        } else {
            objectives_.player_time[static_cast<std::size_t>(
                objectives_.prime_hunter)] += config_.tick_seconds;
            // GameState.ModeStatePrimeHunter applies one point of unavoidable
            // damage every twenty scene frames. Reuse the area-damage path so
            // death counters, survival elimination, sounds, and respawn
            // scheduling stay identical to other native damage sources.
            if ((tick_count_ % 20u) == 0u) {
                const auto runtime = std::find_if(
                    inputs_.begin(), inputs_.end(),
                    [&found](const RuntimeInput& value) {
                        return value.slot == found->slot_index;
                    });
                if (runtime != inputs_.end()) {
                    scene::AreaVolumeData prime_damage;
                    prime_damage.inside_message = MessageDamage;
                    prime_damage.inside_parameter1 = 1;
                    apply_area_effect(prime_damage, *found, *runtime);
                }
            }
        }
    }

    const bool defender = config_.mode == 12 || config_.mode == 13;
    const bool nodes = config_.mode == 10 || config_.mode == 11;
    if (!defender && !nodes) {
        return;
    }

    constexpr float node_capture_seconds = 10.0F;
    constexpr float node_score_seconds = 5.0F;
    for (auto& node : objectives_.nodes) {
        std::uint8_t occupying_team = NeutralObjectiveTeam;
        std::uint8_t occupant_slot = 0xff;
        bool contested = false;
        for (const auto& player : players_) {
            if (!objective_player(player)
                || !node.volume.contains(to_volume_point(player.position))) {
                continue;
            }
            if (occupying_team == NeutralObjectiveTeam) {
                occupying_team = player.team;
                occupant_slot = player.slot_index;
            } else if (occupying_team != player.team) {
                contested = true;
            } else {
                // Match the managed entity's last-occupant bookkeeping. It
                // determines who receives a node point after capture.
                occupant_slot = player.slot_index;
            }
        }

        if (defender) {
            node.contested = contested;
            node.occupying_team = contested
                ? NeutralObjectiveTeam : occupying_team;
            node.progress = 0.0F;
            if (!contested && occupying_team != NeutralObjectiveTeam) {
                node.current_team = occupying_team;
                if (occupying_team < objectives_.team_time.size()) {
                    objectives_.team_time[occupying_team]
                        += config_.tick_seconds;
                }
            } else {
                node.current_team = NeutralObjectiveTeam;
            }
            continue;
        }

        const bool was_occupied = occupying_team != NeutralObjectiveTeam;
        bool captured = false;
        node.contested = contested;
        if (contested) {
            node.occupying_team = occupying_team;
        } else if (was_occupied) {
            node.occupying_team = occupying_team;
            if (node.current_team != occupying_team) {
                node.progress += config_.tick_seconds;
                if (node.progress >= node_capture_seconds) {
                    node.current_team = occupying_team;
                    node.captured_by_slot = occupant_slot;
                    node.occupying_team = NeutralObjectiveTeam;
                    node.progress = 0.0F;
                    node.score_timer = node_score_seconds;
                    captured = true;
                }
            }
        } else {
            node.occupying_team = NeutralObjectiveTeam;
            node.progress = 0.0F;
        }

        // C# NodeDefenseEntity awards a point while a captured node is
        // unoccupied. A completed capture enters that path immediately, so
        // preserve the same five-second initial score timer.
        if ((!was_occupied || captured) && !contested
            && node.current_team != NeutralObjectiveTeam) {
            node.score_timer += config_.tick_seconds;
            if (node.score_timer >= node_score_seconds
                && node.captured_by_slot != 0xff) {
                const auto scorer = std::find_if(
                    players_.begin(), players_.end(),
                    [slot = node.captured_by_slot](
                        const net::PlayerState& player) {
                        return player.slot_index == slot
                            && objective_player(player);
                    });
                if (scorer != players_.end()) {
                    increment_points(scorer->points);
                }
                node.score_timer = 0.0F;
            }
        }
    }
}

} // namespace fruityprime::gameplay
