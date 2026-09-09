#pragma once

#include "Formats/Types.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::testing::print {

// The managed test utility consumes the renderer's model/material objects.
// Keep this small view independent of the renderer so its diagnostics can be
// used by command-line and headless native tests as well.
struct Material {
    std::string name;
    std::uint8_t lighting = 0;
    formats::PolygonMode polygon_mode = formats::PolygonMode::Modulate;
    formats::CullingMode culling = formats::CullingMode::Neither;
    std::uint8_t alpha = 0;
};

struct Model {
    std::string name;
    std::vector<Material> materials;
};

enum class EntityPropertyType {
    Value,
    Bool,
    CollisionVolume,
    Vector3,
    String
};

struct EntityProperty {
    std::string name;
    EntityPropertyType type = EntityPropertyType::Value;
};

[[nodiscard]] std::string print_struct(std::string_view name, int size);

[[nodiscard]] std::uint32_t build_polygon_attr(
    const Material& material, int polygon_id);
[[nodiscard]] std::string get_polygon_attrs(
    const Model& model, int polygon_id);
[[nodiscard]] std::string get_polygon_attrs(
    std::string_view model_name, const Material& material, int polygon_id);
[[nodiscard]] std::string dump_polygon_attr(std::uint32_t attr);

[[nodiscard]] formats::Vector3 light_calc(
    formats::Vector3 light_vec, formats::Vector3 light_col,
    formats::Vector3 normal_vec, formats::Vector3 dif_col,
    formats::Vector3 amb_col, formats::Vector3 spe_col) noexcept;

// PrintEntityEditor.cs writes a constructor generated from reflected
// properties.  Native callers pass the same property metadata explicitly;
// this preserves the generated text without coupling the utility to a GUI
// or a reflection framework.
[[nodiscard]] std::string print_entity_editor(
    std::string_view type_name, std::span<const EntityProperty> properties);

// ParseStruct.cs is a source-generation helper.  Its managed implementation
// emits MemoryClass declarations through Debug.WriteLine; native returns the
// complete generated text so tests and tools can capture it deterministically.
[[nodiscard]] std::string parse_struct(
    std::string_view class_name, std::string_view base_class,
    std::string_view data);

} // namespace fruityprime::testing::print
