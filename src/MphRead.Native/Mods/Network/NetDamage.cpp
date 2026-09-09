#include "Mods/Network/net_damage.hpp"

#include "Entities/gameplay.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fruityprime::net {
namespace {

[[nodiscard]] float length(Vec3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y
                     + value.z * value.z);
}

} // namespace

Vec3 DamageBridge::clamp_impulse(Vec3 impulse) noexcept {
    if (!std::isfinite(impulse.x) || !std::isfinite(impulse.y)
        || !std::isfinite(impulse.z)) {
        return {};
    }
    const float magnitude = length(impulse);
    if (!std::isfinite(magnitude) || magnitude <= MaxImpulse) {
        return impulse;
    }
    const float scale = MaxImpulse / magnitude;
    return {impulse.x * scale, impulse.y * scale, impulse.z * scale};
}

void DamageBridge::reset() noexcept {
    sequence_.fill(0);
    attacker_.fill(NoSlot);
    beam_.fill(NoBeam);
    flags_.fill(0);
    direction_.fill({});
    last_seen_.fill(0);
    ever_seen_.fill(false);
    replaying_ = false;
    replay_beam_ = NoBeam;
    metrics_ = {};
}

void DamageBridge::reset_for_room_change() noexcept {
    // The sequence survives a room change.  It is a byte counter used only for
    // differences, while clearing it on one side of a transition makes the
    // next snapshot look like a large burst of unrelated hits.
    metrics_ = {};
    replaying_ = false;
    replay_beam_ = NoBeam;
}

void DamageBridge::forget_slot(std::uint8_t slot) noexcept {
    if (slot >= NetConfig::SlotCapacity) {
        return;
    }
    sequence_[slot] = 0;
    attacker_[slot] = NoSlot;
    beam_[slot] = NoBeam;
    flags_[slot] = 0;
    direction_[slot] = {};
    last_seen_[slot] = 0;
    ever_seen_[slot] = false;
    metrics_.resolved[slot] = 0;
    metrics_.replayed[slot] = 0;
}

void DamageBridge::note_resolved(std::uint8_t victim_slot,
                                 std::uint8_t attacker_slot,
                                 std::uint8_t beam, std::uint8_t flags,
                                 Vec3 impulse) noexcept {
    if (victim_slot >= NetConfig::SlotCapacity || replaying_) {
        return;
    }
    sequence_[victim_slot] = static_cast<std::uint8_t>(
        sequence_[victim_slot] + 1);
    ++metrics_.resolved[victim_slot];
    attacker_[victim_slot] = attacker_slot;
    beam_[victim_slot] = beam;
    flags_[victim_slot] = flags;
    direction_[victim_slot] = clamp_impulse(impulse);
}

void DamageBridge::write(std::uint8_t slot, PlayerState& state) const noexcept {
    if (slot >= NetConfig::SlotCapacity) {
        return;
    }
    state.damage_sequence = sequence_[slot];
    state.attacker_slot = attacker_[slot];
    state.damage_beam = beam_[slot];
    state.damage_flags = flags_[slot];
    state.hit_direction = direction_[slot];
}

void DamageBridge::replay(gameplay::Session& session,
                          const PlayerState& state) noexcept {
    const std::uint8_t slot = state.slot_index;
    if (slot >= NetConfig::SlotCapacity || !session.has_player(slot)) {
        return;
    }
    if (!ever_seen_[slot]) {
        ever_seen_[slot] = true;
        last_seen_[slot] = state.damage_sequence;
        return;
    }

    const std::uint8_t landed = static_cast<std::uint8_t>(
        state.damage_sequence - last_seen_[slot]);
    if (landed == 0) {
        return;
    }
    last_seen_[slot] = state.damage_sequence;
    if (landed > MaxCatchUp) {
        // A byte counter that appears to jump by almost a full wrap is a stale
        // packet or a reset under a slot change, not a real burst of hits.
        return;
    }
    metrics_.replayed[slot] += landed;

    const auto& player = session.player(slot);
    if (player.health == 0) {
        return;
    }
    const bool lethal = state.health == 0;
    const std::uint32_t difference = player.health > state.health
        ? static_cast<std::uint32_t>(player.health - state.health) : 1u;
    std::uint32_t amount = std::max<std::uint32_t>(1, difference);
    if (!lethal) {
        amount = std::min<std::uint32_t>(
            amount, std::max<std::uint32_t>(1, player.health - 1));
    }
    replaying_ = true;
    replay_beam_ = state.damage_beam;
    session.replay_network_damage(
        slot, static_cast<std::uint16_t>(std::min<std::uint32_t>(
            amount, std::numeric_limits<std::uint16_t>::max())),
        state.attacker_slot, state.damage_beam,
        clamp_impulse(state.hit_direction), lethal);
    replay_beam_ = NoBeam;
    replaying_ = false;
}

} // namespace fruityprime::net

namespace fruityprime::gameplay {

void Session::set_network_ammo(std::uint8_t slot, std::uint16_t ua,
                               std::uint16_t missiles) noexcept {
    const auto found = std::find_if(
        inputs_.begin(), inputs_.end(),
        [slot](const RuntimeInput& value) { return value.slot == slot; });
    if (found == inputs_.end()) {
        return;
    }
    found->inventory.ammo[0] = std::min(ua, found->inventory.ammo_max[0]);
    found->inventory.ammo[1] = std::min(
        missiles, found->inventory.ammo_max[1]);
}

void Session::set_network_damage_authority(bool enabled) noexcept {
    network_damage_authority_ = enabled;
}

void Session::replay_network_damage(std::uint8_t victim_slot,
                                    std::uint16_t amount,
                                    std::uint8_t attacker_slot,
                                    std::uint8_t beam,
                                    net::Vec3 impulse,
                                    bool lethal) noexcept {
    if (!has_player(victim_slot)) {
        return;
    }
    auto& player = mutable_player(victim_slot);
    const std::uint16_t old_health = player.health;
    if (old_health == 0) {
        return;
    }
    if (lethal) {
        player.health = 0;
    } else {
        const std::uint16_t safe_amount = std::min<std::uint16_t>(
            amount, old_health > 1 ? static_cast<std::uint16_t>(old_health - 1)
                                   : 0);
        player.health = old_health > safe_amount
            ? static_cast<std::uint16_t>(old_health - safe_amount) : 1;
    }

    if (player.health != old_health) {
        SoundEvent damage_event;
        damage_event.cue = SoundCue::PlayerDamage;
        damage_event.slot = victim_slot;
        damage_event.weapon = beam;
        damage_event.position = player.position;
        emit_sound(damage_event);
    }
    if (impulse.x != 0.0F || impulse.y != 0.0F || impulse.z != 0.0F) {
        player.speed = {player.speed.x + impulse.x,
                        player.speed.y + impulse.y,
                        player.speed.z + impulse.z};
    }
    if (!lethal) {
        return;
    }

    player.flags &= static_cast<std::uint8_t>(
        ~(net::PlayerState::FlagSpawned
          | net::PlayerState::FlagAltForm
          | net::PlayerState::FlagZoomed));
    player.speed = {};
    SoundEvent death_event;
    death_event.cue = SoundCue::PlayerDeath;
    death_event.slot = victim_slot;
    death_event.weapon = beam;
    death_event.position = player.position;
    emit_sound(death_event);
    // The authority's snapshot owns deaths and points.  This feedback path
    // intentionally does not touch either, mirroring NetDamage.SaveScores.
    static_cast<void>(attacker_slot);
}

} // namespace fruityprime::gameplay
