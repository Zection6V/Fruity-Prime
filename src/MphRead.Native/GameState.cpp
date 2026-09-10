#include "GameState.hpp"

#include "Formats/Types.hpp"
#include "Metadata/Weapons.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fruityprime::game {

State::State() noexcept {
    reset();
}

bool State::is_team_mode(Mode value) noexcept {
    // Keep this table in lockstep with GameState.IsTeamMode. Capture is a
    // team mode even though its enum name does not end in "Teams".
    switch (value) {
    case Mode::BattleTeams:
    case Mode::SurvivalTeams:
    case Mode::Capture:
    case Mode::BountyTeams:
    case Mode::NodesTeams:
    case Mode::DefenderTeams:
        return true;
    default:
        return false;
    }
}

bool State::is_octolith_mode() const noexcept {
    return mode == Mode::Capture || mode == Mode::Bounty
        || mode == Mode::BountyTeams;
}

StorySave::StorySave() noexcept {
    // The managed save starts with the two universal weapons and the basic
    // equipment entries already discovered. Categories are supplied by the
    // string-table reader when a ROM is loaded; these are the known default
    // equipment IDs from the original save initializer.
    for (const auto scan_id : {0, 1, 2, 3, 4, 5, 6, 26, 28}) {
        set_scan_category(scan_id, 'E');
        update_logbook(scan_id);
    }
}

std::int32_t StorySave::init_room_state(
    std::int32_t room_id, std::int32_t entity_id, bool active,
    std::int32_t active_state, std::int32_t inactive_state) noexcept {
    if (entity_id == -1 || room_id < 27) {
        return 0;
    }
    if (room_id > 92 || entity_id > 239 || entity_id < 0) {
        return 1;
    }
    const auto room_index = static_cast<std::size_t>(room_id - 27);
    const auto byte_index = static_cast<std::size_t>(entity_id / 4);
    const auto pair_shift = static_cast<unsigned>(entity_id % 4) * 2u;
    const auto pair_mask = static_cast<std::uint8_t>(3u << pair_shift);
    if ((room_state[room_index][byte_index] & pair_mask) == 0) {
        room_state[room_index][byte_index] = static_cast<std::uint8_t>(
            room_state[room_index][byte_index] & ~pair_mask);
        const auto value = static_cast<std::uint8_t>(
            (active ? active_state : inactive_state) & 3);
        room_state[room_index][byte_index] = static_cast<std::uint8_t>(
            room_state[room_index][byte_index] | (value << pair_shift));
    }
    return room_state_value(room_id, entity_id);
}

std::int32_t StorySave::room_state_value(
    std::int32_t room_id, std::int32_t entity_id) const noexcept {
    if (entity_id == -1 || room_id < 27 || room_id > 92) {
        return 0;
    }
    if (entity_id > 239 || entity_id < 0) {
        return 1;
    }
    const auto room_index = static_cast<std::size_t>(room_id - 27);
    const auto byte_index = static_cast<std::size_t>(entity_id / 4);
    const auto pair_shift = static_cast<unsigned>(entity_id % 4) * 2u;
    return static_cast<std::int32_t>(
               (room_state[room_index][byte_index] >> pair_shift) & 3u)
        - 1;
}

void StorySave::set_room_state(std::int32_t room_id, std::int32_t entity_id,
                               std::int32_t state) noexcept {
    if (entity_id == -1 || room_id < 27 || room_id > 92
        || entity_id > 239 || entity_id < 0) {
        return;
    }
    const auto room_index = static_cast<std::size_t>(room_id - 27);
    const auto byte_index = static_cast<std::size_t>(entity_id / 4);
    const auto pair_shift = static_cast<unsigned>(entity_id % 4) * 2u;
    const auto pair_mask = static_cast<std::uint8_t>(3u << pair_shift);
    room_state[room_index][byte_index] = static_cast<std::uint8_t>(
        room_state[room_index][byte_index] & ~pair_mask);
    room_state[room_index][byte_index] = static_cast<std::uint8_t>(
        room_state[room_index][byte_index]
        | (static_cast<std::uint8_t>(state & 3) << pair_shift));
}

bool StorySave::visited_room(std::int32_t room_id) const noexcept {
    if (room_id < 27 || room_id > 92) {
        return false;
    }
    const auto room_index = static_cast<std::size_t>(room_id - 27);
    return (visited_rooms[room_index / 8] & (1u << (room_index % 8))) != 0;
}

void StorySave::set_visited_room(std::int32_t room_id) noexcept {
    if (room_id < 27 || room_id > 92) {
        return;
    }
    const auto room_index = static_cast<std::size_t>(room_id - 27);
    visited_rooms[room_index / 8] = static_cast<std::uint8_t>(
        visited_rooms[room_index / 8] | (1u << (room_index % 8)));
}

