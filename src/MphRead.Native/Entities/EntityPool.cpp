// Native counterpart of src/MphRead/Entities/EntityPool.cs.
#include "Entities/runtime_entities.hpp"
#include "runtime_entity_helpers.hpp"

namespace fruityprime::runtime {

using namespace detail;

std::size_t EntityPool::process(float seconds) noexcept {
    for (auto& entity : entities_) {
        if (entity != nullptr) {
            static_cast<void>(entity->process(seconds));
        }
    }
    const auto before = entities_.size();
    entities_.erase(
        std::remove_if(entities_.begin(), entities_.end(),
                       [](const std::unique_ptr<Entity>& entity) {
                           return entity == nullptr || !entity->active();
                       }),
        entities_.end());
    return before - entities_.size();
}

std::size_t EntityPool::dispatch(
    const messaging::MessageInfo& message) noexcept {
    std::size_t dispatched = 0;
    for (const auto& entity : entities_) {
        if (entity == nullptr
            || (message.target >= 0
                && entity->id() != static_cast<std::uint32_t>(message.target))) {
            continue;
        }
        entity->handle_message(message);
        ++dispatched;
    }
    return dispatched;
}

} // namespace fruityprime::runtime

