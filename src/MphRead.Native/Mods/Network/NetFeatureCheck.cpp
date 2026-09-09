#include "Mods/Network/net_feature_check.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>

namespace fruityprime::net {
namespace {

[[nodiscard]] Vec3 subtract(Vec3 left, Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] float length(Vec3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y
                     + value.z * value.z);
}

[[nodiscard]] bool finite(Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y)
        && std::isfinite(value.z);
}

[[nodiscard]] double record_height(
    const NetFeatureCheck::Record& record) noexcept {
    return record.max_y > record.min_y
        ? static_cast<double>(record.max_y - record.min_y) : 0.0;
}

[[nodiscard]] bool hunter_lays_bombs(metadata::Hunter hunter) noexcept {
    return hunter == metadata::Hunter::Samus
        || hunter == metadata::Hunter::Kanden
        || hunter == metadata::Hunter::Sylux;
}

[[nodiscard]] double spawned(const NetFeatureCheck::Record& record) noexcept {
    return record.spawned_frames;
}

[[nodiscard]] double movement(const NetFeatureCheck::Record& record) noexcept {
    return record.travelled;
}

[[nodiscard]] double facing(const NetFeatureCheck::Record& record) noexcept {
    return record.facing_degrees;
}

[[nodiscard]] double shooting(const NetFeatureCheck::Record& record) noexcept {
    return record.beam_frames;
}

[[nodiscard]] double shots(const NetFeatureCheck::Record& record) noexcept {
    return record.shots_fired;
}

[[nodiscard]] double weapon_switch(
    const NetFeatureCheck::Record& record) noexcept {
    return record.weapon_changes;
}

[[nodiscard]] double alt_attack(
    const NetFeatureCheck::Record& record) noexcept {
    return record.alt_attack_presses;
}

[[nodiscard]] double alt_form(
    const NetFeatureCheck::Record& record) noexcept {
    return record.alt_form_in_morph_phase;
}

[[nodiscard]] double alt_form_total(
    const NetFeatureCheck::Record& record) noexcept {
    return record.alt_form_frames;
}

[[nodiscard]] double unmorph(const NetFeatureCheck::Record& record) noexcept {
    return record.biped_in_unmorph_phase;
}

[[nodiscard]] double bombs(const NetFeatureCheck::Record& record) noexcept {
    return record.bomb_frames;
}

[[nodiscard]] double halfturret(
    const NetFeatureCheck::Record& record) noexcept {
    return record.halfturret_frames;
}

[[nodiscard]] double zoom(const NetFeatureCheck::Record& record) noexcept {
    return record.zoom_frames;
}

[[nodiscard]] double frozen(const NetFeatureCheck::Record& record) noexcept {
    return record.frozen_frames;
}

[[nodiscard]] double spectating(
    const NetFeatureCheck::Record& record) noexcept {
    return record.spectating_frames;
}

[[nodiscard]] double double_damage(
    const NetFeatureCheck::Record& record) noexcept {
    return record.double_damage_frames;
}

[[nodiscard]] double damage_taken(
    const NetFeatureCheck::Record& record) noexcept {
    return record.damage_events;
}

[[nodiscard]] double hit_in_alt_form(
    const NetFeatureCheck::Record& record) noexcept {
    return record.damage_in_alt_form;
}

[[nodiscard]] double deaths(const NetFeatureCheck::Record& record) noexcept {
    return record.deaths;
}

[[nodiscard]] double teleports(const NetFeatureCheck::Record& record) noexcept {
    return record.teleports;
}

struct FeatureSpec {
    const char* name;
    double (*get)(const NetFeatureCheck::Record&) noexcept;
};

constexpr std::array<FeatureSpec, 21> Features{{
    {"spawn", spawned},
    {"movement", movement},
    {"jump", record_height},
    {"facing", facing},
    {"shooting", shooting},
    {"shots", shots},
    {"weapon-switch", weapon_switch},
    {"alt-attack", alt_attack},
    {"alt-form", alt_form},
    {"alt-form-total", alt_form_total},
    {"unmorph", unmorph},
    {"bombs", bombs},
    {"halfturret", halfturret},
    {"zoom", zoom},
    {"frozen", frozen},
    {"spectating", spectating},
    {"double-damage", double_damage},
    {"damage-taken", damage_taken},
    {"hit-in-alt-form", hit_in_alt_form},
    {"deaths", deaths},
    {"teleports", teleports}
}};

