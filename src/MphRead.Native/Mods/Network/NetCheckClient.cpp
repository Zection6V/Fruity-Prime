#include "Mods/Network/net_check_client.hpp"

#include "Mods/Network/match_client.hpp"
#include "Mods/Network/net_session.hpp"
#include "Mods/Network/net_test_script.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace fruityprime::net {
namespace {

constexpr float TickSeconds = 1.0F / 60.0F;
constexpr auto PollSleep = std::chrono::milliseconds(1);
constexpr auto Tick = std::chrono::milliseconds(16);
constexpr int ChatFrameA = 300;
constexpr int ChatFrameB = 1500;

[[nodiscard]] constexpr std::uint32_t bits(IntentButtons value) noexcept {
    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] constexpr bool has_button(IntentButtons value,
                                         IntentButtons button) noexcept {
    return (bits(value) & bits(button)) != 0;
}

[[nodiscard]] constexpr IntentButtons add_button(IntentButtons value,
                                                 IntentButtons button) noexcept {
    return static_cast<IntentButtons>(bits(value) | bits(button));
}

[[nodiscard]] bool lays_bombs(std::uint8_t hunter) noexcept {
    const auto value = static_cast<metadata::Hunter>(hunter);
    return value == metadata::Hunter::Samus
        || value == metadata::Hunter::Kanden
        || value == metadata::Hunter::Sylux;
}

[[nodiscard]] float length(Vec3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y
                     + value.z * value.z);
}

[[nodiscard]] Vec3 normalize(Vec3 value) noexcept {
    const float magnitude = length(value);
    if (!std::isfinite(magnitude) || magnitude < 0.0001F) {
        return {0.0F, 0.0F, 1.0F};
    }
    return {value.x / magnitude, value.y / magnitude, value.z / magnitude};
}

[[nodiscard]] Vec3 turn_aim(int frame, TestPhase phase) noexcept {
    // The managed tour turns the camera through a repeatable arc. The
    // renderer-free client uses the same idea: every observer receives an
    // actual changing aim vector, rather than a constant packet fixture.
    constexpr float Pi = 3.14159265358979323846F;
    constexpr float DegreesPerFrame = 2.5F;
    const float phase_frame = static_cast<float>(frame % 120);
    const float degrees = phase == TestPhase::Turn
        ? phase_frame * DegreesPerFrame
        : static_cast<float>(frame % 24) * 2.0F;
    const float radians = degrees * Pi / 180.0F;
    return normalize({std::sin(radians), 0.0F, std::cos(radians)});
}

[[nodiscard]] IntentButtons scripted_buttons(TestPhase phase,
                                              int frame) noexcept {
    IntentButtons result = IntentButtons::InPlayState;
    switch (phase) {
    case TestPhase::Idle:
        break;
    case TestPhase::Walk:
        result = add_button(result, IntentButtons::MoveUp);
        break;
    case TestPhase::Jump:
        result = add_button(result, IntentButtons::MoveUp);
        if (frame % 30 < 3) {
            result = add_button(result, IntentButtons::Jump);
        }
        break;
    case TestPhase::Turn:
        break;
    case TestPhase::Shoot:
    case TestPhase::Charge:
    case TestPhase::Afflict:
        result = add_button(result, IntentButtons::Shoot);
        break;
    case TestPhase::SwitchWeapons:
        if (frame % 18 < 3) {
            result = add_button(result, IntentButtons::NextWeapon);
        }
        break;
    case TestPhase::MorphA:
    case TestPhase::MorphB:
        result = add_button(result, IntentButtons::Morph);
        result = add_button(result, IntentButtons::AltFormState);
        break;
    case TestPhase::AltAttackA:
    case TestPhase::AltAttackB:
        result = add_button(result, IntentButtons::AltFormState);
        if (frame % 24 < 4) {
            result = add_button(result, IntentButtons::AltAttack);
        }
        break;
    case TestPhase::Unmorph:
        result = add_button(result, IntentButtons::Morph);
        break;
    case TestPhase::Zoom:
        result = add_button(result, IntentButtons::Zoom);
        result = add_button(result, IntentButtons::ZoomedState);
        break;
    case TestPhase::Duel:
        result = add_button(result, IntentButtons::Shoot);
        result = add_button(result, IntentButtons::MoveUp);
        break;
    }
    return result;
}

