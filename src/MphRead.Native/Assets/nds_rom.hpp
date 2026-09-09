#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::nds {

struct RomHeader {
    std::string title;
    std::string game_code;
    std::string maker_code;
    std::uint8_t unit_code = 0;
    std::uint8_t seed = 0;
    std::uint8_t capacity = 0;
    std::uint8_t region = 0;
    std::uint8_t version = 0;
    std::uint8_t auto_start = 0;
    std::int32_t arm9_offset = 0;
    std::int32_t arm9_entry_address = 0;
    std::int32_t arm9_ram_address = 0;
    std::int32_t arm9_size = 0;
    std::int32_t arm7_offset = 0;
    std::int32_t arm7_entry_address = 0;
    std::int32_t arm7_ram_address = 0;
    std::int32_t arm7_size = 0;
    std::uint32_t fnt_offset = 0;
    std::uint32_t fnt_size = 0;
    std::uint32_t fat_offset = 0;
    std::uint32_t fat_size = 0;
    std::int32_t overlay9_offset = 0;
    std::int32_t overlay9_size = 0;
    std::int32_t overlay7_offset = 0;
    std::int32_t overlay7_size = 0;
    std::uint32_t read_flags = 0;
    std::uint32_t init_flags = 0;
    std::int32_t banner_offset = 0;
};

struct FileEntry {
    std::string path;
    std::uint32_t file_id = 0;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
};

class Rom {
public:
    static constexpr std::size_t HeaderSize = 0x6c;

    [[nodiscard]] static Rom read_file(const std::filesystem::path& path);
    [[nodiscard]] static Rom from_bytes(std::vector<std::uint8_t> bytes);

    [[nodiscard]] const RomHeader& header() const noexcept { return header_; }
    [[nodiscard]] const std::vector<FileEntry>& files() const;
    [[nodiscard]] std::vector<std::uint8_t> file(std::uint32_t file_id) const;
    [[nodiscard]] std::vector<std::uint8_t> file(std::string_view path) const;
    [[nodiscard]] std::vector<std::uint8_t> raw_range(
        std::uint32_t offset, std::uint32_t size) const;
    // Runtime data written by the managed extractor.  These helpers resolve
    // the ARM9/overlay table and apply the reverse-LZ wrapper, so callers can
    // use the same `_bin/` offsets with a direct ROM as with extracted files.
    [[nodiscard]] std::vector<std::uint8_t> arm9() const;
    [[nodiscard]] std::vector<std::uint8_t> overlay9(
        std::uint32_t overlay_id) const;
    [[nodiscard]] std::size_t extract(const std::filesystem::path& destination) const;

private:
    explicit Rom(std::vector<std::uint8_t> bytes);

    void parse_header();
    void parse_files() const;

    std::vector<std::uint8_t> bytes_;
    RomHeader header_;
    mutable bool files_parsed_ = false;
    mutable std::vector<FileEntry> files_;
};

} // namespace fruityprime::nds
