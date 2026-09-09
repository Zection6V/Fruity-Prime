#include "Assets/game_assets.hpp"
#include "Formats/effects.hpp"
#include "Metadata/entity_metadata.hpp"
#include "Formats/enemy_spawn.hpp"
#include "Entities/gameplay.hpp"
#include "Assets/model_catalog.hpp"
#include "Formats/model_instance.hpp"
#include "Metadata/metadata.hpp"
#include "Entities/room_catalog.hpp"
#include "Entities/scene.hpp"
#include "Scene.hpp"
#include "../Entities/Enemies/enemy_common.hpp"
#include "../Entities/Enemies/enemy_catalog.hpp"
#include "../Mods/MapGen/custom_rooms.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path configured_rom(int argc, char** argv) {
    if (argc > 1 && argv[1] != nullptr && argv[1][0] != '\0') {
        return argv[1];
    }
    if (const char* value = std::getenv("FRUITY_PRIME_TEST_NDS");
        value != nullptr && value[0] != '\0') {
        return value;
    }
    return {};
}

std::string room_prefix(const fruityprime::scene::RoomCatalogEntry& entry) {
    return "room " + entry.name + " (id " + std::to_string(entry.id) + "): ";
}

void check_hunter_assets(const fruityprime::assets::Store& assets) {
    std::size_t animated_models = 0;
    for (const auto& info : fruityprime::metadata::hunters()) {
        const auto load_model = [&](bool alt) {
            const std::string archive_name =
                alt && info.id == fruityprime::metadata::Hunter::Guardian
                ? "Samus" : std::string(info.archive);
            const std::string archive_path = "archives/" + archive_name + ".arc";
            const auto resource = assets.archive(archive_path);
            const std::string entry_name = std::string(
                alt ? info.alt_model_entry : info.base_model_entry);
            const auto entry = std::find_if(
                resource.entries().begin(), resource.entries().end(),
                [&entry_name](const auto& candidate) {
                    return candidate.filename == entry_name;
                });
            require(entry != resource.entries().end(),
                    std::string(info.name) + " archive has no " + entry_name);
            const std::string prefix = std::string(
                alt ? info.alt_recolor_prefix : info.base_recolor_prefix);
            return fruityprime::model::File::from_recolor_resources(
                resource.file(static_cast<std::size_t>(
                    entry - resource.entries().begin())),
                assets.bytes("models/" + prefix + "_pal_01_Model.bin"),
                assets.bytes("models/" + prefix + "_pal_01_Tex.bin"));
        };
        const auto load_animation = [&](fruityprime::model::File& model,
                                        bool alt) {
            const std::string archive_name = std::string(info.archive);
            const std::string animation_name = archive_name
                + (alt ? "Alt" : "") + "_Anim.bin";
            model.load_animations(
                assets.bytes("_archives/" + archive_name + "/"
                            + animation_name),
                alt ? std::string(info.alt_model_entry)
                        .substr(0, std::string(info.alt_model_entry).size()
                                    - std::string_view("_Model.bin").size())
                    : std::string(info.base_model_entry)
                        .substr(0, std::string(info.base_model_entry).size()
                                    - std::string_view("_Model.bin").size()));
            const std::string shared =
                info.id == fruityprime::metadata::Hunter::Noxus
                    || info.id == fruityprime::metadata::Hunter::Trace
                ? "models/NoxSharedAnim_Anim.bin"
                : "models/SamusSharedAnim_Anim.bin";
            model.append_animations(assets.bytes(shared));
        };

        auto model = load_model(false);
        load_animation(model, false);
        require(model.header().mesh_count > 0 && !model.meshes().empty(),
                std::string(info.name) + " base model has no meshes");
        require(model.animations().any(),
                std::string(info.name) + " base animation is empty");
        fruityprime::model::ModelInstance instance(model);
        instance.set_animation(0);
        instance.compute_node_matrices();
        instance.animate_nodes();
        instance.update_matrix_stack();
        instance.update_materials();
        ++animated_models;

        auto alt_model = load_model(true);
        const bool alt_animated =
            info.id != fruityprime::metadata::Hunter::Samus
            && info.id != fruityprime::metadata::Hunter::Guardian;
        if (alt_animated) {
            load_animation(alt_model, true);
            require(alt_model.animations().any(),
                    std::string(info.name) + " alt animation is empty");
            fruityprime::model::ModelInstance alt_instance(alt_model);
            alt_instance.set_animation(0);
            alt_instance.compute_node_matrices();
            alt_instance.animate_nodes();
            alt_instance.update_matrix_stack();
            alt_instance.update_materials();
            ++animated_models;
        }
        require(alt_model.header().mesh_count > 0 && !alt_model.meshes().empty(),
                std::string(info.name) + " alt model has no meshes");
    }
    std::cout << "real hunter assets: hunters="
              << fruityprime::metadata::HunterCount
              << " animated_models=" << animated_models << '\n';
}

void check_weapon_effect_assets(const fruityprime::assets::Store& assets) {
    // The same closure loaded by the native BeamProjectile renderer. These
    // IDs cover every normal weapon muzzle/collision variant plus the model
    // and beam-effect paths used by BeamDrawEffects.
    constexpr std::array<std::uint16_t, 33> ids{
        4, 8, 9, 10, 31, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 78,
        89, 94, 95, 96, 102, 116, 130, 134, 137, 138, 174, 176, 183,
        193, 194, 211, 237
    };
    std::size_t parsed = 0;
    std::size_t missing = 0;
    std::size_t elements = 0;
    for (const std::uint16_t id : ids) {
        const auto* info = fruityprime::metadata::effect_info(id);
        require(info != nullptr && !info->name.empty(),
                "weapon effect metadata has an invalid ID");
        const std::string path = info->archive.empty()
            ? "effects/" + std::string(info->name) + "_PS.bin"
            : "_archives/" + std::string(info->archive) + "/"
                + std::string(info->name) + "_PS.bin";
        try {
            const auto effect = fruityprime::effects::File::parse(
                assets.bytes(path), static_cast<std::int32_t>(id),
                std::string(info->name));
            require(effect.id() == id && effect.name() == info->name,
                    "weapon effect parser lost its metadata identity");
            require(effect.header().element_count == effect.elements().size(),
                    "weapon effect element count does not match its table");
            elements += effect.elements().size();
            ++parsed;
        } catch (const std::exception&) {
            ++missing;
        }
    }
    require(parsed >= 20,
            "too few normal weapon effects were found in the ROM");
    std::cout << "real weapon effects: requested=" << ids.size()
              << " parsed=" << parsed
              << " missing=" << missing
              << " elements=" << elements << '\n';
}

using EffectParticleReferences =
    std::map<std::string, std::set<std::string>>;

void check_effect_catalog_assets(
    const fruityprime::assets::Store& assets,
    EffectParticleReferences& particle_references) {
    // Match TestEffects.TestAllEffects.  These entries are intentionally kept
    // in the metadata table for ID compatibility, but have no cartridge file
    // in the USA revision.
    constexpr std::array<std::string_view, 3> known_missing{
        "sparksFall", "mortarSecondary", "powerBeamChargeNoSplatMP"
    };
    std::size_t requested = 0;
    std::size_t parsed = 0;
    std::size_t missing = 0;
    std::size_t elements = 0;
    std::vector<std::string> missing_names;
    for (const auto& info : fruityprime::metadata::effects()) {
        if (info.id == 0 || info.name.empty()
            || std::find(known_missing.begin(), known_missing.end(), info.name)
                != known_missing.end()) {
            continue;
        }
        ++requested;
        const std::string path = info.archive.empty()
            ? "effects/" + std::string(info.name) + "_PS.bin"
            : "_archives/" + std::string(info.archive) + "/"
                + std::string(info.name) + "_PS.bin";
        try {
            const auto effect = fruityprime::effects::File::parse(
                assets.bytes(path), info.id, std::string(info.name));
            require(effect.id() == info.id && effect.name() == info.name,
                    "effect catalog parser lost its metadata identity");
            require(effect.header().element_count == effect.elements().size(),
                    "effect catalog element count does not match its table");
            for (const auto& element : effect.elements()) {
                if (element.model_name.empty()
                    || element.particle_names.empty()) {
                    continue;
                }
                auto& names = particle_references[element.model_name];
                names.insert(element.particle_names.begin(),
                             element.particle_names.end());
            }
            elements += effect.elements().size();
            ++parsed;
        } catch (const std::exception&) {
            ++missing;
            missing_names.emplace_back(std::to_string(info.id) + ":"
                                       + std::string(info.name));
        }
    }
    require(missing == 0,
            "real effect catalog has missing resources: "
                + (missing_names.empty() ? std::string("unknown")
                    : missing_names.front()));
    require(parsed == requested,
            "real effect catalog did not parse every referenced resource");
    std::cout << "real effect catalog: requested=" << requested
              << " parsed=" << parsed
              << " missing=" << missing
              << " elements=" << elements << '\n';
}

void check_effect_particle_assets(
    const fruityprime::assets::Store& assets,
    EffectParticleReferences particle_references) {
    // These are the direct Read.GetSingleParticle dependencies.  They are
    // not necessarily mentioned by an effect PS file, but are still part of
    // the effect renderer's startup resource closure.
    constexpr std::array<std::pair<std::string_view, std::string_view>, 12>
        single_particles{{
            {"deathParticle", "death"},
            {"particles", "fuzzBall"},
            {"icons", "lore"},
            {"icons", "lore_dim"},
            {"icons", "enemy"},
            {"icons", "enemy_dim"},
            {"icons", "object"},
            {"icons", "object_dim"},
            {"icons", "equipment"},
            {"icons", "equipment_dim"},
            {"icons", "red"},
            {"icons", "red_dim"},
        }};
    for (const auto& [model_name, particle_name] : single_particles) {
        particle_references[std::string(model_name)].insert(
            std::string(particle_name));
    }
    // PreloadResources includes these two models even when the current
    // cartridge's effect elements do not reference a named node in them.
    particle_references.try_emplace("particles2");
    particle_references.try_emplace("TearParticle");

    std::size_t references = 0;
    std::size_t resolved_nodes = 0;
    for (const auto& [model_name, particle_names] : particle_references) {
        const auto loaded = fruityprime::assets::try_load_named_model(
            assets, model_name, false);
        require(loaded.has_value(),
                "effect particle model is missing: " + model_name);
        const auto& model = loaded->model;
        require(!model.meshes().empty(),
                "effect particle model has no meshes: " + model_name);
        require(!model.nodes().empty(),
                "effect particle model has no nodes: " + model_name);

        for (const auto& particle_name : particle_names) {
            ++references;
            auto node = std::find_if(
                model.nodes().begin(), model.nodes().end(),
                [&particle_name](const auto& candidate) {
                    return candidate.name == particle_name;
                });
            if (node == model.nodes().end() && model_name == "geo1"
                && particle_name == "gib") {
                node = std::find_if(
                    model.nodes().begin(), model.nodes().end(),
                    [](const auto& candidate) {
                        return candidate.name == "gib3";
                    });
            }
            require(node != model.nodes().end(),
                    "effect particle node is missing: " + model_name
                        + "/" + particle_name);
            require(node->mesh_count > 0,
                    "effect particle node has no mesh: " + model_name
                        + "/" + particle_name);
            const std::size_t mesh_start = node->mesh_id / 2;
            require(mesh_start < model.meshes().size()
                        && node->mesh_count <= model.meshes().size()
                            - mesh_start,
                    "effect particle node mesh range is invalid: "
                        + model_name + "/" + particle_name);
            std::size_t node_vertices = 0;
            for (std::size_t mesh_index = mesh_start;
                 mesh_index < mesh_start + node->mesh_count; ++mesh_index) {
                const auto& mesh = model.meshes()[mesh_index];
                require(mesh.display_list_id < model.instructions().size(),
                        "effect particle mesh display list is invalid: "
                            + model_name + "/" + particle_name);
                require(mesh.material_id < model.materials().size(),
                        "effect particle mesh material is invalid: "
                            + model_name + "/" + particle_name);
                const auto& material = model.materials()[mesh.material_id];
                int texture_width = 0;
                int texture_height = 0;
                if (material.texture_id >= 0
                    && static_cast<std::size_t>(material.texture_id)
                        < model.textures().size()) {
                    const auto& texture = model.textures()[material.texture_id];
                    texture_width = texture.width;
                    texture_height = texture.height;
                }
                for (const auto& primitive : model.decode_geometry(
                         mesh.display_list_id, texture_width, texture_height,
                         material.texcoord_transform_mode == 2)) {
                    node_vertices += primitive.vertices.size();
                }
            }
            require(node_vertices > 0,
                    "effect particle node decoded no geometry: "
                        + model_name + "/" + particle_name);
            ++resolved_nodes;
        }
    }
    std::cout << "real effect particles: models="
              << particle_references.size()
              << " references=" << references
              << " nodes=" << resolved_nodes << '\n';
}

struct EntityModelAudit {
    std::size_t requests = 0;
    std::size_t loaded_requests = 0;
    std::size_t missing_requests = 0;
    std::size_t decoded_models = 0;
    std::size_t decoded_primitives = 0;
    std::size_t decoded_vertices = 0;
    std::size_t decoded_textures = 0;
    std::size_t decoded_palettes = 0;
    std::size_t invalid_ids = 0;
    std::set<std::string> loaded_names;
    std::set<std::string> missing_names;
    std::map<std::string, bool> cache;

