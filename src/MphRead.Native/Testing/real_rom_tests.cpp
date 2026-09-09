#include "Utility/archive.hpp"
#include "Formats/enemy_spawn.hpp"
#include "Formats/entity_format.hpp"
#include "Assets/game_assets.hpp"
#include "Formats/model_format.hpp"
#include "Assets/nds_rom.hpp"
#include "Utility/repack_model.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Port of the managed Test.TestDlistBounds oracle.  Counting decoded
// vertices only proves a display list was walked; it says nothing about where
// those vertices landed, which is how a wrong fixed-point divisor on one
// vertex opcode stayed invisible.  Every model whose vertices are written in
// model space -- the managed oracle skips the skinned ones, whose bounds are
// in another space -- must reproduce the bounds the file itself records.
struct DlistBoundsAudit {
    std::size_t models = 0;
    std::size_t skinned = 0;
    std::size_t display_lists = 0;
    std::size_t compared = 0;
};

void check_dlist_bounds(const fruityprime::model::File& model,
                        const std::string& name, DlistBoundsAudit& audit) {
    ++audit.models;
    if (!model.node_weights().empty()) {
        ++audit.skinned;
        return;
    }
    const float scale = model.world_scale();
    const auto& display_lists = model.display_lists();
    for (std::size_t index = 0; index < display_lists.size(); ++index) {
        ++audit.display_lists;
        const auto primitives = model.decode_geometry(index, 64, 64, false);
        bool any = false;
        float lowest[3]{};
        float highest[3]{};
        for (const auto& primitive : primitives) {
            for (const auto& vertex : primitive.vertices) {
                const float scaled[3] = {vertex.x * scale, vertex.y * scale,
                                         vertex.z * scale};
                for (int axis = 0; axis < 3; ++axis) {
                    if (!any) {
                        lowest[axis] = highest[axis] = scaled[axis];
                    } else {
                        lowest[axis] = std::min(lowest[axis], scaled[axis]);
                        highest[axis] = std::max(highest[axis], scaled[axis]);
                    }
                }
                any = true;
            }
        }
        if (!any) {
            continue;
        }
        const auto& display_list = display_lists[index];
        const float recorded_low[3] = {
            display_list.min_bounds.x.to_float(),
            display_list.min_bounds.y.to_float(),
            display_list.min_bounds.z.to_float()};
        const float recorded_high[3] = {
            display_list.max_bounds.x.to_float(),
            display_list.max_bounds.y.to_float(),
            display_list.max_bounds.z.to_float()};
        // The managed oracle allows one raw 1/4096 unit, which the model
        // scale magnifies.
        const float tolerance = scale / 4096.0F * 1.5F;
        for (int axis = 0; axis < 3; ++axis) {
            require(std::fabs(lowest[axis] - recorded_low[axis]) <= tolerance
                        && std::fabs(highest[axis] - recorded_high[axis])
                            <= tolerance,
                    "model " + name + " display list "
                        + std::to_string(index)
                        + " decoded outside its recorded bounds on axis "
                        + std::to_string(axis));
        }
        ++audit.compared;
    }
}

bool is_model_entry(const std::string& name) {
    return name.size() > 10
        && name.compare(name.size() - 10, 10, "_Model.bin") == 0;
}

bool is_archive_entry(const std::string& name) {
    return name.size() > 4 && name.compare(name.size() - 4, 4, ".arc") == 0;
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

} // namespace

