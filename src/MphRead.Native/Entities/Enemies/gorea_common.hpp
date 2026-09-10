#pragma once

#include "enemy_common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace fruityprime::gameplay::gorea {

// The managed Gorea classes advance their counters in scene frames.  The
// native session normally runs at 60 Hz, but keeping the conversion here
// makes the child controllers deterministic when a test uses another fixed
// tick.
[[nodiscard]] inline std::uint32_t frame_step(float seconds) noexcept {
    return static_cast<std::uint32_t>(std::max(
        1.0F, std::round(std::max(0.0F, seconds * 60.0F))));
}

inline void decrement(std::uint32_t& timer,
                      std::uint32_t amount) noexcept {
    timer = timer > amount ? timer - amount : 0;
}

inline void decrement_signed(std::int32_t& timer,
                             std::uint32_t amount) noexcept {
    if (timer > 0) {
        timer -= static_cast<std::int32_t>(amount);
    }
}

// These are the node-track lengths read from the real cartridge's model
// animation tables. The managed ModelInstance derives FrameCount from the
// selected node group (before material/texture groups), so hand-written state
// durations made the native cursor diverge even when the animation number was
// correct.
[[nodiscard]] inline std::uint16_t authored_animation_length(
    std::uint8_t enemy_type, std::uint8_t animation,
    std::uint16_t fallback) noexcept {
    static constexpr std::array<std::uint16_t, 27> gorea_1a{
        60, 61, 61, 39, 51, 50, 50, 53, 13, 13, 14, 30, 42, 71,
        35, 60, 24, 51, 75, 33, 33, 33, 105, 21, 13, 38, 0};
    static constexpr std::array<std::uint16_t, 9> gorea_1b{
        60, 91, 103, 60, 61, 33, 33, 81, 66};
    static constexpr std::array<std::uint16_t, 11> gorea_2{
        41, 85, 41, 41, 111, 52, 43, 43, 43, 92, 43};
    // LavaDemon: the two throws, the dive and the rise.  Read off the
    // cartridge the same way the three above were.
    static constexpr std::array<std::uint16_t, 4> fire_spawn{
        80, 80, 27, 90};
    // SphinkTick, which is both Ithraks.  Seventeen of them, and the
    // machine waits on most of them ending rather than on any timer.
    static constexpr std::array<std::uint16_t, 17> ithrak{
        31, 55, 31, 46, 31, 31, 31, 21, 21, 21, 20, 41, 41, 20, 61, 0, 21};

    const auto lookup = [animation, fallback](
                            const auto& lengths) noexcept {
        return animation < lengths.size() && lengths[animation] != 0
            ? lengths[animation] : std::max<std::uint16_t>(1, fallback);
    };
    switch (enemy_type) {
    case static_cast<std::uint8_t>(formats::EnemyType::Gorea1A):
        return lookup(gorea_1a);
    case static_cast<std::uint8_t>(formats::EnemyType::Gorea1B):
        return lookup(gorea_1b);
    case static_cast<std::uint8_t>(formats::EnemyType::Gorea2):
        return lookup(gorea_2);
    case static_cast<std::uint8_t>(formats::EnemyType::FireSpawn):
        return lookup(fire_spawn);
    case static_cast<std::uint8_t>(formats::EnemyType::LesserIthrak):
    case static_cast<std::uint8_t>(formats::EnemyType::GreaterIthrak):
        return lookup(ithrak);
    default:
        return std::max<std::uint16_t>(1, fallback);
    }
}

// ModelInstance advances its animation on every other 60 Hz scene frame.
// tick_count is the frame whose state callback is about to run. The first
// visible cursor advance is committed at the end of native tick 2 and is
// observed by the callback on tick 3.
[[nodiscard]] inline bool should_advance_animation(
    std::uint64_t tick_count) noexcept {
    return tick_count > 1 && (tick_count & 1u) != 0;
}

// The native gameplay layer has no ModelInstance, so retain the managed
// animation index/frame/ended contract here. Controllers use it for
// animation-gated damage and projectile events; frontends consume the same
// cursor when drawing the entity.
inline void set_animation(EnemyState& enemy, std::uint8_t animation,
                           std::uint16_t length, bool loop = false) noexcept {
    const std::uint16_t authored_length = authored_animation_length(
        enemy.enemy_type, animation, length);
    if (enemy.gorea_model_animation == animation
        && enemy.gorea_animation_length == authored_length
        && enemy.gorea_animation_loop == loop) {
        return;
    }
    enemy.gorea_model_animation = animation;
    enemy.gorea_animation_frame = 0;
    enemy.gorea_animation_length = authored_length;
    enemy.gorea_animation_loop = loop;
    enemy.gorea_animation_ended = false;
}