struct Threshold {
    double needed;
    bool applicable = true;
    bool pairwise = false;
};

[[nodiscard]] Threshold threshold(std::string_view feature,
                                  const NetFeatureCheck::Record& other) noexcept {
    if (feature == "spawn") return {30.0};
    if (feature == "movement") return {5.0};
    if (feature == "jump") return {1.5};
    if (feature == "facing") return {180.0};
    if (feature == "shots") return {10.0};
    if (feature == "shooting") return {10.0};
    if (feature == "weapon-switch") return {2.0, true, true};
    if (feature == "alt-attack") return {3.0, true, true};
    if (feature == "alt-form") return {30.0};
    if (feature == "unmorph") return {30.0};
    if (feature == "bombs") return {
        5.0, hunter_lays_bombs(other.hunter), false};
    if (feature == "halfturret") return {
        5.0, other.hunter == metadata::Hunter::Weavel, false};
    if (feature == "zoom") return {10.0};
    if (feature == "double-damage") return {10.0};
    if (feature == "damage-taken") return {1.0};
    if (feature == "hit-in-alt-form") return {2.0, true, true};
    if (feature == "deaths") return {1.0, true, true};
    return {0.0};
}

[[nodiscard]] std::string value_text(double value) {
    std::ostringstream text;
    text << std::fixed << std::setprecision(2) << value;
    std::string result = text.str();
    while (result.size() > 1 && result.back() == '0') {
        result.pop_back();
    }
    if (!result.empty() && result.back() == '.') {
        result.pop_back();
    }
    return result;
}

} // namespace

NetFeatureCheck::NetFeatureCheck() noexcept {
    reset();
}

void NetFeatureCheck::reset() noexcept {
    records_ = {};
    item_samples_ = 0;
    item_total_ = 0;
    items_now_ = 0;
    items_picked_up_ = 0;
    last_item_count_ = -1;
    phase_frames_.clear();
    boards_.clear();
    local_slot_ = 0;
}