bool StorySave::visited_connector(std::int32_t connector_id,
                                  std::int32_t area_id) const noexcept {
    if (connector_id < 0 || connector_id > 63 || area_id < 0
        || area_id >= static_cast<std::int32_t>(visited_connectors.size())) {
        return false;
    }
    const auto index = static_cast<std::size_t>((area_id & ~1)
        + (connector_id >= 32 ? 1 : 0));
    if (index >= visited_connectors.size()) {
        return false;
    }
    const auto bit = connector_id >= 32 ? connector_id - 32 : connector_id;
    return (static_cast<std::uint32_t>(visited_connectors[index])
            & (1u << bit)) != 0;
}

void StorySave::set_visited_connector(std::int32_t connector_id,
                                      std::int32_t area_id) noexcept {
    if (connector_id < 0 || connector_id > 63 || area_id < 0
        || area_id >= static_cast<std::int32_t>(visited_connectors.size())) {
        return;
    }
    const auto index = static_cast<std::size_t>((area_id & ~1)
        + (connector_id >= 32 ? 1 : 0));
    if (index >= visited_connectors.size()) {
        return;
    }
    const auto bit = connector_id >= 32 ? connector_id - 32 : connector_id;
    visited_connectors[index] = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(visited_connectors[index]) | (1u << bit));
}

bool StorySave::found_octolith(std::int32_t area_id) const noexcept {
    return area_id >= 0 && area_id < 16
        && (found_octoliths & (1u << area_id)) != 0;
}

std::int32_t StorySave::found_octolith_count() const noexcept {
    std::int32_t count = 0;
    for (std::int32_t index = 0; index < 8; ++index) {
        count += found_octolith(index) ? 1 : 0;
    }
    return count;
}

void StorySave::update_found_octolith(std::int32_t area_id) noexcept {
    if (area_id >= 0 && area_id < 16) {
        found_octoliths = static_cast<std::uint16_t>(
            found_octoliths | (1u << area_id));
        current_octoliths = static_cast<std::uint16_t>(
            current_octoliths | (1u << area_id));
    }
}

bool StorySave::found_artifact(std::int32_t artifact_id,
                               std::int32_t model_id) const noexcept {
    const auto bit = artifact_id + 3 * model_id;
    return bit >= 0 && bit < 32
        && (artifacts & (1u << bit)) != 0;
}

std::int32_t StorySave::found_artifact_count(
    std::int32_t model_id) const noexcept {
    std::int32_t count = 0;
    for (std::int32_t artifact = 0; artifact < 3; ++artifact) {
        count += found_artifact(artifact, model_id) ? 1 : 0;
    }
    return count;
}

void StorySave::update_found_artifact(std::int32_t artifact_id,
                                      std::int32_t model_id) noexcept {
    const auto bit = artifact_id + 3 * model_id;
    if (bit >= 0 && bit < 32) {
        artifacts |= 1u << bit;
    }
}

std::int32_t StorySave::enemy_octolith_drop(
    std::int32_t hunter) const noexcept {
    for (std::int32_t index = 0; index < 8; ++index) {
        if (((lost_octoliths >> (4 * index)) & 15u)
            == static_cast<std::uint32_t>(hunter)) {
            return index;
        }
    }
    return 8;
}

void StorySave::set_scan_category(std::int32_t scan_id,
                                  char category) noexcept {
    if (scan_id >= 0 && scan_id < static_cast<std::int32_t>(ScanCount)) {
        scan_categories[static_cast<std::size_t>(scan_id)] = category;
    }
}

void StorySave::update_logbook(std::int32_t scan_id) noexcept {
    if (scan_id < 0 || scan_id >= static_cast<std::int32_t>(ScanCount)) {
        return;
    }
    const auto index = static_cast<std::size_t>(scan_id / 8);
    const auto bit = static_cast<std::uint8_t>(1u << (scan_id % 8));
    if ((logbook[index] & bit) != 0) {
        return;
    }
    logbook[index] = static_cast<std::uint8_t>(logbook[index] | bit);
    if (scan_categories[static_cast<std::size_t>(scan_id)] == 'E') {
        ++equipment_count;
    } else {
        ++scan_count;
    }
}

bool StorySave::logbook_found(std::int32_t scan_id) const noexcept {
    if (scan_id < 0 || scan_id >= static_cast<std::int32_t>(ScanCount)) {
        return false;
    }
    return (logbook[static_cast<std::size_t>(scan_id / 8)]
            & (1u << (scan_id % 8))) != 0;
}

