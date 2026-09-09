#include "custom_rooms.hpp"

#include "map_bundle.hpp"
#include "Utility/console_setup.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace fruityprime::mapgen::custom_rooms {
namespace {

std::filesystem::path discover_default_directory() noexcept {
    std::error_code error;
    std::filesystem::path current = std::filesystem::current_path(error);
    if (error) {
        return {};
    }
    for (;;) {
        const auto candidate = current / "maps";
        if (std::filesystem::is_directory(candidate, error) && !error) {
            return candidate;
        }
        error.clear();
        const auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return std::filesystem::current_path(error) / "maps";
}

std::filesystem::path g_map_directory = discover_default_directory();
std::vector<MapDefinition> g_definitions;
bool g_loaded = false;
std::mutex g_mutex;

[[nodiscard]] std::string lower(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        result.push_back(static_cast<char>(std::tolower(
            static_cast<unsigned char>(character))));
    }
    return result;
}

[[nodiscard]] std::string upper_ascii(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        result.push_back(static_cast<char>(std::toupper(
            static_cast<unsigned char>(character))));
    }
    return result;
}

[[nodiscard]] bool extension_is(const std::filesystem::path& path,
                                std::string_view extension) {
    return lower(path.extension().string()) == lower(extension);
}

[[nodiscard]] bool regular_file(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error;
}

[[nodiscard]] std::filesystem::path resolve_source(
    const MapDefinition& definition) {
    if (definition.import_source.empty()) {
        return {};
    }
    const std::filesystem::path requested(definition.import_source);
    std::vector<std::filesystem::path> candidates;
    if (requested.is_absolute()) {
        candidates.push_back(requested);
    } else {
        if (!definition.source_path.empty()) {
            candidates.push_back(definition.source_path.parent_path() / requested);
        }
        candidates.push_back(g_map_directory / requested);
        std::error_code error;
        candidates.push_back(std::filesystem::current_path(error) / requested);
        error.clear();
        candidates.push_back(std::filesystem::current_path(error) / "maps"
                             / requested);
    }
    for (const auto& candidate : candidates) {
        if (regular_file(candidate)) {
            return std::filesystem::absolute(candidate);
        }
    }
    return {};
}

[[nodiscard]] bool source_available(const MapDefinition& definition) {
    if (definition.import_source.empty()) {
        return true;
    }
    if (bundle::is_bundle(definition.source_path)) {
        return bundle::read_entry(definition.source_path,
                                  definition.import_source).has_value();
    }
    return !resolve_source(definition).empty();
}

[[nodiscard]] std::vector<std::filesystem::path> map_files() {
    std::vector<std::filesystem::path> bundles;
    std::vector<std::filesystem::path> recipes;
    std::error_code error;
    if (!std::filesystem::is_directory(g_map_directory, error) || error) {
        return {};
    }
    std::filesystem::recursive_directory_iterator iterator(
        g_map_directory, std::filesystem::directory_options::skip_permission_denied,
        error);
    const std::filesystem::recursive_directory_iterator end;
    while (iterator != end && !error) {
        std::error_code entry_error;
        if (iterator->is_regular_file(entry_error) && !entry_error) {
            if (extension_is(iterator->path(), bundle::Extension)) {
                bundles.push_back(iterator->path());
            } else if (extension_is(iterator->path(), ".json")) {
                recipes.push_back(iterator->path());
            }
        }
        iterator.increment(error);
    }
    if (error) {
        throw std::runtime_error("could not enumerate custom map files: "
                                 + error.message());
    }
    std::sort(bundles.begin(), bundles.end());
    std::sort(recipes.begin(), recipes.end());
    std::vector<std::string> bundle_stems;
    bundle_stems.reserve(bundles.size());
    for (const auto& path : bundles) {
        bundle_stems.push_back(lower(path.stem().string()));
    }
    recipes.erase(std::remove_if(recipes.begin(), recipes.end(),
                                 [&bundle_stems](const auto& path) {
        return std::find(bundle_stems.begin(), bundle_stems.end(),
                         lower(path.stem().string())) != bundle_stems.end();
    }), recipes.end());

    bundles.insert(bundles.end(), recipes.begin(), recipes.end());
    std::sort(bundles.begin(), bundles.end());
    return bundles;
}

