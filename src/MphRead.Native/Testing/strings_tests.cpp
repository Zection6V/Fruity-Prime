#include "Strings.hpp"
#include "Assets/game_assets.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void put_u16(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

void put_record(std::vector<std::uint8_t>& bytes, std::size_t offset,
                const char* raw_id, std::uint32_t text_offset,
                std::uint16_t text_length, std::uint8_t speed,
                char category) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(raw_id[index]);
    }
    put_u32(bytes, offset + 4, text_offset);
    put_u16(bytes, offset + 8, text_length);
    bytes[offset + 10] = speed;
    bytes[offset + 11] = static_cast<std::uint8_t>(category);
}

} // namespace

int main() {
    try {
        std::vector<std::uint8_t> normal(96, 0);
        put_u32(normal, 0, 2);
        put_record(normal, 4, "100L", 40, 14, 3, 'O');
        put_record(normal, 16, "200L", 60, 5, 7, 'E');
        const std::string first = "Object\\Details";
        const std::string second = "Ammo";
        std::copy(first.begin(), first.end(), normal.begin() + 40);
        std::copy(second.begin(), second.end(), normal.begin() + 60);

        const auto entries = fruityprime::strings::read_table(normal);
        require(entries.size() == 2, "string table count mismatch");
        const auto* object = fruityprime::strings::find_entry(entries, 'L', 1);
        require(object != nullptr && object->id == "L001"
                    && object->value1 == "Object"
                    && object->value2 == "Details"
                    && object->string1 == "Object"
                    && object->string2 == "Details"
                    && object->speed == 3 && object->category == 'O',
                "string table record mismatch");
        require(fruityprime::strings::find_entry(entries, 'L', 999) == nullptr,
                "missing string table record was found");

        std::vector<std::uint8_t> messages(80, 0);
        put_u32(messages, 0, 1);
        put_record(messages, 4, "100M", 16, 13, 1, ' ');
        const std::string message = "$MHello\\World";
        std::copy(message.begin(), message.end(), messages.begin() + 16);
        const auto message_entries = fruityprime::strings::read_table(
            messages, false, true);
        require(message_entries.size() == 1
                    && message_entries[0].prefix == 'M'
                    && message_entries[0].value1 == "Hello"
                    && message_entries[0].value2 == "World",
                "game message string table mismatch");

        std::vector<std::uint8_t> scan(96, 0);
        put_u32(scan, 0, 1);
        put_record(scan, 8, "412L", 24, 5, 6, 'B');
        const std::string bioform = "Mite";
        std::copy(bioform.begin(), bioform.end(), scan.begin() + 24);
        const auto scan_entries = fruityprime::strings::read_table(scan, true);
        const auto* entry = fruityprime::strings::find_entry(
            scan_entries, 'L', 214);
        require(entry != nullptr && entry->value1 == "Mite"
                    && fruityprime::strings::scan_category(entry->category) == 1
                    && fruityprime::strings::scan_time_seconds(entry->speed)
                        > 1.99F,
                "scan log table mismatch");
        require(fruityprime::strings::scan_category('S') == 5
                    && fruityprime::strings::scan_category('e') == 3,
                "scan category mapping mismatch");

        std::vector<std::uint8_t> invalid(20, 0);
        put_u32(invalid, 0, 1);
        put_record(invalid, 4, "100L", 100, 1, 0, 'L');
        require(fruityprime::strings::read_table(invalid).empty(),
                "invalid string-table offset was not skipped");
        const std::vector<fruityprime::strings::TableEntry> hud{
            {"H001", '\0', "Common", "", 0, ' ', "Common", ""}};
        require(fruityprime::strings::get_hud_message(
                            1, hud, {}, {}) == "Common"
                    && fruityprime::strings::get_hud_message(
                           999, hud, {}, {}) == " "
                    && fruityprime::strings::scan_entry_category(scan_entries, 999)
                           == 0
                    && fruityprime::strings::scan_entry_time(scan_entries, 999)
                           == 2.0F,
                "managed string helper mismatch");
        require(fruityprime::strings::folder_for_language(
                            fruityprime::strings::Language::German)
                        == "stringTables_gr"
                    && fruityprime::strings::text_file_suffix(
                           fruityprime::strings::Language::English, true)
                           == "en-gb"
                    && fruityprime::strings::replace_non_ascii("A") == "A"
                    && fruityprime::strings::replace_non_ascii(
                           std::string("\xC2\x80", 2)) == "€"
                    && fruityprime::strings::replace_non_ascii(
                           std::string("\xA0", 1)) == "<kanji>",
                "managed string conversion mismatch");

        std::vector<std::uint8_t> text_file(40, 0);
        put_u32(text_file, 0, 16);
        put_u32(text_file, 4, 0);
        put_u32(text_file, 16, 32);
        put_u32(text_file, 20, 32);
        put_u16(text_file, 24, 3);
        put_u16(text_file, 26, 3);
        text_file[32] = 'a';
        text_file[33] = 'b';
        text_file[34] = 'c';
        const auto text_values = fruityprime::strings::read_text_file(text_file);
        require(text_values.size() == 1 && text_values[0] == "abc",
                "frontend text-file mismatch");
        fruityprime::strings::Font font;
        const std::vector<std::uint8_t> widths{3, 4};
        const std::vector<std::uint8_t> offsets{0, 0xff};
        const std::vector<std::uint8_t> glyphs{0x21};
        font.set_data(widths, offsets, glyphs, 32);
        require(font.widths().size() == 2 && font.offsets()[1] == -1
                    && font.character_data() == std::vector<std::uint8_t>{1, 2}
                    && font.min_character() == 32,
                "font data mismatch");

        if (const char* rom_value = std::getenv("FRUITY_PRIME_TEST_NDS");
            rom_value != nullptr && rom_value[0] != '\0') {
            const auto assets = fruityprime::assets::Store::from_rom(rom_value);
            const auto real_entries = fruityprime::strings::read_table(
                assets.bytes("stringTables/ScanLog.bin"), true);
            std::size_t lore_entries = 0;
            std::size_t categorized_entries = 0;
            for (const auto& real_entry : real_entries) {
                if (real_entry.id.size() == 4
                    && real_entry.id.front() == 'L') {
                    ++lore_entries;
                    if (fruityprime::strings::scan_category(
                            real_entry.category) < 5) {
                        ++categorized_entries;
                    }
                }
            }
            require(lore_entries > 200 && categorized_entries == lore_entries,
                    "real ScanLog table did not decode its categories");
            std::cout << "real scan log: entries=" << lore_entries
                      << " categorized=" << categorized_entries << '\n';
        }

        std::cout << "native strings tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native strings tests failed: " << error.what() << '\n';
        return 1;
    }
}
