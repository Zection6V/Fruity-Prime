// Native counterpart of src/MphRead/Entities/EnemyInstanceEntity.cs.
#include <limits>
#include <cmath>
#include "Entities/runtime_entities.hpp"
#include "runtime_entity_helpers.hpp"

namespace fruityprime::runtime {

using namespace detail;

EnemyInstanceEntity::EnemyInstanceEntity(std::uint32_t id,
                                         std::uint8_t enemy_type,
                                         net::Vec3 position) noexcept
    : Entity(id, Kind::EnemyInstance, position), enemy_type_(enemy_type) {}

void EnemyInstanceEntity::set_states(std::uint8_t state_a,
                                     std::uint8_t state_b) noexcept {
    state_a_ = state_a;
    state_b_ = state_b;
    sub_id_ = state_a;
}

EnemyInstanceEntity::ContactDamage
EnemyInstanceEntity::ContactDamagePlayer(
    const std::size_t slot, const net::Vec3 player_position,
    const std::uint32_t damage, const bool knockback) const noexcept {
    ContactDamage contact;
    if (!hit_player(slot)) {
        // Contact is dealt once per player per frame, however many times
        // the volumes overlapped during it.
        return contact;
    }
    contact.hit = true;
    contact.damage = damage;
    contact.direction = velocity_;
    if (!knockback) {
        return contact;
    }
    const float x = player_position.x - position_.x;
    const float y = player_position.y - position_.y;
    const float z = player_position.z - position_.z;
    const float length = std::sqrt(x * x + y * y + z * z);
    if (length <= 0.0F) {
        // Standing exactly on it: there is no direction to be pushed in.
        return contact;
    }
    // Divided by five times the distance, so the shove is strongest at
    // point blank and fades as the player is already moving away -- the
    // opposite of a normalised push, and deliberately so.
    const float factor = length * 5.0F;
    contact.knockback = {x / factor, 0.0F, z / factor};
    return contact;
}

EnemyInstanceEntity::EnemySfx EnemyInstanceEntity::PlayEnemySfx(
    const std::int32_t sfx) noexcept {
    EnemySfx result;
    if (sfx == -1) {
        return result;
    }
    result.play = true;
    if ((sfx & 0x20000) != 0) {
        // Never start a second copy while one is playing, and play it on
        // the enemy rather than around it.
        result.recency = std::numeric_limits<float>::max();
        result.source_only = true;
    } else if ((sfx & 0x80000) != 0) {
        // Restart it every time.
        result.recency = 0.0F;
    }
    result.sfx = sfx & ~0xA0000;
    return result;
}

void EnemyInstanceEntity::UpdateHurtVolume() noexcept {
    // The managed alternative transforms the volume by the enemy's whole
    // matrix.  This head keeps facing and up rather than a matrix, and no
    // ported enemy needs the turning form yet, so the volume is carried
    // rather than turned and that is said here rather than approximated.
    hurt_volume_ = hurt_volume_init_.moved(
        {position_.x, position_.y, position_.z});
}

bool EnemyInstanceEntity::CallSubroutine(
    const std::span<const metadata::EnemySubroutine> subroutines,
    const std::function<bool(std::uint8_t)>& behavior) noexcept {
    if (!behavior || sub_id_ >= subroutines.size()) {
        return false;
    }
    const auto& subroutine = subroutines[sub_id_];
    const std::size_t count = std::min<std::size_t>(
        subroutine.count, subroutine.behaviors.size());
    for (std::size_t index = 0; index < count; ++index) {
        const auto& entry = subroutine.behaviors[index];
        if (!behavior(entry.behavior)) {
            continue;
        }
        // The next state is decided now and taken on the next frame's
        // AdvanceState, not here.
        state_b_ = entry.next_state;
        return true;
    }
    return false;
}

bool EnemyInstanceEntity::take_damage(std::uint32_t damage) noexcept {
    if (!active_ || (flags_ & Invincible) != 0 || damage == 0) {
        return false;
    }
    const auto amount = std::min<std::uint32_t>(damage, health_);
    health_ = static_cast<std::uint16_t>(health_ - amount);
    if (health_ == 0) {
        active_ = false;
    }
    return amount != 0;
}

void EnemyInstanceEntity::handle_message(
    const messaging::MessageInfo& message) noexcept {
    if (cartridge_message(message, 7)) { // Damage
        const std::uint32_t amount = message.parameter1 > 0
            ? static_cast<std::uint32_t>(message.parameter1) : 1u;
        static_cast<void>(take_damage(amount));
        return;
    }
    if (cartridge_message(message, 21) // Death
        || cartridge_message(message, 6)) { // Destroyed
        health_ = 0;
        active_ = false;
        return;
    }
    Entity::handle_message(message);
}

bool EnemyInstanceEntity::process(float seconds) noexcept {
    if (!active_) {
        return false;
    }
    seconds = finite_seconds(seconds);
    advance_age(seconds);
    if ((flags_ & Static) == 0) {
        position_ = add(position_, multiply(velocity_, seconds));
    }
    return active_;
}

} // namespace fruityprime::runtime

