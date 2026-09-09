// Native counterpart of MphRead/Mods/Network/PlayerEntityNetAim.cs.
//
// PlayerEntityNetAim.cs is a partial PlayerEntity because the managed
// renderer and simulation keep aim, form, weapon, and collision state on the
// entity itself.  The native port keeps the network-only history here and
// exposes the equivalent operations through gameplay::Session.  In
// particular, this file is not a declaration-only marker and its state is
// used by NetPlayerBridge and the headless map/network probes.
#include "Mods/Network/player_entity_net_aim.hpp"

#include "Entities/gameplay.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "Metadata/metadata.hpp"
#include "Mods/Network/net_log.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fruityprime::net {
namespace {

constexpr Vec3 DefaultAim{0.0F, 0.0F, 1.0F};
constexpr float EpsilonSquared = 0.0001F;
constexpr float PositionLimit = 100000.0F;
constexpr float EyeHeight = 0.9F;
constexpr float DegreesPerRadian = 57.29577951308232F;
constexpr float RadiansPerDegree = 0.017453292519943295F;

[[nodiscard]] float length_squared(Vec3 value) noexcept {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

[[nodiscard]] Vec3 normalized(Vec3 value) noexcept {
    const float squared = length_squared(value);
    if (!std::isfinite(squared) || squared <= EpsilonSquared) {
        return {};
    }
    const float scale = 1.0F / std::sqrt(squared);
    return {value.x * scale, value.y * scale, value.z * scale};
}

[[nodiscard]] bool finite_and_bounded(Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y)
        && std::isfinite(value.z) && std::abs(value.x) < PositionLimit
        && std::abs(value.y) < PositionLimit
        && std::abs(value.z) < PositionLimit;
}

[[nodiscard]] Vec3 subtract(Vec3 left, Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] float horizontal_length(Vec3 value) noexcept {
    return std::sqrt(value.x * value.x + value.z * value.z);
}

} // namespace

PlayerEntityNetAim::PlayerEntityNetAim() noexcept {
    reset();
}

void PlayerEntityNetAim::reset() noexcept {
    positions_.fill({});
    frames_.fill({});
    history_counts_.fill(0);
    aim_.fill(DefaultAim);
    aim_seen_.fill(false);
    last_good_aim_.fill(DefaultAim);
    last_good_facing_.fill(DefaultAim);
    last_good_position_.fill({});
    form_mismatch_.fill(0);
    node_unresolved_.fill(false);
    metrics_ = {};
}

void PlayerEntityNetAim::note_room_changed() noexcept {
    // Positions from the previous room are not a valid lag sample for a new
    // room.  The managed partial class keeps its arrays on the player object,
    // but NetRoomChange resets the placement barrier before any new sample is
    // consumed; clearing the native history makes that boundary explicit.
    history_counts_.fill(0);
    frames_.fill({});
    form_mismatch_.fill(0);
    node_unresolved_.fill(false);
}

void PlayerEntityNetAim::forget_slot(std::uint8_t slot) noexcept {
    if (slot >= SlotCapacity) {
        return;
    }
    positions_[slot].fill({});
    frames_[slot].fill(0);
    history_counts_[slot] = 0;
    aim_[slot] = DefaultAim;
    aim_seen_[slot] = false;
    last_good_aim_[slot] = DefaultAim;
    last_good_facing_[slot] = DefaultAim;
    last_good_position_[slot] = {};
    form_mismatch_[slot] = 0;
    node_unresolved_[slot] = false;
}

bool PlayerEntityNetAim::sane(Vec3 value) noexcept {
    return finite_and_bounded(value);
}

Vec3 PlayerEntityNetAim::normalized_or(Vec3 value, Vec3 fallback) noexcept {
    const Vec3 result = normalized(value);
    return length_squared(result) <= EpsilonSquared ? fallback : result;
}

float PlayerEntityNetAim::degrees_to_radians(float degrees) noexcept {
    return degrees * RadiansPerDegree;
}

float PlayerEntityNetAim::radians_to_degrees(float radians) noexcept {
    return radians * DegreesPerRadian;
}