std::int32_t StorySave::logbook_count(
    bool unlocked_only, std::span<const char> categories) const noexcept {
    std::int32_t result = 0;
    for (std::int32_t scan_id = 0;
         scan_id < static_cast<std::int32_t>(ScanCount); ++scan_id) {
        if (unlocked_only && !logbook_found(scan_id)) {
            continue;
        }
        const char category = scan_categories[static_cast<std::size_t>(scan_id)];
        if (categories.empty()
            || std::find(categories.begin(), categories.end(), category)
                != categories.end()) {
            ++result;
        }
    }
    return result;
}

std::int32_t StorySave::max_scan_count() const noexcept {
    // Menu.cs delegates this to GetLogbookCount(false, 'L', 'B', 'O').
    // The native ScanLog loader copies the table category into
    // scan_categories, so count that table-derived state instead of baking in
    // the usual 82 + 58 + 75 cartridge total.
    constexpr std::array<char, 3> categories{{'L', 'B', 'O'}};
    return logbook_count(false, categories);
}

std::int32_t StorySave::completion_percentage() const noexcept {
    const auto maximum = max_scan_count();
    if (maximum == 0) {
        return 0;
    }
    std::int32_t count = scan_count;
    for (std::int32_t weapon = 1; weapon < 8; ++weapon) {
        if (weapon != 2 && (weapons & (1u << weapon)) != 0) {
            ++count;
        }
    }
    for (std::int32_t model = 0; model < 8; ++model) {
        for (std::int32_t artifact = 0; artifact < 3; ++artifact) {
            count += found_artifact(artifact, model) ? 1 : 0;
        }
    }
    count += found_octolith_count();
    // Menu.cs delegates this value to StorySave.GetCompletionPercentage.
    // The first player's energy-tank unit is 100, while a fresh save's
    // health maximum is 99 (EnergyTank - 1); treating 99 as a tank would
    // incorrectly report one completed expansion on a new save.
    count += health_max / 100;
    count += (ammo_max[1] - 50) / 100;
    count += (ammo_max[0] - 400) / 300;
    return 100 * count / (maximum + 66);
}

void State::reset() noexcept {
    single_player = true;
    first_hunt = false;
    paused = false;
    pause_prevented = false;
    menu_pause = false;
    dialog_pause = false;
    pausing_dialog = false;
    unpausing_dialog = false;
    quit_requested = false;
    loading = false;
    mode = Mode::Story;
    mode_state = ModeStateKind::Adventure;
    match_state = MatchState::InProgress;
    transition_state = TransitionState::None;
    escape_state = EscapeState::None;
    escape_timer = -1.0F;
    escape_paused = false;
    encounter_state.fill(0);
    completed_random_encounter_rooms.fill(false);
    transition_room_id = -1;
    transition_alt_form = false;
    frame_count = 0;
    room_id = 0;
    room_name.clear();
    language = 0;
    local_slot = 0;
    player_count = 1;
    active_players = 0;
    for (std::size_t index = 0; index < nicknames.size(); ++index) {
        nicknames[index] = "Player" + std::to_string(index + 1);
    }
    stars.fill(0);
    team_standings.fill(0);
    result_slots.fill(0);
    prime_hunter = -1;
    match_time = -1.0F;
    match_clock_enabled = false;
    point_goal = 0;
    time_goal = 0.0F;
    teams = false;
    friendly_fire = false;
    damage_level = 1;
    octolith_reset = false;
    radar_players = false;
    affinity_weapons = false;
    force_end_game = false;
    tempo_changed = false;
    state_changed = false;
    match_end_time = 0.0F;
    last_alarm_time = 0.0F;
    next_alarm_index = 0;
    points.fill(0);
    team_points.fill(0);
    kills.fill(0);
    team_kills.fill(0);
    deaths.fill(0);
    team_deaths.fill(0);
    player_time.fill(0.0F);
    team_time.fill(0.0F);
    standings.fill(0);
    beam_damage_max.fill(0);
    beam_damage_dealt.fill(0);
    damage_count.fill(0);
    alt_damage_count.fill(0);
    kill_streak.fill(0);
    suicides.fill(0);
    friendly_kills.fill(0);
    headshot_kills.fill(0);
    for (auto& values : beam_kills) {
        values.fill(0);
    }
    octolith_scores.fill(0);
    octolith_drops.fill(0);
    octolith_stops.fill(0);
    nodes_captured.fill(0);
    nodes_lost.fill(0);
    kills_as_prime.fill(0);
    primes_killed.fill(0);
    queued_octolith_message_id = -1;
    queued_oubliette_unlock_message = false;
    trigger_state.fill(0);
    player_teams.fill(0xff);
    story_save = StorySave{};
}

void State::begin_room(std::string_view name, std::uint32_t id,
                       bool multiplayer_mode, Mode game_mode) {
    ::fruityprime::weapon_catalog::SetCurrent(multiplayer_mode);
    room_name.assign(name);
    room_id = id;
    single_player = !multiplayer_mode;
    mode = game_mode;
    match_state = MatchState::InProgress;
    transition_state = TransitionState::None;
    transition_room_id = -1;
    transition_alt_form = false;
    frame_count = 0;
    paused = false;
    loading = false;
    player_count = multiplayer_mode ? std::max<std::uint8_t>(player_count, 1)
                                    : 1;
    setup();
}