void NetFeatureCheck::observe(const Frame& frame) {
    local_slot_ = std::clamp(frame.local_slot, 0,
                             static_cast<int>(NetConfig::SlotCapacity - 1));
    ++phase_frames_[frame.phase];

    items_now_ = std::max(0, frame.items_now);
    if (last_item_count_ > items_now_) {
        items_picked_up_ += last_item_count_ - items_now_;
    }
    last_item_count_ = items_now_;
    ++item_samples_;
    item_total_ += items_now_;

    // NetConfig.IntentSendInterval is one in the current managed and native
    // protocol. Keep the cadence in this calculation rather than silently
    // sampling a different local path if the packet interval changes later.
    const bool local_sample_path = frame.net_frame
        % NetConfig::IntentSendInterval == 0;

    for (std::size_t index = 0; index < NetConfig::SlotCapacity; ++index) {
        const auto slot = static_cast<std::uint8_t>(index);
        const PlayerObservation& player = frame.players[index];
        if (!player.present || !player.active) {
            continue;
        }
        Record& record = records_[index];
        record.hunter = static_cast<metadata::Hunter>(player.hunter);
        if (frame.beams[index] > 0) {
            ++record.beam_frames;
        }
        if (frame.fired_totals[index] > record.last_fired_total) {
            record.shots_fired += frame.fired_totals[index]
                - record.last_fired_total;
        }
        record.last_fired_total = frame.fired_totals[index];
        if (frame.bombs[index] > 0) {
            ++record.bomb_frames;
        }
        if (player.alt_attack_pressed) {
            ++record.alt_attack_presses;
        }
        if (frame.halfturrets[index] > 0) {
            ++record.halfturret_frames;
        }
        if (!player.spawned) {
            continue;
        }

        ++record.spawned_frames;
        if (player.alt_form) {
            ++record.alt_form_frames;
            if (frame.phase == TestPhase::MorphA
                || frame.phase == TestPhase::AltAttackA
                || frame.phase == TestPhase::MorphB
                || frame.phase == TestPhase::AltAttackB) {
                ++record.alt_form_in_morph_phase;
            }
        } else if (frame.phase == TestPhase::Zoom
                   || frame.phase == TestPhase::Duel) {
            ++record.biped_in_unmorph_phase;
        }
        if (player.zoomed) {
            ++record.zoom_frames;
        }
        if (player.frozen) {
            ++record.frozen_frames;
        }
        if (player.spectating) {
            ++record.spectating_frames;
        }
        if (player.double_damage) {
            ++record.double_damage_frames;
        }
        if (player.current_weapon != record.last_weapon) {
            if (record.last_weapon != NoWeapon) {
                ++record.weapon_changes;
            }
            record.last_weapon = player.current_weapon;
        }
        if (record.last_health > 0 && player.health > 0
            && player.health < record.last_health) {
            ++record.damage_events;
            if (player.alt_form) {
                ++record.damage_in_alt_form;
            }
        }
        if (record.was_alive && player.health == 0) {
            ++record.deaths;
        }

        record.frames_since_launch = player.world_events != record.last_world_events
            ? 0 : record.frames_since_launch + 1;
        record.last_world_events = player.world_events;
        record.frames_since_respawn = player.health > record.last_health
            || player.health == 0 ? 0 : record.frames_since_respawn + 1;
        record.last_health = player.health;
        record.was_alive = player.health > 0;

        if (finite(player.position)) {
            record.min_y = std::min(record.min_y, player.position.y);
            record.max_y = std::max(record.max_y, player.position.y);
        }

        if (record.have_frame_previous && finite(player.position)
            && finite(record.last_frame_position)) {
            const float frame_step = length(subtract(
                player.position, record.last_frame_position));
            if (frame_step > 0.01F) {
                ++record.moved_frames;
            }
            if (frame_step > TeleportStep
                && record.frames_since_respawn > 60
                && record.frames_since_launch > LaunchGraceFrames) {
                ++record.teleports;
                record.worst_step = std::max<double>(record.worst_step,
                                                      frame_step);
            }
        }
        record.last_frame_position = player.position;
        record.have_frame_previous = true;

        const bool sample_path = static_cast<int>(slot) != local_slot_
            || local_sample_path;
        if (sample_path && record.have_previous && finite(player.position)
            && finite(record.last_position) && finite(player.facing)
            && finite(record.last_facing)) {
            const float step = length(subtract(player.position,
                                               record.last_position));
            if (step < 5.0F) {
                record.travelled += step;
            }
            const float dot = std::clamp(
                player.facing.x * record.last_facing.x
                + player.facing.y * record.last_facing.y
                + player.facing.z * record.last_facing.z,
                -1.0F, 1.0F);
            record.facing_degrees +=
                static_cast<double>(std::acos(dot) * 180.0F
                                    / 3.14159265358979323846F);
        }
        if (sample_path) {
            record.last_position = player.position;
            record.last_facing = player.facing;
            record.have_previous = true;
        }

        if (static_cast<int>(slot) == local_slot_
            || !frame.remote_state_valid[index] || frame.authority) {
            continue;
        }
        record.ever_compared = true;
        const PlayerState& state = frame.remote_states[index];
        const bool want_alt = (state.flags & PlayerState::FlagAltForm) != 0;
        const bool visible = player.health > 0
            && (state.flags & PlayerState::FlagSpawned) != 0;
        if (visible && want_alt != player.alt_form) {
            ++record.form_disagree_frames;
            ++record.form_disagree_run;
            if (record.form_disagree_run > record.worst_form_disagree_run) {
                record.worst_form_disagree_run = record.form_disagree_run;
                record.worst_form_context = "phase ";
                record.worst_form_context += phase_name(frame.phase);
                record.worst_form_context += ", authority wanted ";
                record.worst_form_context += want_alt ? "alt" : "biped";
                record.worst_form_context += ", puppet ";
                record.worst_form_context += player.alt_form ? "alt" : "biped";
                record.worst_form_context += ", hp ";
                record.worst_form_context += std::to_string(player.health);
            }
        } else {
            record.form_disagree_run = 0;
        }
        if (!visible || (state.flags & PlayerState::FlagSpawned) == 0) {
            continue;
        }
        if (finite(state.position) && finite(player.position)) {
            const double gap = length(subtract(state.position,
                                               player.position));
            record.worst_position_gap = std::max(record.worst_position_gap,
                                                 gap);
        } else {
            record.worst_position_gap = std::numeric_limits<double>::infinity();
        }
    }
}