    void validate_model_geometry(const fruityprime::model::File& model,
                                 std::string_view name) {
        std::size_t model_primitives = 0;
        std::size_t model_vertices = 0;
        if (model.meshes().empty()) {
            for (std::size_t display_list = 0;
                 display_list < model.instructions().size();
                 ++display_list) {
                for (const auto& primitive : model.decode_geometry(display_list)) {
                    model_vertices += primitive.vertices.size();
                    ++model_primitives;
                }
            }
        } else {
            for (const auto& mesh : model.meshes()) {
                require(mesh.display_list_id < model.instructions().size(),
                        "entity model " + std::string(name)
                            + " references an invalid display list");
                require(mesh.material_id < model.materials().size(),
                        "entity model " + std::string(name)
                            + " references an invalid material");
                const auto& material = model.materials()[mesh.material_id];
                int texture_width = 0;
                int texture_height = 0;
                if (material.texture_id >= 0
                    && static_cast<std::size_t>(material.texture_id)
                        < model.textures().size()) {
                    const auto& texture = model.textures()[material.texture_id];
                    texture_width = texture.width;
                    texture_height = texture.height;
                }
                for (const auto& primitive : model.decode_geometry(
                         mesh.display_list_id, texture_width, texture_height,
                         material.texcoord_transform_mode == 2)) {
                    model_vertices += primitive.vertices.size();
                    ++model_primitives;
                }
            }
        }
        require(model_primitives > 0 && model_vertices > 0,
                "entity model " + std::string(name)
                    + " decoded no geometry");

        std::set<std::size_t> texture_ids;
        std::set<std::size_t> palette_ids;
        for (const auto& material : model.materials()) {
            if (material.texture_id >= 0
                && static_cast<std::size_t>(material.texture_id)
                    < model.textures().size()) {
                texture_ids.insert(static_cast<std::size_t>(material.texture_id));
            }
            if (material.palette_id >= 0
                && static_cast<std::size_t>(material.palette_id)
                    < model.palettes().size()) {
                palette_ids.insert(static_cast<std::size_t>(material.palette_id));
            }
        }
        for (const auto texture_id : texture_ids) {
            const auto pixels = model.decode_texture(texture_id);
            const auto& texture = model.textures()[texture_id];
            require(pixels.size() == static_cast<std::size_t>(texture.width)
                        * static_cast<std::size_t>(texture.height),
                    "entity model " + std::string(name)
                        + " decoded an invalid texture size");
        }
        for (const auto palette_id : palette_ids) {
            require(!model.decode_palette(palette_id).empty(),
                    "entity model " + std::string(name)
                        + " decoded an empty palette");
        }
        ++decoded_models;
        decoded_primitives += model_primitives;
        decoded_vertices += model_vertices;
        decoded_textures += texture_ids.size();
        decoded_palettes += palette_ids.size();
    }

    void inspect_model(const fruityprime::assets::Store& assets,
                       std::string_view name) {
        if (name.empty()) {
            return;
        }
        ++requests;
        const std::string key(name);
        const auto cached = cache.find(key);
        bool loaded = false;
        if (cached != cache.end()) {
            loaded = cached->second;
        } else {
            auto model = fruityprime::assets::try_load_named_model(assets, name);
            loaded = model.has_value();
            if (loaded) {
                try {
                    validate_model_geometry(model->model, name);
                } catch (const std::exception& error) {
                    throw std::runtime_error(
                        "entity model " + key + " geometry failed: "
                        + error.what());
                }
            }
            cache.emplace(key, loaded);
        }
        if (loaded) {
            ++loaded_requests;
            loaded_names.insert(key);
        } else {
            ++missing_requests;
            missing_names.insert(key);
        }
    }

    void inspect_room(const fruityprime::assets::Store& assets,
                      const fruityprime::scene::Room& room,
                      const std::string& prefix) {
        for (const auto& entity : room.entities()) {
            switch (entity.kind) {
            case fruityprime::scene::EntityKind::Object: {
                const auto* data = std::get_if<fruityprime::scene::ObjectData>(
                    &entity.typed_data);
                if (data == nullptr || data->model_id < 0) {
                    break;
                }
                const auto* info = fruityprime::metadata::object_info(
                    static_cast<std::uint32_t>(data->model_id));
                if (info == nullptr) {
                    ++invalid_ids;
                    break;
                }
                inspect_model(assets, info->name);
                break;
            }
            case fruityprime::scene::EntityKind::Platform: {
                const auto* data = std::get_if<fruityprime::scene::PlatformData>(
                    &entity.typed_data);
                if (data == nullptr || data->model_id >=
                        fruityprime::metadata::PlatformCount) {
                    if (data != nullptr) {
                        ++invalid_ids;
                    }
                    break;
                }
                if (const auto* info = fruityprime::metadata::platform_model_info(
                        data->model_id); info != nullptr) {
                    inspect_model(assets, info->name);
                }
                break;
            }
            case fruityprime::scene::EntityKind::Door: {
                const auto* data = std::get_if<fruityprime::scene::DoorData>(
                    &entity.typed_data);
                if (data == nullptr) {
                    break;
                }
                const auto* info = fruityprime::metadata::door_info(
                    data->door_type);
                if (info == nullptr) {
                    ++invalid_ids;
                    break;
                }
                inspect_model(assets, info->name);
                inspect_model(assets, info->lock_name);
                break;
            }
            case fruityprime::scene::EntityKind::JumpPad: {
                const auto* data = std::get_if<fruityprime::scene::JumpPadData>(
                    &entity.typed_data);
                if (data == nullptr) {
                    break;
                }
                const auto name = fruityprime::metadata::jump_pad_name(
                    data->model_id);
                if (name.empty()) {
                    ++invalid_ids;
                    break;
                }
                inspect_model(assets, name);
                inspect_model(assets, "JumpPad_Beam");
                break;
            }
            case fruityprime::scene::EntityKind::ItemSpawn: {
                const auto* data = std::get_if<fruityprime::scene::ItemSpawnData>(
                    &entity.typed_data);
                if (data == nullptr) {
                    break;
                }
                if (data->has_base) {
                    inspect_model(assets, "items_base");
                }
                break;
            }
            case fruityprime::scene::EntityKind::Artifact: {
                const auto* data = std::get_if<fruityprime::scene::ArtifactData>(
                    &entity.typed_data);
                if (data == nullptr) {
                    break;
                }
                std::string name = "Artifact0";
                if (data->model_id >= 8) {
                    name = "Octolith";
                } else {
                    name += std::to_string(data->model_id + 1);
                }
                inspect_model(assets, name);
                if (data->has_base) {
                    inspect_model(assets, "ArtifactBase");
                }
                break;
            }
            case fruityprime::scene::EntityKind::Teleporter: {
                const auto* data = std::get_if<fruityprime::scene::TeleporterData>(
                    &entity.typed_data);
                if (data == nullptr || data->invisible) {
                    break;
                }
                inspect_model(assets, data->artifact_id < 8
                    ? "Teleporter" : "TeleporterSmall");
                if (data->artifact_id < 8) {
                    std::string name = "Artifact0";
                    name += std::to_string(data->artifact_id + 1);
                    inspect_model(assets, name);
                }
                break;
            }
            case fruityprime::scene::EntityKind::EnemySpawn: {
                const auto* data = std::get_if<fruityprime::scene::EnemySpawnData>(
                    &entity.typed_data);
                if (data == nullptr) {
                    break;
                }
                std::string_view model_name = fruityprime::enemy::model_name(
                    data->enemy_type);
                if (model_name.empty()
                    && data->enemy_type == static_cast<std::uint8_t>(
                        fruityprime::formats::EnemyType::CarnivorousPlant)) {
                    const auto plant =
                        fruityprime::enemy::decode_carnivorous_plant_profile(
                            data->enemy_type, data->fields,
                            {entity.position.x.to_float(),
                             entity.position.y.to_float(),
                             entity.position.z.to_float()});
                    model_name = plant.model_name;
                }
                inspect_model(assets, model_name);
                break;
            }
            default:
                break;
            }
        }
        if (invalid_ids != 0 && prefix.empty()) {
            throw std::runtime_error("entity model audit found invalid IDs");
        }
    }
};

} // namespace

