#include "Export/export.hpp"
#include "Assets/game_assets.hpp"
#include "Metadata/metadata.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    const char* rom_value = std::getenv("FRUITY_PRIME_TEST_NDS");
    if (rom_value == nullptr || rom_value[0] == '\0') {
        std::cout << "native export test skipped: set FRUITY_PRIME_TEST_NDS\n";
        return 0;
    }
    try {
        const auto assets = fruityprime::assets::Store::from_rom(rom_value);
        const auto& samus = fruityprime::metadata::hunter_info(0);
        const auto resource = assets.archive(
            "archives/" + std::string(samus.archive) + ".arc");
        const auto found = std::find_if(
            resource.entries().begin(), resource.entries().end(),
            [&samus](const auto& entry) {
                return entry.filename == samus.base_model_entry;
            });
        if (found == resource.entries().end()) {
            throw std::runtime_error("hunter model is missing from the ROM");
        }
        const std::string recolor_prefix(samus.base_recolor_prefix);
        const auto model = fruityprime::model::File::from_recolor_resources(
            resource.file(static_cast<std::size_t>(
                found - resource.entries().begin())),
            assets.bytes("models/" + recolor_prefix + "_pal_01_Model.bin"),
            assets.bytes("models/" + recolor_prefix + "_pal_01_Tex.bin"));
        const auto unique = std::chrono::steady_clock::now()
            .time_since_epoch().count();
        const auto directory = std::filesystem::temp_directory_path()
            / ("fruity_prime_export_" + std::to_string(unique));
        const auto output = directory / "samus.obj";
        const auto stats = fruityprime::exporter::write_obj(model, output);
        assert(stats.meshes == model.meshes().size());
        assert(stats.primitives > 0);
        assert(stats.vertices > 0);
        assert(stats.faces > 0);
        assert(std::filesystem::is_regular_file(output));
        auto material = output;
        material.replace_extension(".mtl");
        assert(std::filesystem::is_regular_file(material));
        std::ifstream input(output, std::ios::binary);
        std::string first_line;
        std::getline(input, first_line);
        assert(first_line == "# Fruity Prime native model export");
        const auto collada_output = directory / "samus.dae";
        const auto collada_stats = fruityprime::exporter::write_collada(
            model, collada_output);
        assert(collada_stats.meshes == stats.meshes);
        assert(collada_stats.primitives == stats.primitives);
        assert(collada_stats.vertices == stats.vertices);
        assert(collada_stats.faces > 0);
        assert(std::filesystem::is_regular_file(collada_output));
        std::ifstream collada(collada_output, std::ios::binary);
        std::string collada_text((std::istreambuf_iterator<char>(collada)),
                                 std::istreambuf_iterator<char>());
        assert(collada_text.find("<COLLADA ") != std::string::npos);
        assert(collada_text.find("<library_geometries>")
               != std::string::npos);
        assert(collada_text.find("<triangles ") != std::string::npos);
        fruityprime::exporter::ScriptingOptions script_options;
        script_options.model_name = "Samus";
        script_options.export_root = directory / "export";
        script_options.recolor_names = {"base", "red"};
        const std::string script = fruityprime::exporter::generate_script(
            model, script_options);
        assert(script.find("import bpy\n") != std::string::npos);
        assert(script.find("# recolors: base, red\n") != std::string::npos);
        assert(script.find("uv_anims = [") != std::string::npos);
        assert(script.find("mat_anims = [") != std::string::npos);
        assert(script.find("node_anims = [") != std::string::npos);
        assert(script.find("tex_anims = [") != std::string::npos);
        assert(script.find("def import_dae(suffix):") != std::string::npos);
        assert(script.find("anim_setup()") != std::string::npos);
        const auto script_output = directory / "import_Samus.py";
        fruityprime::exporter::write_script(
            model, script_output, script_options);
        assert(std::filesystem::is_regular_file(script_output));
        std::ifstream script_file(script_output, std::ios::binary);
        const std::string written_script(
            (std::istreambuf_iterator<char>(script_file)),
            std::istreambuf_iterator<char>());
        assert(written_script == script);
        const auto image_directory = directory / "textures";
        const auto image_stats = fruityprime::exporter::write_model_textures(
            model, image_directory);
        assert(image_stats.images > 0);
        assert(image_stats.pixels > 0);
        std::size_t png_count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(
                 image_directory)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            ++png_count;
            std::ifstream png(entry.path(), std::ios::binary);
            std::array<unsigned char, 8> signature{};
            png.read(reinterpret_cast<char*>(signature.data()),
                     static_cast<std::streamsize>(signature.size()));
            const std::array<unsigned char, 8> png_signature{
                0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
            assert(signature == png_signature);
        }
        assert(png_count == image_stats.images);
        std::error_code error;
        std::filesystem::remove_all(directory, error);
        std::cout << "native model export test passed: meshes=" << stats.meshes
                  << " vertices=" << stats.vertices << " faces=" << stats.faces
                  << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native export test failed: " << error.what() << '\n';
        return 1;
    }
}