void State::tick() noexcept {
    if (!paused && !loading) {
        ++frame_count;
    }
}

void State::pause_menu() noexcept {
    menu_pause = true;
}

void State::unpause_menu() noexcept {
    menu_pause = false;
}

void State::pause_dialog() noexcept {
    pausing_dialog = true;
}

void State::unpause_dialog() noexcept {
    unpausing_dialog = true;
}

void State::apply_pause() noexcept {
    // CameraSequence.Current?.Flags.TestFlag(BlockInput) is owned by the
    // camera frontend.  The state object still preserves the managed
    // pending-request ordering; callers that know a camera is blocking input
    // simply defer this call, as the C# caller does.
    if (pausing_dialog) {
        dialog_pause = true;
    }
    if (unpausing_dialog) {
        dialog_pause = false;
    }
    pausing_dialog = false;
    unpausing_dialog = false;
    paused = !pause_prevented
        && (menu_pause || dialog_pause || escape_paused);
}

void State::reset_match_progress() noexcept {
    match_state = MatchState::InProgress;
    force_end_game = false;
    tempo_changed = false;
    state_changed = false;
    match_end_time = 0.0F;
    last_alarm_time = 0.0F;
    next_alarm_index = 0;
}

namespace {

[[nodiscard]] int compare_match_players(const State& state,
                                         std::size_t slot1,
                                         std::size_t slot2) noexcept {
    const auto normalized_time = [&state](std::size_t slot) {
        if ((state.mode == Mode::Survival
             || state.mode == Mode::SurvivalTeams)
            && state.player_time[slot] == -1.0F) {
            return std::numeric_limits<float>::max();
        }
        return state.player_time[slot];
    };
    const int points1 = state.points[slot1];
    const int points2 = state.points[slot2];
    const float time1 = normalized_time(slot1);
    const float time2 = normalized_time(slot2);
    const int deaths1 = state.deaths[slot1];
    const int deaths2 = state.deaths[slot2];
    const int kills1 = state.kills[slot1];
    const int kills2 = state.kills[slot2];

    switch (state.mode) {
    case Mode::Battle:
    case Mode::BattleTeams:
        if (points1 == points2 && deaths1 == deaths2) {
            return 0;
        }
        return points1 < points2 || (points1 == points2 && deaths1 > deaths2)
            ? -1 : 1;
    case Mode::Survival:
    case Mode::SurvivalTeams:
        if (time1 == time2 && deaths1 == deaths2) {
            return 0;
        }
        return time1 < time2 || (time1 == time2 && deaths1 > deaths2)
            ? -1 : 1;
    case Mode::Defender:
    case Mode::DefenderTeams:
    case Mode::PrimeHunter:
        if (time1 == time2 && kills1 == kills2) {
            return 0;
        }
        return time1 < time2 || (time1 == time2 && kills1 < kills2)
            ? -1 : 1;
    case Mode::Capture:
    case Mode::Bounty:
    case Mode::BountyTeams:
    case Mode::Nodes:
    case Mode::NodesTeams:
        if (points1 == points2 && kills1 == kills2) {
            return 0;
        }
        return points1 < points2 || (points1 == points2 && kills1 < kills2)
            ? -1 : 1;
    default:
        break;
    }
    if (points1 < points2) {
        return -1;
    }
    return points1 > points2 ? 1 : 0;
}

[[nodiscard]] int compare_match_teams(const State& state,
                                      std::size_t team1,
                                      std::size_t team2) noexcept {
    const auto normalized_time = [&state](std::size_t team) {
        if ((state.mode == Mode::Survival
             || state.mode == Mode::SurvivalTeams)
            && state.team_time[team] == -1.0F) {
            return std::numeric_limits<float>::max();
        }
        return state.team_time[team];
    };
    const int points1 = state.team_points[team1];
    const int points2 = state.team_points[team2];
    const float time1 = normalized_time(team1);
    const float time2 = normalized_time(team2);
    const int deaths1 = state.team_deaths[team1];
    const int deaths2 = state.team_deaths[team2];
    const int kills1 = state.team_kills[team1];
    const int kills2 = state.team_kills[team2];

    if (state.mode == Mode::BattleTeams) {
        if (points1 == points2 && deaths1 == deaths2) {
            return 0;
        }
        return points1 < points2 || (points1 == points2 && deaths1 > deaths2)
            ? -1 : 1;
    }
    if (state.mode == Mode::SurvivalTeams) {
        if (time1 == time2 && deaths1 == deaths2) {
            return 0;
        }
        return time1 < time2 || (time1 == time2 && deaths1 > deaths2)
            ? -1 : 1;
    }
    if (state.mode == Mode::DefenderTeams) {
        if (time1 == time2 && kills1 == kills2) {
            return 0;
        }
        return time1 < time2 || (time1 == time2 && kills1 < kills2)
            ? -1 : 1;
    }
    if (state.mode == Mode::Capture || state.mode == Mode::BountyTeams
        || state.mode == Mode::NodesTeams) {
        if (points1 == points2 && kills1 == kills2) {
            return 0;
        }
        return points1 < points2 || (points1 == points2 && kills1 < kills2)
            ? -1 : 1;
    }
    return 0;
}

} // namespace

