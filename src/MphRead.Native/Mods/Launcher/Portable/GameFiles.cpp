#include "Mods/Launcher/Portable/game_files.hpp"

#include "Utility/extract.hpp"

#include <array>
#include <charconv>
#include <fstream>
#include <system_error>
#include <utility>

namespace fruityprime::launcher {
namespace {

[[nodiscard]] std::array<int, 4> parse_version(std::string value) noexcept {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
    std::array<int, 4> result{};
    std::size_t begin = 0;
    for (std::size_t index = 0; index < result.size(); ++index) {
        const std::size_t end = value.find('.', begin);
        const std::string part = value.substr(
            begin, end == std::string::npos ? std::string::npos
                                             : end - begin);
        if (part.empty()) {
            return {};
        }
        const auto parsed = std::from_chars(
            part.data(), part.data() + part.size(), result[index]);
        if (parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size()
            || result[index] < 0) {
            return {};
        }
        if (end == std::string::npos) {
            return index == result.size() - 1 ? result
                                               : std::array<int, 4>{};
        }
        begin = end + 1;
    }
    return {};
}

[[nodiscard]] bool compatible_version(std::string value) noexcept {
    const auto actual = parse_version(std::move(value));
    const auto minimum = parse_version(std::string(formats::MinimumExtractVersion));
    return actual != std::array<int, 4>{}
        && minimum != std::array<int, 4>{} && actual >= minimum;
}

} // namespace

std::string GameFilesStatus::describe() const {
    if (!problem.empty()) {
        return problem;
    }
    return "Ready -- " + mph_key;
}

GameFilesStatus GameFiles::inspect(const std::filesystem::path& root) {
    GameFilesStatus result;
    std::ifstream input(root / "paths.txt");
    if (!input) {
        result.problem = "No game files yet";
        return result;
    }

    try {
        std::string version;
        if (!std::getline(input, version)
            || !compatible_version(std::move(version))) {
            result.problem = "The extracted files are from an older version -- set up again";
            return result;
        }
    } catch (...) {
        result.problem = "paths.txt could not be read";
        return result;
    }

    try {
        formats::Paths paths;
        paths.update(root);
        paths.choose_mph_path();
        paths.choose_fh_path();
        result.mph_key = paths.mph_key;
        result.fh_key = paths.fh_key;
        result.mph_file_system = std::filesystem::path(paths.file_system());
        result.fh_file_system = std::filesystem::path(paths.fh_file_system());
        if (result.mph_file_system.empty()
            || !std::filesystem::is_directory(result.mph_file_system)) {
            result.problem = "The extracted files are missing -- set up again";
            return result;
        }
    } catch (...) {
        result.problem = "No Metroid Prime Hunters files are configured";
        return result;
    }
    result.ready = true;
    return result;
}

SetupResult GameFiles::run_setup(const std::filesystem::path& root,
                                 const std::filesystem::path& rom_path,
                                 SetupReporter report) {
    if (!std::filesystem::is_regular_file(rom_path)) {
        SetupResult result;
        result.error = "The specified ROM file does not exist.";
        if (report) {
            report(result.error);
        }
        return result;
    }
    const auto extracted = utility::extract::setup(root, rom_path,
        [report](std::string_view message) {
            if (report) {
                report(message);
            }
        });
    SetupResult result;
    result.ok = extracted.ok;
    result.first_hunt = extracted.first_hunt;
    result.game_key = extracted.game_key;
    result.rom_file_count = extracted.rom_file_count;
    result.game_root = extracted.game_root;
    result.error = extracted.error;
    return result;
}

} // namespace fruityprime::launcher
