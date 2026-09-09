#include "Testing/test_overlay.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace fruityprime::testing::overlay {
namespace {

struct DirectorySnapshot {
    std::vector<std::string> directories;
    std::map<std::string, std::vector<std::string>> files;
    std::filesystem::path root;
};

[[nodiscard]] std::vector<std::uint8_t> read_bytes(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open overlay comparison file: "
                                 + path.string());
    }
    input.seekg(0, std::ios::end);
    const std::streamoff length = input.tellg();
    if (length < 0) {
        throw std::runtime_error("failed to size overlay comparison file: "
                                 + path.string());
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), length);
        if (!input) {
            throw std::runtime_error("failed to read overlay comparison file: "
                                     + path.string());
        }
    }
    return bytes;
}

[[nodiscard]] DirectorySnapshot snapshot(
    const std::filesystem::path& root) {
    if (!std::filesystem::is_directory(root)) {
        throw std::invalid_argument("overlay comparison root is not a directory: "
                                    + root.string());
    }
    DirectorySnapshot result;
    result.root = root;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_directory()) {
            continue;
        }
        const std::string relative = std::filesystem::relative(
            entry.path(), root).generic_string();
        result.directories.push_back(relative);
        auto& names = result.files[relative];
        for (const auto& file : std::filesystem::directory_iterator(entry.path())) {
            if (file.is_regular_file()) {
                names.push_back(file.path().filename().generic_string());
            }
        }
        std::sort(names.begin(), names.end());
    }
    return result;
}

[[nodiscard]] std::string default_name(const std::filesystem::path& root) {
    const std::string name = root.filename().generic_string();
    return name.empty() ? root.generic_string() : name;
}

} // namespace

GameTreeComparison compare_game_trees(
    const std::filesystem::path& first_root,
    const std::filesystem::path& second_root) {
    const DirectorySnapshot first = snapshot(first_root);
    const DirectorySnapshot second = snapshot(second_root);
    GameTreeComparison result;

    for (const std::string& directory : first.directories) {
        if (std::find(second.directories.begin(), second.directories.end(),
                      directory) == second.directories.end()) {
            result.directories_first_only.push_back(directory);
        }
    }
    for (const std::string& directory : second.directories) {
        if (std::find(first.directories.begin(), first.directories.end(),
                      directory) == first.directories.end()) {
            result.directories_second_only.push_back(directory);
        }
    }

    for (const std::string& directory : first.directories) {
        if (std::find(second.directories.begin(), second.directories.end(),
                      directory) == second.directories.end()) {
            continue;
        }
        const auto first_files = first.files.find(directory);
        const auto second_files = second.files.find(directory);
        if (first_files == first.files.end() || second_files == second.files.end()) {
            continue;
        }
        DirectoryDifference difference;
        difference.path = directory;
        for (const std::string& file : first_files->second) {
            if (std::find(second_files->second.begin(), second_files->second.end(),
                          file) == second_files->second.end()) {
                difference.files_first_only.push_back(file);
            }
        }
        for (const std::string& file : second_files->second) {
            if (std::find(first_files->second.begin(), first_files->second.end(),
                          file) == first_files->second.end()) {
                difference.files_second_only.push_back(file);
            }
        }
        for (const std::string& file : first_files->second) {
            if (std::find(second_files->second.begin(), second_files->second.end(),
                          file) == second_files->second.end()) {
                continue;
            }
            if (read_bytes(first.root / directory / file)
                != read_bytes(second.root / directory / file)) {
                difference.changed_files.push_back(file);
            }
        }
        if (!difference.files_first_only.empty()
            || !difference.files_second_only.empty()
            || !difference.changed_files.empty()) {
            result.common_directories.push_back(std::move(difference));
        }
    }
    return result;
}

std::string format_comparison(const GameTreeComparison& comparison,
                              std::string_view first_name,
                              std::string_view second_name) {
    std::ostringstream output;
    if (!comparison.directories_first_only.empty()) {
        output << "Directories in " << first_name << " not in "
               << second_name << ":\n";
        for (const std::string& directory : comparison.directories_first_only) {
            output << "-- " << directory << "\n";
        }
        output << "\n";
    }
    if (!comparison.directories_second_only.empty()) {
        output << "Directories in " << second_name << " not in "
               << first_name << ":\n";
        for (const std::string& directory : comparison.directories_second_only) {
            output << "-- " << directory << "\n";
        }
        output << "\n";
    }
    for (const DirectoryDifference& difference : comparison.common_directories) {
        if (!difference.files_first_only.empty()
            || !difference.files_second_only.empty()) {
            output << difference.path << "\n";
        }
        if (!difference.files_first_only.empty()) {
            output << "Files in " << first_name << " not in "
                   << second_name << ":\n";
            for (const std::string& file : difference.files_first_only) {
                output << "-- " << file << "\n";
            }
        }
        if (!difference.files_second_only.empty()) {
            output << "Files in " << second_name << " not in "
                   << first_name << ":\n";
            for (const std::string& file : difference.files_second_only) {
                output << "-- " << file << "\n";
            }
        }
        if (!difference.files_first_only.empty()
            || !difference.files_second_only.empty()) {
            output << "\n";
        }
    }
    for (const DirectoryDifference& difference : comparison.common_directories) {
        if (difference.changed_files.empty()) {
            continue;
        }
        output << difference.path << "\nChanged files:\n";
        for (const std::string& file : difference.changed_files) {
            output << file << "\n";
        }
        output << "\n";
    }
    return output.str();
}

std::string compare_games(const std::filesystem::path& first_root,
                          const std::filesystem::path& second_root,
                          std::string_view first_name,
                          std::string_view second_name) {
    const std::string first_default = default_name(first_root);
    const std::string second_default = default_name(second_root);
    return format_comparison(compare_game_trees(first_root, second_root),
                             first_name.empty() ? first_default : first_name,
                             second_name.empty() ? second_default : second_name);
}

std::vector<int> translate(int mask) {
    (void)mask;
    constexpr std::uint32_t diagnostic_mask = 0x21;
    std::vector<int> active;
    for (std::size_t index = 0; index < OverlayMap.size(); ++index) {
        if ((diagnostic_mask & (1U << index)) != 0) {
            active.push_back(OverlayMap[index]);
        }
    }
    std::sort(active.begin(), active.end());
    return active;
}

} // namespace fruityprime::testing::overlay