Vec3 PlayerEntityNetAim::rotate_aim(Vec3 aim, float delta_x_degrees,
                                    float delta_y_degrees) noexcept {
    Vec3 current = normalized_or(aim, DefaultAim);
    float yaw = std::atan2(current.x, current.z);
    float pitch = std::atan2(current.y, horizontal_length(current));
    yaw += degrees_to_radians(delta_x_degrees);
    pitch += degrees_to_radians(delta_y_degrees);
    const float pitch_limit = degrees_to_radians(85.0F);
    pitch = std::clamp(pitch, -pitch_limit, pitch_limit);
    const float horizontal = std::cos(pitch);
    return {std::sin(yaw) * horizontal, std::sin(pitch),
            std::cos(yaw) * horizontal};
}

void PlayerEntityNetAim::record_position(const gameplay::Session& session,
                                         std::uint8_t slot,
                                         std::uint32_t frame) noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot)) {
        return;
    }
    const Vec3 position = session.player(slot).position;
    if (!sane(position)) {
        ++metrics_.rejected_values;
        return;
    }
    const std::size_t count = std::min(
        history_counts_[slot], HistoryLength - static_cast<std::size_t>(1));
    for (std::size_t index = count; index > 0; --index) {
        positions_[slot][index] = positions_[slot][index - 1];
        frames_[slot][index] = frames_[slot][index - 1];
    }
    positions_[slot][0] = position;
    frames_[slot][0] = frame;
    history_counts_[slot] = std::min(count + 1, HistoryLength);
}

bool PlayerEntityNetAim::get_network_position(
    std::uint8_t slot, std::uint32_t frame, Vec3& position) const noexcept {
    if (slot >= SlotCapacity) {
        position = {};
        return false;
    }
    for (std::size_t index = 0; index < history_counts_[slot]; ++index) {
        if (frames_[slot][index] <= frame) {
            position = positions_[slot][index];
            return true;
        }
    }
    position = {};
    return false;
}

Vec3 PlayerEntityNetAim::gun_vector(std::uint8_t slot) const noexcept {
    return slot < SlotCapacity ? aim_[slot] : DefaultAim;
}

void PlayerEntityNetAim::remember_aim(std::uint8_t slot, Vec3 aim) noexcept {
    if (slot >= SlotCapacity || !sane(aim)) {
        ++metrics_.rejected_values;
        return;
    }
    const Vec3 value = normalized(aim);
    if (length_squared(value) <= EpsilonSquared) {
        ++metrics_.rejected_values;
        return;
    }
    aim_[slot] = value;
    aim_seen_[slot] = true;
    last_good_aim_[slot] = value;
}

bool PlayerEntityNetAim::set_aim(gameplay::Session& session,
                                 std::uint8_t slot, Vec3 aim) noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot) || !sane(aim)) {
        ++metrics_.rejected_values;
        return false;
    }
    const Vec3 value = normalized(aim);
    if (length_squared(value) <= EpsilonSquared) {
        ++metrics_.rejected_values;
        return false;
    }
    aim_[slot] = value;
    aim_seen_[slot] = true;
    last_good_aim_[slot] = value;
    session.set_network_aim(slot, value);
    return true;
}

bool PlayerEntityNetAim::refresh_network_aim(
    gameplay::Session& session, std::uint8_t slot, Vec3 aim) noexcept {
    // Kept as a separate entry point because PlayerInput.cs invokes the
    // network refresh from its own input phase.  It must not alter buttons or
    // weapon selection, which is why it ultimately calls set_network_aim.
    return set_aim(session, slot, aim);
}

bool PlayerEntityNetAim::set_facing(gameplay::Session& session,
                                    std::uint8_t slot, Vec3 facing) noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot) || !sane(facing)) {
        ++metrics_.rejected_values;
        return false;
    }
    const Vec3 value = normalized(facing);
    if (length_squared(value) <= EpsilonSquared) {
        ++metrics_.rejected_values;
        return false;
    }
    session.mutable_player(slot).facing = value;
    last_good_facing_[slot] = value;
    return true;
}

void PlayerEntityNetAim::set_spectating(gameplay::Session& session,
                                        std::uint8_t slot,
                                        bool value) noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot)) {
        return;
    }
    auto& player = session.mutable_player(slot);
    if (value) {
        player.flags |= PlayerState::FlagSpectating;
        player.speed = {};
    } else {
        player.flags &= static_cast<std::uint8_t>(
            ~PlayerState::FlagSpectating);
    }
}

bool PlayerEntityNetAim::in_play(const gameplay::Session& session,
                                 std::uint8_t slot) const noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot)) {
        return false;
    }
    const auto& player = session.player(slot);
    return player.health > 0
        && (player.flags & PlayerState::FlagSpectating) == 0;
}

