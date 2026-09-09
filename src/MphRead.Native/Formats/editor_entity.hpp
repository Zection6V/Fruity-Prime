#pragma once

#include "Entities/scene.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::editor {

// Native counterpart of Formats/EntityClass.cs.  The managed editor classes
// are views over the decoded cartridge records; they do not own a second
// binary format.  Keep that same ownership rule here by reusing the typed
// scene data and adding the editor's common header and comparison surface.
struct EntityEditorBase {
    scene::EntityKind type = scene::EntityKind::Unknown;
    std::uint16_t raw_type = 0;
    std::int16_t id = -1;
    std::uint16_t layer_mask = 0;
    formats::Vector3 position;
    formats::Vector3 up;
    formats::Vector3 facing;
    std::string node_name;
};

struct EntityEditor {
    EntityEditorBase base;
    scene::TypedEntityData data;
    bool first_hunt = false;
};

struct FieldValue {
    std::string name;
    std::string value;
};

struct Difference {
    std::string name;
    std::string left;
    std::string right;
};

[[nodiscard]] EntityEditor make_entity_editor(
    const scene::EntityInstance& entity);

[[nodiscard]] std::string_view entity_type_name(
    scene::EntityKind type) noexcept;

[[nodiscard]] std::string entity_type_name(
    const EntityEditor& entity);

// Returns the common header followed by the exact fields exposed by the
// corresponding managed editor class.  Array-valued C# properties are kept
// as one deterministic field so comparison output remains readable.
[[nodiscard]] std::vector<FieldValue> field_values(
    const EntityEditor& entity);

[[nodiscard]] std::vector<Difference> compare(
    const EntityEditor& left, const EntityEditor& right);

[[nodiscard]] std::string describe(const EntityEditor& entity);

} // namespace fruityprime::editor