[[nodiscard]] std::array<std::string, NetConfig::SlotCapacity> names_from(
    const NetSession& session, std::string_view local_name) {
    std::array<std::string, NetConfig::SlotCapacity> names{};
    for (std::size_t index = 0; index < names.size(); ++index) {
        names[index] = "slot" + std::to_string(index);
    }
    if (session.roster_valid()) {
        const RosterPacket& roster = session.roster();
        const std::size_t count = std::min<std::size_t>(
            roster.count, NetConfig::SlotCapacity);
        for (std::size_t entry = 0; entry < count; ++entry) {
            const std::size_t slot = roster.slots[entry];
            if (slot < names.size() && !roster.names[entry].empty()) {
                names[slot] = roster.names[entry];
            }
        }
    }
    if (session.local_slot() >= 0
        && session.local_slot() < static_cast<int>(names.size())) {
        names[static_cast<std::size_t>(session.local_slot())] =
            std::string(local_name);
    }
    return names;
}

[[nodiscard]] double server_elapsed(const NetSession& session,
                                    int frame) noexcept {
    if (const auto match = session.server_match(); match.has_value()
        && std::isfinite(match->time_elapsed)
        && match->time_elapsed >= 0.0F) {
        return match->time_elapsed;
    }
    return static_cast<double>(frame) * TickSeconds;
}

void copy_state_observation(const PlayerState& state,
                            NetFeatureCheck::PlayerObservation& observation,
                            std::uint8_t hunter) noexcept {
    observation.present = true;
    observation.active = (state.flags & PlayerState::FlagActive) != 0;
    observation.spawned = (state.flags & PlayerState::FlagSpawned) != 0;
    observation.alt_form = (state.flags & PlayerState::FlagAltForm) != 0;
    observation.zoomed = (state.flags & PlayerState::FlagZoomed) != 0;
    observation.frozen = (state.flags & PlayerState::FlagFrozen) != 0;
    observation.spectating = (state.flags & PlayerState::FlagSpectating) != 0;
    observation.hunter = hunter;
    observation.current_weapon = state.current_weapon;
    observation.health = state.health;
    observation.position = state.position;
    observation.facing = normalize(state.facing);
}

void copy_intent_observation(const IntentState& intent,
                             NetFeatureCheck::PlayerObservation& observation,
                             std::uint8_t hunter) noexcept {
    observation.present = true;
    observation.active = true;
    observation.spawned = has_button(intent.buttons,
                                     IntentButtons::InPlayState);
    observation.alt_form = has_button(intent.buttons,
                                      IntentButtons::AltFormState);
    observation.zoomed = has_button(intent.buttons,
                                    IntentButtons::ZoomedState);
    observation.spectating = has_button(intent.buttons,
                                        IntentButtons::SpectatingState);
    observation.hunter = hunter;
    observation.current_weapon = intent.weapon_select;
    observation.health = 100;
    observation.position = intent.position;
    observation.facing = normalize(intent.aim);
}

void fill_scoreboard(const NetFeatureCheck::Frame& frame,
                     std::array<bool, NetConfig::SlotCapacity>& present,
                     std::array<std::int32_t, NetConfig::SlotCapacity>& kills,
                     std::array<std::int32_t, NetConfig::SlotCapacity>& deaths,
                     std::array<std::int32_t, NetConfig::SlotCapacity>& points) noexcept {
    present.fill(false);
    kills.fill(0);
    deaths.fill(0);
    points.fill(0);
    for (std::size_t index = 0; index < frame.players.size(); ++index) {
        const auto& player = frame.players[index];
        if (!player.present || !player.active) {
            continue;
        }
        present[index] = true;
        if (frame.remote_state_valid[index]) {
            const PlayerState& state = frame.remote_states[index];
            kills[index] = state.kills;
            deaths[index] = state.deaths;
            points[index] = state.points;
        }
    }
}

[[nodiscard]] bool saw_moving_remote(
    const NetFeatureCheck::ReportResult& report,
    int local_slot,
    const std::array<NetFeatureCheck::Record, NetConfig::SlotCapacity>& records) noexcept {
    for (const auto& pair : report.pairs) {
        if (static_cast<int>(pair.slot) == local_slot) {
            continue;
        }
        const auto& record = records[pair.slot];
        if (record.spawned_frames >= 30 && record.moved_frames > 5
            && record.travelled > 2.0) {
            return true;
        }
    }
    return false;
}

} // namespace