bool PlayerEntityNetAim::is_in_play(const gameplay::Session& session,
                                    std::uint8_t slot) const noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot)) {
        return false;
    }
    const auto& player = session.player(slot);
    return (player.flags & PlayerState::FlagSpawned) != 0
        && player.health > 0;
}

bool PlayerEntityNetAim::network_spawn(gameplay::Session& session,
                                       std::uint8_t slot, Vec3 position,
                                       Vec3 facing, bool alt_form) noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot) || !sane(position)) {
        ++metrics_.rejected_values;
        return false;
    }
    const Vec3 forward = normalized_or(facing, {0.0F, 0.0F, 1.0F});
    session.place_player(slot, position, forward, alt_form);
    auto& player = session.mutable_player(slot);
    if (player.health == 0) {
        const auto& inventory = session.inventory(slot);
        player.health = inventory.health_max;
    }
    player.flags |= PlayerState::FlagActive | PlayerState::FlagSpawned;
    player.flags &= static_cast<std::uint8_t>(
        ~PlayerState::FlagSpectating);
    remember_aim(slot, forward);
    last_good_facing_[slot] = forward;
    last_good_position_[slot] = position;
    node_unresolved_[slot] = false;
    ++metrics_.network_spawns;
    return true;
}

PlayerEntityNetAim::AimDelta PlayerEntityNetAim::aim_delta_towards(
    const gameplay::Session& session, std::uint8_t slot, Vec3 target) const
    noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot) || !sane(target)) {
        return {};
    }
    const auto& player = session.player(slot);
    // All eight managed biped volumes have minPickupHeight=-0.5 and radius
    // 0.5, so PlayerVolumes[hunter, 0].SpherePosition is the player's origin.
    // The native projectile starts at that origin plus AimYOffset=0.9.
    const Vec3 desired = subtract(target,
                                  {player.position.x,
                                   player.position.y + EyeHeight,
                                   player.position.z});
    const float desired_flat = horizontal_length(desired);
    const Vec3 current = normalized_or(aim_[slot], player.facing);
    const float current_flat = horizontal_length(current);
    if (desired_flat < 0.001F || current_flat < 0.001F) {
        return {};
    }
    const float desired_yaw = std::atan2(desired.x, desired.z);
    const float current_yaw = std::atan2(current.x, current.z);
    float turn = radians_to_degrees(desired_yaw - current_yaw);
    while (turn > 180.0F) {
        turn -= 360.0F;
    }
    while (turn < -180.0F) {
        turn += 360.0F;
    }
    const float desired_pitch = std::atan2(desired.y, desired_flat);
    const float current_pitch = std::atan2(current.y, current_flat);
    return {turn, radians_to_degrees(desired_pitch - current_pitch)};
}

Vec3 PlayerEntityNetAim::aim_target(const gameplay::Session& session,
                                    std::uint8_t slot) const noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot)) {
        return {};
    }
    const auto& player = session.player(slot);
    return player.position;
}

void PlayerEntityNetAim::set_hunter(gameplay::Session& session,
                                    std::uint8_t slot,
                                    std::uint8_t hunter) noexcept {
    if (slot < SlotCapacity && session.has_player(slot)) {
        session.set_player_hunter(slot, hunter);
    }
}

bool PlayerEntityNetAim::start_form_switch(gameplay::Session& session,
                                           std::uint8_t slot) noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot)) {
        return false;
    }
    const bool target = (session.player(slot).flags
                         & PlayerState::FlagAltForm) == 0;
    ++form_mismatch_[slot];
    return force_form(session, slot, target);
}

bool PlayerEntityNetAim::force_form(gameplay::Session& session,
                                    std::uint8_t slot,
                                    bool alt_form) noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot)) {
        return false;
    }
    auto& player = session.mutable_player(slot);
    const bool current = (player.flags & PlayerState::FlagAltForm) != 0;
    if (current == alt_form) {
        form_mismatch_[slot] = 0;
        return false;
    }
    if (alt_form) {
        player.flags |= PlayerState::FlagAltForm;
    } else {
        player.flags &= static_cast<std::uint8_t>(
            ~PlayerState::FlagAltForm);
    }
    form_mismatch_[slot] = 0;
    ++metrics_.form_switches;
    return true;
}