bool NetFeatureCheck::lays_bombs(metadata::Hunter hunter) noexcept {
    return hunter_lays_bombs(hunter);
}

double NetFeatureCheck::height(const Record& record) noexcept {
    return record_height(record);
}

const char* NetFeatureCheck::phase_name(TestPhase phase) noexcept {
    switch (phase) {
    case TestPhase::Idle: return "Idle";
    case TestPhase::Walk: return "Walk";
    case TestPhase::Jump: return "Jump";
    case TestPhase::Turn: return "Turn";
    case TestPhase::Shoot: return "Shoot";
    case TestPhase::SwitchWeapons: return "SwitchWeapons";
    case TestPhase::Charge: return "Charge";
    case TestPhase::MorphA: return "MorphA";
    case TestPhase::AltAttackA: return "AltAttackA";
    case TestPhase::MorphB: return "MorphB";
    case TestPhase::AltAttackB: return "AltAttackB";
    case TestPhase::Unmorph: return "Unmorph";
    case TestPhase::Zoom: return "Zoom";
    case TestPhase::Afflict: return "Afflict";
    case TestPhase::Duel: return "Duel";
    }
    return "Unknown";
}

std::string NetFeatureCheck::hunter_name(metadata::Hunter hunter) {
    switch (hunter) {
    case metadata::Hunter::Samus: return "Samus";
    case metadata::Hunter::Kanden: return "Kanden";
    case metadata::Hunter::Trace: return "Trace";
    case metadata::Hunter::Sylux: return "Sylux";
    case metadata::Hunter::Noxus: return "Noxus";
    case metadata::Hunter::Spire: return "Spire";
    case metadata::Hunter::Weavel: return "Weavel";
    case metadata::Hunter::Guardian: return "Guardian";
    case metadata::Hunter::Random: return "Random";
    }
    return "Unknown";
}

NetFeatureCheck::ReportResult NetFeatureCheck::report(
    const std::array<std::string, NetConfig::SlotCapacity>&) const {
    ReportResult result;
    const Record& mine = records_[static_cast<std::size_t>(local_slot_)];
    for (std::size_t index = 0; index < records_.size(); ++index) {
        if (static_cast<int>(index) == local_slot_
            || records_[index].spawned_frames == 0) {
            continue;
        }
        result.any_remote = true;
        const Record& other = records_[index];
        PairResult pair;
        pair.slot = static_cast<std::uint8_t>(index);
        pair.hunter = other.hunter;
        pair.ever_compared = other.ever_compared;
        pair.form_disagree_frames = other.form_disagree_frames;
        pair.worst_form_disagree_run = other.worst_form_disagree_run;
        pair.worst_position_gap = other.worst_position_gap;
        pair.worst_step = other.worst_step;
        pair.worst_form_context = other.worst_form_context;

        for (const FeatureSpec& feature : Features) {
            const Threshold limits = threshold(feature.name, other);
            FeatureResult row;
            row.feature = feature.name;
            row.mine = feature.get(mine);
            row.seen = feature.get(other);
            row.needed = limits.needed;
            row.applicable = limits.applicable;
            row.pairwise = limits.pairwise;
            row.tested = row.applicable
                && (row.pairwise ? row.mine + row.seen >= row.needed
                                 : row.mine >= row.needed);
            row.ok = row.seen >= row.needed;
            row.verdict = !row.applicable ? "n/a"
                : !row.tested ? "untested"
                : row.pairwise ? "ok" : row.ok ? "ok" : "FAIL";
            if (row.tested && !row.pairwise && !row.ok) {
                ++pair.failures;
            }
            pair.features.push_back(std::move(row));
        }
        if (other.worst_form_disagree_run > 60) {
            ++pair.failures;
        }
        if (other.worst_position_gap > 8.0
            || !std::isfinite(other.worst_position_gap)) {
            ++pair.failures;
        }
        result.failures += pair.failures;
        result.pairs.push_back(std::move(pair));
    }
    if (!result.any_remote) {
        result.failures = 1;
    }
    return result;
}

