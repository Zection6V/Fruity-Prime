#include "Mods/Network/match_client.hpp"
#include "Mods/Network/net_session.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace fruityprime::net {

SpectateSchedule::SpectateSchedule(double spectate_at_seconds,
                                   double rejoin_at_seconds) noexcept
    : spectate_at_seconds_(spectate_at_seconds),
      rejoin_at_seconds_(rejoin_at_seconds) {}

bool SpectateSchedule::reached(double seconds, std::uint32_t frame) noexcept {
    return seconds >= 0.0
        && static_cast<double>(frame) >= seconds * 60.0;
}

void SpectateSchedule::update(std::uint32_t frame) noexcept {
    if (!spectating_ && started_frame_ < 0
        && reached(spectate_at_seconds_, frame)) {
        spectating_ = true;
        started_frame_ = static_cast<int>(frame);
    }
    if (spectating_ && rejoined_frame_ < 0
        && reached(rejoin_at_seconds_, frame)) {
        spectating_ = false;
        rejoined_frame_ = static_cast<int>(frame);
    }
}

IntentButtons SpectateSchedule::buttons(IntentButtons base) const noexcept {
    constexpr std::uint32_t SpectatorMetadata =
        static_cast<std::uint32_t>(IntentButtons::ZoomedState)
        | static_cast<std::uint32_t>(IntentButtons::AltFormState)
        | static_cast<std::uint32_t>(IntentButtons::InPlayState);
    std::uint32_t value = static_cast<std::uint32_t>(base);
    value &= ~static_cast<std::uint32_t>(IntentButtons::SpectatingState);
    if (spectating_) {
        value &= SpectatorMetadata;
        value |= static_cast<std::uint32_t>(IntentButtons::SpectatingState);
    }
    return static_cast<IntentButtons>(value);
}

MatchClientStats run_match_client(Endpoint server,
                                  const MatchClientOptions& options,
                                  std::chrono::milliseconds duration,
                                  std::chrono::milliseconds tick) {
    if (duration.count() < 0 || tick.count() <= 0) {
        throw std::invalid_argument("match client duration and tick must be positive");
    }
    NetSession session;
    if (!session.start_client(server, options.network)) {
        throw std::runtime_error("match client connect failed: "
                                 + std::string(session.last_error()));
    }
    MatchClientStats stats;
    stats.local_slot = session.local_slot();
    session.set_player(options.hunter, options.name);
    session.identify();

    const auto started = std::chrono::steady_clock::now();
    const auto deadline = started + duration;
    auto next_tick = started;
    std::uint32_t frame = 1;
    SpectateSchedule schedule(options.spectate_at_seconds,
                              options.rejoin_at_seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto network_now = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>(
            network_now - started).count();
        for (const ReceivedPacket& packet : session.update(elapsed)) {
            switch (packet.type()) {
            case PacketType::Snapshot: {
                const auto snapshot = SnapshotPacket::decode(packet.payload());
                if (snapshot) {
                    ++stats.snapshots_received;
                    stats.last_snapshot_frame = snapshot->header.frame;
                    for (const auto& player : snapshot->players) {
                        if (player.slot_index != session.local_slot()
                            && player.slot_index < stats.remote_spectating_frames.size()
                            && (player.flags & PlayerState::FlagSpectating) != 0) {
                            ++stats.remote_spectating_frames[player.slot_index];
                        }
                    }
                }
                break;
            }
            case PacketType::Roster: {
                const auto roster = RosterPacket::decode(packet.payload());
                if (roster) {
                    ++stats.rosters_received;
                    stats.last_roster_count = roster->count;
                }
                break;
            }
            case PacketType::MatchState: {
                const auto state = MatchStatePacket::decode(packet.payload());
                if (state) {
                    ++stats.match_states_received;
                    stats.last_match_state = *state;
                }
                break;
            }
            case PacketType::Chat:
                if (ChatPacket::decode(packet.payload())) {
                    ++stats.chats_received;
                }
                break;
            default:
                break;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= next_tick) {
            IntentState intent;
            intent.frame = frame;
            schedule.update(frame);
            intent.buttons = schedule.buttons(options.buttons);
            intent.aim = options.aim;
            intent.position = options.position;
            intent.ammo_ua = options.ammo_ua;
            intent.ammo_missiles = options.ammo_missiles;
            session.send_intent(intent);
            ++stats.intents_sent;
            if (schedule.spectating()) {
                ++stats.spectating_frames;
            }
            ++frame;
            next_tick += tick;
            if (next_tick < now) {
                next_tick = now + tick;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    stats.spectate_started_frame = schedule.started_frame();
    stats.rejoined_frame = schedule.rejoined_frame();
    session.stop();
    return stats;
}

} // namespace fruityprime::net