bool PlayerEntityNetAim::set_weapon(gameplay::Session& session,
                                    std::uint8_t slot,
                                    std::uint8_t weapon) noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot)
        || weapon >= metadata::WeaponCount) {
        ++metrics_.rejected_values;
        return false;
    }
    auto& inventory = session.inventory(slot);
    inventory.available_weapons[weapon] = true;
    auto& player = session.mutable_player(slot);
    const std::uint8_t previous_weapon = player.current_weapon;
    player.current_weapon = weapon;
    players::PlayerEntity::SessionWeaponChanged(
        session, slot, previous_weapon, weapon);
    return true;
}

PlayerEntityNetAim::Ammo PlayerEntityNetAim::ammo(
    const gameplay::Session& session, std::uint8_t slot) const noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot)) {
        return {};
    }
    const auto& inventory = session.inventory(slot);
    return {inventory.ammo[0], inventory.ammo[1]};
}

void PlayerEntityNetAim::set_ammo(gameplay::Session& session,
                                  std::uint8_t slot, std::uint16_t ua,
                                  std::uint16_t missiles) noexcept {
    if (slot < SlotCapacity && session.has_player(slot)) {
        session.set_network_ammo(slot, ua, missiles);
    }
}

bool PlayerEntityNetAim::set_zoom(gameplay::Session& session,
                                  std::uint8_t slot,
                                  bool zoomed) noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot)) {
        return false;
    }
    auto& player = session.mutable_player(slot);
    const bool possible = metadata::weapon_info(
        std::min<std::uint8_t>(player.current_weapon,
                               static_cast<std::uint8_t>(
                                   metadata::WeaponCount - 1))).can_zoom;
    const bool wanted = zoomed && possible;
    if (wanted) {
        player.flags |= PlayerState::FlagZoomed;
    } else {
        player.flags &= static_cast<std::uint8_t>(~PlayerState::FlagZoomed);
    }
    return wanted == zoomed;
}

bool PlayerEntityNetAim::set_frozen(gameplay::Session& session,
                                    std::uint8_t slot,
                                    bool frozen_value) noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot)) {
        return false;
    }
    auto& player = session.mutable_player(slot);
    if (player.health == 0 && frozen_value) {
        return false;
    }
    if (frozen_value) {
        player.flags |= PlayerState::FlagFrozen;
        player.speed = {};
    } else {
        player.flags &= static_cast<std::uint8_t>(~PlayerState::FlagFrozen);
    }
    return true;
}

bool PlayerEntityNetAim::frozen(const gameplay::Session& session,
                                std::uint8_t slot) const noexcept {
    return slot < SlotCapacity && session.has_player(slot)
        && (session.player(slot).flags & PlayerState::FlagFrozen) != 0;
}

bool PlayerEntityNetAim::can_zoom(const gameplay::Session& session,
                                  std::uint8_t slot) const noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot)) {
        return false;
    }
    const auto weapon = std::min<std::uint8_t>(
        session.player(slot).current_weapon,
        static_cast<std::uint8_t>(metadata::WeaponCount - 1));
    return metadata::weapon_info(weapon).can_zoom;
}

PlayerEntityNetAim::WeaponState PlayerEntityNetAim::weapon_state(
    const gameplay::Session& session, std::uint8_t slot) const noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot)) {
        return {};
    }
    const auto& player = session.player(slot);
    const auto weapon = std::min<std::uint8_t>(
        player.current_weapon,
        static_cast<std::uint8_t>(metadata::WeaponCount - 1));
    const auto& info = metadata::weapon_info(weapon);
    const auto carried = ammo(session, slot);
    return {player.current_weapon, carried, info.can_charge, info.can_zoom,
            (player.flags & PlayerState::FlagZoomed) != 0};
}

void PlayerEntityNetAim::apply_script_aim(
    gameplay::Session& session, std::uint8_t slot, float delta_x_degrees,
    float delta_y_degrees) noexcept {
    if (!std::isfinite(delta_x_degrees) || !std::isfinite(delta_y_degrees)
        || slot >= SlotCapacity || !session.has_player(slot)) {
        ++metrics_.rejected_values;
        return;
    }
    const Vec3 base = length_squared(aim_[slot]) > EpsilonSquared
        ? aim_[slot] : session.player(slot).facing;
    static_cast<void>(set_aim(session, slot,
                               rotate_aim(base, delta_x_degrees,
                                          delta_y_degrees)));
}

