#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::strings {

enum class Language : std::uint8_t {
    English,
    French,
    German,
    Italian,
    Japanese,
    Spanish
};

// The cartridge string tables use a four-byte reversed ID followed by a
// pointer/length pair and two display attributes.  Keep the decoded record
// independent of the frontend so HUD, menu, and scan code can share the same
// parser on a ROM-backed or extracted Store.
struct TableEntry {
    std::string id;
    char prefix = '\0';
    std::string value1;
    std::string value2;
    std::uint8_t speed = 0;
    char category = '\0';
    std::string string1;
    std::string string2;
};

namespace tables {
inline constexpr std::string_view GameMessages = "GameMessages.bin";
inline constexpr std::string_view HudMessagesMp = "HudMessagesMP.bin";
inline constexpr std::string_view HudMessagesSp = "HudMessagesSP.bin";
inline constexpr std::string_view HudMsgsCommon = "HudMsgsCommon.bin";
inline constexpr std::string_view LocationNames = "LocationNames.bin";
inline constexpr std::string_view MbBanner = "MBBanner.bin";
inline constexpr std::string_view ScanLog = "ScanLog.bin";
inline constexpr std::string_view ScanLogSorted = "ScanLogSorted.bin";
inline constexpr std::string_view ShipInSpace = "ShipInSpace.bin";
inline constexpr std::string_view ShipOnGround = "ShipOnGround.bin";
inline constexpr std::string_view WeaponNames = "WeaponNames.bin";
inline constexpr std::array<std::string_view, 10> All{{
    GameMessages, HudMessagesMp, HudMessagesSp, HudMsgsCommon,
    LocationNames, MbBanner, ScanLog, ShipInSpace, ShipOnGround, WeaponNames
}};
}

// Parse one of the binary string tables used by Strings.cs.  ScanLog has an
// eight-byte header and starts its offset table at byte 8; all other tables
// start at byte 4.  The managed Marshal layout is 12 bytes (the final
// category is a UTF-16 char even though the source comment calls it size 11).
// Invalid entry offsets are skipped just like the managed reader, while
// malformed table bounds fail at the format boundary.
[[nodiscard]] std::vector<TableEntry> read_table(
    std::span<const std::uint8_t> bytes, bool scan_log = false,
    bool game_messages = false);

[[nodiscard]] const TableEntry* find_entry(
    const std::vector<TableEntry>& entries, char prefix,
    std::uint32_t id) noexcept;
[[nodiscard]] std::string get_message(
    const std::vector<TableEntry>& entries, char prefix,
    std::uint32_t id);
[[nodiscard]] std::string get_hud_message(
    std::uint32_t id, const std::vector<TableEntry>& common,
    const std::vector<TableEntry>& single_player,
    const std::vector<TableEntry>& multiplayer);

// Strings.GetScanEntryCategory's category alphabet.  Unknown categories use
// 5, which is the managed "not renderable as a scan icon" result.
[[nodiscard]] int scan_category(char category) noexcept;
[[nodiscard]] float scan_time_seconds(std::uint8_t speed) noexcept;
[[nodiscard]] int scan_entry_category(
    const std::vector<TableEntry>& entries, std::int32_t scan_id) noexcept;
[[nodiscard]] float scan_entry_time(
    const std::vector<TableEntry>& entries, std::int32_t scan_id) noexcept;

[[nodiscard]] std::string folder_for_language(Language language);
[[nodiscard]] std::string text_file_suffix(Language language,
                                             bool mph_europe) ;
[[nodiscard]] std::vector<std::string> read_text_file(
    std::span<const std::uint8_t> bytes);
[[nodiscard]] TableEntry empty_scan_entry();

class Font {
public:
    [[nodiscard]] static Font& normal() noexcept;
    [[nodiscard]] static Font& kanji() noexcept;

    void set_data(std::span<const std::uint8_t> widths,
                  std::span<const std::uint8_t> offsets,
                  std::span<const std::uint8_t> chars, std::int32_t min_char,
                  bool packed = true);
    [[nodiscard]] const std::vector<std::int32_t>& widths() const noexcept {
        return widths_;
    }
    [[nodiscard]] const std::vector<std::int32_t>& offsets() const noexcept {
        return offsets_;
    }
    [[nodiscard]] const std::vector<std::uint8_t>& character_data() const
        noexcept {
        return character_data_;
    }
    [[nodiscard]] std::int32_t min_character() const noexcept {
        return min_character_;
    }

private:
    std::vector<std::int32_t> widths_;
    std::vector<std::int32_t> offsets_;
    std::vector<std::uint8_t> character_data_;
    std::int32_t min_character_ = 0;
};

class Catalog {
public:
    void clear() noexcept { values_.clear(); }
    void set(Language language, std::string key, std::string value);
    [[nodiscard]] std::string lookup(Language language, std::string_view key,
                                      std::string_view fallback = {}) const;
    [[nodiscard]] bool contains(Language language, std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }

    [[nodiscard]] static Catalog builtins();

private:
    using Key = std::pair<Language, std::string>;
    std::map<Key, std::string> values_;
};

// The DS text tables use a restricted byte-oriented alphabet. This helper
// makes malformed bytes visible without making the frontend depend on a
// process locale.
[[nodiscard]] std::string replace_non_ascii(std::string_view value);

} // namespace fruityprime::strings

namespace MphReadNative {
namespace Strings = ::fruityprime::strings;
using StringCatalog = ::fruityprime::strings::Catalog;
}
