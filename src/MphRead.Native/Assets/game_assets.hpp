#pragma once

#include "Utility/archive.hpp"
#include "Formats/model_format.hpp"
#include "Assets/nds_rom.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::assets {

// Common asset access for an extracted game directory and a cartridge image.
// Keeping this boundary independent of the renderer lets the native runtime
// use the same model/archive code for command-line probes and a real window.
class Store {
public:
    [[nodiscard]] static Store from_path(const std::filesystem::path& path);
    [[nodiscard]] static Store from_directory(const std::filesystem::path& path);
    [[nodiscard]] static Store from_rom(const std::filesystem::path& path);

    [[nodiscard]] bool is_rom() const noexcept { return rom_.has_value(); }
    [[nodiscard]] std::optional<nds::RomHeader> rom_header() const {
        if (!rom_.has_value()) {
            return std::nullopt;
        }
        return rom_->header();
    }
    [[nodiscard]] std::optional<std::string> game_key() const;
    [[nodiscard]] std::vector<std::uint8_t> bytes(std::string_view path) const;
    [[nodiscard]] archive::Archive archive(std::string_view path) const;
    [[nodiscard]] model::File model(std::string_view path) const;
    [[nodiscard]] model::File model_from_archive(
        std::string_view archive_path, std::string_view entry_name) const;

private:
    explicit Store(std::filesystem::path root);
    explicit Store(nds::Rom rom);

    [[nodiscard]] static std::string normalize_relative(std::string_view path);

    std::filesystem::path root_;
    std::optional<nds::Rom> rom_;
};

} // namespace fruityprime::assets