inline bool advance_animation(EnemyState& enemy,
                              std::uint32_t amount) noexcept {
    if (enemy.gorea_animation_length == 0) {
        enemy.gorea_animation_length = 1;
    }
    if (enemy.gorea_animation_ended) {
        return true;
    }
    const auto old_frame = enemy.gorea_animation_frame;
    const auto next = static_cast<std::uint32_t>(old_frame) + amount;
    if (enemy.gorea_animation_loop) {
        enemy.gorea_animation_frame = static_cast<std::uint16_t>(
            next % static_cast<std::uint32_t>(enemy.gorea_animation_length));
        return false;
    }
    // Model.UpdateAnimFrames sets Ended as soon as the cursor reaches the
    // final frame, not one frame after it. Keep the cursor on that frame so
    // node sampling and the gameplay callback see the same value.
    const auto final_frame = static_cast<std::uint32_t>(
        enemy.gorea_animation_length - 1);
    enemy.gorea_animation_frame = static_cast<std::uint16_t>(std::min(
        next, final_frame));
    if (next >= final_frame) {
        enemy.gorea_animation_ended = true;
    }
    return enemy.gorea_animation_ended;
}

inline void enter_state(EnemyState& enemy, std::uint8_t state) noexcept {
    if (enemy.gorea_state == state) {
        return;
    }
    enemy.gorea_previous_state = enemy.gorea_state;
    enemy.gorea_state = state;
    enemy.gorea_state_initialized = false;
    enemy.gorea_animation_frame = 0;
    enemy.gorea_animation_ended = false;
}

[[nodiscard]] inline bool animation_ended(
    const EnemyState& enemy) noexcept {
    return enemy.gorea_animation_ended;
}

[[nodiscard]] inline net::Vec3 local_position(
    const EnemyState& parent, net::Vec3 local) noexcept {
    const net::Vec3 facing = normalized_or(parent.facing,
                                           {0.0F, 0.0F, 1.0F});
    const net::Vec3 up = normalized_or(parent.up, {0.0F, 1.0F, 0.0F});
    const net::Vec3 right = normalized_or(cross(up, facing),
                                          {1.0F, 0.0F, 0.0F});
    return add(parent.position,
               add(multiply(right, local.x),
                   add(multiply(up, local.y), multiply(facing, local.z))));
}

[[nodiscard]] inline std::size_t nearest_player(
    const std::vector<net::PlayerState>& players, net::Vec3 position,
    float radius) noexcept {
    std::size_t result = players.size();
    float nearest_squared = radius * radius;
    for (std::size_t index = 0; index < players.size(); ++index) {
        const auto& player = players[index];
        if (!objective_player(player)) {
            continue;
        }
        const float distance = distance_squared(player.position, position);
        if (distance <= nearest_squared) {
            nearest_squared = distance;
            result = index;
        }
    }
    return result;
}

[[nodiscard]] inline net::Vec3 aim_at(
    net::Vec3 origin, net::Vec3 target, net::Vec3 fallback) noexcept {
    return normalized_or(subtract(target, origin), fallback);
}

constexpr std::uint32_t Gorea1AReveal = 1u << 0;
// Enemy24's Bit2 keeps the two shoulders invincible until the reveal
// subroutine completes. The native arm-loss transition uses a private bit so
// it cannot alias that managed flag.
constexpr std::uint32_t Gorea1AArmInvulnerable = 1u << 2;
constexpr std::uint32_t Gorea1AWeaponError = 1u << 4;
constexpr std::uint32_t Gorea1AArmsDown = 1u << 18;
// Enemy24.State00 consumes this edge once per reveal/regeneration and asks
// Enemy25 to respawn its attached eye flash. Keep it separate from Bit2,
// which controls the shoulder invulnerability window.
constexpr std::uint32_t Gorea1AHeadFlashPending = 1u << 19;

constexpr std::uint32_t Gorea1BHidden = 1u << 0;
constexpr std::uint32_t Gorea1BGrappling = 1u << 1;
constexpr std::uint32_t Gorea1BTargetTurning = 1u << 2;
constexpr std::uint32_t Gorea1BSwingDirection = 1u << 3;
constexpr std::uint32_t Gorea1BActivated = 1u << 4;
constexpr std::uint32_t Gorea1BIntroDone = 1u << 5;

constexpr std::uint32_t Gorea2LaserActive = 1u << 1;
constexpr std::uint32_t Gorea2LaserBlocked = 1u << 2;
constexpr std::uint32_t Gorea2LaserOnTarget = 1u << 3;
constexpr std::uint32_t Gorea2Teleporting = 1u << 4;
constexpr std::uint32_t Gorea2Teleporting2 = 1u << 5;
constexpr std::uint32_t Gorea2IntroDone = 1u << 6;
// These names preserve the raw managed bit positions used by Enemy31.  They
// are kept separate from the state-machine names because Bit7/Bit8 are also
// consumed by the hover and meteor choreography.
constexpr std::uint32_t Gorea2HoverFlip = 1u << 7;
constexpr std::uint32_t Gorea2MeteorLeft = 1u << 8;
constexpr std::uint32_t Gorea2OmegaUsed = 1u << 9;
constexpr std::uint32_t Gorea2DamageSequence = 1u << 10;
constexpr std::uint32_t Gorea2Death = 1u << 11;
constexpr std::uint32_t Gorea2Finish = 1u << 12;
constexpr std::uint32_t Gorea2DamageFlash = 1u << 13;
constexpr std::uint32_t Gorea2Phase0 = 1u << 14;
constexpr std::uint32_t Gorea2Phase1 = 1u << 15;
constexpr std::uint32_t Gorea2HitFlash = 1u << 16;
constexpr std::uint32_t Gorea2Subroutine = 1u << 17;

} // namespace fruityprime::gameplay::gorea