void NetFeatureCheck::print_report(
    std::ostream& output,
    const std::array<std::string, NetConfig::SlotCapacity>& names) const {
    const ReportResult result = report(names);
    const Record& mine = records_[static_cast<std::size_t>(local_slot_)];
    const std::string& local_name = names[static_cast<std::size_t>(local_slot_)];

    output << '\n'
           << "  feature coverage (mine = what my player did, "
           << "theirs = what I saw of them)\n";
    for (const FeatureSpec& feature : Features) {
        output << "  netcheck " << local_name << " mine " << local_name
               << ' ' << feature.name << ' ' << value_text(feature.get(mine))
               << '\n';
    }
    for (const PairResult& pair : result.pairs) {
        const std::size_t index = pair.slot;
        const Record& other = records_[index];
        const std::string& remote_name = names[index];
        output << "    --- as I saw " << remote_name << " (slot "
               << static_cast<unsigned>(pair.slot) << ", "
               << hunter_name(pair.hunter) << ") ---\n";
        for (const FeatureResult& row : pair.features) {
            output << "  netcheck " << local_name << " saw " << remote_name
                   << ' ' << row.feature << ' ' << value_text(row.seen) << '\n';
            output << "    " << std::left << std::setw(16) << row.feature
                   << std::right << " mine " << std::setw(7)
                   << value_text(row.mine) << " theirs " << std::setw(7)
                   << value_text(row.seen) << ' ' << row.verdict << '\n';
        }
        output << "    " << remote_name << ": " << other.teleports
               << " teleport(s), worst jump " << value_text(other.worst_step)
               << " units\n";
        if (!other.ever_compared) {
            output << "    " << remote_name
                   << ": form and position agreement not measured here -- "
                   << "this client is the authority and receives no snapshot "
                   << "to compare with\n";
        } else {
            output << "    " << remote_name << ": form disagreed on "
                   << other.form_disagree_frames << " frame(s) (longest run "
                   << other.worst_form_disagree_run << "), worst position gap "
                   << value_text(other.worst_position_gap) << " units\n";
        }
        if (other.worst_form_disagree_run > 60) {
            output << "    FAIL: their form stayed wrong for "
                   << other.worst_form_disagree_run << " frames in a row -- "
                   << other.worst_form_context << '\n';
        }
        if (other.worst_position_gap > 8.0
            || !std::isfinite(other.worst_position_gap)) {
            output << "    FAIL: their position drifted far from the authority's\n";
        }
    }
    if (!result.any_remote) {
        output << "    no other player was ever spawned in this scene -- "
               << "nothing to compare\n";
    }
    output << "    items: " << items_now_ << " on the map now, "
           << (item_samples_ > 0
                   ? static_cast<double>(item_total_) / item_samples_ : 0.0)
           << " on average, " << items_picked_up_ << " taken or expired\n";
    for (const auto& [second, board] : boards_) {
        static_cast<void>(second);
        output << board << '\n';
    }
    output << "    phases seen:";
    for (const auto& [phase, count] : phase_frames_) {
        output << ' ' << phase_name(phase) << '=' << count;
    }
    output << "\n    failures: " << result.failures << '\n';
}

void NetFeatureCheck::sample_scoreboard(
    int server_second,
    const std::array<std::string, NetConfig::SlotCapacity>& names,
    const std::array<bool, NetConfig::SlotCapacity>& present,
    const std::array<std::int32_t, NetConfig::SlotCapacity>& kills,
    const std::array<std::int32_t, NetConfig::SlotCapacity>& deaths,
    const std::array<std::int32_t, NetConfig::SlotCapacity>& points) {
    std::ostringstream board;
    board << "    scoreboard at t=" << server_second << "s:";
    for (std::size_t index = 0; index < NetConfig::SlotCapacity; ++index) {
        if (!present[index] || records_[index].spawned_frames == 0) {
            continue;
        }
        board << " [" << index << "] " << names[index] << ' ' << kills[index]
              << 'k' << '/' << deaths[index] << 'd' << '/' << points[index]
              << 'p';
    }
    boards_[server_second] = board.str();
}

} // namespace fruityprime::net
