#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fruityprime::mapgen {

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct Material {
    std::string name = "mat";
    int source_material = 0;
    float tex_scale = 16.0F;
};

struct Brush {
    Vec3 min;
    Vec3 max;
    int material = 0;
    float shade = 1.0F;
    bool solid = true;
    bool damaging = false;
    std::string terrain = "Metal";
};

struct Spawn {
    Vec3 position;
    float yaw = 0.0F;
};

struct JumpPad {
    Vec3 position;
    std::optional<Vec3> target;
    std::optional<Vec3> vector;
    float speed = 0.0F;
    Vec3 size{1.6F, 1.8F, 1.6F};
    std::uint32_t model_id = 0;
    std::uint16_t cooldown_time = 20;
    std::uint16_t control_lock_time = 30;
};

struct Item {
    Vec3 position;
    std::string type = "MissileExpansion";
    bool has_base = true;
    std::uint16_t spawn_interval = 300;
};

struct MapPreview {
    Vec3 position;
    Vec3 target;
};

// Source-side map recipe. This is intentionally independent from the native
// room files: the JSON is checked in, while the binary assets are generated
// from the player's own game files or from these brushes.
struct MapDefinition {
    std::string name = "CUSTOM";
    std::string in_game_name;
    std::string texture_source = "MP3 PROVING GROUND";
    int scale_factor = 4;
    float kill_height = -40.0F;
    float far_clip = 350.0F;
    bool fog_enabled = true;
    std::array<int, 3> fog_color{8, 10, 16};
    int fog_slope = 5;
    int fog_offset = 65180;
    std::array<int, 3> light1_color{31, 28, 24};
    Vec3 light1_vector{0.3F, -1.0F, 0.2F};
    std::array<int, 3> light2_color{10, 11, 16};
    Vec3 light2_vector{-0.3F, 1.0F, -0.2F};
    std::uint32_t battle_time_limit = 7U * 60U * 30U;
    std::int16_t point_limit = 7;
    std::optional<MapPreview> preview;

    // Q3 import settings. The source is resolved relative to source_path's
    // directory first, then from the process working directory. Keeping the
    // fields on the recipe makes the native importer consume the same JSON as
    // the managed MapImport class.
    std::string import_source;
    std::string import_map_name;
    float import_units_per_unit = 28.0F;
    std::string import_textures;
    int import_default_material = 0;
    std::vector<std::pair<std::string, int>> import_shader_materials;
    float import_tex_scale = 24.0F;
    bool import_keep_sky = false;
    bool import_keep_clip = true;
    int import_patch_level = 3;
    bool import_keep_spawns = true;

    std::filesystem::path source_path;
    std::vector<Material> materials;
    std::vector<Brush> brushes;
    std::vector<Spawn> spawns;
    std::vector<JumpPad> jump_pads;
    std::vector<Item> items;
};

// Intermediate geometry produced by MapBuilder.  Keeping the recipe and
// built geometry separate mirrors the managed MapDefinition/BuiltMap split;
// importers can produce the same face contract without knowing how the final
// room binaries are packed.
struct BuiltFace {
    std::vector<Vec3> points;
    std::vector<std::array<float, 2>> texcoords;
    Vec3 normal;
    int material = 0;
    float shade = 1.0F;
    bool damaging = false;
    std::uint16_t flags = 0;
    bool has_texcoords = false;
};

struct BuiltMap {
    MapDefinition definition;
    std::vector<BuiltFace> faces;
    std::vector<BuiltFace> solid;

    explicit BuiltMap(MapDefinition value);
};

struct BuildStats {
    std::size_t model_faces = 0;
    std::size_t model_vertices = 0;
    std::size_t collision_faces = 0;
    std::size_t collision_points = 0;
    std::size_t entities = 0;
    std::size_t navigation_nodes = 0;
    std::size_t navigation_edges = 0;
};

struct GeneratedMap {
    std::vector<std::uint8_t> model;
    std::vector<std::uint8_t> collision;
    std::vector<std::uint8_t> entities;
    std::vector<std::uint8_t> nodes;
    std::vector<std::uint8_t> animation;
    BuildStats stats;
};

struct Q3ConvertOptions {
    std::filesystem::path source;
    std::string map_name;
    std::string room_name;
    std::filesystem::path output_directory;
    bool drop_clip = false;
    float forced_units_per_unit = 0.0F;
    int texture_size = 64;
};

struct Q3ConvertResult {
    std::filesystem::path recipe_path;
    std::filesystem::path level_path;
    std::filesystem::path texture_pack_path;
    std::string room_name;
    float units_per_unit = 0.0F;
    std::size_t baked_textures = 0;
    std::size_t spawn_count = 0;
    std::size_t texture_pack_bytes = 0;
    std::vector<std::string> missing_textures;
    Vec3 drawn_extent;
    std::size_t clip_brushes = 0;
};

[[nodiscard]] MapDefinition load_definition(const std::filesystem::path& path);
[[nodiscard]] std::string serialize_definition(const MapDefinition& definition);
[[nodiscard]] GeneratedMap build(const MapDefinition& definition);
[[nodiscard]] Q3ConvertResult convert_q3(const Q3ConvertOptions& options);
void write_generated(const MapDefinition& definition,
                     const GeneratedMap& generated,
                     const std::filesystem::path& archive_directory,
                     const std::filesystem::path& entity_directory,
                     const std::filesystem::path& node_directory);

[[nodiscard]] std::string file_prefix(const MapDefinition& definition);
[[nodiscard]] int item_type_from_name(const std::string& name);

} // namespace fruityprime::mapgen