int main(int argc, char** argv) {
    const auto rom_path = configured_rom(argc, argv);
    if (rom_path.empty()) {
        std::cout << "real NDS ROM test skipped: set FRUITY_PRIME_TEST_NDS\n";
        return 0;
    }

    std::filesystem::path extraction_path;
    try {
        require(std::filesystem::is_regular_file(rom_path),
                "configured NDS ROM is not a regular file: "
                    + rom_path.string());

        const auto rom = fruityprime::nds::Rom::read_file(rom_path);
        const auto& files = rom.files();
        require(!files.empty(), "real NDS ROM contains no files");
        require(rom.header().game_code.size() == 4,
                "real NDS ROM game code is not four characters");

        std::size_t enemy_spawn_records = 0;
        std::size_t decoded_enemy_spawn_records = 0;
        std::size_t unknown_enemy_spawner_records = 0;
        std::size_t first_hunt_enemy_spawn_records = 0;
        std::size_t decoded_first_hunt_enemy_spawn_records = 0;
        for (const auto& file : files) {
            if (file.path.find("Ent.bin") == std::string::npos) {
                continue;
            }
            const auto bytes = rom.file(file.file_id);
            if (bytes.size() < 4
                || (bytes[0] != 1 && bytes[0] != 2)
                || bytes[1] != 0 || bytes[2] != 0 || bytes[3] != 0) {
                continue;
            }
            const auto entity_file = fruityprime::entity::File::from_bytes(bytes);
            if (entity_file.is_first_hunt()) {
                for (const auto& record : entity_file.first_hunt_records()) {
                    if (record.header.type != 6) {
                        continue;
                    }
                    ++first_hunt_enemy_spawn_records;
                    static_cast<void>(fruityprime::enemy_spawn::decode_first_hunt(
                        record.payload));
                    ++decoded_first_hunt_enemy_spawn_records;
                }
                continue;
            }
            for (const auto& record : entity_file.records()) {
                if (record.header.type != 6) {
                    continue;
                }
                ++enemy_spawn_records;
                const auto decoded = fruityprime::enemy_spawn::decode(
                    record.payload);
                ++decoded_enemy_spawn_records;
                if (decoded.fields.spawner_type
                    == fruityprime::enemy_spawn::SpawnerType::Unknown) {
                    ++unknown_enemy_spawner_records;
                }
            }
        }

        const auto first_file = rom.file(files.front().file_id);
        require(first_file.size() == files.front().size,
                "FAT size does not match the first FNT entry");

        const auto samus_archive = rom.file("archives/Samus.arc");
        const auto archive = fruityprime::archive::Archive::parse(samus_archive);
        require(!archive.entries().empty(),
                "real ROM Samus archive contains no entries");
        require(!archive.file(0).empty(),
                "real ROM Samus archive has an empty first entry");

        const auto assets = fruityprime::assets::Store::from_rom(rom_path);
        require(assets.is_rom(), "ROM asset store did not retain ROM source");
        const auto samus_model = assets.model_from_archive(
            "archives/Samus.arc", "Samus_lod0_Model.bin");
        require(!samus_model.meshes().empty(),
                "real ROM Samus model has no meshes");
        std::size_t samus_model_index = archive.entries().size();
        for (std::size_t i = 0; i < archive.entries().size(); ++i) {
            if (archive.entries()[i].filename == "Samus_lod0_Model.bin") {
                samus_model_index = i;
                break;
            }
        }
        require(samus_model_index < archive.entries().size(),
                "real ROM Samus model entry was not found");
        const auto samus_model_bytes = archive.file(samus_model_index);
        const auto samus_recolor_model = assets.bytes(
            "models/Samus_pal_01_Model.bin");
        const auto samus_texture = assets.bytes("models/Samus_pal_01_Tex.bin");
        const auto recolored_samus =
            fruityprime::model::File::from_recolor_resources(
                samus_model_bytes, samus_recolor_model, samus_texture);
        require(recolored_samus.textures().size() > 0
                    && recolored_samus.palettes().size() > 0,
                "real ROM Samus external texture has no tables");
        std::size_t decoded_samus_textures = 0;
        for (const auto& material : recolored_samus.materials()) {
            if (material.texture_id < 0
                || static_cast<std::size_t>(material.texture_id)
                    >= recolored_samus.textures().size()) {
                continue;
            }
            require(!recolored_samus.decode_texture(material.texture_id).empty(),
                    "real ROM Samus external texture decoded no pixels");
            if (material.palette_id >= 0
                && static_cast<std::size_t>(material.palette_id)
                    < recolored_samus.palettes().size()) {
                require(!recolored_samus.decode_palette(material.palette_id).empty(),
                        "real ROM Samus external palette decoded no colors");
            }
            ++decoded_samus_textures;
        }
        require(decoded_samus_textures > 0,
                "real ROM Samus model has no textured materials");

        // Exercise the full native PackModel path against the user's real
        // external-recolor layout.  A parser-only test would miss wrong
        // offsets between the model and companion texture streams.
        const auto repacked_samus =
            fruityprime::utility::repack_model::pack_model(
                recolored_samus,
                {fruityprime::utility::repack_model::TextureStorage::Separate,
                 false,
                 fruityprime::utility::repack_model::BoundsMode::None});
        require(!repacked_samus.model.empty() && !repacked_samus.texture.empty(),
                "native model repacker produced an empty external resource");
        const auto reparsed_samus = fruityprime::model::File::from_resources(
            repacked_samus.model, repacked_samus.texture);
        require(reparsed_samus.header().mesh_count
                    == recolored_samus.header().mesh_count
                && reparsed_samus.meshes().size() == recolored_samus.meshes().size(),
                "native model repacker changed the mesh table");
        require(reparsed_samus.textures().size()
                    == recolored_samus.textures().size()
                && reparsed_samus.palettes().size()
                    == recolored_samus.palettes().size(),
                "native model repacker changed external texture tables");
        require(!reparsed_samus.decode_texture(0).empty(),
                "native model repacker produced an undecodable texture");

        std::size_t samus_alt_model_index = archive.entries().size();
        for (std::size_t i = 0; i < archive.entries().size(); ++i) {
            if (archive.entries()[i].filename == "SamusAlt_lod0_Model.bin") {
                samus_alt_model_index = i;
                break;
            }
        }
        require(samus_alt_model_index < archive.entries().size(),
                "real ROM Samus archive has no alt-form model entry");
        const auto alt_samus = fruityprime::model::File::from_recolor_resources(
            archive.file(samus_alt_model_index), samus_recolor_model,
            samus_texture);
        require(!alt_samus.meshes().empty(),
                "real ROM Samus alt-form model has no meshes");
        require(!alt_samus.textures().empty() && !alt_samus.palettes().empty(),
                "real ROM Samus alt-form external texture has no tables");
        std::size_t decoded_alt_textures = 0;
        for (const auto& material : alt_samus.materials()) {
            if (material.texture_id < 0
                || static_cast<std::size_t>(material.texture_id)
                    >= alt_samus.textures().size()) {
                continue;
            }
            require(!alt_samus.decode_texture(material.texture_id).empty(),
                    "real ROM Samus alt-form texture decoded no pixels");
            ++decoded_alt_textures;
        }
        require(decoded_alt_textures > 0,
                "real ROM Samus alt-form model has no textured materials");

        const auto candidate = std::filesystem::temp_directory_path()
            / ("fruity_prime_native_real_rom_test_"
               + std::to_string(std::chrono::steady_clock::now()
                                    .time_since_epoch().count()));
        require(!std::filesystem::exists(candidate),
                "temporary ROM extraction directory already exists");
        extraction_path = candidate;
        const auto extracted_count = rom.extract(extraction_path);
        require(extracted_count == files.size(),
                "ROM extraction count does not match the FNT file count");

        const auto extracted_first = extraction_path
            / std::filesystem::path(files.front().path);
        require(std::filesystem::is_regular_file(extracted_first),
                "first FNT entry was not extracted");
        require(std::filesystem::file_size(extracted_first) == first_file.size(),
                "extracted first FNT entry has the wrong size");

        DlistBoundsAudit bounds_audit;
        for (const auto& entry : files) {
            if (is_model_entry(entry.path)) {
                check_dlist_bounds(
                    fruityprime::model::File::from_bytes(
                        rom.file(entry.file_id)),
                    entry.path, bounds_audit);
            } else if (is_archive_entry(entry.path)) {
                const auto archive_bytes = rom.file(entry.file_id);
                const auto entry_archive =
                    fruityprime::archive::Archive::parse(archive_bytes);
                for (std::size_t i = 0; i < entry_archive.entries().size();
                     ++i) {
                    const std::string& inner =
                        entry_archive.entries()[i].filename;
                    if (!is_model_entry(inner)) {
                        continue;
                    }
                    check_dlist_bounds(
                        fruityprime::model::File::from_bytes(
                            entry_archive.file(i)),
                        entry.path + "/" + inner, bounds_audit);
                }
            }
        }
        require(bounds_audit.compared > 1000,
                "ROM display-list bounds audit compared too few lists");

        std::cout << "real NDS ROM: " << rom.header().title
                  << " (" << rom.header().game_code << ")\n"
                  << "files: " << files.size()
                  << ", extracted: " << extracted_count
                  << ", enemy_spawns=" << enemy_spawn_records
                  << ", enemy_spawns_decoded=" << decoded_enemy_spawn_records
                  << ", unknown_enemy_spawners=" << unknown_enemy_spawner_records
                  << ", fh_enemy_spawns=" << first_hunt_enemy_spawn_records
                  << ", fh_enemy_spawns_decoded="
                  << decoded_first_hunt_enemy_spawn_records
                  << ", models=" << bounds_audit.models
                  << ", skinned_models=" << bounds_audit.skinned
                  << ", dlist_bounds_checked=" << bounds_audit.compared
                  << '\n';

        std::error_code cleanup_error;
        std::filesystem::remove_all(extraction_path, cleanup_error);
        require(!std::filesystem::exists(extraction_path),
                "temporary ROM extraction directory could not be removed");
        return 0;
    } catch (const std::exception& error) {
        if (!extraction_path.empty()) {
            std::error_code cleanup_error;
            std::filesystem::remove_all(extraction_path, cleanup_error);
        }
        std::cerr << "real ROM test failed: " << error.what() << '\n';
        return 1;
    }
}
