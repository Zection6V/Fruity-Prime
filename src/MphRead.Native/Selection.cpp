#include "Selection.hpp"

namespace fruityprime::selection {

void State::select(Key key) noexcept {
    selected_ = key;
    selected_path_ = {};
    switch (key.type) {
    case Type::Entity:
        selected_path_.entity = key.id;
        break;
    case Type::Instance:
        selected_path_.instance = key.id;
        break;
    case Type::Node:
        selected_path_.node = key.id;
        break;
    case Type::Mesh:
        selected_path_.mesh = key.id;
        break;
    case Type::None:
    case Type::Parent:
    case Type::Child:
        break;
    }
}

void State::select_path(Path path) noexcept {
    selected_path_ = path;
    if (path.mesh >= 0) {
        selected_ = Key{Type::Mesh, path.mesh};
    } else if (path.node >= 0) {
        selected_ = Key{Type::Node, path.node};
    } else if (path.instance >= 0) {
        selected_ = Key{Type::Instance, path.instance};
    } else if (path.entity >= 0) {
        selected_ = Key{Type::Entity, path.entity};
    } else {
        selected_.reset();
    }
}

void State::clear() noexcept {
    selected_.reset();
    selected_path_ = {};
}

bool State::check_volume(std::int32_t entity_id) const noexcept {
    return !hide_unselected_volumes_ || selected_path_.entity < 0
        || entity_id == selected_path_.entity;
}

formats::SelectionType State::check_selection(
    Path candidate, std::int32_t parent_entity,
    std::int32_t child_entity) const noexcept {
    if (selected_) {
        bool selected = false;
        switch (selected_->type) {
        case Type::Mesh:
            selected = candidate.mesh == selected_path_.mesh
                && candidate.node == selected_path_.node
                && candidate.instance == selected_path_.instance
                && candidate.entity == selected_path_.entity;
            break;
        case Type::Node:
            selected = candidate.node == selected_path_.node
                && candidate.instance == selected_path_.instance
                && candidate.entity == selected_path_.entity;
            break;
        case Type::Instance:
            selected = candidate.instance == selected_path_.instance
                && candidate.entity == selected_path_.entity;
            break;
        case Type::Entity:
            selected = candidate.entity == selected_path_.entity;
            break;
        case Type::None:
        case Type::Parent:
        case Type::Child:
            break;
        }
        if (selected) return formats::SelectionType::Selected;
    }
    if (selected_path_.entity >= 0) {
        if (parent_entity >= 0 && parent_entity == candidate.entity)
            return formats::SelectionType::Parent;
        if (child_entity >= 0 && child_entity == candidate.entity)
            return formats::SelectionType::Child;
    }
    return formats::SelectionType::None;
}

bool State::is_selected(Key key) const noexcept {
    return selected_.has_value() && *selected_ == key;
}

Color State::color(Type type) noexcept {
    switch (type) {
    case Type::Entity:
        return {1.0F, 1.0F, 1.0F, 1.0F};
    case Type::Instance:
        return {1.0F, 1.0F, 0.78431375F, 1.0F};
    case Type::Node:
        return {0.78431375F, 1.0F, 0.78431375F, 1.0F};
    case Type::Mesh:
        return {1.0F, 0.78431375F, 1.0F, 1.0F};
    case Type::Parent:
        return {1.0F, 0.0F, 0.0F, 1.0F};
    case Type::Child:
        return {0.0F, 0.0F, 1.0F, 1.0F};
    case Type::None:
        return {1.0F, 1.0F, 1.0F, 0.0F};
    }
    return {};
}

std::optional<Color> State::selection_color(
    formats::SelectionType type, std::uint64_t tick_ms) const noexcept {
    if (!show_selection_ || type == formats::SelectionType::None)
        return std::nullopt;

    const auto seconds = tick_ms / 1000;
    float factor = static_cast<float>(tick_ms % 1000) / 1000.0F;
    if ((seconds % 10) % 2 == 0) factor = 1.0F - factor;

    Type color_type = Type::None;
    switch (type) {
    case formats::SelectionType::Selected:
        color_type = selected_ ? selected_->type : Type::None;
        break;
    case formats::SelectionType::Parent:
        color_type = Type::Parent;
        break;
    case formats::SelectionType::Child:
        color_type = Type::Child;
        break;
    case formats::SelectionType::None:
        return std::nullopt;
    }
    Color result = color(color_type);
    result.red *= factor;
    result.green *= factor;
    result.blue *= factor;
    result.alpha = 1.0F;
    return result;
}

} // namespace fruityprime::selection
