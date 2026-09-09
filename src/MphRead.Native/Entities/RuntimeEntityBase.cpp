// Native counterpart of src/MphRead/Entities/EntityBase.cs.
#include "Entities/runtime_entities.hpp"
#include "runtime_entity_helpers.hpp"

namespace fruityprime::runtime {

using namespace detail;

std::string_view kind_name(Kind kind) noexcept {
    switch (kind) {
    case Kind::ItemInstance:
        return "item";
    case Kind::BeamEffect:
        return "beam_effect";
    case Kind::Bomb:
        return "bomb";
    case Kind::EnemyInstance:
        return "enemy";
    case Kind::EnemySpawner:
        return "enemy_spawner";
    case Kind::Halfturret:
        return "halfturret";
    case Kind::Player:
        return "player";
    case Kind::BeamProjectile:
        return "beam_projectile";
    }
    return "unknown";
}

Entity::Entity(std::uint32_t id, Kind kind, net::Vec3 position) noexcept
    : id_(id), kind_(kind), position_(position) {}

void Entity::advance_age(float seconds) noexcept {
    seconds = finite_seconds(seconds);
    if (seconds > 0.0F) {
        age_seconds_ += seconds;
    }
}

void Entity::reposition(net::Vec3 offset) noexcept {
    position_ = add(position_, offset);
}

void Entity::handle_message(
    const messaging::MessageInfo& message) noexcept {
    // The managed queue carries the cartridge enum, while a few native-only
    // callers use the compact messaging enum.  Accept both at this boundary.
    if (cartridge_message(message, 5)) { // SetActive
        active_ = message.parameter1 != 0;
    } else if (cartridge_message(message, 6) // Destroyed
               || cartridge_message(message, 45) // PlatformSleep
               || internal_message(message, messaging::Message::Deactivate)
               || internal_message(message, messaging::Message::Close)
               || internal_message(message, messaging::Message::Stop)) {
        active_ = false;
    } else if (cartridge_message(message, 18) // Activate
               || cartridge_message(message, 44) // PlatformWakeup
               || internal_message(message, messaging::Message::Activate)
               || internal_message(message, messaging::Message::Open)
               || internal_message(message, messaging::Message::Start)) {
        active_ = true;
    }
}

} // namespace fruityprime::runtime

