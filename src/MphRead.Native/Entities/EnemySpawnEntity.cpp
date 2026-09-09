// Native counterpart of src/MphRead/Entities/EnemySpawnEntity.cs.
#include "Entities/runtime_entities.hpp"
#include "Entities/static_entities.hpp"
#include "runtime_entity_helpers.hpp"

namespace fruityprime::runtime {

using namespace detail;

EnemySpawnerEntity::EnemySpawnerEntity(
    std::uint32_t id, std::uint8_t enemy_type, net::Vec3 position,
    std::uint8_t spawn_total, std::uint8_t spawn_limit,
    std::uint8_t spawn_count, std::uint16_t cooldown_time,
    std::uint16_t initial_cooldown, bool active, bool always_active,
    float active_distance) noexcept
    : Entity(id, Kind::EnemySpawner, position), enemy_type_(enemy_type),
      spawn_total_(spawn_total), spawn_limit_(spawn_limit),
      spawn_count_(spawn_count), flags_(Suspended | (active ? Active : 0)),
      always_active_(always_active), active_distance_(active_distance),
      cooldown_period_seconds_(static_cast<float>(cooldown_time) * 2.0F
                               / 60.0F),
      cooldown_seconds_(static_cast<float>(initial_cooldown) * 2.0F
                       / 60.0F) {}

EnemySpawnerEntity::TickResult EnemySpawnerEntity::tick(
    float seconds, std::span<const net::Vec3> player_positions,
    bool player_camera, bool range_node_ready) noexcept {
    TickResult result;
    if (!active_) {
        return result;
    }
    seconds = finite_seconds(seconds);
    advance_age(seconds);
    if (cooldown_seconds_ > 0.0F) {
        cooldown_seconds_ = std::max(0.0F, cooldown_seconds_ - seconds);
    }
    if (!spawner_active()) {
        return result;
    }

    if ((flags_ & Suspended) != 0) {
        if (cooldown_seconds_ > 0.0F || !range_node_ready) {
            return result;
        }
        flags_ &= static_cast<std::uint8_t>(~Suspended);
    }

    // CarnivorousPlant is the one managed enemy that intentionally ignores
    // the player-distance condition.  The numeric value is the cartridge
    // EnemyType.CarnivorousPlant value and is kept here to avoid coupling the
    // runtime entity header to the complete format enum table.
    constexpr std::uint8_t kCarnivorousPlant = 51;
    bool in_range = true;
    if (player_camera && enemy_type_ != kCarnivorousPlant) {
        const float distance_squared = active_distance_ * active_distance_;
        in_range = std::any_of(
            player_positions.begin(), player_positions.end(),
            [this, distance_squared](net::Vec3 player) {
                const net::Vec3 delta{
                    player.x - position_.x, player.y - position_.y,
                    player.z - position_.z};
                return delta.x * delta.x + delta.y * delta.y
                    + delta.z * delta.z < distance_squared;
            });
    }
    if (!in_range) {
        flags_ |= Suspended;
        return result;
    }

    if (active_count_ < spawn_limit_ && cooldown_seconds_ == 0.0F
        && spawn_count_ > 0
        && (spawn_total_ == 0 || spawned_count_ < spawn_total_)) {
        std::uint32_t available = static_cast<std::uint32_t>(spawn_count_);
        available = std::min(available,
                             static_cast<std::uint32_t>(spawn_limit_)
                                 - active_count_);
        if (spawn_total_ > 0) {
            available = std::min(
                available,
                static_cast<std::uint32_t>(spawn_total_) - spawned_count_);
        }
        if (available > 0) {
            active_count_ += available;
            spawned_count_ += available;
            cooldown_seconds_ = cooldown_period_seconds_;
            if ((flags_ & HasModel) != 0) {
                flags_ |= PlayAnimation;
            }
            result.spawned = static_cast<std::uint8_t>(available);
        }
    }

    result.deactivated = complete_if_exhausted();
    return result;
}

bool EnemySpawnerEntity::on_enemy_destroyed(bool out_of_range) noexcept {
    if (active_count_ == 0) {
        return false;
    }
    --active_count_;
    if (out_of_range) {
        if (spawned_count_ > 0) {
            --spawned_count_;
        }
        cooldown_seconds_ = 0.0F;
    } else {
        cooldown_seconds_ = cooldown_period_seconds_;
    }
    return complete_if_exhausted();
}

void EnemySpawnerEntity::activate(bool value) noexcept {
    if (value) {
        flags_ |= Active;
    } else {
        flags_ &= static_cast<std::uint8_t>(~Active);
    }
}

void EnemySpawnerEntity::set_has_model(bool value) noexcept {
    if (value) {
        flags_ |= HasModel;
    } else {
        flags_ &= static_cast<std::uint8_t>(~(HasModel | PlayAnimation));
    }
}

void EnemySpawnerEntity::handle_message(
    const messaging::MessageInfo& message) noexcept {
    if (cartridge_message(message, 6)) { // Destroyed
        static_cast<void>(on_enemy_destroyed(message.parameter1 != 0));
        return;
    }
    if (cartridge_message(message, 5)) { // SetActive
        activate(message.parameter1 != 0);
        return;
    }
    if (cartridge_message(message, 18)
        || cartridge_message(message, 44)
        || internal_message(message, messaging::Message::Activate)) {
        activate(true);
        return;
    }
    if (cartridge_message(message, 45)
        || internal_message(message, messaging::Message::Deactivate)
        || internal_message(message, messaging::Message::Stop)) {
        activate(false);
        return;
    }
    Entity::handle_message(message);
}

bool EnemySpawnerEntity::process(float seconds) noexcept {
    if (!active_) {
        return false;
    }
    // Player proximity is a Scene concern, so a pooled caller must use
    // tick() for spawner logic.  process() still fulfils Entity's lifetime
    // contract and advances the common age counter.
    advance_age(seconds);
    return active_;
}

bool EnemySpawnerEntity::complete_if_exhausted() noexcept {
    if (!spawner_active() || spawn_total_ == 0
        || spawned_count_ < spawn_total_ || active_count_ != 0) {
        return false;
    }
    flags_ &= static_cast<std::uint8_t>(~Active);
    return true;
}

} // namespace fruityprime::runtime

namespace fruityprime::entities::static_entities {

FhEnemySpawnEntity::FhEnemySpawnEntity(
    const scene::EntityInstance& source)
    : Entity(source) {
    if (const auto* data = std::get_if<enemy_spawn::FirstHuntData>(
            &source.typed_data); data != nullptr) {
        data_ = *data;
    }
}

} // namespace fruityprime::entities::static_entities