void State::sync_players(std::span<const net::PlayerState> players) noexcept {
    player_count = static_cast<std::uint8_t>(std::min<std::size_t>(
        players.size(), SlotCapacity));
    active_players = 0;
    points.fill(0);
    kills.fill(0);
    deaths.fill(0);
    team_points.fill(0);
    team_kills.fill(0);
    team_deaths.fill(0);
    player_teams.fill(0xff);

    for (const auto& player : players) {
        if (player.slot_index >= SlotCapacity) {
            continue;
        }
        const std::size_t slot = player.slot_index;
        points[slot] = player.points;
        kills[slot] = static_cast<std::int32_t>(player.kills);
        deaths[slot] = static_cast<std::int32_t>(player.deaths);
        if ((player.flags & net::PlayerState::FlagActive) == 0) {
            continue;
        }
        player_teams[slot] = player.team;
        ++active_players;
        const std::size_t team = std::min<std::size_t>(
            player.team, SlotCapacity - 1);
        team_points[team] += player.points;
        team_kills[team] += static_cast<std::int32_t>(player.kills);
        team_deaths[team] += static_cast<std::int32_t>(player.deaths);
    }
    update_results();
}

void State::update_results() noexcept {
    result_slots.fill(0);
    standings.fill(static_cast<std::int32_t>(SlotCapacity - 1));
    team_standings.fill(static_cast<std::int32_t>(SlotCapacity - 1));

    std::array<std::uint8_t, SlotCapacity> slots{};
    std::size_t count = 0;
    for (std::size_t slot = 0; slot < player_teams.size(); ++slot) {
        if (player_teams[slot] == 0xff || count >= slots.size()) {
            continue;
        }
        slots[count++] = static_cast<std::uint8_t>(slot);
    }
    active_players = static_cast<std::uint8_t>(count);
    for (std::size_t index = 0; index < count; ++index) {
        for (std::size_t next = index + 1; next < count; ++next) {
            const std::size_t slot = slots[index];
            const std::size_t next_slot = slots[next];
            const std::size_t team = std::min<std::size_t>(
                player_teams[slot], SlotCapacity - 1);
            const std::size_t next_team = std::min<std::size_t>(
                player_teams[next_slot], SlotCapacity - 1);
            const bool lower = teams && team != next_team
                ? compare_match_teams(*this, team, next_team) < 0
                : compare_match_players(*this, slot, next_slot) < 0;
            if (lower) {
                std::swap(slots[index], slots[next]);
            }
        }
    }
    for (std::size_t index = 0; index < count; ++index) {
        result_slots[index] = slots[index];
    }

    if (!teams) {
        std::int32_t rank = 0;
        for (std::size_t index = 0; index < count; ++index) {
            const std::size_t slot = slots[index];
            standings[slot] = rank;
            if (index + 1 < count
                && compare_match_players(*this, slot, slots[index + 1]) != 0) {
                rank = static_cast<std::int32_t>(index + 1);
            }
        }
        return;
    }

    const int team_compare = compare_match_teams(*this, 0, 1);
    const std::int32_t team_rank0 = team_compare < 0 ? 1 : 0;
    const std::int32_t team_rank1 = team_compare > 0 ? 1 : 0;
    std::array<std::int32_t, SlotCapacity> within_rank{};
    within_rank.fill(0);
    for (std::size_t index = 0; index < count; ++index) {
        const std::size_t slot = slots[index];
        const std::size_t team = std::min<std::size_t>(
            player_teams[slot], SlotCapacity - 1);
        standings[slot] = team == 0 ? team_rank0 : team_rank1;
        if (index > 0) {
            const std::size_t previous = slots[index - 1];
            const std::size_t previous_team = std::min<std::size_t>(
                player_teams[previous], SlotCapacity - 1);
            if (previous_team == team
                && compare_match_players(*this, previous, slot) != 0) {
                ++within_rank[team];
            }
        }
        team_standings[slot] = within_rank[team];
    }
}

