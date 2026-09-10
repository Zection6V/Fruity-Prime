#include "Mods/Network/net_player_bridge.hpp"

#include "Entities/gameplay.hpp"
#include "Mods/Network/net_damage.hpp"

#include <algorithm>
#include <cmath>

namespace fruityprime::net {
namespace {

constexpr std::uint32_t PressedMask =
    static_cast<std::uint32_t>(IntentButtons::MoveLeft)
    | static_cast<std::uint32_t>(IntentButtons::MoveRight)
    | static_cast<std::uint32_t>(IntentButtons::MoveUp)
    | static_cast<std::uint32_t>(IntentButtons::MoveDown)
    | static_cast<std::uint32_t>(IntentButtons::Shoot)
    | static_cast<std::uint32_t>(IntentButtons::Zoom)
    | static_cast<std::uint32_t>(IntentButtons::Jump)
    | static_cast<std::uint32_t>(IntentButtons::Morph)
    | static_cast<std::uint32_t>(IntentButtons::Boost)
    | static_cast<std::uint32_t>(IntentButtons::AltAttack)
    | static_cast<std::uint32_t>(IntentButtons::ScanVisor)
    | static_cast<std::uint32_t>(IntentButtons::NextWeapon)
    | static_cast<std::uint32_t>(IntentButtons::PrevWeapon)
    | static_cast<std::uint32_t>(IntentButtons::RollLeft)
    | static_cast<std::uint32_t>(IntentButtons::RollRight)
    | static_cast<std::uint32_t>(IntentButtons::RollUp)
    | static_cast<std::uint32_t>(IntentButtons::RollDown);

[[nodiscard]] Vec3 subtract(Vec3 left, Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] Vec3 divide(Vec3 value, float divisor) noexcept {
    return {value.x / divisor, value.y / divisor, value.z / divisor};
}

[[nodiscard]] float length_squared(Vec3 value) noexcept {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

[[nodiscard]] float length(Vec3 value) noexcept {
    return std::sqrt(length_squared(value));
}

[[nodiscard]] bool zero(Vec3 value) noexcept {
    return value.x == 0.0F && value.y == 0.0F && value.z == 0.0F;
}

[[nodiscard]] bool in_play(const PlayerState& player) noexcept {
    return (player.flags & PlayerState::FlagSpawned) != 0
        && player.health > 0;
}

} // namespace

bool NetPlayerBridge::sane(Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y)
        && std::isfinite(value.z) && std::abs(value.x) < PositionLimit
        && std::abs(value.y) < PositionLimit
        && std::abs(value.z) < PositionLimit;
}

bool NetPlayerBridge::has_button(IntentButtons buttons,
                                 IntentButtons button) noexcept {
    return (static_cast<std::uint32_t>(buttons)
            & static_cast<std::uint32_t>(button)) != 0;
}

void NetPlayerBridge::reset() noexcept {
    last_press_frame_.fill(0);
    press_seen_.fill(false);
    spawn_intent_frame_.fill(0);
    was_in_play_.fill(false);
    stale_frames_.fill(0);
    last_report_position_.fill({});
    last_report_frame_.fill(0);
    report_seen_.fill(false);
    authority_spawned_.fill(false);
    latest_intent_.fill({});
    latest_intent_seen_.fill(false);
    state_valid_.fill(false);
    intent_valid_.fill(false);
    aim_.reset();
    local_previous_buttons_ = IntentButtons::None;
    local_press_history_.fill(0);
    metrics_ = {};
}

void NetPlayerBridge::note_room_changed() noexcept {
    authority_spawned_.fill(false);
    report_seen_.fill(false);
    spawn_intent_frame_.fill(0);
    was_in_play_.fill(false);
    stale_frames_.fill(0);
    latest_intent_seen_.fill(false);
    latest_intent_.fill({});
    state_valid_.fill(false);
    intent_valid_.fill(false);
    aim_.note_room_changed();
}

void NetPlayerBridge::forget_slot(std::uint8_t slot) noexcept {
    if (slot >= Slots) {
        return;
    }
    last_press_frame_[slot] = 0;
    press_seen_[slot] = false;
    spawn_intent_frame_[slot] = 0;
    was_in_play_[slot] = false;
    stale_frames_[slot] = 0;
    last_report_position_[slot] = {};
    last_report_frame_[slot] = 0;
    report_seen_[slot] = false;
    authority_spawned_[slot] = false;
    latest_intent_[slot] = {};
    latest_intent_seen_[slot] = false;
    state_valid_[slot] = false;
    intent_valid_[slot] = false;
    aim_.forget_slot(slot);
}

IntentState NetPlayerBridge::capture_intent(
    const gameplay::Session& session, std::uint8_t slot,
    std::uint32_t frame, IntentButtons buttons, Vec3 aim,
    std::uint8_t weapon_select) noexcept {
    IntentState intent;
    intent.frame = frame;
    intent.buttons = buttons;
    aim_.remember_aim(slot, aim);
    intent.aim = aim_.gun_vector(slot);
    intent.weapon_select = weapon_select;

    // The managed implementation records rising edges every simulation frame,
    // then copies the short history into a packet. The native host currently
    // emits one packet per rendered frame, so deriving the edge here keeps the
    // same wire contract and still protects a future slower sender.
    const std::uint32_t current =
        static_cast<std::uint32_t>(buttons) & PressedMask;
    const std::uint32_t previous =
        static_cast<std::uint32_t>(local_previous_buttons_) & PressedMask;
    const std::uint32_t pressed = current & ~previous;
    for (std::size_t i = local_press_history_.size() - 1; i > 0; --i) {
        local_press_history_[i] = local_press_history_[i - 1];
    }
    local_press_history_[0] = pressed;
    intent.presses = local_press_history_;
    local_previous_buttons_ = static_cast<IntentButtons>(current);

    if (slot < Slots && session.has_player(slot)) {
        const auto& player = session.player(slot);
        intent.position = player.position;
        const auto& inventory = session.inventory(slot);
        intent.ammo_ua = inventory.ammo[0];
        intent.ammo_missiles = inventory.ammo[1];
        // The caller normally derives this from the input state. Keep the
        // owner's actual weapon in the packet when no direct selection was
        // requested, matching CaptureIntent's CurrentWeapon field.
        if (intent.weapon_select == 0xff) {
            intent.weapon_select = player.current_weapon;
        }
    }
    return intent;
}

IntentButtons NetPlayerBridge::missed_presses(
    std::uint8_t slot, const IntentState& intent) noexcept {
    if (slot >= Slots) {
        return IntentButtons::None;
    }
    if (!press_seen_[slot]) {
        // Do not replay history that predates this peer becoming visible to
        // the authority. This is the same first-packet rule as the managed
        // implementation and prevents a reconnect from opening with a burst.
        press_seen_[slot] = true;
        last_press_frame_[slot] = intent.frame;
        return IntentButtons::None;
    }

    std::uint32_t missed = 0;
    for (std::size_t i = IntentState::PressHistory; i-- > 0;) {
        if (intent.frame < i) {
            continue;
        }
        const std::uint32_t frame = intent.frame
            - static_cast<std::uint32_t>(i);
        if (frame <= last_press_frame_[slot]) {
            continue;
        }
        missed |= intent.presses[i] & PressedMask;
    }
    last_press_frame_[slot] = std::max(last_press_frame_[slot], intent.frame);
    return static_cast<IntentButtons>(missed);
}

bool NetPlayerBridge::apply_intent(gameplay::Session& session,
                                   std::uint8_t slot,
                                   const IntentState& intent,
                                   bool sync_reported_position) noexcept {
    if (slot >= Slots || !session.has_player(slot) || !sane(intent.aim)) {
        ++metrics_.rejected_updates;
        return false;
    }

    const IntentButtons missed = missed_presses(slot, intent);
    const auto buttons = static_cast<IntentButtons>(
        static_cast<std::uint32_t>(intent.buttons)
        | static_cast<std::uint32_t>(missed));
    session.set_input(slot, gameplay::Input{buttons, intent.aim,
                                             intent.weapon_select});
    static_cast<void>(aim_.set_aim(session, slot, intent.aim));
    if (intent.weapon_select != 0xff) {
        static_cast<void>(aim_.set_weapon(session, slot,
                                          intent.weapon_select));
    }
    aim_.set_ammo(session, slot, intent.ammo_ua, intent.ammo_missiles);
    static_cast<void>(aim_.set_zoom(session, slot, has_button(
        intent.buttons, IntentButtons::ZoomedState)));
    aim_.set_spectating(session, slot, has_button(
        intent.buttons, IntentButtons::SpectatingState));
    if ((static_cast<std::uint32_t>(buttons) & PressedMask) != 0) {
        aim_.note_input(session, slot);
    }
    latest_intent_[slot] = intent;
    latest_intent_seen_[slot] = true;
    intent_valid_[slot] = true;
    if (sync_reported_position) {
        static_cast<void>(apply_reported_position(session, slot, intent));
    }
    return true;
}

bool NetPlayerBridge::stale_since_spawn(
    const gameplay::Session& session, std::uint8_t slot,
    const IntentState& intent) noexcept {
    if (slot >= Slots || !session.has_player(slot)) {
        return false;
    }
    const bool in_play_now = in_play(session.player(slot));
    if (in_play_now && !was_in_play_[slot]) {
        spawn_intent_frame_[slot] = intent.frame;
        stale_frames_[slot] = 0;
    }
    was_in_play_[slot] = in_play_now;
    if (!in_play_now) {
        stale_frames_[slot] = 0;
        return false;
    }
    if (!has_button(intent.buttons, IntentButtons::InPlayState)) {
        return true;
    }
    if (intent.frame > spawn_intent_frame_[slot]) {
        stale_frames_[slot] = 0;
        return false;
    }
    if (++stale_frames_[slot] <= StaleAfterSpawnFrames) {
        return true;
    }
    spawn_intent_frame_[slot] = intent.frame;
    stale_frames_[slot] = 0;
    return false;
}

void NetPlayerBridge::note_reported_velocity(
    gameplay::Session& session, std::uint8_t slot, Vec3 reported,
    std::uint32_t frame) noexcept {
    if (slot >= Slots || !session.has_player(slot)) {
        return;
    }
    if (report_seen_[slot] && frame > last_report_frame_[slot]) {
        const std::uint32_t elapsed = std::min<std::uint32_t>(
            frame - last_report_frame_[slot], 8);
        const Vec3 travelled = subtract(reported, last_report_position_[slot]);
        const float distance = length(travelled);
        if (!sane(travelled) || distance > SnapDistance || elapsed == 0) {
            session.mutable_player(slot).speed = {};
        } else {
            Vec3 speed = divide(travelled, static_cast<float>(elapsed));
            const float magnitude = length(speed);
            if (magnitude > MaxReportedSpeed && magnitude > 0.0F) {
                const float scale = MaxReportedSpeed / magnitude;
                speed = {speed.x * scale, speed.y * scale, speed.z * scale};
            }
            session.mutable_player(slot).speed = speed;
        }
    }
    if (!report_seen_[slot] || frame > last_report_frame_[slot]) {
        report_seen_[slot] = true;
        last_report_frame_[slot] = frame;
        last_report_position_[slot] = reported;
    }
}

bool NetPlayerBridge::apply_reported_position(
    gameplay::Session& session, std::uint8_t slot,
    const IntentState& intent) noexcept {
    if (!sane(intent.position)) {
        ++metrics_.rejected_updates;
        return false;
    }
    if (zero(intent.position) || stale_since_spawn(session, slot, intent)) {
        return false;
    }

    const Vec3 reported = intent.position;
    note_reported_velocity(session, slot, reported, intent.frame);
    const Vec3 delta = subtract(reported, session.player(slot).position);
    const float distance = length(delta);
    if (distance > SnapDistance) {
        ++metrics_.snaps;
        metrics_.worst_snap = std::max(metrics_.worst_snap, distance);
    }
    session.mutable_player(slot).position = reported;
    return true;
}

bool NetPlayerBridge::apply_snapshot(gameplay::Session& session,
                                     const SnapshotPacket& snapshot,
                                     std::uint8_t local_slot,
                                     bool settling,
                                     DamageBridge& damage) noexcept {
    // Validate and de-duplicate before handing state to Session. The managed
    // path rejects non-finite transforms and never lets a duplicate slot make
    // the final state depend on packet ordering.
    SnapshotPacket sanitized;
    sanitized.header = snapshot.header;
    state_valid_.fill(false);
    std::array<bool, Slots> seen{};
    std::array<bool, Slots> had_player{};
    std::array<Vec3, Slots> local_position{};
    std::array<Vec3, Slots> local_speed{};
    std::array<Vec3, Slots> local_facing{};
    std::array<bool, Slots> spawned_before{};
    std::array<bool, Slots> just_placed_state{};
    sanitized.players.reserve(std::min<std::size_t>(snapshot.players.size(), Slots));

    for (const auto& state : snapshot.players) {
        const std::uint8_t slot = state.slot_index;
        if (slot >= Slots || seen[slot] || !sane(state.position)
            || !sane(state.speed) || !sane(state.facing)) {
            ++metrics_.rejected_updates;
            continue;
        }
        seen[slot] = true;
        state_valid_[slot] = true;
        if (session.has_player(slot)) {
            had_player[slot] = true;
            const auto& current = session.player(slot);
            local_position[slot] = current.position;
            local_speed[slot] = current.speed;
            local_facing[slot] = current.facing;
            spawned_before[slot] = in_play(current);
        }

        const bool spawned = (state.flags & PlayerState::FlagSpawned) != 0;
        const bool just_placed = spawned && !authority_spawned_[slot];
        authority_spawned_[slot] = spawned;
        just_placed_state[slot] = just_placed;

        damage.replay(session, state);

        PlayerState applied = state;
        if (!spawned) {
            // ApplySnapshot owns the authoritative health/flags, but a dead
            // local hunter keeps its local transform until a real spawn. The
            // same separation prevents an old corpse coordinate from dragging
            // a client back across a room while it is waiting to respawn.
            if (slot == local_slot && had_player[slot]) {
                applied.position = local_position[slot];
                applied.speed = local_speed[slot];
                applied.facing = local_facing[slot];
            }
        } else if (slot == local_slot && had_player[slot]
                   && spawned_before[slot] && !just_placed) {
            // Local prediction stays authoritative for movement and aim. The
            // authority still owns health, score, weapon, and all flags.
            applied.position = local_position[slot];
            applied.speed = local_speed[slot];
            applied.facing = local_facing[slot];
        } else if (slot == local_slot && had_player[slot] && just_placed) {
            applied.speed = {};
        }
        sanitized.players.push_back(applied);
    }

    if (sanitized.players.empty()) {
        return false;
    }
    session.apply_snapshot(sanitized);

    // Session::apply_snapshot performs the common state copy. Restore the
    // predicted local movement after that copy, and never restore remote
    // positions: their owner's intent is the one simulation that should move
    // them on the authority.
    for (const auto& state : sanitized.players) {
        const auto slot = state.slot_index;
        if (slot != local_slot || !had_player[slot]
            || settling || !spawned_before[slot]
            || just_placed_state[slot]) {
            continue;
        }
        auto& player = session.mutable_player(slot);
        player.position = local_position[slot];
        player.speed = local_speed[slot];
        player.facing = local_facing[slot];
    }
    // Session owns the compact snapshot copy.  The partial PlayerEntity
    // operations from PlayerEntityNetAim still run for remote players so
    // their cached aim and the native input/weapon/form gates converge at the
    // same point as the managed bridge.
    for (const auto& state : sanitized.players) {
        const auto slot = state.slot_index;
        if (slot == local_slot || !session.has_player(slot)) {
            continue;
        }
        aim_.remember_aim(slot, state.facing);
        static_cast<void>(aim_.set_facing(session, slot, state.facing));
        static_cast<void>(aim_.set_weapon(session, slot,
                                          state.current_weapon));
        static_cast<void>(aim_.force_form(
            session, slot,
            (state.flags & PlayerState::FlagAltForm) != 0));
        static_cast<void>(aim_.set_zoom(
            session, slot,
            (state.flags & PlayerState::FlagZoomed) != 0));
        aim_.set_spectating(session, slot,
                            (state.flags & PlayerState::FlagSpectating) != 0);
        static_cast<void>(aim_.set_frozen(
            session, slot,
            (state.flags & PlayerState::FlagFrozen) != 0));
    }
    return true;
}

void NetPlayerBridge::restore_reported_positions(
    gameplay::Session& session, std::uint8_t local_slot,
    bool settling) noexcept {
    for (std::size_t index = 0; index < Slots; ++index) {
        const auto slot = static_cast<std::uint8_t>(index);
        if (slot == local_slot || settling || !latest_intent_seen_[index]
            || !session.has_player(slot)) {
            continue;
        }
        const auto& player = session.player(slot);
        if ((player.flags & PlayerState::FlagActive) == 0
            || (player.flags & PlayerState::FlagSpawned) == 0
            || player.health == 0) {
            continue;
        }
        const auto& intent = latest_intent_[index];
        if (!sane(intent.position) || zero(intent.position)
            || stale_since_spawn(session, slot, intent)) {
            continue;
        }
        session.mutable_player(slot).position = intent.position;
    }
}

} // namespace fruityprime::net
