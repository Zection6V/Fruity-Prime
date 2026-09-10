#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>

namespace fruityprime::formats {

// The paths.txt keys are part of the managed install contract.  Keep the
// table here rather than making each launcher/runtime frontend know which
// regional dumps exist.
inline constexpr std::string_view A76E0 = "A76E0";
inline constexpr std::string_view AMHE0 = "AMHE0";
inline constexpr std::string_view AMHE1 = "AMHE1";
inline constexpr std::string_view AMHP0 = "AMHP0";
inline constexpr std::string_view AMHP1 = "AMHP1";
inline constexpr std::string_view AMHJ0 = "AMHJ0";
inline constexpr std::string_view AMHJ1 = "AMHJ1";
inline constexpr std::string_view AMHK0 = "AMHK0";
inline constexpr std::string_view AMFE0 = "AMFE0";
inline constexpr std::string_view AMFP0 = "AMFP0";
inline constexpr std::string_view Export = "Export";

inline constexpr std::string_view CurrentDataVersion = "0.35.1.0";
inline constexpr std::string_view MinimumExtractVersion = "0.19.0.0";

// Value-style counterpart of the managed Formats.Paths static table.  A
// caller can keep one instance per install, which avoids the global state
// making tests and a launcher preview affect one another.
class Paths {
public:
    Paths();

    void update(const std::filesystem::path& root);
    void choose_mph_path() noexcept;
    void choose_fh_path() noexcept;

    [[nodiscard]] bool set_path(std::string_view key,
                                const std::filesystem::path& path);
    [[nodiscard]] std::string path(std::string_view key) const;
    [[nodiscard]] const std::map<std::string, std::string>& all_paths() const
        noexcept {
        return all_paths_;
    }

    [[nodiscard]] const std::string& file_system() const noexcept;
    [[nodiscard]] const std::string& fh_file_system() const noexcept;
    [[nodiscard]] const std::string& export_path() const noexcept;

    [[nodiscard]] bool write(const std::filesystem::path& root,
                             std::string_view version = CurrentDataVersion)
        const;

    std::string mph_key = std::string(AMHE0);
    std::string fh_key = std::string(AMFE0);

    // Paths.IsMphAmericas / IsMphEurope / IsMphJapan / IsMphKorea.  Some
    // cartridge tables differ by region, so the readers ask which dump is
    // selected rather than assuming the American one.
    [[nodiscard]] bool is_mph_americas() const noexcept {
        return mph_key == AMHE0 || mph_key == AMHE1;
    }
    [[nodiscard]] bool is_mph_europe() const noexcept {
        return mph_key == AMHP0 || mph_key == AMHP1;
    }
    [[nodiscard]] bool is_mph_japan() const noexcept {
        return mph_key == AMHJ0 || mph_key == AMHJ1;
    }
    [[nodiscard]] bool is_mph_korea() const noexcept {
        return mph_key == AMHK0;
    }

private:
    void reset();

    std::map<std::string, std::string> all_paths_;
};

// C# Formats.Paths is process-wide static state. Existing value-style callers
// remain available for isolated tests and launcher previews; the Program
// entry path uses this singleton for the managed global contract.
[[nodiscard]] Paths& global_paths() noexcept;

} // namespace fruityprime::formats