void State::process_match_frame(
    float seconds, std::span<const net::PlayerState> players,
    bool score_authority) noexcept {
    sync_players(players);
    if (paused || match_state != MatchState::InProgress || single_player) {
        return;
    }

    update_time(seconds);
    const auto end_match = [this]() noexcept {
        match_time = 0.0F;
        match_state = MatchState::GameOver;
        update_results();
    };
    if (force_end_game
        || (match_clock_enabled && match_time <= 0.0F)) {
        end_match();
        return;
    }
    if (!score_authority) {
        return;
    }

    if (mode == Mode::Survival || mode == Mode::SurvivalTeams) {
        std::size_t active = 0;
        std::size_t eligible = 0;
        std::array<std::uint32_t, SlotCapacity> team_deaths{};
        std::array<bool, 2> teams_alive{};
        for (const auto& player : players) {
            if ((player.flags & net::PlayerState::FlagActive) == 0) {
                continue;
            }
            ++active;
            const std::size_t team = std::min<std::size_t>(
                player.team, SlotCapacity - 1);
            team_deaths[team] += player.deaths;
        }
        for (const auto& player : players) {
            if ((player.flags & net::PlayerState::FlagActive) == 0) {
                continue;
            }
            const std::size_t team = std::min<std::size_t>(
                player.team,
                team_deaths.size() - 1);
            const bool can_continue = player.health > 0
                || team_deaths[team] <= point_goal;
            if (can_continue) {
                ++eligible;
                if (teams) {
                    teams_alive[team & 1u] = true;
                }
            }
        }
        if (active < 2 || eligible < 2
            || (teams && (!teams_alive[0] || !teams_alive[1]))) {
            for (const auto& player : players) {
                const std::size_t team = std::min<std::size_t>(
                    player.team, team_deaths.size() - 1);
                if ((player.flags & net::PlayerState::FlagActive) != 0
                    && (player.health > 0
                        || team_deaths[team] <= point_goal)
                    && player.slot_index < player_time.size()) {
                    player_time[player.slot_index] = -1.0F;
                    team_time[team] = -1.0F;
                }
            }
            end_match();
        }
        return;
    }

    const bool point_mode = mode == Mode::Battle
        || mode == Mode::BattleTeams || mode == Mode::Capture
        || mode == Mode::Bounty || mode == Mode::BountyTeams
        || mode == Mode::Nodes || mode == Mode::NodesTeams;
    if (point_mode && point_goal > 0) {
        bool reached = false;
        if (teams) {
            reached = team_points[0] >= point_goal
                || team_points[1] >= point_goal;
        } else {
            for (const auto& player : players) {
                if ((player.flags & net::PlayerState::FlagActive) != 0
                    && player.points >= point_goal) {
                    reached = true;
                    break;
                }
            }
        }
        if (reached) {
            end_match();
            return;
        }
    }

    if ((mode == Mode::Defender || mode == Mode::DefenderTeams)
        && time_goal > 0.0F) {
        for (const auto& player : players) {
            if ((player.flags & net::PlayerState::FlagActive) == 0) {
                continue;
            }
            const std::size_t team = std::min<std::size_t>(
                player.team, team_time.size() - 1);
            if (team_time[team] >= time_goal) {
                end_match();
                return;
            }
        }
    }

    if (mode == Mode::PrimeHunter && prime_hunter >= 0
        && static_cast<std::size_t>(prime_hunter) < player_teams.size()) {
        const auto slot = static_cast<std::size_t>(prime_hunter);
        if (player_teams[slot] == 0xff) {
            prime_hunter = -1;
        } else if (time_goal > 0.0F && player_time[slot] >= time_goal) {
            end_match();
        }
    }
}

