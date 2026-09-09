#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace fruityprime::mods::thumbnail {

// Append-only diagnostic file for thumbnail workers.  A write failure is
// recorded and then ignored: a missing cosmetic preview must not terminate
// the launcher or the game.
class Log final {
public:
    Log() = default;

    [[nodiscard]] bool begin(const std::filesystem::path& path,
                             std::size_t rooms,
                             std::string_view build,
                             std::string_view assembly,
                             std::string_view file_stamp = {}) noexcept;

    [[nodiscard]] bool write(std::string_view line) noexcept;

    [[nodiscard]] bool failed() const noexcept { return failed_; }
    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
    bool failed_ = false;
};

} // namespace fruityprime::mods::thumbnail
