#pragma once

#include "Formats/raw_formats.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace fruityprime::mapgen::raw_structs {

// These are the native equivalents of RawStructs.MakeNode/MakeMaterial/
// MakeMesh. The returned records are the exact fixed-layout values consumed
// by the model reader; encoding is kept here too so MapPacker does not need
// to know the raw record byte layout.
[[nodiscard]] raw::RawNode make_node(std::string_view name, int mesh_count,
                                      int first_mesh_id, int parent = -1,
                                      int child = -1, int next = -1);

[[nodiscard]] raw::RawMaterial make_material(
    std::string_view name, int texture_id, int palette_id,
    formats::RepeatMode x_repeat, formats::RepeatMode y_repeat, bool lighting,
    formats::ColorRgb diffuse, formats::ColorRgb ambient);

[[nodiscard]] raw::RawMesh make_mesh(int material_id, int display_list_id);

[[nodiscard]] std::vector<std::uint8_t> encode(const raw::RawNode& value);
[[nodiscard]] std::vector<std::uint8_t> encode(const raw::RawMaterial& value);
[[nodiscard]] std::vector<std::uint8_t> encode(const raw::RawMesh& value);

} // namespace fruityprime::mapgen::raw_structs
