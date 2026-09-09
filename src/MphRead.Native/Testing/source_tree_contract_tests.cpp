#include "../MphReadNative.hpp"
#include "../Entities/Enemies/enemy_modules.hpp"

#include <array>
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

// `obj/` and `bin/` hold the managed build's generated sources
// (MphRead.AssemblyInfo.cs and friends).  They are compiler output, not part
// of the implementation this contract is about, so they are not counted and
// do not need a native pair.
[[nodiscard]] bool is_managed_build_output(
    const std::filesystem::path& relative) {
    for (const auto& part : relative) {
        if (part == "obj" || part == "bin") {
            return true;
        }
    }
    return false;
}


[[nodiscard]] std::filesystem::path find_native_root() {
    // __FILE__ is an absolute path in the source-facing CMake target and a
    // path relative to native/ in the standalone Makefile. Both forms point
    // at Testing/. Walking two parents reaches src/MphRead.Native; the
    // fallback makes the standalone binary safe to invoke from either the
    // repository root or native/.
    std::filesystem::path native_root =
        std::filesystem::path(__FILE__).parent_path().parent_path();
    if (!std::filesystem::is_directory(native_root)) {
        native_root = std::filesystem::current_path() / "src"
            / "MphRead.Native";
    }
    if (!std::filesystem::is_directory(native_root)) {
        native_root = std::filesystem::current_path() / ".." / "src"
            / "MphRead.Native";
    }
    return native_root;
}

void assert_source_tree_layout() {
    const auto native_root = find_native_root();
    constexpr std::array categories{
        "Assets", "Entities", "Export", "Formats", "HUD", "Metadata",
        "Mods", "Sound", "Testing", "Utility"
    };
    for (const char* category : categories) {
        assert(std::filesystem::is_directory(native_root / category));
    }

    constexpr std::array root_sources{
        "Features.cpp", "GameState.cpp", "Memory.cpp", "MemoryArrays.cpp",
        "MemoryClasses.cpp", "Menu.cpp", "Messaging.cpp", "Program.cpp",
        "Read.cpp", "Renderer.cpp", "Scene.cpp", "SceneSetup.cpp",
        "Selection.cpp", "Shaders.cpp", "Strings.cpp", "Test.cpp"
    };
    for (const char* source : root_sources) {
        assert(std::filesystem::is_regular_file(native_root / source));
    }
}

void assert_enemy_module_layout() {
    const auto native_root = find_native_root();
    const auto managed_enemies = native_root.parent_path() / "MphRead"
        / "Entities" / "Enemies";
    const auto native_enemies = native_root / "Entities" / "Enemies";
    assert(std::filesystem::is_directory(managed_enemies));
    assert(std::filesystem::is_directory(native_enemies));

    std::array<bool, 52> seen{};
    std::size_t managed_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(
             managed_enemies)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".cs") {
            continue;
        }
        ++managed_count;
        const auto stem = entry.path().stem().string();
        assert(std::filesystem::is_regular_file(
            native_enemies / (stem + ".cpp")));
        assert(std::filesystem::is_regular_file(
            native_enemies / (stem + ".hpp")));
    }

    // The descriptor table is generated from the same list as the native
    // module headers. Check both directions so a new managed class cannot be
    // silently omitted from the C++ build, and an orphan module cannot drift
    // away from the cartridge source tree.
    assert(managed_count == fruityprime::enemy::kModules.size());
    for (const auto& module : fruityprime::enemy::kModules) {
        assert(module.id < seen.size());
        assert(!seen[module.id]);
        seen[module.id] = true;
        assert(!module.managed_source.empty());
        assert(!module.managed_class.empty());
        assert(std::filesystem::is_regular_file(
            managed_enemies / std::string(module.managed_source)));
        assert(std::filesystem::is_regular_file(
            native_enemies / (std::filesystem::path(module.managed_source)
                              .replace_extension(".cpp"))));
        assert(std::filesystem::is_regular_file(
            native_enemies / (std::filesystem::path(module.managed_source)
                              .replace_extension(".hpp"))));
    }
}