NetCheckClientReport run_net_check_client(
    Endpoint server, const NetCheckClientOptions& options,
    std::ostream& output) {
    NetCheckClientReport result;
    if (options.seconds < 0) {
        result.error = "netcheck seconds must not be negative";
        output << "[netcheck] " << result.error << '\n';
        return result;
    }

    NetSession session;
    if (!session.start_client(server, options.network)) {
        result.error = "could not join: " + std::string(session.last_error());
        output << "[netcheck] " << result.error << '\n';
        return result;
    }
    result.connected = true;
    session.set_player(options.hunter, options.name);
    session.identify();
    result.local_slot = session.local_slot();

    NetTestScript script;
    script.set_enabled(true);
    script.set_network_active(true);
    script.set_local_slot(result.local_slot);
    NetFeatureCheck features;
    SpectateSchedule schedule(options.spectate_at_seconds,
                              options.rejoin_at_seconds);

    std::array<std::uint32_t, NetConfig::SlotCapacity>
        last_remote_intent_frame{};
    std::array<int, NetConfig::SlotCapacity> fired_totals{};
    std::array<std::uint8_t, NetConfig::SlotCapacity> hunters{};
    std::array<bool, NetConfig::SlotCapacity> previous_remote_alt_attack{};

    Vec3 local_position{-4.0F, 0.0F, 0.0F};
    std::uint8_t local_weapon = 0;
    bool local_alt = false;
    bool local_zoom = false;
    bool previous_local_alt_attack = false;
    int next_scoreboard_sample = -1;
    int frame = 0;
    auto started = std::chrono::steady_clock::now();
    auto next_tick = started;
    const auto deadline = started + std::chrono::seconds(options.seconds);

    output << "[netcheck] " << options.name << " joined slot "
           << result.local_slot << '\n';

    while (std::chrono::steady_clock::now() < deadline
           || (options.seconds == 0 && frame == 0)) {
        const auto now = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>(
            now - started).count();
        for (const ReceivedPacket& packet : session.update(elapsed)) {
            if (options.packet_sink) {
                options.packet_sink(static_cast<std::uint32_t>(frame),
                                   packet.data);
            }
            switch (packet.type()) {
            case PacketType::Snapshot: {
                const auto snapshot = SnapshotPacket::decode(packet.payload());
                if (!snapshot) {
                    break;
                }
                ++result.snapshots_received;
                for (const PlayerState& player : snapshot->players) {
                    if (player.slot_index == result.local_slot
                        || player.slot_index >= result.remote_spectating_frames.size()) {
                        continue;
                    }
                    if ((player.flags & PlayerState::FlagSpectating) != 0) {
                        ++result.remote_spectating_frames[player.slot_index];
                    }
                }
                break;
            }
            case PacketType::Roster: {
                if (const auto roster = RosterPacket::decode(packet.payload())) {
                    ++result.rosters_received;
                    for (std::size_t index = 0; index < roster->count
                         && index < roster->slots.size(); ++index) {
                        if (roster->slots[index] < hunters.size()) {
                            hunters[roster->slots[index]] = roster->hunters[index];
                        }
                    }
                }
                break;
            }
            case PacketType::MatchState:
            case PacketType::MapChange:
                if (MatchStatePacket::decode(packet.payload())) {
                    ++result.match_states_received;
                }
                break;
            case PacketType::Chat:
                if (ChatPacket::decode(packet.payload())) {
                    ++result.chats_received;
                }
                break;
            default:
                break;
            }
        }

        if (now >= next_tick) {
            ++frame;
            const double match_time = server_elapsed(session, frame);
            const TestPhase phase = script.phase(match_time);
            schedule.update(static_cast<std::uint32_t>(frame));
            IntentButtons buttons = scripted_buttons(phase, frame);

            local_alt = phase == TestPhase::MorphA
                || phase == TestPhase::AltAttackA
                || phase == TestPhase::MorphB
                || phase == TestPhase::AltAttackB;
            local_zoom = phase == TestPhase::Zoom;
            if (phase == TestPhase::Walk || phase == TestPhase::Jump
                || phase == TestPhase::Duel) {
                local_position.z += phase == TestPhase::Duel ? 0.16F : 0.12F;
            }
            if (phase == TestPhase::SwitchWeapons && frame % 18 == 0) {
                local_weapon = static_cast<std::uint8_t>((local_weapon + 1) % 9);
            }
            const Vec3 aim = turn_aim(frame, phase);
            const bool alt_attack = has_button(buttons,
                                               IntentButtons::AltAttack);
            const bool alt_attack_pressed = alt_attack
                && !previous_local_alt_attack;
            previous_local_alt_attack = alt_attack;
            const bool shooting = has_button(buttons, IntentButtons::Shoot);
            const std::size_t local_index = static_cast<std::size_t>(
                std::clamp(result.local_slot, 0,
                           static_cast<int>(NetConfig::SlotCapacity - 1)));
            if (shooting && frame % 6 == 0) {
                ++fired_totals[local_index];
            }
            if (schedule.spectating()) {
                buttons = schedule.buttons(buttons);
            }

            IntentState intent;
            intent.frame = static_cast<std::uint32_t>(frame);
            intent.buttons = buttons;
            intent.aim = aim;
            intent.weapon_select = local_weapon;
            intent.position = local_position;
            intent.ammo_ua = 599;
            intent.ammo_missiles = 599;
            session.send_intent(intent);
            ++result.intents_sent;
            if (schedule.spectating()) {
                ++result.spectating_frames;
            }
            if (options.packet_sink) {
                const auto encoded = intent.encode();
                std::array<std::uint8_t, 2 + IntentState::Size> local_packet{};
                local_packet[0] = static_cast<std::uint8_t>(
                    PacketType::SlotIntent);
                local_packet[1] = static_cast<std::uint8_t>(local_index);
                std::copy(encoded.begin(), encoded.end(), local_packet.begin() + 2);
                options.packet_sink(static_cast<std::uint32_t>(frame),
                                    local_packet);
            }

            NetFeatureCheck::Frame observation;
            observation.local_slot = result.local_slot;
            observation.net_frame = static_cast<std::uint32_t>(frame);
            observation.phase = phase;
            observation.authority = session.is_authority();
            observation.remote_states = session.remote_states();
            observation.remote_state_valid = session.remote_state_valid();
            observation.beams.fill(0);
            observation.bombs.fill(0);
            observation.halfturrets.fill(0);
            observation.fired_totals = fired_totals;
            observation.beams[local_index] = shooting ? 1 : 0;
            observation.bombs[local_index] = alt_attack_pressed
                && lays_bombs(options.hunter) ? 1 : 0;
            observation.halfturrets[local_index] = alt_attack_pressed
                && options.hunter == static_cast<std::uint8_t>(
                    metadata::Hunter::Weavel) ? 1 : 0;
            auto& local = observation.players[local_index];
            local.present = true;
            local.active = true;
            local.spawned = true;
            local.hunter = options.hunter;
            local.alt_form = local_alt;
            local.zoomed = local_zoom;
            local.spectating = schedule.spectating();
            local.alt_attack_pressed = alt_attack_pressed;
            local.current_weapon = local_weapon;
            local.health = 100;
            local.position = local_position;
            local.facing = aim;

            if (observation.remote_state_valid[local_index]) {
                const PlayerState& state = observation.remote_states[local_index];
                local.active = (state.flags & PlayerState::FlagActive) != 0;
                local.spawned = (state.flags & PlayerState::FlagSpawned) != 0;
                local.health = state.health;
                // The scripted flags are the native equivalent of the local
                // Scene state; the snapshot supplies health/transform.
                local.position = state.position;
                local.facing = normalize(state.facing);
            }

            const auto& remote_intents = session.remote_intents();
            const auto& remote_valid = session.remote_intent_valid();
            const auto& states = session.remote_states();
            const auto& state_valid = session.remote_state_valid();
            std::array<bool, NetConfig::SlotCapacity>
                remote_alt_attack_pressed{};
            for (std::size_t index = 0; index < observation.players.size(); ++index) {
                if (index == local_index) {
                    continue;
                }
                if (remote_valid[index]) {
                    const IntentState& remote = remote_intents[index];
                    if (has_button(remote.buttons, IntentButtons::Shoot)) {
                        observation.beams[index] = 1;
                    }
                    const bool remote_alt_attack = has_button(
                        remote.buttons, IntentButtons::AltAttack);
                    remote_alt_attack_pressed[index] = remote_alt_attack
                        && !previous_remote_alt_attack[index];
                    previous_remote_alt_attack[index] = remote_alt_attack;
                    if (remote_alt_attack_pressed[index]
                        && lays_bombs(hunters[index])) {
                        observation.bombs[index] = 1;
                    }
                    if (remote_alt_attack_pressed[index]
                        && hunters[index] == static_cast<std::uint8_t>(
                            metadata::Hunter::Weavel)) {
                        observation.halfturrets[index] = 1;
                    }
                    if (remote.frame != last_remote_intent_frame[index]) {
                        if (has_button(remote.buttons, IntentButtons::Shoot)) {
                            ++fired_totals[index];
                        }
                        last_remote_intent_frame[index] = remote.frame;
                    }
                }
                if (state_valid[index]) {
                    copy_state_observation(states[index], observation.players[index],
                                           hunters[index]);
                    if (remote_valid[index]) {
                        observation.players[index].alt_attack_pressed =
                            remote_alt_attack_pressed[index];
                    }
                } else if (remote_valid[index]) {
                    const IntentState& remote = remote_intents[index];
                    copy_intent_observation(remote, observation.players[index],
                                            hunters[index]);
                    observation.players[index].alt_attack_pressed =
                        remote_alt_attack_pressed[index];
                }
            }
            observation.fired_totals = fired_totals;
            features.observe(observation);

            if (options.send_chat
                && (frame == ChatFrameA || frame == ChatFrameB)) {
                session.send_chat("hello from " + options.name
                                  + " at frame " + std::to_string(frame));
            }

            if (next_scoreboard_sample < 0) {
                next_scoreboard_sample = static_cast<int>(
                    std::ceil((match_time + 20.0) / 30.0) * 30.0);
            } else if (match_time >= next_scoreboard_sample) {
                const auto names = names_from(session, options.name);
                std::array<bool, NetConfig::SlotCapacity> present{};
                std::array<std::int32_t, NetConfig::SlotCapacity> kills{};
                std::array<std::int32_t, NetConfig::SlotCapacity> deaths{};
                std::array<std::int32_t, NetConfig::SlotCapacity> points{};
                fill_scoreboard(observation, present, kills, deaths, points);
                features.sample_scoreboard(next_scoreboard_sample, names,
                                           present, kills, deaths, points);
                next_scoreboard_sample += 30;
            }

            next_tick += Tick;
            if (next_tick < now) {
                next_tick = now + Tick;
            }
        }
        std::this_thread::sleep_for(PollSleep);
    }

    result.frames = frame;
    result.elapsed_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    result.spectate_started_frame = schedule.started_frame();
    result.rejoined_frame = schedule.rejoined_frame();
    const auto names = names_from(session, options.name);
    result.feature_report = features.report(names);
    result.feature_failures = result.feature_report.failures;
    result.features_passed = result.feature_failures == 0;
    result.passed = result.feature_report.any_remote
        && saw_moving_remote(result.feature_report, result.local_slot,
                             features.records());

    output << "  ran " << result.frames << " frame(s) in "
           << result.elapsed_seconds << " s\n"
           << "  slot " << result.local_slot
           << ", authority=" << (session.is_authority() ? "true" : "false")
           << ", packets: snapshots=" << result.snapshots_received
           << " rosters=" << result.rosters_received
           << " match_states=" << result.match_states_received
           << " intents=" << result.intents_sent
           << " chats=" << result.chats_received
           << " late_snapshots=" << session.snapshots_out_of_order()
           << " intents_late=" << session.intents_out_of_order()
           << " dropped=unavailable"
           << '\n';
    if (const std::string description = options.network.describe();
        !description.empty()) {
        output << "  SIMULATED LINE: " << description
               << " -- these numbers are a reproduction, not a real-line measurement\n";
    }
    output << "  spectating: " << result.spectating_frames
           << " frame(s), started " << result.spectate_started_frame
           << ", rejoined " << result.rejoined_frame << '\n';
    for (std::size_t index = 0; index < result.remote_spectating_frames.size(); ++index) {
        if (result.remote_spectating_frames[index] > 0) {
            output << "  slot " << index << " was spectating on "
                   << result.remote_spectating_frames[index]
                   << " received frame(s)\n";
        }
    }
    features.print_report(output, names);
    output << '\n' << (result.passed && result.features_passed
        ? "  RESULT: PASS"
        : "  RESULT: FAIL -- feature or moving-remote coverage did not cross")
           << '\n';

    session.stop();
    return result;
}

} // namespace fruityprime::net
