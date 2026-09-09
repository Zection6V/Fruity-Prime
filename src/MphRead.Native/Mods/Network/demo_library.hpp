#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fruityprime::demo {

struct Recording final {
    std::filesystem::path path;
    std::string room;
    std::chrono::system_clock::time_point recorded{};
    std::uintmax_t bytes = 0;

    [[nodiscard]] std::string file_name() const {
        return path.filename().string();
    }
};

// Native counterpart of Mods.Network.DemoLibrary.  The caller supplies the
// export root because a native headless process and the Win32 game have
// different executable/data directories.
[[nodiscard]] std::filesystem::path demos_directory(
    const std::filesystem::path& export_root);

[[nodiscard]] std::vector<Recording> list_recordings(
    const std::filesystem::path& directory);

[[nodiscard]] std::string describe(const Recording& recording);

} // namespace fruityprime::demo