void assert_managed_entity_source_pairs() {
    const auto native_root = find_native_root();
    const auto managed_root = native_root.parent_path() / "MphRead"
        / "Entities";
    const auto native_entities = native_root / "Entities";
    std::size_t managed_count = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             managed_root)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".cs") {
            continue;
        }
        const auto relative = std::filesystem::relative(
            entry.path(), managed_root);
        if (is_managed_build_output(relative)) {
            continue;
        }
        ++managed_count;
        auto native_cpp_relative = relative;
        native_cpp_relative.replace_extension(".cpp");
        auto native_hpp_relative = relative;
        native_hpp_relative.replace_extension(".hpp");
        const auto native_cpp = native_entities / native_cpp_relative;
        const auto native_hpp = native_entities / native_hpp_relative;
        // Every managed Entity partial has a same-path native translation
        // unit.  The implementation may expose a public aggregate header,
        // but the source file itself cannot disappear into gameplay.cpp.
        assert(std::filesystem::is_regular_file(native_cpp)
            || std::filesystem::is_regular_file(native_hpp));
    }
    assert(managed_count == 83);
}

void assert_managed_source_tree_pairs() {
    const auto native_root = find_native_root();
    const auto managed_root = native_root.parent_path() / "MphRead";
    std::size_t managed_count = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             managed_root)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".cs") {
            continue;
        }
        const auto relative = std::filesystem::relative(
            entry.path(), managed_root);
        if (is_managed_build_output(relative)) {
            continue;
        }
        ++managed_count;
        auto native_cpp_relative = relative;
        native_cpp_relative.replace_extension(".cpp");
        auto native_hpp_relative = relative;
        native_hpp_relative.replace_extension(".hpp");
        const auto native_cpp = native_root / native_cpp_relative;
        const auto native_hpp = native_root / native_hpp_relative;
        // The native tree must retain a same-path source/header boundary for
        // every managed source. Entity files have the stricter check above;
        // this wider check prevents the rest of the C# application from
        // silently collapsing into one aggregate C++ file.
        assert(std::filesystem::is_regular_file(native_cpp)
            || std::filesystem::is_regular_file(native_hpp));
    }
    assert(managed_count == 272);
}

[[nodiscard]] std::size_t count_boundary_only_sources() {
    const auto native_root = find_native_root();
    std::size_t boundary_count = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             native_root)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".cpp") {
            continue;
        }
        // This test contains the marker string as the thing it searches for;
        // it is not one of the source units still waiting for a port.
        if (entry.path().filename() == "source_tree_contract_tests.cpp") {
            continue;
        }
        std::ifstream stream(entry.path(), std::ios::binary);
        if (!stream.good()) {
            continue;
        }
        const std::string source{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
        if (source.find("FRUITY_PRIME_DECLARE_SOURCE_UNIT(\"")
                != std::string::npos) {
            ++boundary_count;
        }
    }
    return boundary_count;
}

void assert_entity_sources_are_implemented() {
    const auto entities = find_native_root() / "Entities";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             entities)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".cpp") {
            continue;
        }
        std::ifstream stream(entry.path(), std::ios::binary);
        assert(stream.good());
        const std::string source{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
        // The screenshot-visible Entities tree is not allowed to regress to
        // name-only boundary files. Other categories may still expose an
        // explicit boundary while their platform implementation is ported.
        assert(source.find("FRUITY_PRIME_DECLARE_SOURCE_UNIT(\"")
            == std::string::npos);
    }
}

