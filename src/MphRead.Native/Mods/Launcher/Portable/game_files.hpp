#pragma once

#include "Formats/paths.hpp"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>

namespace fruityprime::launcher {

struct GameFilesStatus {
    bool ready = false;
    std::string problem;
    std::string mph_key = std::string(formats::AMHE0);
    std::string fh_key = std::string(formats::AMFE0);
    std::filesystem::path mph_file_system;
    std::filesystem::path fh_file_system;

    [[nodiscard]] std::string describe() const;
};

struct SetupResult {
    bool ok = false;
    bool first_hunt = false;
    std::string game_key;
    std::size_t rom_file_count = 0;
    std::filesystem::path game_root;
    std::string error;
};

using SetupReporter = std::function<void(std::string_view)>;

class GameFiles {
public:
    [[nodiscard]] static GameFilesStatus inspect(
        const std::filesystem::path& root);

    // Extract a supported .nds into root/files/<game-key>, unpacking the
    // native archive format into the same _archives tree used by MphRead.
    // Unlike the managed child-process path, this is safe to call directly
    // from the native launcher and reports each phase through the callback.
    [[nodiscard]] static SetupResult run_setup(
        const std::filesystem::path& root,
        const std::filesystem::path& rom_path,
        SetupReporter report = {});
};

} // namespace fruityprime::launcher
