#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

namespace fruityprime::assets {
class Store;
}

namespace fruityprime::utility::extract {

struct SetupResult {
    bool ok = false;
    bool first_hunt = false;
    bool cancelled = false;
    std::string game_key;
    std::size_t rom_file_count = 0;
    std::filesystem::path game_root;
    std::string error;
};

using SetupReporter = std::function<void(std::string_view)>;

// Extract.Setup. root is AppDomain.CurrentDomain.BaseDirectory in the managed
// application; keeping it explicit makes launcher and command-line callers
// share exactly one extraction implementation.
[[nodiscard]] SetupResult setup(
    const std::filesystem::path& root,
    const std::filesystem::path& rom_path,
    SetupReporter report = {});

// Extract.ConvertSdat. Writes mph.ncsflib and one .minincsf per populated
// sequence INFO entry, preserving the managed NCSF header and tag layout.
[[nodiscard]] std::size_t convert_sdat(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_directory);

// Extract.LoadRuntimeData. Returns false for First Hunt/unknown layouts,
// where the managed table likewise has no complete MPH runtime data row.
[[nodiscard]] bool load_runtime_data(const assets::Store& assets);

// Command-line equivalent of MphRead/Utility/Extract.cs's ROM entry point.
// The argument scan stays here so Utility/main.cpp only dispatches the
// command; ROM validation and extraction use the shared nds::Rom boundary.
int run_command(int argc, char** argv);

} // namespace fruityprime::utility::extract