void assert_gameplay_module_layout() {
    const auto entities = find_native_root() / "Entities";
    constexpr std::array gameplay_modules{
        "gameplay_combat.cpp", "gameplay_enemies.cpp",
        "gameplay_environment.cpp", "gameplay_items.cpp",
        "gameplay_objectives.cpp",
        "gameplay_projectiles.cpp", "gameplay_snapshot.cpp",
        "gameplay_story.cpp"
    };
    for (const char* module : gameplay_modules) {
        assert(std::filesystem::is_regular_file(entities / module));
    }

    // gameplay.cpp is intentionally only the fixed-step orchestration layer.
    // This contract prevents a future port from regressing to the former
    // God-file shape when another managed entity is migrated.
    const auto gameplay_path = entities / "gameplay.cpp";
    assert(std::filesystem::is_regular_file(gameplay_path));
    std::ifstream stream(gameplay_path, std::ios::binary);
    assert(stream.good());
    const std::string source{
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
    assert(std::count(source.begin(), source.end(), '\n') <= 300);
    assert(source.find("Session::damage_enemy") == std::string::npos);
    assert(source.find("Session::destroy_enemy") == std::string::npos);
    assert(source.find("Session::spawn_projectile") == std::string::npos);
    assert(source.find("Session::update_enemies") == std::string::npos);
    assert(source.find("Session::apply_story_save") == std::string::npos);
}

void assert_entity_translation_units() {
    const auto entities = find_native_root() / "Entities";
    constexpr std::array static_entities{
        "EntityBase.cpp", "PlatformEntity.cpp", "ObjectEntity.cpp",
        "PlayerSpawnEntity.cpp", "ItemSpawnEntity.cpp", "DoorEntity.cpp",
        "VolumeEntity.cpp", "AreaVolumeEntity.cpp",
        "TriggerVolumeEntity.cpp", "JumpPadEntity.cpp",
        "MorphCameraEntity.cpp", "NodeDefenseEntity.cpp",
        "FlagBaseEntity.cpp", "LightSourceEntity.cpp",
        "TeleporterEntity.cpp", "OctolithFlagEntity.cpp",
        "ArtifactEntity.cpp", "ForceFieldEntity.cpp",
        "PointModuleEntity.cpp", "RoomEntity.cpp"
    };
    constexpr std::array runtime_entities{
        "RuntimeEntityBase.cpp", "BeamProjectileEntity.cpp",
        "ItemInstanceEntity.cpp", "BeamEffectEntity.cpp", "BombEntity.cpp",
        "EnemyInstanceEntity.cpp", "EnemySpawnEntity.cpp",
        "EntityPool.cpp"
    };
    for (const char* source : static_entities) {
        assert(std::filesystem::is_regular_file(entities / source));
    }
    for (const char* source : runtime_entities) {
        assert(std::filesystem::is_regular_file(entities / source));
    }

    const auto players = entities / "Players";
    constexpr std::array player_entities{
        "DynamicLightEntity.cpp", "HalfturretEntity.cpp",
        "PlayerAi.cpp", "PlayerCamera.cpp", "PlayerCollision.cpp",
        "PlayerDialog.cpp", "PlayerDraw.cpp", "PlayerEntity.cpp",
        "PlayerHud.cpp", "PlayerInput.cpp", "PlayerPause.cpp",
        "PlayerProcess.cpp", "PlayerScan.cpp", "PlayerSound.cpp"
    };
    for (const char* source : player_entities) {
        assert(std::filesystem::is_regular_file(players / source));
    }
}

} // namespace

int main() {
    assert_source_tree_layout();
    assert_enemy_module_layout();
    assert_managed_entity_source_pairs();
    assert_managed_source_tree_pairs();
    assert_entity_sources_are_implemented();
    assert_gameplay_module_layout();
    assert_entity_translation_units();
    assert(MphReadNative::Testing::has_metadata_contract());
    const auto* samus = MphReadNative::Metadata::find_hunter("samus");
    assert(samus != nullptr);
    assert(samus->base_model_entry == "Samus_lod0_Model.bin");

    const MphReadNative::Hud::Viewport viewport{1280, 960};
    assert(viewport.scale() == 5.0F);
    const auto area = viewport.design_area();
    assert(area.left == 0.0F && area.top == 0.0F);
    assert(area.right == 1280.0F && area.bottom == 960.0F);

    MphReadNative::Sound::Mixer mixer;
    assert(mixer.start());
    const std::array<std::int16_t, 4> samples{};
    assert(mixer.submit(samples));
    assert(mixer.submitted_samples() == samples.size());
    mixer.stop();
    assert(!mixer.running());

    std::cout << "native source tree contract passed: managed=272"
              << " entity_sources=implemented"
              << " boundary_only=" << count_boundary_only_sources() << '\n';
    return 0;
}
