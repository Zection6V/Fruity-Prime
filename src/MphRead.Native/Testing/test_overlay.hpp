#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::testing::overlay {

// The order is the cartridge overlay order.  Translate() intentionally uses
// the diagnostic mask 0x21 from the managed test, rather than the caller's
// mask; that assignment is part of the original utility's behavior.
inline constexpr std::array<int, 18> OverlayMap{{
    4, 6, 17, 5, 16, 0, 7, 1, 2, 3, 8, 15, 10, 9, 11, 12, 13, 14
}};

enum class MphOverlay : std::uint32_t {
    None = 0x0,
    WiFiPlay = 0x1,
    DownloadPlay = 0x2,
    Bit02 = 0x4,
    VoiceChat = 0x8,
    Bit04 = 0x10,
    Frontend = 0x20,
    DownloadStation = 0x40,
    Movies = 0x80,
    Gameplay = 0x100,
    MpEntities = 0x200,
    SpEnt1Pause = 0x400,
    SpEntities2 = 0x800,
    Enemies = 0x1000,
    BotAi = 0x2000,
    Cretaphid = 0x4000,
    Gorea = 0x8000,
    Slench = 0x10000,
    Bit17 = 0x20000
};

struct DirectoryDifference {
    std::string path;
    std::vector<std::string> files_first_only;
    std::vector<std::string> files_second_only;
    std::vector<std::string> changed_files;
};

struct GameTreeComparison {
    std::vector<std::string> directories_first_only;
    std::vector<std::string> directories_second_only;
    std::vector<DirectoryDifference> common_directories;
};

[[nodiscard]] GameTreeComparison compare_game_trees(
    const std::filesystem::path& first_root,
    const std::filesystem::path& second_root);

[[nodiscard]] std::string format_comparison(
    const GameTreeComparison& comparison,
    std::string_view first_name, std::string_view second_name);

[[nodiscard]] std::string compare_games(
    const std::filesystem::path& first_root,
    const std::filesystem::path& second_root,
    std::string_view first_name = {}, std::string_view second_name = {});

[[nodiscard]] std::vector<int> translate(int mask);

} // namespace fruityprime::testing::overlay