[[nodiscard]] std::vector<MapDefinition> load_definitions() {
    std::vector<MapDefinition> result;
    for (const auto& path : map_files()) {
        try {
            MapDefinition definition = load_definition(path);
            definition.name = upper_ascii(definition.name);
            if (!source_available(definition)) {
                std::cout << "Leaving out map " << definition.name
                          << ": its source level " << definition.import_source
                          << " is not here. Put it beside "
                          << definition.source_path.filename().string()
                          << " to have this map.\n";
                continue;
            }
            result.push_back(std::move(definition));
        } catch (const std::exception& error) {
            std::cout << "Ignoring map " << path.filename().string()
                      << ": " << error.what() << '\n';
        }
    }
    return result;
}

[[nodiscard]] bool needs_generating(const MapDefinition& definition) {
    const auto directory = generated_directory(definition);
    const std::string prefix = file_prefix(definition);
    const auto model = directory / (prefix + "_Model.bin");
    if (!regular_file(model)
        || !regular_file(directory / (prefix + "_Ent.bin"))
        || !regular_file(directory / (prefix + "_Node.bin"))) {
        return true;
    }
    if (definition.source_path.empty() || !regular_file(definition.source_path)) {
        return false;
    }
    std::error_code error;
    const auto source_time = std::filesystem::last_write_time(
        definition.source_path, error);
    if (error) {
        return false;
    }
    const auto model_time = std::filesystem::last_write_time(model, error);
    return !error && source_time > model_time;
}

std::size_t generate(const std::vector<MapDefinition>& definitions_to_build,
                     bool force, bool verbose, bool swallow_errors) {
    std::size_t count = 0;
    for (const MapDefinition& definition : definitions_to_build) {
        if (!force && !needs_generating(definition)) {
            continue;
        }
        try {
            if (verbose) {
                std::cout << "[mapgen] building " << definition.name << '\n';
            }
            const GeneratedMap generated = build(definition);
            write_generated(definition, generated,
                            generated_directory(definition));
            ++count;
        } catch (const std::exception& error) {
            if (!swallow_errors) {
                throw;
            }
            std::cout << "[mapgen] " << definition.name
                      << " could not be built: " << error.what() << '\n';
        }
    }
    return count;
}

} // namespace

void set_map_directory(const std::filesystem::path& path) {
    std::filesystem::path resolved = path;
    if (resolved.empty()) {
        resolved = discover_default_directory();
    }
    std::error_code error;
    resolved = std::filesystem::absolute(resolved, error).lexically_normal();
    if (error) {
        throw std::runtime_error("could not resolve custom map directory: "
                                 + error.message());
    }
    std::scoped_lock lock(g_mutex);
    if (g_map_directory == resolved) {
        return;
    }
    g_map_directory = std::move(resolved);
    g_definitions.clear();
    g_loaded = false;
}

const std::filesystem::path& map_directory() noexcept {
    return g_map_directory;
}

const std::vector<MapDefinition>& definitions() {
    std::scoped_lock lock(g_mutex);
    if (!g_loaded) {
        g_definitions = load_definitions();
        g_loaded = true;
    }
    return g_definitions;
}

std::filesystem::path generated_directory(const MapDefinition& definition) {
    // The native packer writes the prefixed room files directly into the
    // export directory (the prefix keeps different custom rooms distinct).
    // Keep the catalog and the -mapgen command on that same directory; a
    // per-map child here would make a successful generation impossible to
    // load because RoomDefinition names <prefix>_*.bin below this root.
    static_cast<void>(definition);
    if (!g_map_directory.empty() && g_map_directory.has_parent_path()) {
        return g_map_directory.parent_path() / "export/_mapgen";
    }
    return fruityprime::utility::console::resolve_launch_path("export/_mapgen");
}

std::size_t generate_all(bool force, bool verbose) {
    const auto& current = definitions();
    return generate(current, force, verbose, false);
}

std::size_t generate_missing(bool verbose) {
    const auto& current = definitions();
    return generate(current, false, verbose, true);
}

std::optional<std::string> why_unplayable(std::string_view room_name) {
    const auto& current = definitions();
    for (const auto& definition : current) {
        if (lower(definition.name) != lower(room_name)) {
            continue;
        }
        if (!needs_generating(definition)) {
            return std::nullopt;
        }
        return definition.name + " could not be built from "
            + definition.source_path.filename().string()
            + ", so there is no room to load. The [mapgen] line above says "
              "what went wrong with it.";
    }
    return std::nullopt;
}

} // namespace fruityprime::mapgen::custom_rooms