void State::process_message(const messaging::MessageInfo& info) noexcept {
    // These two values are the compact native queue messages.  They are kept
    // separate from formats::Message because the cartridge uses a different
    // numeric table (SetTriggerState is 42 there, not 3).
    if (info.cartridge_message == 0) {
        if (info.message == messaging::Message::SetTriggerState
            && info.parameter1 >= 0) {
            set_trigger(static_cast<std::size_t>(info.parameter1));
        } else if (info.message == messaging::Message::ClearTriggerState
                   && info.parameter1 >= 0) {
            clear_trigger(static_cast<std::size_t>(info.parameter1));
        }
        return;
    }

    const auto message = static_cast<formats::Message>(
        info.cartridge_message);
    switch (message) {
    case formats::Message::SetTriggerState:
        if (info.parameter1 >= 0
            && static_cast<std::size_t>(info.parameter1)
                < story_save.trigger_state.size() * 8) {
            const auto index = static_cast<std::size_t>(info.parameter1);
            story_save.trigger_state[index / 8] |= static_cast<std::uint8_t>(
                1u << (index % 8));
        }
        return;
    case formats::Message::ClearTriggerState:
        if (info.parameter1 >= 0
            && static_cast<std::size_t>(info.parameter1)
                < story_save.trigger_state.size() * 8) {
            const auto index = static_cast<std::size_t>(info.parameter1);
            story_save.trigger_state[index / 8] &= static_cast<std::uint8_t>(
                ~(1u << (index % 8)));
        }
        return;
    case formats::Message::Complete:
        // Complete is a global message.  The scene frontend decides how to
        // present the result, while GameState owns the stop request.
        match_time = 0.0F;
        return;
    case formats::Message::Checkpoint:
        if (single_player && info.sender >= 0) {
            story_save.checkpoint_entity_id = info.sender;
            story_save.checkpoint_room_id = static_cast<std::int32_t>(room_id);
        }
        return;
    case formats::Message::ShipHatch:
        if (single_player && info.sender >= 0) {
            reset_escape_state();
            story_save.checkpoint_entity_id = info.sender;
            story_save.checkpoint_room_id = static_cast<std::int32_t>(room_id);
        }
        return;
    case formats::Message::UnlockOubliette:
        if (single_player && story_save.current_octoliths == 0xff) {
            pause_prevented = true;
            queued_oubliette_unlock_message = true;
        }
        return;
    case formats::Message::LoadOubliette:
        if (single_player) {
            transition_room_id = 91; // Gorea_b1
            transition_state = TransitionState::Start;
        }
        return;
    case formats::Message::EscapeUpdate1:
        update_escape_state(info.parameter1 * 30, info.parameter2);
        return;
    case formats::Message::EscapeUpdate2:
        update_escape_state(info.parameter1, info.parameter2);
        return;
    default:
        // Entity-facing cartridge messages are deliberately left for World,
        // runtime entities, and camera-sequence entities.
        return;
    }
}

void State::process_story_frame(float seconds) noexcept {
    if (!single_player || escape_timer < 0.0F || paused || menu_pause
        || dialog_pause || escape_paused || !std::isfinite(seconds)
        || seconds <= 0.0F) {
        return;
    }
    escape_timer = std::max(0.0F, escape_timer - seconds);
    if (escape_timer == 0.0F) {
        // The player entity owns the actual Death message.  Retain the
        // EscapeState until that entity reports the transition, matching the
        // managed state instead of silently changing the story outcome here.
        escape_timer = -1.0F;
    }
}

void State::update_escape_state(std::int32_t frames,
                                std::int32_t state_id) noexcept {
    const auto state = static_cast<EscapeState>(state_id);
    const float time = static_cast<float>(frames) / 30.0F;
    if (state == EscapeState::None) {
        escape_timer = -1.0F;
        escape_paused = false;
    } else if (time == 0.0F) {
        escape_paused = !escape_paused;
    } else if (escape_state != state || escape_timer < 0.0F) {
        escape_timer = std::max(0.0F, time);
        escape_paused = false;
        if (state == EscapeState::Escape) {
            story_save.trigger_state[2] |= 0x80;
        }
    }
    escape_state = state;
}

void State::reset_escape_state() noexcept {
    escape_state = EscapeState::None;
    escape_timer = -1.0F;
    escape_paused = false;
}

void State::setup() noexcept {
    reset_match_progress();
    teams = is_team_mode(mode);
    match_clock_enabled = false;
    point_goal = 0;
    time_goal = 0.0F;
    match_time = -1.0F;
    mode_state = ModeStateKind::Adventure;

    switch (mode) {
    case Mode::Battle:
    case Mode::BattleTeams:
        mode_state = ModeStateKind::Battle;
        point_goal = 7;
        match_time = 7.0F * 60.0F;
        match_clock_enabled = true;
        break;
    case Mode::Survival:
    case Mode::SurvivalTeams:
        mode_state = ModeStateKind::Survival;
        point_goal = 2;
        match_time = 15.0F * 60.0F;
        match_clock_enabled = true;
        break;
    case Mode::Bounty:
    case Mode::BountyTeams:
        mode_state = ModeStateKind::Bounty;
        point_goal = 3;
        match_time = 15.0F * 60.0F;
        match_clock_enabled = true;
        break;
    case Mode::Capture:
        mode_state = ModeStateKind::Capture;
        point_goal = 5;
        match_time = 15.0F * 60.0F;
        match_clock_enabled = true;
        break;
    case Mode::Defender:
    case Mode::DefenderTeams:
        mode_state = ModeStateKind::Defender;
        time_goal = 1.5F * 60.0F;
        match_time = 15.0F * 60.0F;
        match_clock_enabled = true;
        break;
    case Mode::Nodes:
    case Mode::NodesTeams:
        mode_state = ModeStateKind::Nodes;
        point_goal = 70;
        match_time = 15.0F * 60.0F;
        match_clock_enabled = true;
        break;
    case Mode::PrimeHunter:
        mode_state = ModeStateKind::PrimeHunter;
        time_goal = 1.5F * 60.0F;
        match_time = 15.0F * 60.0F;
        match_clock_enabled = true;
        break;
    case Mode::SinglePlayer:
    default:
        break;
    }
}

