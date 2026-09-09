// Native counterpart of src/MphRead/Entities/ItemSpawnEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

ItemSpawnEntity::ItemSpawnEntity(const scene::EntityInstance& source)
    : Entity(source), data_(data_or_default<scene::ItemSpawnData>(source)) {
    active_ = data_.enabled;
    cooldown_ticks_ = static_cast<std::uint32_t>(data_.spawn_delay) * 2u;
}

void ItemSpawnEntity::on_item_picked_up() noexcept {
    if (!item_present_) {
        return;
    }
    item_present_ = false;
    cooldown_ticks_ = static_cast<std::uint32_t>(data_.spawn_interval) * 2u;
}

bool ItemSpawnEntity::process(float seconds) noexcept {
    advance_age(seconds);
    if (!active_) {
        return true;
    }
    if (cooldown_ticks_ > 0) {
        --cooldown_ticks_;
    }
    if (!item_present_ && cooldown_ticks_ == 0
        && (data_.max_spawn_count == 0
            || spawn_count_ < data_.max_spawn_count)) {
        item_present_ = true;
        ++spawn_count_;
        cooldown_ticks_ = static_cast<std::uint32_t>(data_.spawn_interval)
            * 2u;
    }
    return true;
}

void ItemSpawnEntity::handle_message(
    const messaging::MessageInfo& message) noexcept {
    if (cartridge_message(message, 5)) { // SetActive
        active_ = message.parameter1 != 0;
        if (!active_) {
            item_present_ = false;
        }
        return;
    }
    if (activation_message(message)) {
        active_ = true;
        return;
    }
    if (deactivation_message(message)) {
        active_ = false;
        item_present_ = false;
        return;
    }
    Entity::handle_message(message);
}

FhItemSpawnEntity::FhItemSpawnEntity(const scene::EntityInstance& source)
    : Entity(source), data_(data_or_default<scene::FhItemSpawnData>(source)) {}

bool FhItemSpawnEntity::process(float seconds) noexcept {
    static_cast<void>(Entity::process(seconds));
    if (!spawned_) {
        // FhItemSpawnEntity creates exactly one FhItemEntity on its first
        // Process call. The dynamic item is represented by the gameplay item
        // pool; this flag preserves the spawner's one-shot class state.
        spawned_ = true;
    }
    return true;
}

} // namespace fruityprime::entities::static_entities
