#include "Testing/test_overlay.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

void write_file(const std::filesystem::path& path, const char* text) {
    std::ofstream output(path, std::ios::binary);
    output << text;
}

} // namespace

int main() {
    using namespace fruityprime::testing::overlay;

    const auto translated = translate(static_cast<int>(MphOverlay::Gameplay));
    assert((translated == std::vector<int>{0, 4}));
    assert(static_cast<std::uint32_t>(MphOverlay::Slench) == 0x10000U);

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "fruity_prime_test_overlay";
    const std::filesystem::path first = root / "first";
    const std::filesystem::path second = root / "second";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(first / "same");
    std::filesystem::create_directories(first / "only_first");
    std::filesystem::create_directories(second / "same");
    std::filesystem::create_directories(second / "only_second");
    write_file(first / "same" / "changed.bin", "first");
    write_file(second / "same" / "changed.bin", "second");
    write_file(first / "same" / "first.bin", "same");
    write_file(second / "same" / "second.bin", "same");

    const GameTreeComparison comparison = compare_game_trees(first, second);
    assert((comparison.directories_first_only
            == std::vector<std::string>{"only_first"}));
    assert((comparison.directories_second_only
            == std::vector<std::string>{"only_second"}));
    assert(comparison.common_directories.size() == 1);
    assert(comparison.common_directories[0].path == "same");
    assert((comparison.common_directories[0].files_first_only
            == std::vector<std::string>{"first.bin"}));
    assert((comparison.common_directories[0].files_second_only
            == std::vector<std::string>{"second.bin"}));
    assert((comparison.common_directories[0].changed_files
            == std::vector<std::string>{"changed.bin"}));
    const std::string text = compare_games(first, second, "game1", "game2");
    assert(text.find("Directories in game1 not in game2") != std::string::npos);
    assert(text.find("Changed files:\nchanged.bin") != std::string::npos);

    std::filesystem::remove_all(root, error);
    return 0;
}
