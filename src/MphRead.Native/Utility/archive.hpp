#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::archive {

struct Entry {
    std::string filename;
    std::uint32_t offset = 0;
    std::uint32_t padded_size = 0;
    std::uint32_t target_size = 0;
};

class Archive {
public:
    static constexpr std::string_view Magic = "SNDFILE";
    static constexpr std::size_t HeaderSize = 32;
    static constexpr std::size_t EntrySize = 64;

    Archive() = default;

    [[nodiscard]] static Archive parse(std::span<const std::uint8_t> bytes);
    [[nodiscard]] static Archive read_file(const std::filesystem::path& path);

    [[nodiscard]] const std::vector<Entry>& entries() const noexcept {
        return entries_;
    }

    [[nodiscard]] std::vector<std::uint8_t> file(std::size_t index) const;

    // Extracts target_size bytes, excluding the alignment padding kept in the
    // archive. Returns the number of files written.
    [[nodiscard]] std::size_t extract(const std::filesystem::path& destination) const;

    static void create(const std::filesystem::path& destination,
                       const std::vector<std::filesystem::path>& files);

private:
    std::vector<std::uint8_t> bytes_;
    std::vector<Entry> entries_;
};

} // namespace fruityprime::archive