int main(int argc, char** argv) {
    const auto rom_path = configured_rom(argc, argv);
    if (rom_path.empty()) {
        std::cout << "real room test skipped: set FRUITY_PRIME_TEST_NDS\n";
        return 0;
    }

    try {
        require(std::filesystem::is_regular_file(rom_path),
                "configured NDS ROM is not a regular file: "
                    + rom_path.string());

        fruityprime::mapgen::custom_rooms::set_map_directory(
            std::filesystem::temp_directory_path()
                / "fruityprime-native-real-room-no-custom-maps");
        const auto assets = fruityprime::assets::Store::from_rom(rom_path);
        check_weapon_effect_assets(assets);
        EffectParticleReferences effect_particle_references;
        check_effect_catalog_assets(assets, effect_particle_references);
        check_effect_particle_assets(assets, effect_particle_references);
        const auto samus_archive = assets.archive("archives/Samus.arc");
        const auto samus_model_entry = std::find_if(
            samus_archive.entries().begin(), samus_archive.entries().end(),
            [](const auto& entry) {
                return entry.filename == "Samus_lod0_Model.bin";
            });
        require(samus_model_entry != samus_archive.entries().end(),
                "Samus archive has no lod0 model");
        auto samus_model = fruityprime::model::File::from_recolor_resources(
            samus_archive.file(static_cast<std::size_t>(
                samus_model_entry - samus_archive.entries().begin())),
            assets.bytes("models/Samus_pal_01_Model.bin"),
            assets.bytes("models/Samus_pal_01_Tex.bin"));
        samus_model.load_animations(
            assets.bytes("_archives/Samus/Samus_Anim.bin"), "Samus_lod0");
        samus_model.append_animations(
            assets.bytes("models/SamusSharedAnim_Anim.bin"), "Samus_lod0");
        require(samus_model.animations().any()
                    && samus_model.animations().node_groups.size()
                        == samus_model.animations().node_group_offsets.size()
                    && samus_model.animations().node_groups.size() > 1,
                "Samus animation resources did not merge");
        fruityprime::model::ModelInstance samus_instance(samus_model);
        samus_instance.set_animation(0);
        samus_instance.compute_node_matrices();
        samus_instance.animate_nodes();
        samus_instance.update_matrix_stack();
        samus_instance.update_materials();
        require(samus_instance.animation_info().frame_count[0] >= 0,
                "Samus animation instance did not initialize");
        std::cout << "real Samus animation: node_groups="
                  << samus_model.animations().node_groups.size()
                  << " material_groups="
                  << samus_model.animations().material_groups.size()
                  << " texcoord_groups="
                  << samus_model.animations().texcoord_groups.size()
                  << " texture_groups="
                  << samus_model.animations().texture_groups.size() << '\n';
        check_hunter_assets(assets);
        fruityprime::scene_runtime::Scene runtime_scene(assets);
        fruityprime::gameplay::Config runtime_config;
        runtime_config.mode = 2; // GameMode.SinglePlayer
        require(runtime_scene.load_room(
                    fruityprime::scene::RoomDefinition{
                        "UNIT1_C0", "archives/unit1_C0.arc",
                        "unit1_c0_model.bin",
                        "levels/textures/unit1_c0_tex.bin",
                        "unit1_c0_collision.bin",
                        "levels/entities/Unit1_C0_Ent.bin", {}, {}, {}},
                    runtime_config),
                "root Scene facade could not load UNIT1_C0");
        require(runtime_scene.session() != nullptr
                    && runtime_scene.session()->add_player(0, 0) == 0,
                "root Scene facade did not create its session player");
        runtime_scene.tick();
        require(runtime_scene.state().frame_count == 1
                    && runtime_scene.session()->tick_count() == 1,
                "root Scene facade did not advance state and gameplay");
        require(runtime_scene.start_camera_sequence(
                    0, "cameraEditor/unit1_land_intro.bin",
                    fruityprime::camera::CameraState{
                        {100.0F, 100.0F, 100.0F}, {},
                        {100.0F, 100.0F, 101.0F},
                        {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F},
                        45.0F, 0.0F, {}}),
                "root Scene facade could not start a ROM camera sequence");
        require(runtime_scene.camera_sequence() != nullptr
                    && runtime_scene.camera_state() != nullptr,
                "root Scene facade did not expose camera playback state");
        runtime_scene.tick();
        require(runtime_scene.camera_state()->fov > 0.0F,
                "root Scene facade did not process camera playback");
        runtime_scene.stop_camera_sequence();
        require(runtime_scene.camera_sequence() == nullptr,
                "root Scene facade did not stop camera playback");
        runtime_scene.set_preview_camera({0.0F, 1.0F, -2.0F},
                                         {0.0F, 1.0F, 0.0F});
        require(runtime_scene.camera_state() != nullptr
                    && runtime_scene.camera_state()->facing.z > 0.99F,
                "root Scene facade did not install preview camera");
        runtime_scene.unload();
        require(!runtime_scene.loaded(),
                "root Scene facade did not unload its room");

        const auto& story_catalog = fruityprime::scene::story_rooms();
        require(story_catalog.size() == 66,
                "native story room catalog must contain 66 rooms");
        std::size_t story_entities = 0;
        std::size_t story_meshes = 0;
        std::size_t story_nodes = 0;
        std::size_t story_node_animation_groups = 0;
        std::size_t story_material_animation_groups = 0;
        std::size_t story_texcoord_animation_groups = 0;
        std::size_t story_texture_animation_groups = 0;
        std::size_t cretaphid_profiles = 0;
        std::size_t story_force_fields = 0;
        std::optional<fruityprime::scene::Room> enemy_test_room;
        fruityprime::net::Vec3 enemy_test_position{};
        std::uint8_t enemy_test_type = 0xff;
        std::optional<fruityprime::scene::RoomDefinition>
            blastcap_room_definition;
        fruityprime::net::Vec3 blastcap_position{};
        std::optional<fruityprime::scene::RoomDefinition>
            temroid_room_definition;
        fruityprime::net::Vec3 temroid_position{};
        std::optional<fruityprime::scene::RoomDefinition>
            carnivorous_plant_room_definition;
        fruityprime::net::Vec3 carnivorous_plant_position{};
        std::optional<fruityprime::scene::RoomDefinition>
            voldrum_room_definition;
        fruityprime::net::Vec3 voldrum_position{};
        std::uint8_t voldrum_type = 0xff;
        std::optional<fruityprime::scene::RoomDefinition>
            psychobit_room_definition;
        fruityprime::net::Vec3 psychobit_position{};
        std::optional<fruityprime::scene::RoomDefinition>
            firespawn_room_definition;
        fruityprime::net::Vec3 firespawn_position{};
        std::optional<fruityprime::scene::RoomDefinition>
            quadtroid_room_definition;
        fruityprime::net::Vec3 quadtroid_position{};
        std::optional<fruityprime::scene::RoomDefinition>
            crash_pillar_room_definition;
        fruityprime::net::Vec3 crash_pillar_position{};
        std::optional<fruityprime::scene::RoomDefinition>
            slench_room_definition;
        fruityprime::net::Vec3 slench_position{};
        std::optional<fruityprime::scene::RoomDefinition>
            force_field_room_definition;
        std::int16_t force_field_entity_id = -1;
        fruityprime::net::Vec3 force_field_position{};
        fruityprime::net::Vec3 force_field_facing{};
        std::uint32_t force_field_type = 9;
        float force_field_width = 0.0F;
        float force_field_height = 0.0F;
        EntityModelAudit entity_model_audit;
        for (std::size_t item_id = 0;
             item_id < fruityprime::metadata::ItemCount; ++item_id) {
            const auto* item = fruityprime::metadata::item_info(
                static_cast<std::int32_t>(item_id));
            if (item != nullptr) {
                entity_model_audit.inspect_model(assets, item->asset_name);
            }
        }
        for (const std::string_view name : {
                 std::string_view{"iceShard"},
                 std::string_view{"energyBeam"},
                 std::string_view{"trail"},
                 std::string_view{"electroTrail"},
                 std::string_view{"arcWelder"}}) {
            entity_model_audit.inspect_model(assets, name);
        }
        for (const std::string_view name : {
                 std::string_view{"iceWave"},
                 std::string_view{"sniperBeam"},
                 std::string_view{"cylBossLaserBurn"}}) {
            entity_model_audit.inspect_model(assets, name);
        }
        for (const auto& entry : story_catalog) {
            const auto prefix = room_prefix(entry);
            try {
                auto room = fruityprime::scene::Room::load(
                    assets, entry.definition);
                entity_model_audit.inspect_room(assets, room, prefix);
                for (const auto& entity : room.entities()) {
                    if (entity.kind != fruityprime::scene::EntityKind::EnemySpawn
                        || entity.type != static_cast<std::uint16_t>(
                            fruityprime::formats::EntityType::EnemySpawn)) {
                        continue;
                    }
                    const auto* spawn = std::get_if<
                        fruityprime::scene::EnemySpawnData>(&entity.typed_data);
                    if (spawn == nullptr
                        || spawn->enemy_type != static_cast<std::uint8_t>(
                            fruityprime::formats::EnemyType::Cretaphid)) {
                        continue;
                    }
                    const auto profile = fruityprime::enemy::decode_cretaphid_profile(
                        spawn->enemy_type, spawn->fields,
                        {entity.position.x.to_float(),
                         entity.position.y.to_float(),
                         entity.position.z.to_float()});
                    require(profile.supported && profile.crystal_health != 0
                                && profile.eye_health != 0
                                && profile.phases[0].eye_state.size() == 12,
                            prefix + "Cretaphid S05 profile did not decode");
                    ++cretaphid_profiles;
                }
                for (const auto& entity : room.entities()) {
                    if (entity.kind != fruityprime::scene::EntityKind::ForceField) {
                        continue;
                    }
                    const auto* field = std::get_if<
                        fruityprime::scene::ForceFieldData>(&entity.typed_data);
                    require(field != nullptr,
                            prefix + "force field has no typed payload");
                    require(std::isfinite(field->width)
                                && std::isfinite(field->height)
                                && field->width >= 0.0F
                                && field->height >= 0.0F,
                            prefix + "force field geometry is invalid");
                    ++story_force_fields;
                    if (!force_field_room_definition.has_value()
                        && field->type <= 8) {
                        force_field_room_definition = entry.definition;
                        force_field_entity_id = entity.entity_id;
                        force_field_position = {
                            entity.position.x.to_float(),
                            entity.position.y.to_float(),
                            entity.position.z.to_float()};
                        force_field_facing = {
                            entity.facing_vector.x.to_float(),
                            entity.facing_vector.y.to_float(),
                            entity.facing_vector.z.to_float()};
                        force_field_type = field->type;
                        force_field_width = field->width;
                        force_field_height = field->height;
                    }
                }
                require(room.model().header().mesh_count > 0,
                        prefix + "story model has no meshes");
                const auto story_collision_points = room.collision().is_mph()
                    ? room.collision().mph().points.size()
                    : room.collision().first_hunt().points.size();
                require(story_collision_points > 0,
                        prefix + "story collision has no points");
                require(room.has_entities() && !room.entities().empty(),
                        prefix + "story entity resource has no instances");
                require(room.has_animation(),
                        prefix + "story animation resource is empty");
                const auto& story_animations = room.model().animations();
                require(story_animations.node_groups.size()
                            == story_animations.node_group_offsets.size(),
                        prefix + "node animation table size mismatch");
                require(story_animations.material_groups.size()
                            == story_animations.material_group_offsets.size(),
                        prefix + "material animation table size mismatch");
                require(story_animations.texcoord_groups.size()
                            == story_animations.texcoord_group_offsets.size(),
                        prefix + "texcoord animation table size mismatch");
                require(story_animations.texture_groups.size()
                            == story_animations.texture_group_offsets.size(),
                        prefix + "texture animation table size mismatch");
                require(story_animations.any(),
                        prefix + "story animation resource decoded no groups");
                story_node_animation_groups += story_animations.node_groups.size();
                story_material_animation_groups
                    += story_animations.material_groups.size();
                story_texcoord_animation_groups
                    += story_animations.texcoord_groups.size();
                story_texture_animation_groups
                    += story_animations.texture_groups.size();
                if (!entry.definition.node_path.empty()) {
                    require(room.has_node_data()
                                && !room.node_data()->data().empty(),
                            prefix + "story node resource has no lists");
                    for (const auto& group : room.node_data()->data()) {
                        for (const auto& list : group) {
                            story_nodes += list.size();
                        }
                    }
                }
                story_meshes += room.model().meshes().size();
                story_entities += room.entities().size();

                if (!blastcap_room_definition.has_value()) {
                    for (const auto& entity : room.entities()) {
                        if (entity.kind
                                != fruityprime::scene::EntityKind::EnemySpawn) {
                            continue;
                        }
                        const auto* spawn = std::get_if<
                            fruityprime::scene::EnemySpawnData>(
                                &entity.typed_data);
                        if (spawn == nullptr || !spawn->active
                            || spawn->spawn_count == 0
                            || spawn->spawn_limit == 0
                            || spawn->enemy_type
                                != static_cast<std::uint8_t>(
                                    fruityprime::formats::EnemyType::Blastcap)) {
                            continue;
                        }
                        blastcap_room_definition = entry.definition;
                        blastcap_position = {
                            entity.position.x.to_float(),
                            entity.position.y.to_float(),
                            entity.position.z.to_float()};
                        break;
                    }
                }
                if (!temroid_room_definition.has_value()) {
                    for (const auto& entity : room.entities()) {
                        if (entity.kind
                                != fruityprime::scene::EntityKind::EnemySpawn) {
                            continue;
                        }
                        const auto* spawn = std::get_if<
                            fruityprime::scene::EnemySpawnData>(
                                &entity.typed_data);
                        if (spawn == nullptr || !spawn->active
                            || spawn->spawn_count == 0
                            || spawn->spawn_limit == 0
                            || spawn->enemy_type
                                != static_cast<std::uint8_t>(
                                    fruityprime::formats::EnemyType::Temroid)) {
                            continue;
                        }
                        temroid_room_definition = entry.definition;
                        temroid_position = {
                            entity.position.x.to_float(),
                            entity.position.y.to_float(),
                            entity.position.z.to_float()};
                        break;
                    }
                }
                if (!carnivorous_plant_room_definition.has_value()) {
                    for (const auto& entity : room.entities()) {
                        if (entity.kind
                                != fruityprime::scene::EntityKind::EnemySpawn) {
                            continue;
                        }
                        const auto* spawn = std::get_if<
                            fruityprime::scene::EnemySpawnData>(
                                &entity.typed_data);
                        if (spawn == nullptr || !spawn->active
                            || spawn->spawn_count == 0
                            || spawn->spawn_limit == 0
                            || spawn->enemy_type
                                != static_cast<std::uint8_t>(
                                    fruityprime::formats::EnemyType::CarnivorousPlant)) {
                            continue;
                        }
                        carnivorous_plant_room_definition = entry.definition;
                        carnivorous_plant_position = {
                            entity.position.x.to_float(),
                            entity.position.y.to_float(),
                            entity.position.z.to_float()};
                        break;
                    }
                }
                if (!voldrum_room_definition.has_value()) {
                    for (const auto& entity : room.entities()) {
                        if (entity.kind
                                != fruityprime::scene::EntityKind::EnemySpawn) {
                            continue;
                        }
                        const auto* spawn = std::get_if<
                            fruityprime::scene::EnemySpawnData>(
                                &entity.typed_data);
                        if (spawn == nullptr || !spawn->active
                            || spawn->spawn_count == 0
                            || spawn->spawn_limit == 0
                            || (spawn->enemy_type != static_cast<std::uint8_t>(
                                    fruityprime::formats::EnemyType::Voldrum2)
                                && spawn->enemy_type != static_cast<std::uint8_t>(
                                    fruityprime::formats::EnemyType::Voldrum1))) {
                            continue;
                        }
                        voldrum_room_definition = entry.definition;
                        voldrum_position = {
                            entity.position.x.to_float(),
                            entity.position.y.to_float(),
                            entity.position.z.to_float()};
                        voldrum_type = spawn->enemy_type;
                        break;
                    }
                }
                if (!psychobit_room_definition.has_value()) {
                    for (const auto& entity : room.entities()) {
                        if (entity.kind
                                != fruityprime::scene::EntityKind::EnemySpawn) {
                            continue;
                        }
                        const auto* spawn = std::get_if<
                            fruityprime::scene::EnemySpawnData>(
                                &entity.typed_data);
                        if (spawn == nullptr || !spawn->active
                            || spawn->spawn_count == 0
                            || spawn->spawn_limit == 0
                            || spawn->enemy_type != static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::PsychoBit1)) {
                            continue;
                        }
                        psychobit_room_definition = entry.definition;
                        psychobit_position = {
                            entity.position.x.to_float(),
                            entity.position.y.to_float(),
                            entity.position.z.to_float()};
                        break;
                    }
                }
                if (!firespawn_room_definition.has_value()) {
                    for (const auto& entity : room.entities()) {
                        if (entity.kind
                                != fruityprime::scene::EntityKind::EnemySpawn) {
                            continue;
                        }
                        const auto* spawn = std::get_if<
                            fruityprime::scene::EnemySpawnData>(
                                &entity.typed_data);
                        if (spawn == nullptr || !spawn->active
                            || spawn->spawn_count == 0
                            || spawn->spawn_limit == 0
                            || spawn->enemy_type != static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::FireSpawn)) {
                            continue;
                        }
                        firespawn_room_definition = entry.definition;
                        firespawn_position = {
                            entity.position.x.to_float(),
                            entity.position.y.to_float(),
                            entity.position.z.to_float()};
                        break;
                    }
                }
                if (!quadtroid_room_definition.has_value()) {
                    for (const auto& entity : room.entities()) {
                        if (entity.kind
                                != fruityprime::scene::EntityKind::EnemySpawn) {
                            continue;
                        }
                        const auto* spawn = std::get_if<
                            fruityprime::scene::EnemySpawnData>(
                                &entity.typed_data);
                        if (spawn == nullptr || !spawn->active
                            || spawn->spawn_count == 0
                            || spawn->spawn_limit == 0
                            || spawn->enemy_type != static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::Quadtroid)) {
                            continue;
                        }
                        quadtroid_room_definition = entry.definition;
                        quadtroid_position = {
                            entity.position.x.to_float(),
                            entity.position.y.to_float(),
                            entity.position.z.to_float()};
                        break;
                    }
                }
                if (!crash_pillar_room_definition.has_value()) {
                    for (const auto& entity : room.entities()) {
                        if (entity.kind
                                != fruityprime::scene::EntityKind::EnemySpawn) {
                            continue;
                        }
                        const auto* spawn = std::get_if<
                            fruityprime::scene::EnemySpawnData>(
                                &entity.typed_data);
                        if (spawn == nullptr || !spawn->active
                            || spawn->spawn_count == 0
                            || spawn->spawn_limit == 0
                            || spawn->enemy_type != static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::CrashPillar)) {
                            continue;
                        }
                        crash_pillar_room_definition = entry.definition;
                        crash_pillar_position = {
                            entity.position.x.to_float(),
                            entity.position.y.to_float(),
                            entity.position.z.to_float()};
                        break;
                    }
                }
                if (!slench_room_definition.has_value()) {
                    for (const auto& entity : room.entities()) {
                        if (entity.kind
                                != fruityprime::scene::EntityKind::EnemySpawn) {
                            continue;
                        }
                        const auto* spawn = std::get_if<
                            fruityprime::scene::EnemySpawnData>(
                                &entity.typed_data);
                        if (spawn == nullptr || !spawn->active
                            || spawn->spawn_count == 0
                            || spawn->spawn_limit == 0
                            || spawn->enemy_type != static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::Slench)) {
                            continue;
                        }
                        slench_room_definition = entry.definition;
                        slench_position = {
                            entity.position.x.to_float(),
                            entity.position.y.to_float(),
                            entity.position.z.to_float()};
                        break;
                    }
                }
                if (!enemy_test_room.has_value()) {
                    for (const auto& entity : room.entities()) {
                        if (entity.kind
                                != fruityprime::scene::EntityKind::EnemySpawn) {
                            continue;
                        }
                        const auto* spawn = std::get_if<
                            fruityprime::scene::EnemySpawnData>(
                                &entity.typed_data);
                        if (spawn == nullptr || !spawn->active
                            || spawn->spawn_count == 0
                            || spawn->spawn_limit == 0
                            || spawn->active_distance <= 0.0F
                            || spawn->enemy_type == 40) {
                            continue;
                        }
                        const auto tuning = fruityprime::enemy::combat_tuning(
                            spawn->enemy_type);
                        if (tuning.contact_damage == 0) {
                            continue;
                        }
                        enemy_test_position = {
                            entity.position.x.to_float(),
                            entity.position.y.to_float(),
                            entity.position.z.to_float()};
                        enemy_test_type = spawn->enemy_type;
                        enemy_test_room.emplace(std::move(room));
                        break;
                    }
                }
            } catch (const std::exception& error) {
                throw std::runtime_error(prefix + error.what());
            }
        }
        require(story_force_fields > 0,
                "story catalog has no ForceField records");
        require(force_field_room_definition.has_value()
                    && force_field_entity_id >= 0,
                "story catalog has no supported ForceField record");
        require(enemy_test_room.has_value(),
                "story catalog has no active contact-damage enemy spawner");
        fruityprime::gameplay::Config enemy_config;
        enemy_config.mode = 2; // GameMode.SinglePlayer
        enemy_config.gravity = 0.0F;
        enemy_config.walk_speed = 0.0F;
        enemy_config.air_acceleration = 0.0F;
        enemy_config.world_padding = 0.0F;
        fruityprime::gameplay::Session enemy_session(
            *enemy_test_room, enemy_config);
        static_cast<void>(enemy_session.add_player(0, 0));
        auto enemy_setup = enemy_session.snapshot(0);
        enemy_setup.players[0].position = enemy_test_position;
        enemy_session.apply_snapshot(enemy_setup);
        enemy_session.set_input(0, fruityprime::gameplay::Input{});
        for (int i = 0; i < 600 && enemy_session.enemies().empty(); ++i) {
            enemy_session.tick();
        }
        require(!enemy_session.enemies().empty(),
                "active story enemy spawner did not create an enemy");
        const auto& enemy = enemy_session.enemies().front();
        const bool is_surface_follower = enemy.enemy_type
            == static_cast<std::uint8_t>(fruityprime::formats::EnemyType::Zoomer)
            || enemy.enemy_type == static_cast<std::uint8_t>(
                fruityprime::formats::EnemyType::Geemer);
        require(enemy.enemy_type == enemy_test_type
                    && (is_surface_follower
                        ? enemy.target_slot == 0xff && enemy.state == 0
                        : enemy.target_slot == 0 && enemy.state != 0),
                "story enemy did not execute its target or surface behavior");
        require(enemy_session.player(0).health < enemy_config.max_health,
                "story enemy did not apply contact damage");
        std::cout << "real enemy simulation: type="
                  << static_cast<unsigned>(enemy.enemy_type)
                  << " health=" << enemy_session.player(0).health
                  << " active=" << enemy_session.enemies().size() << '\n';

        if (blastcap_room_definition.has_value()) {
            const auto blastcap_room = fruityprime::scene::Room::load(
                assets, *blastcap_room_definition);
            fruityprime::gameplay::Session blastcap_session(
                blastcap_room, enemy_config);
            static_cast<void>(blastcap_session.add_player(0, 0));
            auto blastcap_setup = blastcap_session.snapshot(0);
            blastcap_setup.players[0].position = blastcap_position;
            blastcap_session.apply_snapshot(blastcap_setup);
            blastcap_session.set_input(0, fruityprime::gameplay::Input{});
            const auto find_blastcap = [&blastcap_session]() {
                return std::find_if(
                    blastcap_session.enemies().begin(),
                    blastcap_session.enemies().end(),
                    [](const auto& value) {
                        return value.enemy_type
                            == static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::Blastcap);
                    });
            };
            for (int i = 0; i < 600
                    && find_blastcap() == blastcap_session.enemies().end(); ++i) {
                blastcap_session.tick();
            }
            const auto blastcap = find_blastcap();
            require(blastcap != blastcap_session.enemies().end(),
                    "active Blastcap spawner did not create an enemy");
            const auto blastcap_id = blastcap->id;
            require(blastcap_session.damage_enemy(blastcap_id, 999),
                    "Blastcap did not accept the lethal hit");
            const auto armed_blastcap = find_blastcap();
            require(armed_blastcap != blastcap_session.enemies().end()
                        && !armed_blastcap->visible
                        && armed_blastcap->health == 1,
                    "Blastcap lethal hit did not arm its invisible cloud");
            const auto health_after_initial_cloud =
                blastcap_session.player(0).health;
            require(health_after_initial_cloud == enemy_config.max_health - 2,
                    "Blastcap did not apply its immediate cloud damage");
            for (int i = 0; i < 20; ++i) {
                blastcap_session.set_input(0, fruityprime::gameplay::Input{});
                blastcap_session.tick();
            }
            require(blastcap_session.player(0).health
                        == health_after_initial_cloud - 2,
                    "Blastcap cloud did not apply its periodic damage");
            for (int i = 0; i < 310
                    && !blastcap_session.enemies().empty(); ++i) {
                blastcap_session.set_input(0, fruityprime::gameplay::Input{});
                blastcap_session.tick();
            }
            const auto lingering_blastcap = std::find_if(
                blastcap_session.enemies().begin(),
                blastcap_session.enemies().end(),
                [blastcap_id](const auto& value) {
                    return value.id == blastcap_id;
                });
            if (lingering_blastcap != blastcap_session.enemies().end()) {
                throw std::runtime_error(
                    "Blastcap cloud did not expire into spawner destruction"
                    " (health="
                    + std::to_string(lingering_blastcap->health)
                    + ", timer="
                    + std::to_string(lingering_blastcap->blastcap_cloud_timer)
                    + ", exploded="
                    + std::to_string(lingering_blastcap->blastcap_exploded)
                    + ")");
            }
            std::cout << "real Blastcap simulation: cloud_damage=2 expired=1\n";
        } else {
            std::cout << "real Blastcap simulation skipped: no active story spawner\n";
        }
        if (temroid_room_definition.has_value()) {
            const auto temroid_room = fruityprime::scene::Room::load(
                assets, *temroid_room_definition);
            fruityprime::gameplay::Session temroid_session(
                temroid_room, enemy_config);
            static_cast<void>(temroid_session.add_player(0, 0));
            auto temroid_setup = temroid_session.snapshot(0);
            temroid_setup.players[0].position = temroid_position;
            temroid_session.apply_snapshot(temroid_setup);
            temroid_session.set_input(0, fruityprime::gameplay::Input{});
            for (int i = 0; i < 600; ++i) {
                const auto found = std::find_if(
                    temroid_session.enemies().begin(),
                    temroid_session.enemies().end(),
                    [](const auto& value) {
                        return value.enemy_type
                            == static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::Temroid);
                    });
                if (found != temroid_session.enemies().end()) {
                    auto close_setup = temroid_session.snapshot(0);
                    close_setup.players[0].position = found->position;
                    temroid_session.apply_snapshot(close_setup);
                    temroid_session.set_input(
                        0, fruityprime::gameplay::Input{});
                    break;
                }
                temroid_session.tick();
            }
            auto temroid = std::find_if(
                temroid_session.enemies().begin(),
                temroid_session.enemies().end(),
                [](const auto& value) {
                    return value.enemy_type
                        == static_cast<std::uint8_t>(
                            fruityprime::formats::EnemyType::Temroid);
                });
            require(temroid != temroid_session.enemies().end(),
                    "active Temroid spawner did not create an enemy");
            temroid_session.tick();
            temroid = std::find_if(
                temroid_session.enemies().begin(),
                temroid_session.enemies().end(),
                [](const auto& value) {
                    return value.enemy_type
                        == static_cast<std::uint8_t>(
                            fruityprime::formats::EnemyType::Temroid);
                });
            require(temroid != temroid_session.enemies().end()
                        && temroid->state == 8
                        && temroid->target_slot == 0
                        && temroid_session.player(0).health < enemy_config.max_health,
                    "Temroid did not enter its attached contact state");
            const auto health_after_attach = temroid_session.player(0).health;
            for (int i = 0; i < 16; ++i) {
                temroid_session.set_input(
                    0, fruityprime::gameplay::Input{});
                temroid_session.tick();
            }
            require(temroid_session.player(0).health < health_after_attach,
                    "Temroid attached state did not drain health");
            std::cout << "real Temroid simulation: attached=1 drain=2\n";
        } else {
            std::cout << "real Temroid simulation skipped: no active story spawner\n";
        }
        if (carnivorous_plant_room_definition.has_value()) {
            const auto plant_room = fruityprime::scene::Room::load(
                assets, *carnivorous_plant_room_definition);
            fruityprime::gameplay::Session plant_session(
                plant_room, enemy_config);
            static_cast<void>(plant_session.add_player(0, 0));
            auto plant_setup = plant_session.snapshot(0);
            plant_setup.players[0].position = carnivorous_plant_position;
            plant_session.apply_snapshot(plant_setup);
            plant_session.set_input(0, fruityprime::gameplay::Input{});
            for (int i = 0; i < 600
                    && plant_session.enemies().empty(); ++i) {
                plant_session.tick();
            }
            const auto plant = std::find_if(
                plant_session.enemies().begin(), plant_session.enemies().end(),
                [](const auto& value) {
                    return value.enemy_type
                        == static_cast<std::uint8_t>(
                            fruityprime::formats::EnemyType::CarnivorousPlant);
                });
            require(plant != plant_session.enemies().end()
                        && plant->carnivorous_plant.supported
                        && plant->health_max == plant->carnivorous_plant.health
                        && plant->carnivorous_plant.damage > 0,
                    "active Carnivorous Plant spawner did not decode S07");
            require(plant_session.player(0).health < enemy_config.max_health,
                    "Carnivorous Plant did not apply decoded contact damage");
            std::cout << "real Carnivorous Plant simulation: damage="
                      << plant->carnivorous_plant.damage << " model="
                      << plant->carnivorous_plant.model_name << '\n';
        } else {
            std::cout << "real Carnivorous Plant simulation skipped: no active story spawner\n";
        }
        if (voldrum_room_definition.has_value()) {
            const auto voldrum_room = fruityprime::scene::Room::load(
                assets, *voldrum_room_definition);
            fruityprime::gameplay::Session voldrum_session(
                voldrum_room, enemy_config);
            static_cast<void>(voldrum_session.add_player(0, 0));
            auto voldrum_setup = voldrum_session.snapshot(0);
            voldrum_setup.players[0].position = voldrum_position;
            voldrum_session.apply_snapshot(voldrum_setup);
            voldrum_session.set_input(0, fruityprime::gameplay::Input{});
            const auto find_voldrum = [&voldrum_session]() {
                return std::find_if(
                    voldrum_session.enemies().begin(),
                    voldrum_session.enemies().end(),
                    [](const auto& value) {
                        return value.voldrum.supported;
                    });
            };
            for (int i = 0; i < 600
                    && find_voldrum() == voldrum_session.enemies().end(); ++i) {
                voldrum_session.tick();
            }
            auto voldrum = find_voldrum();
            require(voldrum != voldrum_session.enemies().end()
                        && voldrum->enemy_type == voldrum_type,
                    "active Voldrum spawner did not decode its typed profile");
            auto close_setup = voldrum_session.snapshot(0);
            close_setup.players[0].position = voldrum->position;
            voldrum_session.apply_snapshot(close_setup);
            bool saw_target_state = false;
            bool saw_contact_damage = false;
            bool fired = false;
            for (int i = 0; i < 180; ++i) {
                voldrum_session.set_input(0, fruityprime::gameplay::Input{});
                voldrum_session.tick();
                const auto current = find_voldrum();
                saw_target_state = saw_target_state
                    || (current != voldrum_session.enemies().end()
                        && current->target_slot == 0 && current->state != 0);
                saw_contact_damage = saw_contact_damage
                    || voldrum_session.player(0).health
                        < enemy_config.max_health;
                fired = fired || std::any_of(
                    voldrum_session.sound_events().begin(),
                    voldrum_session.sound_events().end(),
                    [](const auto& event) {
                        return event.cue == fruityprime::gameplay::SoundCue::BeamShot
                            && event.enemy_type == static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::Voldrum1);
                    });
            }
            voldrum = find_voldrum();
            require(voldrum != voldrum_session.enemies().end()
                        && saw_target_state && saw_contact_damage,
                    "Voldrum did not execute its target/contact state");
            if (voldrum_type == static_cast<std::uint8_t>(
                    fruityprime::formats::EnemyType::Voldrum1)) {
                require(fired,
                        "Voldrum1 did not emit a paired enemy beam volley");
            }
            std::cout << "real Voldrum simulation: type="
                      << static_cast<unsigned>(voldrum_type)
                      << " health=" << voldrum_session.player(0).health
                      << " beams=" << (fired ? 1 : 0) << '\n';
        } else {
            std::cout << "real Voldrum simulation skipped: no active story spawner\n";
        }
        if (psychobit_room_definition.has_value()) {
            const auto psychobit_room = fruityprime::scene::Room::load(
                assets, *psychobit_room_definition);
            fruityprime::gameplay::Session psychobit_session(
                psychobit_room, enemy_config);
            static_cast<void>(psychobit_session.add_player(0, 0));
            auto psychobit_setup = psychobit_session.snapshot(0);
            psychobit_setup.players[0].position = psychobit_position;
            psychobit_session.apply_snapshot(psychobit_setup);
            psychobit_session.set_input(0, fruityprime::gameplay::Input{});
            const auto find_psychobit = [&psychobit_session]() {
                return std::find_if(
                    psychobit_session.enemies().begin(),
                    psychobit_session.enemies().end(),
                    [](const auto& value) {
                        return value.psychobit.supported;
                    });
            };
            for (int i = 0; i < 600
                    && find_psychobit()
                        == psychobit_session.enemies().end(); ++i) {
                psychobit_session.tick();
            }
            auto psychobit = find_psychobit();
            require(psychobit != psychobit_session.enemies().end(),
                    "active PsychoBit spawner did not decode S06");
            const auto psychobit_profile = psychobit->psychobit;
            const auto psychobit_spawned_position = psychobit->position;

            auto contact_setup = psychobit_session.snapshot(0);
            contact_setup.players[0].position = psychobit_spawned_position;
            psychobit_session.apply_snapshot(contact_setup);
            psychobit_session.set_input(0, fruityprime::gameplay::Input{});
            psychobit_session.tick();
            const bool saw_contact_damage =
                psychobit_session.player(0).health < enemy_config.max_health;

            auto range_setup = psychobit_session.snapshot(0);
            const auto range_center = psychobit->psychobit.range_volume.center();
            range_setup.players[0].position = {
                range_center.x, range_center.y, range_center.z};
            psychobit_session.apply_snapshot(range_setup);
            bool saw_target_state = false;
            bool fired = false;
            for (int i = 0; i < 240; ++i) {
                psychobit_session.set_input(
                    0, fruityprime::gameplay::Input{});
                psychobit_session.tick();
                const auto current = find_psychobit();
                saw_target_state = saw_target_state
                    || (current != psychobit_session.enemies().end()
                        && current->target_slot == 0 && current->state == 3);
                fired = fired || std::any_of(
                    psychobit_session.sound_events().begin(),
                    psychobit_session.sound_events().end(),
                    [](const auto& event) {
                        return event.cue
                                == fruityprime::gameplay::SoundCue::BeamShot
                            && event.enemy_type == static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::PsychoBit1);
                    });
            }
            psychobit = find_psychobit();
            require(psychobit != psychobit_session.enemies().end()
                        && saw_contact_damage && saw_target_state && fired,
                    "PsychoBit did not execute contact, target, and beam states");
            std::cout << "real PsychoBit simulation: health="
                      << psychobit_profile.health << " beams=" << (fired ? 1 : 0)
                      << '\n';
        } else {
            std::cout << "real PsychoBit simulation skipped: no active story spawner\n";
        }
        if (firespawn_room_definition.has_value()) {
            const auto firespawn_room = fruityprime::scene::Room::load(
                assets, *firespawn_room_definition);
            fruityprime::gameplay::Session firespawn_session(
                firespawn_room, enemy_config);
            static_cast<void>(firespawn_session.add_player(0, 0));
            auto firespawn_setup = firespawn_session.snapshot(0);
            firespawn_setup.players[0].position = firespawn_position;
            firespawn_session.apply_snapshot(firespawn_setup);
            firespawn_session.set_input(0, fruityprime::gameplay::Input{});
            const auto find_firespawn = [&firespawn_session]() {
                return std::find_if(
                    firespawn_session.enemies().begin(),
                    firespawn_session.enemies().end(),
                    [](const auto& value) {
                        return value.firespawn.supported;
                    });
            };
            for (int i = 0; i < 600
                    && find_firespawn()
                        == firespawn_session.enemies().end(); ++i) {
                firespawn_session.tick();
            }
            auto firespawn = find_firespawn();
            require(firespawn != firespawn_session.enemies().end(),
                    "active Fire Spawn spawner did not decode S06");
            const auto firespawn_profile = firespawn->firespawn;
            const auto firespawn_id = firespawn->id;
            const auto find_firespawn_hit_zone = [&firespawn_session,
                                                   firespawn_id]() {
                return std::find_if(
                    firespawn_session.enemies().begin(),
                    firespawn_session.enemies().end(),
                    [firespawn_id](const auto& value) {
                        return value.parent_enemy_id == firespawn_id
                            && value.enemy_type == static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::HitZone);
                    });
            };
            require(find_firespawn_hit_zone()
                        != firespawn_session.enemies().end(),
                    "Fire Spawn did not create its linked HitZone");
            const auto firespawn_active_center =
                firespawn_profile.active_volume.center();
            auto active_setup = firespawn_session.snapshot(0);
            active_setup.players[0].position = {
                firespawn_active_center.x, firespawn_active_center.y,
                firespawn_active_center.z};
            firespawn_session.apply_snapshot(active_setup);
            bool surfaced = false;
            bool fired = false;
            bool hit_zone_collidable = false;
            bool hit_zone_forwarded_damage = false;
            for (int i = 0; i < 240; ++i) {
                firespawn_session.set_input(
                    0, fruityprime::gameplay::Input{});
                firespawn_session.tick();
                firespawn = find_firespawn();
                if (firespawn != firespawn_session.enemies().end()) {
                    surfaced = surfaced || !firespawn->firespawn_submerged;
                    const auto hit_zone = find_firespawn_hit_zone();
                    if (hit_zone != firespawn_session.enemies().end()) {
                        hit_zone_collidable = hit_zone_collidable
                            || hit_zone->hit_zone_collidable;
                        if (!hit_zone_forwarded_damage
                            && hit_zone->hit_zone_collidable) {
                            const auto parent_id = firespawn->id;
                            const auto parent_health = firespawn->health;
                            const bool accepted = firespawn_session.damage_enemy(
                                hit_zone->id, 1);
                            const auto parent_after = std::find_if(
                                firespawn_session.enemies().begin(),
                                firespawn_session.enemies().end(),
                                [parent_id](const auto& value) {
                                    return value.id == parent_id;
                                });
                            hit_zone_forwarded_damage = accepted
                                && parent_after
                                    != firespawn_session.enemies().end()
                                && parent_after->health < parent_health;
                        }
                    }
                }
                fired = fired || std::any_of(
                    firespawn_session.sound_events().begin(),
                    firespawn_session.sound_events().end(),
                    [](const auto& event) {
                        return event.cue
                                == fruityprime::gameplay::SoundCue::BeamShot
                            && event.enemy_type == static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::FireSpawn);
                    });
            }
            require(surfaced && fired && hit_zone_collidable
                        && hit_zone_forwarded_damage,
                    "Fire Spawn did not execute surface, HitZone, and fire states");
            std::cout << "real Fire Spawn simulation: health="
                      << firespawn_profile.health << " beams="
                      << (fired ? 1 : 0) << " hit_zone="
                      << (hit_zone_collidable ? 1 : 0) << " forwarded="
                      << (hit_zone_forwarded_damage ? 1 : 0) << '\n';
        } else {
            std::cout << "real Fire Spawn simulation skipped: no active story spawner\n";
        }
        if (quadtroid_room_definition.has_value()) {
            const auto quadtroid_room = fruityprime::scene::Room::load(
                assets, *quadtroid_room_definition);
            fruityprime::gameplay::Session quadtroid_session(
                quadtroid_room, enemy_config);
            static_cast<void>(quadtroid_session.add_player(0, 0));
            auto quadtroid_setup = quadtroid_session.snapshot(0);
            quadtroid_setup.players[0].position = quadtroid_position;
            quadtroid_session.apply_snapshot(quadtroid_setup);
            quadtroid_session.set_input(0, fruityprime::gameplay::Input{});
            const auto find_quadtroid = [&quadtroid_session]() {
                return std::find_if(
                    quadtroid_session.enemies().begin(),
                    quadtroid_session.enemies().end(),
                    [](const auto& value) {
                        return value.quadtroid.supported;
                    });
            };
            for (int i = 0; i < 600
                    && find_quadtroid() == quadtroid_session.enemies().end();
                 ++i) {
                quadtroid_session.tick();
            }
            auto quadtroid = find_quadtroid();
            require(quadtroid != quadtroid_session.enemies().end(),
                    "active Quadtroid spawner did not create an enemy");
            auto close_setup = quadtroid_session.snapshot(0);
            close_setup.players[0].position = quadtroid->position;
            close_setup.players[0].flags |=
                fruityprime::net::PlayerState::FlagAltForm;
            quadtroid_session.apply_snapshot(close_setup);
            bool saw_target_state = false;
            bool saw_contact_damage = false;
            for (int i = 0; i < 120; ++i) {
                quadtroid_session.set_input(0, fruityprime::gameplay::Input{});
                quadtroid_session.tick();
                quadtroid = find_quadtroid();
                saw_target_state = saw_target_state
                    || (quadtroid != quadtroid_session.enemies().end()
                        && quadtroid->target_slot == 0
                        && quadtroid->state != 0);
                saw_contact_damage = saw_contact_damage
                    || quadtroid_session.player(0).health
                        < enemy_config.max_health;
            }
            require(quadtroid != quadtroid_session.enemies().end()
                        && saw_target_state && saw_contact_damage,
                    "Quadtroid did not execute its target/contact state");
            std::cout << "real Quadtroid simulation: contact_damage=3 target=1\n";
        } else {
            std::cout << "real Quadtroid simulation skipped: no active story spawner\n";
        }
        if (crash_pillar_room_definition.has_value()) {
            const auto crash_pillar_room = fruityprime::scene::Room::load(
                assets, *crash_pillar_room_definition);
            fruityprime::gameplay::Session crash_pillar_session(
                crash_pillar_room, enemy_config);
            static_cast<void>(crash_pillar_session.add_player(0, 0));
            auto crash_pillar_setup = crash_pillar_session.snapshot(0);
            crash_pillar_setup.players[0].position = crash_pillar_position;
            crash_pillar_session.apply_snapshot(crash_pillar_setup);
            crash_pillar_session.set_input(
                0, fruityprime::gameplay::Input{});
            const auto find_crash_pillar = [&crash_pillar_session]() {
                return std::find_if(
                    crash_pillar_session.enemies().begin(),
                    crash_pillar_session.enemies().end(),
                    [](const auto& value) {
                        return value.crash_pillar.supported;
                    });
            };
            for (int i = 0; i < 600
                    && find_crash_pillar()
                        == crash_pillar_session.enemies().end(); ++i) {
                crash_pillar_session.tick();
            }
            auto crash_pillar = find_crash_pillar();
            require(crash_pillar != crash_pillar_session.enemies().end(),
                    "active CrashPillar spawner did not create an enemy");
            auto close_setup = crash_pillar_session.snapshot(0);
            close_setup.players[0].position = crash_pillar->position;
            crash_pillar_session.apply_snapshot(close_setup);
            bool saw_target_state = false;
            bool saw_contact_damage = false;
            for (int i = 0; i < 120; ++i) {
                crash_pillar_session.set_input(
                    0, fruityprime::gameplay::Input{});
                crash_pillar_session.tick();
                crash_pillar = find_crash_pillar();
                saw_target_state = saw_target_state
                    || (crash_pillar != crash_pillar_session.enemies().end()
                        && crash_pillar->target_slot == 0
                        && crash_pillar->state != 0);
                saw_contact_damage = saw_contact_damage
                    || crash_pillar_session.player(0).health
                        < enemy_config.max_health;
            }
            require(crash_pillar != crash_pillar_session.enemies().end()
                        && saw_target_state && saw_contact_damage,
                    "CrashPillar did not execute its target/contact state");
            std::cout << "real CrashPillar simulation: contact_damage=10 target=1\n";
        } else {
            std::cout << "real CrashPillar simulation skipped: no active story spawner\n";
        }
        if (slench_room_definition.has_value()) {
            const auto slench_room = fruityprime::scene::Room::load(
                assets, *slench_room_definition);
            fruityprime::gameplay::Session slench_session(
                slench_room, enemy_config);
            static_cast<void>(slench_session.add_player(0, 0));
            auto slench_setup = slench_session.snapshot(0);
            slench_setup.players[0].position = slench_position;
            slench_session.apply_snapshot(slench_setup);
            slench_session.set_input(0, fruityprime::gameplay::Input{});
            const auto find_slench = [&slench_session]() {
                return std::find_if(
                    slench_session.enemies().begin(),
                    slench_session.enemies().end(),
                    [](const auto& value) {
                        return value.slench.supported;
                    });
            };
            for (int i = 0; i < 600
                    && find_slench() == slench_session.enemies().end();
                 ++i) {
                slench_session.tick();
            }
            auto slench = find_slench();
            require(slench != slench_session.enemies().end(),
                    "active Slench spawner did not decode its typed profile");
            const auto slench_profile = slench->slench;
            const auto slench_id = slench->id;
            const auto child_count = std::count_if(
                slench_session.enemies().begin(), slench_session.enemies().end(),
                [slench_id](const auto& value) {
                    return value.parent_enemy_id == slench_id;
                });
            require(child_count == 4,
                    "Slench did not create its shield and three synapses");
            bool saw_boss_state = false;
            bool saw_roam_state = false;
            bool saw_vulnerable_state = false;
            bool saw_synapse_idle = false;
            bool saw_synapse_damage = false;
            bool saw_contact_damage = false;
            bool fired = false;
            bool damaged_synapse = false;
            bool saw_indexed_turret = false;
            bool saw_turret_activation = false;
            bool fired_turret = false;
            for (int i = 0; i < 900; ++i) {
                slench_session.set_input(
                    0, fruityprime::gameplay::Input{});
                slench_session.tick();
                slench = find_slench();
                saw_boss_state = saw_boss_state
                    || (slench != slench_session.enemies().end()
                        && slench->slench_state != 0);
                saw_roam_state = saw_roam_state
                    || (slench != slench_session.enemies().end()
                        && slench->slench_state == 10);
                saw_vulnerable_state = saw_vulnerable_state
                    || (slench != slench_session.enemies().end()
                        && !slench->slench_shielded);
                saw_contact_damage = saw_contact_damage
                    || slench_session.player(0).health
                        < enemy_config.max_health;
                const auto synapse = std::find_if(
                    slench_session.enemies().begin(),
                    slench_session.enemies().end(),
                    [slench_id](const auto& value) {
                        return value.parent_enemy_id == slench_id
                            && value.slench_synapse.supported;
                    });
                if (synapse != slench_session.enemies().end()) {
                    saw_synapse_idle = saw_synapse_idle
                        || synapse->slench_part_state == 2; // Idle
                    if (!damaged_synapse
                        && synapse->slench_part_state == 2) {
                        damaged_synapse = slench_session.damage_enemy(
                            synapse->id, 1);
                    }
                    saw_synapse_damage = saw_synapse_damage
                        || synapse->slench_part_state == 3 // Damaged
                        || synapse->slench_part_state == 4; // Dying
                }
                for (const auto& enemy : slench_session.enemies()) {
                    if (enemy.enemy_type != static_cast<std::uint8_t>(
                            fruityprime::formats::EnemyType::SlenchTurret)) {
                        continue;
                    }
                    saw_indexed_turret = saw_indexed_turret
                        || enemy.turret_index >= 0;
                    saw_turret_activation = saw_turret_activation
                        || enemy.turret_enabled;
                }
                fired = fired || std::any_of(
                    slench_session.sound_events().begin(),
                    slench_session.sound_events().end(),
                    [](const auto& event) {
                        return event.cue
                                == fruityprime::gameplay::SoundCue::BeamShot
                            && event.enemy_type == static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::Slench);
                    });
                fired_turret = fired_turret || std::any_of(
                    slench_session.sound_events().begin(),
                    slench_session.sound_events().end(),
                    [](const auto& event) {
                        return event.cue
                                == fruityprime::gameplay::SoundCue::TurretAttack
                            && event.enemy_type == static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::SlenchTurret);
                    });
            }
            require(slench != slench_session.enemies().end()
                        && saw_boss_state && saw_roam_state
                        && saw_vulnerable_state && saw_contact_damage && fired
                        && saw_synapse_idle && damaged_synapse
                        && saw_synapse_damage && saw_indexed_turret
                        && saw_turret_activation && fired_turret,
                    "Slench did not execute its child shield/synapse and contact states");
            std::cout << "real Slench simulation: subtype="
                      << static_cast<unsigned>(slench_profile.subtype)
                      << " health=" << slench_profile.health
                      << " beams=" << (fired ? 1 : 0)
                      << " vulnerable=" << (saw_vulnerable_state ? 1 : 0)
                      << " children=" << child_count
                      << " synapse_damage=" << (damaged_synapse ? 1 : 0)
                      << '\n';
        } else {
            std::cout << "real Slench simulation skipped: no active story spawner\n";
        }
        if (const auto* gorea_entry = fruityprime::scene::find_room(
                "Gorea_b1"); gorea_entry != nullptr) {
            const auto gorea_room = fruityprime::scene::Room::load(
                assets, gorea_entry->definition);
            std::optional<fruityprime::net::Vec3> gorea_position;
            for (const auto& entity : gorea_room.entities()) {
                if (entity.kind != fruityprime::scene::EntityKind::EnemySpawn) {
                    continue;
                }
                const auto* spawn = std::get_if<
                    fruityprime::scene::EnemySpawnData>(&entity.typed_data);
                if (spawn == nullptr || !spawn->active
                    || spawn->enemy_type != static_cast<std::uint8_t>(
                        fruityprime::formats::EnemyType::Gorea1A)) {
                    continue;
                }
                gorea_position = {
                    entity.position.x.to_float(),
                    entity.position.y.to_float(),
                    entity.position.z.to_float()};
                break;
            }
            require(gorea_position.has_value(),
                    "Gorea_b1 has no active Gorea1A spawner");
            auto gorea_model = fruityprime::assets::try_load_named_model(
                assets, "Gorea1A_lod0");
            require(gorea_model.has_value()
                        && gorea_model->model.animations().any(),
                    "Gorea1A attachment model or animation is missing");
            auto gorea1b_model = fruityprime::assets::try_load_named_model(
                assets, "Gorea1B_lod0");
            require(gorea1b_model.has_value()
                        && gorea1b_model->model.animations().any(),
                    "Gorea1B attachment model or animation is missing");
            fruityprime::gameplay::Session gorea_session(
                gorea_room, enemy_config);
            gorea_session.set_gorea1a_model(&gorea_model->model);
            gorea_session.set_gorea1b_model(&gorea1b_model->model);
            static_cast<void>(gorea_session.add_player(0, 0));
            auto gorea_setup = gorea_session.snapshot(0);
            gorea_setup.players[0].position = *gorea_position;
            gorea_session.apply_snapshot(gorea_setup);
            gorea_session.set_input(0, fruityprime::gameplay::Input{});
            const auto find_gorea_body = [&gorea_session]() {
                return std::find_if(
                    gorea_session.enemies().begin(), gorea_session.enemies().end(),
                    [](const auto& value) {
                        return value.active
                            && value.enemy_type == static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::Gorea1A);
                    });
            };
            for (int i = 0; i < 600
                    && find_gorea_body() == gorea_session.enemies().end();
                 ++i) {
                gorea_session.tick();
            }
            const auto gorea_body = find_gorea_body();
            require(gorea_body != gorea_session.enemies().end(),
                    "Gorea1A spawner did not create the boss body");
            const auto gorea_id = gorea_body->id;
            const auto find_gorea_head = [&gorea_session, gorea_id]() {
                return std::find_if(
                    gorea_session.enemies().begin(), gorea_session.enemies().end(),
                    [gorea_id](const auto& value) {
                        return value.active
                            && value.parent_enemy_id == gorea_id
                            && value.enemy_type == static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::GoreaHead);
                    });
            };
            const auto gorea_head = find_gorea_head();
            require(gorea_head != gorea_session.enemies().end()
                        && gorea_head->gorea_targetable
                        && !gorea_head->visible
                        && std::fabs(gorea_head->body_radius
                                     - 1314.0F / 4096.0F) < 0.0001F,
                    "Gorea head did not preserve its invisible beam target");
            const auto gorea_head_id = gorea_head->id;
            const fruityprime::net::Vec3 initial_head_position =
                gorea_head->position;
            require(gorea_session.damage_enemy(gorea_head_id, 1000),
                    "Gorea head damage was not accepted");
            const auto damaged_gorea_head = find_gorea_head();
            require(damaged_gorea_head != gorea_session.enemies().end()
                        && damaged_gorea_head->gorea_damage == 1000
                        && damaged_gorea_head->health == 65535,
                    "Gorea head did not retain its sentinel damage contract");
            const auto find_gorea_arms = [&gorea_session, gorea_id]() {
                std::vector<std::uint32_t> ids;
                for (const auto& value : gorea_session.enemies()) {
                    if (value.active && value.parent_enemy_id == gorea_id
                        && value.enemy_type == static_cast<std::uint8_t>(
                            fruityprime::formats::EnemyType::GoreaArm)) {
                        ids.push_back(value.id);
                    }
                }
                return ids;
            };
            const auto gorea_arm_ids = find_gorea_arms();
            require(gorea_arm_ids.size() == 2,
                    "Gorea1A did not create both shoulder parts");
            const auto find_gorea_legs = [&gorea_session, gorea_id]() {
                std::vector<std::uint32_t> ids;
                for (const auto& value : gorea_session.enemies()) {
                    if (value.active && value.parent_enemy_id == gorea_id
                        && value.enemy_type == static_cast<std::uint8_t>(
                            fruityprime::formats::EnemyType::GoreaLeg)) {
                        ids.push_back(value.id);
                    }
                }
                return ids;
            };
            const auto gorea_leg_ids = find_gorea_legs();
            require(gorea_leg_ids.size() == 3,
                    "Gorea1A did not create all three knee parts");
            std::array<fruityprime::net::Vec3, 2> initial_arm_positions{};
            for (std::size_t index = 0; index < gorea_arm_ids.size(); ++index) {
                const auto arm = std::find_if(
                    gorea_session.enemies().begin(), gorea_session.enemies().end(),
                    [id = gorea_arm_ids[index]](const auto& value) {
                        return value.id == id;
                    });
                require(arm != gorea_session.enemies().end(),
                        "Gorea1A shoulder disappeared before node audit");
                initial_arm_positions[index] = arm->position;
            }
            std::array<fruityprime::net::Vec3, 3> initial_leg_positions{};
            for (std::size_t index = 0; index < gorea_leg_ids.size(); ++index) {
                const auto leg = std::find_if(
                    gorea_session.enemies().begin(), gorea_session.enemies().end(),
                    [id = gorea_leg_ids[index]](const auto& value) {
                        return value.id == id;
                    });
                require(leg != gorea_session.enemies().end(),
                        "Gorea1A knee disappeared before node audit");
                initial_leg_positions[index] = leg->position;
            }
            bool animated_attachment_moved = false;
            for (int i = 0; i < 180 && !animated_attachment_moved; ++i) {
                gorea_session.tick();
                const auto head_now = find_gorea_head();
                if (head_now != gorea_session.enemies().end()
                    && fruityprime::gameplay::distance_squared(
                           head_now->position, initial_head_position) > 0.0001F) {
                    animated_attachment_moved = true;
                }
                for (std::size_t index = 0;
                     index < gorea_arm_ids.size() && !animated_attachment_moved;
                     ++index) {
                    const auto arm = std::find_if(
                        gorea_session.enemies().begin(), gorea_session.enemies().end(),
                        [id = gorea_arm_ids[index]](const auto& value) {
                            return value.id == id;
                        });
                    animated_attachment_moved = arm != gorea_session.enemies().end()
                        && fruityprime::gameplay::distance_squared(
                               arm->position, initial_arm_positions[index])
                            > 0.0001F;
                }
                for (std::size_t index = 0;
                     index < gorea_leg_ids.size() && !animated_attachment_moved;
                     ++index) {
                    const auto leg = std::find_if(
                        gorea_session.enemies().begin(), gorea_session.enemies().end(),
                        [id = gorea_leg_ids[index]](const auto& value) {
                            return value.id == id;
                        });
                    animated_attachment_moved = leg != gorea_session.enemies().end()
                        && fruityprime::gameplay::distance_squared(
                               leg->position, initial_leg_positions[index])
                            > 0.0001F;
                }
            }
            require(animated_attachment_moved,
                    "Gorea1A linked parts stayed at their bind-pose positions");
            bool shoulders_ready = false;
            bool saw_head_flash = false;
            bool head_flash_followed_head = false;
            for (int i = 0; i < 600 && !shoulders_ready; ++i) {
                gorea_session.tick();
                shoulders_ready = true;
                for (const auto id : gorea_arm_ids) {
                    const auto arm = std::find_if(
                        gorea_session.enemies().begin(),
                        gorea_session.enemies().end(),
                        [id](const auto& value) { return value.id == id; });
                    shoulders_ready = shoulders_ready
                        && arm != gorea_session.enemies().end()
                        && arm->gorea_activated && !arm->invulnerable;
                }
                const auto head_now = find_gorea_head();
                const auto body_now = find_gorea_body();
                if (head_now != gorea_session.enemies().end()
                    && head_now->gorea_head_flash_effect_id != 0) {
                    const auto flash = std::find_if(
                        gorea_session.effects().begin(),
                        gorea_session.effects().end(),
                        [effect_id = head_now->gorea_head_flash_effect_id](
                            const auto& value) {
                            return value.id == effect_id;
                        });
                    require(flash != gorea_session.effects().end()
                                && flash->effect_id == 104
                                && flash->persistent,
                            "Gorea head flash lost its attached EffectEntry");
                    saw_head_flash = true;
                    if (body_now != gorea_session.enemies().end()) {
                        constexpr float EyeFlashFacingOffset = 2949.0F / 4096.0F;
                        constexpr float EyeFlashUpOffset = -939.0F / 4096.0F;
                        const auto expected = fruityprime::gameplay::add(
                            head_now->position,
                            fruityprime::gameplay::add(
                                fruityprime::gameplay::multiply(
                                    body_now->facing, EyeFlashFacingOffset),
                                fruityprime::gameplay::multiply(
                                    body_now->up, EyeFlashUpOffset)));
                        head_flash_followed_head =
                            fruityprime::gameplay::distance_squared(
                                flash->position, expected) < 0.0001F;
                    }
                }
            }
            require(shoulders_ready,
                    "Gorea1A shoulders remained invulnerable after intro");
            require(saw_head_flash && head_flash_followed_head,
                    "Gorea head flash was not attached at its animated offset");
            const auto partially_damaged_arm = std::find_if(
                gorea_session.enemies().begin(), gorea_session.enemies().end(),
                [&gorea_id](const auto& value) {
                    return value.active && value.parent_enemy_id == gorea_id
                        && value.enemy_type == static_cast<std::uint8_t>(
                            fruityprime::formats::EnemyType::GoreaArm)
                        && value.gorea_index == 0;
                });
            require(partially_damaged_arm != gorea_session.enemies().end(),
                    "Gorea1A left shoulder disappeared before damage test");
            const auto partially_damaged_arm_id = partially_damaged_arm->id;
            require(gorea_session.damage_enemy(partially_damaged_arm_id, 70),
                    "Gorea1A shoulder did not accept a non-lethal hit");
            gorea_session.tick();
            const auto damaged_arm_after_tick = std::find_if(
                gorea_session.enemies().begin(), gorea_session.enemies().end(),
                [partially_damaged_arm_id](const auto& value) {
                    return value.id == partially_damaged_arm_id;
                });
            require(damaged_arm_after_tick != gorea_session.enemies().end()
                        && damaged_arm_after_tick->gorea_arm_effect_id != 0,
                    "Gorea1A shoulder damage loop was not retained as an effect");
            const auto shoulder_effect = std::find_if(
                gorea_session.effects().begin(), gorea_session.effects().end(),
                [effect_id = damaged_arm_after_tick->gorea_arm_effect_id](
                    const auto& value) {
                    return value.id == effect_id;
                });
            require(shoulder_effect != gorea_session.effects().end()
                        && shoulder_effect->effect_id == 43
                        && shoulder_effect->persistent
                        && shoulder_effect->element_extension,
                    "Gorea1A shoulder damage effect lost its managed flags");
            for (const auto id : gorea_arm_ids) {
                const bool accepted = gorea_session.damage_enemy(id, 121);
                require(accepted, "Gorea1A shoulder damage was not accepted");
            }
            const auto find_gorea_phase = [&gorea_session, gorea_id]() {
                return std::find_if(
                    gorea_session.enemies().begin(), gorea_session.enemies().end(),
                    [gorea_id](const auto& value) {
                        return value.active
                            && value.parent_enemy_id == gorea_id
                            && value.enemy_type == static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::Gorea1B);
                    });
            };
            bool handed_off = false;
            for (int i = 0; i < 12; ++i) {
                gorea_session.tick();
                const auto body = find_gorea_body();
                const auto phase = find_gorea_phase();
                if (body != gorea_session.enemies().end()
                    && phase != gorea_session.enemies().end()
                    && !body->visible && phase->visible
                    && phase->gorea_activated) {
                    handed_off = true;
                    break;
                }
            }
            require(handed_off,
                    "Gorea1A did not hand the encounter to Gorea1B");
            const auto phase_after_handoff = find_gorea_phase();
            require(phase_after_handoff != gorea_session.enemies().end(),
                    "Gorea1B disappeared immediately after handoff");
            const auto phase_id = phase_after_handoff->id;
            const auto find_gorea_phase_sphere = [&gorea_session, phase_id]() {
                return std::find_if(
                    gorea_session.enemies().begin(), gorea_session.enemies().end(),
                    [phase_id](const auto& value) {
                        return value.active
                            && value.parent_enemy_id == phase_id
                        && value.enemy_type == static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::GoreaSealSphere1);
                    });
            };
            const auto sphere_at_handoff = find_gorea_phase_sphere();
            require(sphere_at_handoff != gorea_session.enemies().end(),
                    "Gorea1B did not create its seal sphere");
            const auto initial_sphere_position = sphere_at_handoff->position;
            bool animated_sphere_attachment_moved = false;
            for (int i = 0; i < 180 && !animated_sphere_attachment_moved; ++i) {
                gorea_session.tick();
                const auto sphere_now = find_gorea_phase_sphere();
                animated_sphere_attachment_moved =
                    sphere_now != gorea_session.enemies().end()
                    && fruityprime::gameplay::distance_squared(
                           sphere_now->position, initial_sphere_position)
                        > 0.0001F;
            }
            require(animated_sphere_attachment_moved,
                    "Gorea1B seal sphere stayed at its bind-pose attachment");
            const auto phase_facing = fruityprime::gameplay::normalized_or(
                {phase_after_handoff->facing.x, 0.0F,
                 phase_after_handoff->facing.z},
                {0.0F, 0.0F, 1.0F});
            const auto grapple_start = fruityprime::gameplay::add(
                phase_after_handoff->position,
                fruityprime::gameplay::add(
                    fruityprime::gameplay::multiply(phase_facing, 4.0F),
                    {0.0F, 4.0F, 0.0F}));
            gorea_session.mutable_player(0).position = grapple_start;
            gorea_session.mutable_player(0).speed = {};
            gorea_session.set_input(0, fruityprime::gameplay::Input{});
            bool saw_grapple_state = false;
            bool saw_rope_points = false;
            bool saw_grapple_damage = false;
            std::uint8_t highest_grapple_state = 0;
            const auto grapple_health = gorea_session.player(0).health;
            for (int i = 0; i < 720; ++i) {
                gorea_session.set_input(0, fruityprime::gameplay::Input{});
                gorea_session.tick();
                const auto phase_now = find_gorea_phase();
                if (phase_now == gorea_session.enemies().end()) {
                    break;
                }
                highest_grapple_state = std::max(
                    highest_grapple_state, phase_now->gorea_state);
                saw_grapple_state = saw_grapple_state
                    || phase_now->gorea_state >= 4;
                const auto sphere_now = find_gorea_phase_sphere();
                saw_rope_points = saw_rope_points
                    || (phase_now->gorea_grappling
                        && sphere_now != gorea_session.enemies().end()
                        && fruityprime::gameplay::distance_squared(
                            phase_now->gorea_grapple_points.back(),
                            sphere_now->position) > 0.01F);
                saw_grapple_damage = saw_grapple_damage
                    || gorea_session.player(0).health < grapple_health;
            }
            require(saw_grapple_state && saw_rope_points && saw_grapple_damage,
                    "Gorea1B did not execute its grapple rope and damage states");
            const auto sphere_before_phase = find_gorea_phase_sphere();
            require(sphere_before_phase != gorea_session.enemies().end()
                        && sphere_before_phase->gorea_targetable
                        && !sphere_before_phase->invulnerable,
                    "Gorea1B seal sphere was not targetable after grapple");
            const auto sphere_id = sphere_before_phase->id;
            static_cast<void>(gorea_session.damage_enemy(sphere_id, 1000, 8));
            const auto sphere_after_hit = find_gorea_phase_sphere();
            require(sphere_after_hit != gorea_session.enemies().end()
                        && sphere_after_hit->gorea_damage == 1000,
                    "Gorea1B seal sphere did not accumulate phase damage");
            bool phase_reopened = false;
            // Gorea1B's collapse sequence is animation 4 -> 2 -> 8. The
            // cartridge contains 61, 103, and 66 node frames respectively;
            // ModelInstance advances them at 30 Hz, so the sequence alone
            // needs 460 native 60 Hz ticks. Leave room for the state-change
            // frame and the final callback instead of relying on the old
            // hand-written 420-tick budget.
            constexpr int phase_collapse_tick_budget = 600;
            for (int i = 0; i < phase_collapse_tick_budget; ++i) {
                gorea_session.set_input(0, fruityprime::gameplay::Input{});
                gorea_session.tick();
                const auto body = find_gorea_body();
                const auto phase = find_gorea_phase();
                phase_reopened = phase_reopened
                    || (body != gorea_session.enemies().end()
                        && body->visible && body->gorea_activated
                        && phase != gorea_session.enemies().end()
                        && !phase->gorea_activated
                        && phase->gorea_phases_left == 2);
                if (phase_reopened) {
                    break;
                }
            }
            require(phase_reopened,
                    "Gorea1B did not complete its phase-collapse handoff");
            std::cout << "real Gorea1B simulation: state="
                      << static_cast<unsigned>(highest_grapple_state)
                      << " rope=" << (saw_rope_points ? 1 : 0)
                      << " damage=" << (saw_grapple_damage ? 1 : 0)
                      << " phase=" << (phase_reopened ? 1 : 0)
                      << " attachment="
                      << (animated_sphere_attachment_moved ? 1 : 0) << '\n';
            std::cout << "real Gorea1A simulation: head_targetable=1 damage="
                      << damaged_gorea_head->gorea_damage << '\n';
        } else {
            std::cout << "real Gorea1A simulation skipped: room unavailable\n";
        }
        if (const auto* gorea2_entry = fruityprime::scene::find_room(
                "Gorea_b2"); gorea2_entry != nullptr) {
            const auto gorea2_room = fruityprime::scene::Room::load(
                assets, gorea2_entry->definition);
            std::optional<fruityprime::net::Vec3> gorea2_position;
            for (const auto& entity : gorea2_room.entities()) {
                if (entity.kind != fruityprime::scene::EntityKind::EnemySpawn) {
                    continue;
                }
                const auto* spawn = std::get_if<
                    fruityprime::scene::EnemySpawnData>(&entity.typed_data);
                if (spawn == nullptr || !spawn->active
                    || spawn->enemy_type != static_cast<std::uint8_t>(
                        fruityprime::formats::EnemyType::Gorea2)) {
                    continue;
                }
                gorea2_position = {
                    entity.position.x.to_float(),
                    entity.position.y.to_float(),
                    entity.position.z.to_float()};
                break;
            }
            require(gorea2_position.has_value(),
                    "Gorea_b2 has no active Gorea2 spawner");
            auto gorea2_model = fruityprime::assets::try_load_named_model(
                assets, "Gorea2_lod0");
            require(gorea2_model.has_value()
                        && gorea2_model->model.animations().any(),
                    "Gorea2 attachment model or animation is missing");
            fruityprime::gameplay::Session gorea2_session(
                gorea2_room, enemy_config);
            gorea2_session.set_gorea2_model(&gorea2_model->model);
            static_cast<void>(gorea2_session.add_player(0, 0));
            auto gorea2_setup = gorea2_session.snapshot(0);
            gorea2_setup.players[0].position = *gorea2_position;
            gorea2_session.apply_snapshot(gorea2_setup);
            gorea2_session.set_input(0, fruityprime::gameplay::Input{});
            const auto find_gorea2_body = [&gorea2_session]() {
                return std::find_if(
                    gorea2_session.enemies().begin(),
                    gorea2_session.enemies().end(),
                    [](const auto& value) {
                        return value.active
                            && value.enemy_type == static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::Gorea2);
                    });
            };
            const auto find_gorea2_sphere = [&gorea2_session](
                                                std::int32_t parent_id) {
                return std::find_if(
                    gorea2_session.enemies().begin(),
                    gorea2_session.enemies().end(),
                    [parent_id](const auto& value) {
                        return value.active
                            && value.parent_enemy_id == parent_id
                            && value.enemy_type == static_cast<std::uint8_t>(
                                fruityprime::formats::EnemyType::GoreaSealSphere2);
                    });
            };
            for (int i = 0; i < 1200
                    && find_gorea2_body() == gorea2_session.enemies().end();
                 ++i) {
                gorea2_session.tick();
            }
            const auto gorea2_body = find_gorea2_body();
            require(gorea2_body != gorea2_session.enemies().end(),
                    "Gorea2 spawner did not create the boss body");
            const auto gorea2_id = gorea2_body->id;
            const auto initial_gorea2_sphere = find_gorea2_sphere(gorea2_id);
            require(initial_gorea2_sphere != gorea2_session.enemies().end(),
                    "Gorea2 spawner did not create its seal sphere");
            const auto initial_gorea2_sphere_position =
                initial_gorea2_sphere->position;
            const auto initial_gorea2_position = gorea2_body->position;
            bool moved = false;
            bool sphere_attachment_moved = false;
            bool damaged = false;
            std::uint8_t observed_state = gorea2_body->gorea_state;
            for (int i = 0; i < 2400; ++i) {
                gorea2_session.tick();
                const auto body = find_gorea2_body();
                if (body == gorea2_session.enemies().end()) {
                    break;
                }
                observed_state = body->gorea_state;
                const auto delta = fruityprime::net::Vec3{
                    body->position.x - initial_gorea2_position.x,
                    body->position.y - initial_gorea2_position.y,
                    body->position.z - initial_gorea2_position.z};
                moved = moved || delta.x * delta.x + delta.y * delta.y
                    + delta.z * delta.z > 0.0001F;
                const auto sphere = find_gorea2_sphere(gorea2_id);
                sphere_attachment_moved = sphere_attachment_moved
                    || (sphere != gorea2_session.enemies().end()
                        && fruityprime::gameplay::distance_squared(
                               sphere->position,
                               initial_gorea2_sphere_position) > 0.0001F);
                if (!damaged && sphere != gorea2_session.enemies().end()
                    && sphere->gorea_targetable && !sphere->invulnerable) {
                    static_cast<void>(gorea2_session.damage_enemy(
                        sphere->id, 40, 8)); // Omega Cannon
                    const auto after_hit = find_gorea2_sphere(gorea2_id);
                    damaged = after_hit != gorea2_session.enemies().end()
                        && after_hit->gorea_damage == 40;
                }
            }
            const auto gorea2_sphere = find_gorea2_sphere(gorea2_id);
            require(gorea2_sphere != gorea2_session.enemies().end()
                        && gorea2_sphere->health_max == 840
                        && moved && sphere_attachment_moved && damaged
                        && observed_state != 0,
                    "Gorea2 did not execute linked sphere, hover, and damage state");
            std::cout << "real Gorea2 simulation: state="
                      << static_cast<unsigned>(observed_state)
                      << " moved=" << (moved ? 1 : 0)
                      << " damage=" << gorea2_sphere->gorea_damage
                      << " attachment=" << (sphere_attachment_moved ? 1 : 0)
                      << '\n';
        } else {
            std::cout << "real Gorea2 simulation skipped: room unavailable\n";
        }
        {
            constexpr std::uint32_t message_set_active = 5;
            constexpr std::uint32_t message_unlock = 16;
            constexpr std::uint32_t message_lock = 17;
            fruityprime::scene_runtime::Scene force_field_scene(assets);
            require(force_field_scene.load_room(
                        *force_field_room_definition, enemy_config),
                    "supported ForceField room could not be loaded");
            auto* force_field_session = force_field_scene.session();
            require(force_field_session != nullptr
                        && force_field_session->add_player(0, 0) == 0,
                    "ForceField test could not create its player");
            auto force_field_setup = force_field_session->snapshot(0);
            force_field_setup.players[0].position = {
                force_field_position.x + force_field_facing.x * 2.0F,
                force_field_position.y + force_field_facing.y * 2.0F,
                force_field_position.z + force_field_facing.z * 2.0F};
            force_field_session->apply_snapshot(force_field_setup);
            force_field_session->set_input(0, fruityprime::gameplay::Input{});
            const auto find_force_field_lock = [force_field_session,
                                                force_field_entity_id]() {
                return std::find_if(
                    force_field_session->enemies().begin(),
                    force_field_session->enemies().end(),
                    [force_field_entity_id](const auto& value) {
                        return value.active
                            && value.force_field_entity_id == force_field_entity_id;
                    });
            };
            const auto send_force_field_message =
                [&force_field_scene, &force_field_entity_id](
                    std::uint32_t message,
                                     std::int32_t parameter1 = 0) {
                    return force_field_scene.messages().send(
                        fruityprime::messaging::Message::None, -1,
                        force_field_entity_id, parameter1, 0,
                        force_field_scene.state().frame_count, -1,
                        message);
                };
            require(send_force_field_message(message_set_active, 0),
                    "ForceField SetActive(0) could not be queued");
            force_field_scene.tick();
            require(find_force_field_lock()
                        == force_field_session->enemies().end(),
                    "ForceField SetActive(0) did not remove its lock");
            require(send_force_field_message(message_lock),
                    "ForceField Lock could not be queued");
            force_field_scene.tick();
            auto force_field_lock = find_force_field_lock();
            require(force_field_lock != force_field_session->enemies().end()
                        && force_field_lock->force_field_type == force_field_type
                        && std::fabs(force_field_lock->force_field_width
                                     - force_field_width) < 0.01F
                        && std::fabs(force_field_lock->force_field_height
                                     - force_field_height) < 0.01F,
                    "ForceField Lock did not materialize its typed runtime child");
            const auto initial_lock_position = force_field_lock->position;
            for (int i = 0; i < 120; ++i) {
                force_field_session->set_input(
                    0, fruityprime::gameplay::Input{});
                force_field_scene.tick();
            }
            force_field_lock = find_force_field_lock();
            require(force_field_lock != force_field_session->enemies().end(),
                    "ForceField lock did not survive its movement sequence");
            const bool lock_moved =
                std::fabs(force_field_lock->position.x
                          - initial_lock_position.x) > 0.0001F
                || std::fabs(force_field_lock->position.y
                             - initial_lock_position.y) > 0.0001F
                || std::fabs(force_field_lock->position.z
                             - initial_lock_position.z) > 0.0001F;
            require(lock_moved,
                    "ForceField lock did not execute its floating motion");
            require(send_force_field_message(message_unlock),
                    "ForceField Unlock could not be queued");
            force_field_scene.tick();
            require(find_force_field_lock()
                        == force_field_session->enemies().end(),
                    "ForceField Unlock did not remove its lock");
            require(send_force_field_message(message_lock),
                    "ForceField relock could not be queued");
            force_field_scene.tick();
            require(find_force_field_lock()
                        != force_field_session->enemies().end(),
                    "ForceField relock did not recreate its lock");
            std::cout << "real ForceField simulation: type="
                      << force_field_type << " fields=" << story_force_fields
                      << " moved=" << (lock_moved ? 1 : 0)
                      << " relock=1\n";
        }
        std::cout << "real story room catalog: rooms=" << story_catalog.size()
                  << " meshes=" << story_meshes
                  << " entities=" << story_entities
                  << " nodes=" << story_nodes
                  << " node_animation_groups=" << story_node_animation_groups
                  << " material_animation_groups="
                  << story_material_animation_groups
                  << " texcoord_animation_groups="
                  << story_texcoord_animation_groups
                  << " texture_animation_groups="
                  << story_texture_animation_groups
                  << " cretaphid_profiles=" << cretaphid_profiles
                  << " force_fields=" << story_force_fields << '\n';
        const auto& catalog = fruityprime::scene::multiplayer_rooms();
        require(catalog.size() == 27,
                "native multiplayer room catalog must contain 27 rooms");

        std::size_t total_meshes = 0;
        std::size_t total_entities = 0;
        std::size_t total_primitives = 0;
        std::size_t total_vertices = 0;
        std::size_t total_collision_points = 0;
        std::size_t total_player_spawns = 0;
        std::size_t total_typed_entities = 0;
        std::size_t total_node_defenses = 0;
        std::size_t total_flag_bases = 0;
        std::size_t total_trigger_volumes = 0;
        std::size_t total_area_volumes = 0;
        std::size_t total_enemy_spawns_decoded = 0;
        std::size_t total_unknown_enemy_spawners = 0;
        std::size_t total_textures = 0;
        std::size_t total_palettes = 0;

        for (const auto& entry : catalog) {
            const auto prefix = room_prefix(entry);
            const auto room = fruityprime::scene::Room::load(
                assets, entry.definition);
            entity_model_audit.inspect_room(assets, room, prefix);
            const auto& model = room.model();
            const auto& collision = room.collision();

            require(room.definition().name == entry.definition.name,
                    prefix + "definition name changed during load");
            require(model.header().mesh_count == model.meshes().size(),
                    prefix + "model mesh count does not match its table");
            require(model.header().material_count == model.materials().size(),
                    prefix + "model material count does not match its table");
            require(model.header().mesh_count > 0 && !model.meshes().empty(),
                    prefix + "model has no meshes");
            require(!model.display_lists().empty()
                        && model.display_lists().size()
                            == model.instructions().size(),
                    prefix + "model display-list tables are empty or mismatched");
            require(room.has_entities() && !room.entities().empty(),
                    prefix + "entity resource has no instances");
            std::size_t room_player_spawns = 0;
            std::size_t room_typed_entities = 0;
            for (const auto& entity : room.entities()) {
                require(entity.kind != fruityprime::scene::EntityKind::Unknown,
                        prefix + "entity has an unknown cartridge type: "
                            + std::to_string(entity.type));
                const auto require_volume = [&](const fruityprime::scene::EntityVolume& volume,
                                                const char* name) {
                    require(volume.kind != fruityprime::scene::VolumeKind::Invalid,
                            prefix + name + " has an invalid volume");
                    require(volume.contains(volume.center()),
                            prefix + name + " volume rejects its center");
                };
                switch (entity.kind) {
                case fruityprime::scene::EntityKind::Platform:
                    require(std::get_if<fruityprime::scene::PlatformData>(
                                &entity.typed_data) != nullptr,
                            prefix + "platform has no typed payload");
                    ++room_typed_entities;
                    break;
                case fruityprime::scene::EntityKind::Object: {
                    const auto* object = std::get_if<
                        fruityprime::scene::ObjectData>(&entity.typed_data);
                    require(object != nullptr,
                            prefix + "object has no typed payload");
                    require_volume(object->volume, "object");
                    ++room_typed_entities;
                    break;
                }
                default:
                    break;
                }
                if (entity.kind == fruityprime::scene::EntityKind::EnemySpawn) {
                    const auto* enemy = std::get_if<
                        fruityprime::scene::EnemySpawnData>(&entity.typed_data);
                    require(enemy != nullptr,
                            prefix + "enemy spawn has no typed payload");
                    require(enemy->spawn_count <= enemy->spawn_limit
                                || enemy->spawn_limit == 0,
                            prefix + "enemy spawn count exceeds its limit");
                    if (entity.payload.size()
                        == fruityprime::enemy_spawn::Data::Size) {
                        const auto decoded = fruityprime::enemy_spawn::decode(
                            entity.payload);
                        require(static_cast<std::uint8_t>(decoded.enemy_type)
                                    == enemy->enemy_type,
                                prefix + "enemy spawn decoder changed enemy type");
                        require(decoded.spawn_count == enemy->spawn_count
                                    && decoded.spawn_limit == enemy->spawn_limit,
                                prefix + "enemy spawn decoder changed spawn limits");
                        ++total_enemy_spawns_decoded;
                        if (decoded.fields.spawner_type
                            == fruityprime::enemy_spawn::SpawnerType::Unknown) {
                            ++total_unknown_enemy_spawners;
                        }
                    }
                    ++room_typed_entities;
                }
                if (entity.kind == fruityprime::scene::EntityKind::PlayerSpawn) {
                    ++room_player_spawns;
                    const auto* spawn = std::get_if<
                        fruityprime::scene::PlayerSpawnData>(&entity.typed_data);
                    require(spawn != nullptr,
                            prefix + "player spawn has no typed payload");
                    require(spawn->availability <= 2,
                            prefix + "player spawn availability is invalid");
                    ++room_typed_entities;
                }
                if (entity.kind == fruityprime::scene::EntityKind::Door) {
                    require(std::get_if<fruityprime::scene::DoorData>(
                                &entity.typed_data) != nullptr,
                            prefix + "door has no typed payload");
                    ++room_typed_entities;
                }
                if (entity.kind == fruityprime::scene::EntityKind::TriggerVolume) {
                    const auto* trigger = std::get_if<
                        fruityprime::scene::TriggerVolumeData>(&entity.typed_data);
                    require(trigger != nullptr,
                            prefix + "trigger volume has no typed payload");
                    require_volume(trigger->volume, "trigger volume");
                    ++room_typed_entities;
                }
                if (entity.kind == fruityprime::scene::EntityKind::AreaVolume) {
                    const auto* area = std::get_if<
                        fruityprime::scene::AreaVolumeData>(&entity.typed_data);
                    require(area != nullptr,
                            prefix + "area volume has no typed payload");
                    require_volume(area->volume, "area volume");
                    ++room_typed_entities;
                }
                if (entity.kind == fruityprime::scene::EntityKind::JumpPad) {
                    const auto* jump_pad = std::get_if<
                        fruityprime::scene::JumpPadData>(&entity.typed_data);
                    require(jump_pad != nullptr,
                            prefix + "jump pad has no typed payload");
                    require_volume(jump_pad->volume, "jump pad");
                    ++room_typed_entities;
                }
                if (entity.kind == fruityprime::scene::EntityKind::PointModule) {
                    require(std::get_if<fruityprime::scene::PointModuleData>(
                                &entity.typed_data) != nullptr,
                            prefix + "point module has no typed payload");
                    ++room_typed_entities;
                }
                if (entity.kind == fruityprime::scene::EntityKind::MorphCamera) {
                    const auto* camera = std::get_if<
                        fruityprime::scene::MorphCameraData>(&entity.typed_data);
                    require(camera != nullptr,
                            prefix + "morph camera has no typed payload");
                    require_volume(camera->volume, "morph camera");
                    ++room_typed_entities;
                }
                if (entity.kind == fruityprime::scene::EntityKind::OctolithFlag) {
                    require(std::get_if<fruityprime::scene::OctolithFlagData>(
                                &entity.typed_data) != nullptr,
                            prefix + "octolith flag has no typed payload");
                    ++room_typed_entities;
                }
                if (entity.kind == fruityprime::scene::EntityKind::NodeDefense) {
                    ++total_node_defenses;
                    const auto* node = std::get_if<
                        fruityprime::scene::NodeDefenseData>(&entity.typed_data);
                    require(node != nullptr,
                            prefix + "node defense has no typed payload");
                    require_volume(node->volume, "node defense");
                    ++room_typed_entities;
                }
                if (entity.kind == fruityprime::scene::EntityKind::FlagBase) {
                    ++total_flag_bases;
                    const auto* base = std::get_if<
                        fruityprime::scene::FlagBaseData>(&entity.typed_data);
                    require(base != nullptr,
                            prefix + "flag base has no typed payload");
                    require_volume(base->volume, "flag base");
                    ++room_typed_entities;
                }
                if (entity.kind == fruityprime::scene::EntityKind::Teleporter) {
                    require(std::get_if<fruityprime::scene::TeleporterData>(
                                &entity.typed_data) != nullptr,
                            prefix + "teleporter has no typed payload");
                    ++room_typed_entities;
                }
                if (entity.kind == fruityprime::scene::EntityKind::LightSource) {
                    const auto* light = std::get_if<
                        fruityprime::scene::LightSourceData>(&entity.typed_data);
                    require(light != nullptr,
                            prefix + "light source has no typed payload");
                    require_volume(light->volume, "light source");
                    ++room_typed_entities;
                }
                if (entity.kind == fruityprime::scene::EntityKind::Artifact) {
                    require(std::get_if<fruityprime::scene::ArtifactData>(
                                &entity.typed_data) != nullptr,
                            prefix + "artifact has no typed payload");
                    ++room_typed_entities;
                }
                if (entity.kind == fruityprime::scene::EntityKind::CameraSequence) {
                    require(std::get_if<fruityprime::scene::CameraSequenceData>(
                                &entity.typed_data) != nullptr,
                            prefix + "camera sequence has no typed payload");
                    ++room_typed_entities;
                }
                if (entity.kind == fruityprime::scene::EntityKind::ForceField) {
                    require(std::get_if<fruityprime::scene::ForceFieldData>(
                                &entity.typed_data) != nullptr,
                            prefix + "force field has no typed payload");
                    ++room_typed_entities;
                }
            }
            require(room_player_spawns > 0,
                    prefix + "room has no player spawn entities");
            require(room_typed_entities > 0,
                    prefix + "room has no typed entity payloads");
            total_typed_entities += room_typed_entities;

            if (collision.is_mph()) {
                const auto& data = collision.mph();
                require(!data.points.empty() && !data.entries.empty(),
                        prefix + "MPH collision has no points or entries");
                total_collision_points += data.points.size();
            } else {
                const auto& data = collision.first_hunt();
                require(!data.points.empty() && !data.entries.empty(),
                        prefix + "First Hunt collision has no points or entries");
                total_collision_points += data.points.size();
            }

            std::size_t room_primitives = 0;
            std::size_t room_vertices = 0;
            for (const auto& mesh : model.meshes()) {
                require(mesh.display_list_id < model.display_lists().size(),
                        prefix + "mesh references an invalid display list");
                require(mesh.material_id < model.materials().size(),
                        prefix + "mesh references an invalid material");

                const auto& material = model.materials()[mesh.material_id];
                int texture_width = 0;
                int texture_height = 0;
                if (material.texture_id >= 0
                    && static_cast<std::size_t>(material.texture_id)
                        < model.textures().size()) {
                    const auto& texture = model.textures()[material.texture_id];
                    texture_width = texture.width;
                    texture_height = texture.height;
                }
                const auto geometry = model.decode_geometry(
                    mesh.display_list_id, texture_width, texture_height,
                    material.texcoord_transform_mode == 2);
                room_primitives += geometry.size();
                for (const auto& primitive : geometry) {
                    room_vertices += primitive.vertices.size();
                    for (const auto& vertex : primitive.vertices) {
                        require(std::isfinite(vertex.x)
                                    && std::isfinite(vertex.y)
                                    && std::isfinite(vertex.z)
                                    && std::isfinite(vertex.texture_s)
                                    && std::isfinite(vertex.texture_t),
                                prefix + "display-list geometry contains a non-finite vertex");
                    }
                }
            }
            require(room_primitives > 0 && room_vertices > 0,
                    prefix + "display lists decoded no geometry");

            std::set<std::size_t> texture_ids;
            std::set<std::size_t> palette_ids;
            for (const auto& material : model.materials()) {
                if (material.texture_id >= 0
                    && static_cast<std::size_t>(material.texture_id)
                        < model.textures().size()) {
                    texture_ids.insert(static_cast<std::size_t>(
                        material.texture_id));
                }
                if (material.palette_id >= 0
                    && static_cast<std::size_t>(material.palette_id)
                        < model.palettes().size()) {
                    palette_ids.insert(static_cast<std::size_t>(
                        material.palette_id));
                }
            }
            for (const std::size_t texture_id : texture_ids) {
                const auto pixels = model.decode_texture(texture_id);
                const auto& texture = model.textures()[texture_id];
                require(pixels.size() == static_cast<std::size_t>(texture.width)
                            * static_cast<std::size_t>(texture.height),
                        prefix + "texture decode size does not match dimensions");
            }
            for (const std::size_t palette_id : palette_ids) {
                require(!model.decode_palette(palette_id).empty(),
                        prefix + "palette decoded no colors");
            }

            total_meshes += model.meshes().size();
            total_entities += room.entities().size();
            total_player_spawns += room_player_spawns;
            const auto room_trigger_volumes = std::count_if(
                room.entities().begin(), room.entities().end(),
                [](const auto& entity) {
                    return entity.kind
                        == fruityprime::scene::EntityKind::TriggerVolume;
                });
            const auto room_area_volumes = std::count_if(
                room.entities().begin(), room.entities().end(),
                [](const auto& entity) {
                    return entity.kind
                        == fruityprime::scene::EntityKind::AreaVolume;
                });
            total_trigger_volumes += room_trigger_volumes;
            total_area_volumes += room_area_volumes;
            total_textures += texture_ids.size();
            total_palettes += palette_ids.size();
            total_primitives += room_primitives;
            total_vertices += room_vertices;
            std::cout << "room=" << entry.name
                      << " id=" << entry.id
                      << " meshes=" << model.meshes().size()
                      << " entities=" << room.entities().size()
                      << " typed=" << room_typed_entities
                      << " player_spawns=" << room_player_spawns
                      << " node_defenses="
                      << std::count_if(
                             room.entities().begin(), room.entities().end(),
                             [](const auto& entity) {
                                 return entity.kind
                                     == fruityprime::scene::EntityKind::NodeDefense;
                             })
                      << " flag_bases="
                      << std::count_if(
                             room.entities().begin(), room.entities().end(),
                             [](const auto& entity) {
                                 return entity.kind
                                     == fruityprime::scene::EntityKind::FlagBase;
                             })
                      << " trigger_volumes=" << room_trigger_volumes
                      << " area_volumes=" << room_area_volumes
                      << " collision=" << (collision.is_mph() ? "MPH" : "FH")
                      << " collision_points="
                      << (collision.is_mph() ? collision.mph().points.size()
                                             : collision.first_hunt().points.size())
                      << " primitives=" << room_primitives
                      << " vertices=" << room_vertices
                      << " textures=" << texture_ids.size()
                      << " palettes=" << palette_ids.size() << '\n';
        }

        require(entity_model_audit.invalid_ids == 0,
                "entity model audit found invalid metadata IDs");
        std::cout << "entity model audit: requests="
                  << entity_model_audit.requests
                  << " loaded=" << entity_model_audit.loaded_requests
                  << " missing=" << entity_model_audit.missing_requests
                  << " unique=" << entity_model_audit.cache.size()
                  << " decoded_models=" << entity_model_audit.decoded_models
                  << " primitives=" << entity_model_audit.decoded_primitives
                  << " vertices=" << entity_model_audit.decoded_vertices
                  << " textures=" << entity_model_audit.decoded_textures
                  << " palettes=" << entity_model_audit.decoded_palettes
                  << '\n';
        for (const auto& name : entity_model_audit.missing_names) {
            std::cout << "missing entity model=" << name << '\n';
        }
        std::cout << "real room catalog: rooms=" << catalog.size()
                  << " meshes=" << total_meshes
                  << " entities=" << total_entities
                  << " typed=" << total_typed_entities
                  << " player_spawns=" << total_player_spawns
                  << " node_defenses=" << total_node_defenses
                  << " flag_bases=" << total_flag_bases
                  << " trigger_volumes=" << total_trigger_volumes
                  << " area_volumes=" << total_area_volumes
                  << " enemy_spawns_decoded=" << total_enemy_spawns_decoded
                  << " unknown_enemy_spawners=" << total_unknown_enemy_spawners
                  << " collision_points=" << total_collision_points
                  << " primitives=" << total_primitives
                  << " vertices=" << total_vertices
                  << " textures=" << total_textures
                  << " palettes=" << total_palettes << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "real room test failed: " << error.what() << '\n';
        return 1;
    }
}