void State::start_new_save() noexcept {
    story_save = StorySave{};
}

void State::complete_random_encounter(std::int32_t encounter_room_id) noexcept {
    // GameState.CompleteRandomEncounter receives the cartridge room ID, not
    // the zero-based save-array index.  Story rooms occupy IDs 27..92.
    if (encounter_room_id < 27 || encounter_room_id > 92) {
        return;
    }
    const auto index = static_cast<std::size_t>(encounter_room_id - 27);
    completed_random_encounter_rooms[index] = true;
}

void State::update_boss_flags(std::int32_t area_id) noexcept {
    if (area_id < 0 || area_id >= 9) {
        return;
    }
    const auto shift = static_cast<unsigned>(area_id * 2);
    const auto mask = static_cast<std::uint32_t>(3u << shift);
    // This is GameState.UpdateBossFlags: a defeated boss starts the escape
    // state. EnterShip later converts the corresponding *Kill bit to Done;
    // it is not the same operation as this public update method.
    story_save.boss_flags = (story_save.boss_flags & ~mask)
        | static_cast<std::uint32_t>(1u << shift);
}

void State::enter_ship() noexcept {
    // GameState.EnterShip clears the active escape trigger before recording
    // each boss encounter as completed.
    story_save.trigger_state[2] &= static_cast<std::uint8_t>(0x7f);

    constexpr std::array<std::array<std::uint32_t, 2>, 8> flag_pairs{{
        {static_cast<std::uint32_t>(BossFlags::Unit1B1Kill),
         static_cast<std::uint32_t>(BossFlags::Unit1B1Done)},
        {static_cast<std::uint32_t>(BossFlags::Unit1B2Kill),
         static_cast<std::uint32_t>(BossFlags::Unit1B2Done)},
        {static_cast<std::uint32_t>(BossFlags::Unit2B1Kill),
         static_cast<std::uint32_t>(BossFlags::Unit2B1Done)},
        {static_cast<std::uint32_t>(BossFlags::Unit2B2Kill),
         static_cast<std::uint32_t>(BossFlags::Unit2B2Done)},
        {static_cast<std::uint32_t>(BossFlags::Unit3B1Kill),
         static_cast<std::uint32_t>(BossFlags::Unit3B1Done)},
        {static_cast<std::uint32_t>(BossFlags::Unit3B2Kill),
         static_cast<std::uint32_t>(BossFlags::Unit3B2Done)},
        {static_cast<std::uint32_t>(BossFlags::Unit4B1Kill),
         static_cast<std::uint32_t>(BossFlags::Unit4B1Done)},
        {static_cast<std::uint32_t>(BossFlags::Unit4B2Kill),
         static_cast<std::uint32_t>(BossFlags::Unit4B2Done)},
    }};
    for (const auto& pair : flag_pairs) {
        if ((story_save.boss_flags & pair[0]) == 0) {
            continue;
        }
        story_save.boss_flags = (story_save.boss_flags & ~pair[0]) | pair[1];
    }
}

AreaState State::area_state(std::int32_t area_id,
                            const StorySave* save) const noexcept {
    if (area_id < 0 || area_id >= 16) {
        return AreaState::None;
    }
    const StorySave& selected = save != nullptr ? *save : story_save;
    const auto shift = static_cast<unsigned>(area_id * 2);
    const auto state = (selected.boss_flags >> shift) & 3u;
    if (state == 1u) {
        return AreaState::Escape;
    }
    if (state == 2u) {
        return AreaState::Clear;
    }
    return AreaState::None;
}

void State::update_time(float seconds) noexcept {
    if (paused || match_state != MatchState::InProgress
        || !std::isfinite(seconds) || seconds <= 0.0F) {
        return;
    }
    if (match_time > 0.0F) {
        match_time = std::max(0.0F, match_time - seconds);
    }
}

void State::set_trigger(std::size_t index) noexcept {
    if (index < TriggerBytes * 8) {
        trigger_state[index / 8] |=
            static_cast<std::uint8_t>(1u << (index % 8));
    }
}

void State::clear_trigger(std::size_t index) noexcept {
    if (index < TriggerBytes * 8) {
        trigger_state[index / 8] &= static_cast<std::uint8_t>(
            ~(1u << (index % 8)));
    }
}

bool State::trigger_set(std::size_t index) const noexcept {
    return index < TriggerBytes * 8
        && (trigger_state[index / 8] & (1u << (index % 8))) != 0;
}

} // namespace fruityprime::game
