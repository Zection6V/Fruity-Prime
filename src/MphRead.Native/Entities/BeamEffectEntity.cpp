// Native counterpart of src/MphRead/Entities/BeamEffectEntity.cs.
#include "Entities/runtime_entities.hpp"
#include "runtime_entity_helpers.hpp"

namespace fruityprime::runtime {

using namespace detail;

BeamEffectEntity::BeamEffectEntity(std::uint32_t id, std::int32_t type,
                                   float lifespan_seconds,
                                   net::Vec3 position) noexcept
    : Entity(id, Kind::BeamEffect, position), type_(type),
      lifespan_seconds_(std::max(0.0F, lifespan_seconds)) {}

std::string_view BeamEffectEntity::model_name() const noexcept {
    switch (type_) {
    case 0:
        return "iceWave";
    case 1:
        return "sniperBeam";
    case 2:
        return "cylBossLaserBurn";
    default:
        return {};
    }
}

std::int32_t BeamEffectEntity::spawned_effect_id() const noexcept {
    if (type_ < 3) {
        // Type 0 creates the ice-wave splat in BeamEffectEntity::Spawn;
        // the other two types are model-only.
        return type_ == 0 ? 78 : -1;
    }
    const std::int32_t effect_id = type_ - 3;
    if (no_splat_ && effect_id == 1) {
        return 2;
    }
    if (no_splat_ && effect_id == 92) {
        return 98;
    }
    return effect_id;
}

void BeamEffectEntity::handle_message(
    const messaging::MessageInfo& message) noexcept {
    Entity::handle_message(message);
    if (cartridge_message(message, 6) || cartridge_message(message, 21)) {
        active_ = false;
    }
}

bool BeamEffectEntity::process(float seconds) noexcept {
    if (!active_) {
        return false;
    }
    seconds = finite_seconds(seconds);
    advance_age(seconds);
    lifespan_seconds_ -= seconds;
    if (lifespan_seconds_ <= 0.0F) {
        lifespan_seconds_ = 0.0F;
        active_ = false;
    }
    return active_;
}

} // namespace fruityprime::runtime