void PlayerEntityNetAim::note_input(gameplay::Session& session,
                                     std::uint8_t slot) noexcept {
    if (slot < SlotCapacity && session.has_player(slot)) {
        session.note_network_input(slot);
        ++metrics_.noted_inputs;
    }
}

void PlayerEntityNetAim::repair_vectors(gameplay::Session& session,
                                        std::uint8_t slot) noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot)) {
        return;
    }
    auto& player = session.mutable_player(slot);
    if (sane(player.facing) && length_squared(player.facing) > EpsilonSquared) {
        last_good_facing_[slot] = normalized(player.facing);
    } else {
        player.facing = last_good_facing_[slot];
        ++metrics_.repaired_facing;
    }
    if (!aim_seen_[slot]) {
        aim_[slot] = normalized_or(player.facing, DefaultAim);
        aim_seen_[slot] = true;
        last_good_aim_[slot] = aim_[slot];
    } else if (sane(aim_[slot])
               && length_squared(aim_[slot]) > EpsilonSquared) {
        aim_[slot] = normalized(aim_[slot]);
        last_good_aim_[slot] = aim_[slot];
    } else {
        aim_[slot] = last_good_aim_[slot];
        ++metrics_.repaired_aim;
    }
    if (sane(player.position)) {
        last_good_position_[slot] = player.position;
    } else {
        player.position = last_good_position_[slot];
        ++metrics_.repaired_position;
    }
    if (!sane(player.speed)) {
        player.speed = {};
        ++metrics_.repaired_speed;
    }
    session.set_network_aim(slot, aim_[slot]);
}

bool PlayerEntityNetAim::net_die(gameplay::Session& session,
                                 std::uint8_t slot) noexcept {
    if (slot >= SlotCapacity || !session.has_player(slot)) {
        return false;
    }
    auto& player = session.mutable_player(slot);
    if (player.health == 0) {
        return false;
    }
    player.health = 0;
    player.speed = {};
    player.flags &= static_cast<std::uint8_t>(
        ~(PlayerState::FlagSpawned | PlayerState::FlagAltForm
          | PlayerState::FlagZoomed));
    ++metrics_.network_deaths;
    return true;
}

bool PlayerEntityNetAim::can_be_hurt(const gameplay::Session& session,
                                     std::uint8_t slot) const noexcept {
    // PlayerState has no separate BeamEffectiveness array; Session creates a
    // live player with the complete native weapon table.  Active is therefore
    // the native equivalent of the managed post-Spawn effectiveness check.
    return slot < SlotCapacity && session.has_player(slot)
        && (session.player(slot).flags & PlayerState::FlagActive) != 0;
}

PlayerEntityNetAim::ScoreboardSize PlayerEntityNetAim::scoreboard_size(
    std::size_t active_players, bool ending, bool teams) noexcept {
    constexpr float StartSpace = 13.0F;
    constexpr float TeamHeaderSpace = 4.0F;
    constexpr float TeamLineSpace = 18.0F;
    constexpr float PlayerSpace = 28.0F;
    constexpr float MinPlayerSpace = 19.0F;
    if (active_players == 0) {
        return {0, ending ? StartSpace * 2.0F : StartSpace};
    }
    float row_space = PlayerSpace;
    if (active_players > 4) {
        float available = 168.0F - StartSpace;
        if (ending) {
            available -= StartSpace;
        }
        if (teams) {
            available -= 2.0F * TeamLineSpace;
        }
        row_space = std::clamp(
            available / static_cast<float>(active_players),
            MinPlayerSpace, PlayerSpace);
    }
    float height = ending ? StartSpace * 2.0F : StartSpace;
    if (teams && active_players > 1) {
        // The result list is team-sorted by GameState before this helper is
        // asked for its size.  There are at most two team headers in the
        // multiplayer modes represented by the native State.
        height += TeamLineSpace * 2.0F - TeamHeaderSpace;
    }
    height += row_space * static_cast<float>(active_players);
    return {active_players, height};
}

void PlayerEntityNetAim::log_collision_range(NetLog& log, std::uint8_t slot,
                                             Vec3 previous,
                                             Vec3 current) const noexcept {
    if (log.enabled()) {
        log.collision_range(slot, "pre-check", previous, current);
    }
}

bool PlayerEntityNetAim::node_unresolved(
    std::uint8_t slot) const noexcept {
    return slot < SlotCapacity && node_unresolved_[slot];
}

} // namespace fruityprime::net
