#include "Strings.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <utility>

namespace fruityprime::strings {

namespace {

constexpr std::array<std::string_view, 416> NonAscii{
    /* 0 */ "€",
    /* 1 */ " ",
    /* 2 */ "‚",
    /* 3 */ "ƒ",
    /* 4 */ "„",
    /* 5 */ "…",
    /* 6 */ "†",
    /* 7 */ "‡",
    /* 8 */ "ˆ",
    /* 9 */ "‰",
    /* 10 */ "Š",
    /* 11 */ "‹",
    /* 12 */ "Œ",
    /* 13 */ " ",
    /* 14 */ "Ž",
    /* 15 */ " ",
    /* 16 */ " ",
    /* 17 */ "‘",
    /* 18 */ "’",
    /* 19 */ "“",
    /* 20 */ "”",
    /* 21 */ "•",
    /* 22 */ "–",
    /* 23 */ "—",
    /* 24 */ "˜",
    /* 25 */ "™",
    /* 26 */ "š",
    /* 27 */ "›",
    /* 28 */ "œ",
    /* 29 */ " ",
    /* 30 */ "ž",
    /* 31 */ "Ÿ",
    /* 32 */ " ",
    /* 33 */ "¡",
    /* 34 */ "¢",
    /* 35 */ "£",
    /* 36 */ "¤",
    /* 37 */ "¥",
    /* 38 */ "¦",
    /* 39 */ "§",
    /* 40 */ "¨",
    /* 41 */ "©",
    /* 42 */ "ª",
    /* 43 */ "«",
    /* 44 */ "¬",
    /* 45 */ "–",
    /* 46 */ "®",
    /* 47 */ "¯",
    /* 48 */ "°",
    /* 49 */ "±",
    /* 50 */ "²",
    /* 51 */ "³",
    /* 52 */ "´",
    /* 53 */ "µ",
    /* 54 */ "¶",
    /* 55 */ "·",
    /* 56 */ "¸",
    /* 57 */ "¹",
    /* 58 */ "º",
    /* 59 */ "»",
    /* 60 */ "¼",
    /* 61 */ "½",
    /* 62 */ "¾",
    /* 63 */ "¿",
    /* 64 */ "À",
    /* 65 */ "Á",
    /* 66 */ "Â",
    /* 67 */ "Ã",
    /* 68 */ "Ä",
    /* 69 */ "Å",
    /* 70 */ "Æ",
    /* 71 */ "Ç",
    /* 72 */ "È",
    /* 73 */ "É",
    /* 74 */ "Ê",
    /* 75 */ "Ë",
    /* 76 */ "Ì",
    /* 77 */ "Í",
    /* 78 */ "Î",
    /* 79 */ "Ï",
    /* 80 */ "Ð",
    /* 81 */ "Ñ",
    /* 82 */ "Ò",
    /* 83 */ "Ó",
    /* 84 */ "Ô",
    /* 85 */ "Õ",
    /* 86 */ "Ö",
    /* 87 */ "×",
    /* 88 */ "Ø",
    /* 89 */ "Ù",
    /* 90 */ "Ú",
    /* 91 */ "Û",
    /* 92 */ "Ü",
    /* 93 */ "Ý",
    /* 94 */ "Þ",
    /* 95 */ "ß",
    /* 96 */ "à",
    /* 97 */ "á",
    /* 98 */ "â",
    /* 99 */ "ã",
    /* 100 */ "ä",
    /* 101 */ "å",
    /* 102 */ "æ",
    /* 103 */ "ç",
    /* 104 */ "è",
    /* 105 */ "é",
    /* 106 */ "ê",
    /* 107 */ "ë",
    /* 108 */ "ì",
    /* 109 */ "í",
    /* 110 */ "î",
    /* 111 */ "ï",
    /* 112 */ "ð",
    /* 113 */ "ñ",
    /* 114 */ "ò",
    /* 115 */ "ó",
    /* 116 */ "ô",
    /* 117 */ "õ",
    /* 118 */ "ö",
    /* 119 */ "÷",
    /* 120 */ "ø",
    /* 121 */ "ù",
    /* 122 */ "ú",
    /* 123 */ "û",
    /* 124 */ "ü",
    /* 125 */ "ý",
    /* 126 */ "þ",
    /* 127 */ "ÿ",
    /* 128 */ " ",
    /* 129 */ "ぁ",
    /* 130 */ "あ",
    /* 131 */ "ぃ",
    /* 132 */ "い",
    /* 133 */ "ぅ",
    /* 134 */ "う",
    /* 135 */ "ぇ",
    /* 136 */ "え",
    /* 137 */ "ぉ",
    /* 138 */ "お",
    /* 139 */ "か",
    /* 140 */ "が",
    /* 141 */ "き",
    /* 142 */ "ぎ",
    /* 143 */ "く",
    /* 144 */ "ぐ",
    /* 145 */ "け",
    /* 146 */ "げ",
    /* 147 */ "こ",
    /* 148 */ "ご",
    /* 149 */ "さ",
    /* 150 */ "ざ",
    /* 151 */ "し",
    /* 152 */ "じ",
    /* 153 */ "す",
    /* 154 */ "ず",
    /* 155 */ "せ",
    /* 156 */ "ぜ",
    /* 157 */ "そ",
    /* 158 */ "ぞ",
    /* 159 */ "た",
    /* 160 */ "だ",
    /* 161 */ "ち",
    /* 162 */ "ぢ",
    /* 163 */ "っ",
    /* 164 */ "つ",
    /* 165 */ "づ",
    /* 166 */ "て",
    /* 167 */ "で",
    /* 168 */ "と",
    /* 169 */ "ど",
    /* 170 */ "な",
    /* 171 */ "に",
    /* 172 */ "ぬ",
    /* 173 */ "ね",
    /* 174 */ "の",
    /* 175 */ "は",
    /* 176 */ "ば",
    /* 177 */ "ぱ",
    /* 178 */ "ひ",
    /* 179 */ "び",
    /* 180 */ "ぴ",
    /* 181 */ "ふ",
    /* 182 */ "ぶ",
    /* 183 */ "ぷ",
    /* 184 */ "へ",
    /* 185 */ "べ",
    /* 186 */ "ぺ",
    /* 187 */ "ほ",
    /* 188 */ "ぼ",
    /* 189 */ "ぽ",
    /* 190 */ "ま",
    /* 191 */ "み",
    /* 192 */ "む",
    /* 193 */ "め",
    /* 194 */ "も",
    /* 195 */ "ゃ",
    /* 196 */ "や",
    /* 197 */ "ゅ",
    /* 198 */ "ゆ",
    /* 199 */ "ょ",
    /* 200 */ "よ",
    /* 201 */ "ら",
    /* 202 */ "り",
    /* 203 */ "る",
    /* 204 */ "れ",
    /* 205 */ "ろ",
    /* 206 */ "ゎ",
    /* 207 */ "わ",
    /* 208 */ "ゐ",
    /* 209 */ "ゑ",
    /* 210 */ "を",
    /* 211 */ "ん",
    /* 212 */ "ァ",
    /* 213 */ "ア",
    /* 214 */ "ィ",
    /* 215 */ "イ",
    /* 216 */ "ゥ",
    /* 217 */ "ウ",
    /* 218 */ "ェ",
    /* 219 */ "エ",
    /* 220 */ "ォ",
    /* 221 */ "オ",
    /* 222 */ "カ",
    /* 223 */ "ガ",
    /* 224 */ "キ",
    /* 225 */ "ギ",
    /* 226 */ "ク",
    /* 227 */ "グ",
    /* 228 */ "ケ",
    /* 229 */ "ゲ",
    /* 230 */ "コ",
    /* 231 */ "ゴ",
    /* 232 */ "サ",
    /* 233 */ "ザ",
    /* 234 */ "シ",
    /* 235 */ "ジ",
    /* 236 */ "ス",
    /* 237 */ "ズ",
    /* 238 */ "セ",
    /* 239 */ "ゼ",
    /* 240 */ "ソ",
    /* 241 */ "ゾ",
    /* 242 */ "タ",
    /* 243 */ "ダ",
    /* 244 */ "チ",
    /* 245 */ "ヂ",
    /* 246 */ "ッ",
    /* 247 */ "ツ",
    /* 248 */ "ヅ",
    /* 249 */ "テ",
    /* 250 */ "デ",
    /* 251 */ "ト",
    /* 252 */ "ド",
    /* 253 */ "ナ",
    /* 254 */ "ニ",
    /* 255 */ "ヌ",
    /* 256 */ "ネ",
    /* 257 */ "ノ",
    /* 258 */ "ハ",
    /* 259 */ "バ",
    /* 260 */ "パ",
    /* 261 */ "ヒ",
    /* 262 */ "ビ",
    /* 263 */ "ピ",
    /* 264 */ "フ",
    /* 265 */ "ブ",
    /* 266 */ "プ",
    /* 267 */ "ヘ",
    /* 268 */ "ベ",
    /* 269 */ "ペ",
    /* 270 */ "ホ",
    /* 271 */ "ボ",
    /* 272 */ "ポ",
    /* 273 */ "マ",
    /* 274 */ "ミ",
    /* 275 */ "ム",
    /* 276 */ "メ",
    /* 277 */ "モ",
    /* 278 */ "ャ",
    /* 279 */ "ヤ",
    /* 280 */ "ュ",
    /* 281 */ "ユ",
    /* 282 */ "ョ",
    /* 283 */ "ヨ",
    /* 284 */ "ラ",
    /* 285 */ "リ",
    /* 286 */ "ル",
    /* 287 */ "レ",
    /* 288 */ "ロ",
    /* 289 */ "ヮ",
    /* 290 */ "ワ",
    /* 291 */ "ヰ",
    /* 292 */ "ヱ",
    /* 293 */ "ヲ",
    /* 294 */ "ン",
    /* 295 */ "ヴ",
    /* 296 */ "ヵ",
    /* 297 */ "ㇰ",
    /* 298 */ " ",
    /* 299 */ " ",
    /* 300 */ " ",
    /* 301 */ " ",
    /* 302 */ " ",
    /* 303 */ " ",
    /* 304 */ " ",
    /* 305 */ "、",
    /* 306 */ "。",
    /* 307 */ "'",
    /* 308 */ "・",
    /* 309 */ "・",
    /* 310 */ ":",
    /* 311 */ ";",
    /* 312 */ "?",
    /* 313 */ "!",
    /* 314 */ "゛",
    /* 315 */ "゜",
    /* 316 */ "´",
    /* 317 */ "`",
    /* 318 */ "¨",
    /* 319 */ "^",
    /* 320 */ "‾",
    /* 321 */ "_",
    /* 322 */ " ",
    /* 323 */ " ",
    /* 324 */ "ゝ",
    /* 325 */ "ゞ",
    /* 326 */ " ",
    /* 327 */ " ",
    /* 328 */ "々",
    /* 329 */ " ",
    /* 330 */ " ",
    /* 331 */ "–",
    /* 332 */ "—",
    /* 333 */ "−",
    /* 334 */ "／",
    /* 335 */ "＼",
    /* 336 */ "˜",
    /* 337 */ " ",
    /* 338 */ "|",
    /* 339 */ "…",
    /* 340 */ " ",
    /* 341 */ "'",
    /* 342 */ "'",
    /* 343 */ "\"",
    /* 344 */ "\"",
    /* 345 */ "(",
    /* 346 */ ")",
    /* 347 */ "(",
    /* 348 */ ")",
    /* 349 */ "[",
    /* 350 */ "]",
    /* 351 */ "{",
    /* 352 */ "}",
    /* 353 */ "<",
    /* 354 */ ">",
    /* 355 */ " ",
    /* 356 */ " ",
    /* 357 */ "「",
    /* 358 */ "」",
    /* 359 */ " ",
    /* 360 */ " ",
    /* 361 */ " ",
    /* 362 */ " ",
    /* 363 */ "+",
    /* 364 */ "-",
    /* 365 */ "±",
    /* 366 */ "×",
    /* 367 */ "÷",
    /* 368 */ "=",
    /* 369 */ " ",
    /* 370 */ " ",
    /* 371 */ " ",
    /* 372 */ " ",
    /* 373 */ " ",
    /* 374 */ "∞",
    /* 375 */ "∴",
    /* 376 */ " ",
    /* 377 */ " ",
    /* 378 */ "°",
    /* 379 */ "ᐟ",
    /* 380 */ "ᐥ ",
    /* 381 */ " ",
    /* 382 */ " ",
    /* 383 */ " ",
    /* 384 */ " ",
    /* 385 */ " ",
    /* 386 */ " ",
    /* 387 */ " ",
    /* 388 */ " ",
    /* 389 */ " ",
    /* 390 */ " ",
    /* 391 */ " ",
    /* 392 */ " ",
    /* 393 */ " ",
    /* 394 */ " ",
    /* 395 */ " ",
    /* 396 */ " ",
    /* 397 */ " ",
    /* 398 */ " ",
    /* 399 */ " ",
    /* 400 */ " ",
    /* 401 */ " ",
    /* 402 */ " ",
    /* 403 */ " ",
    /* 404 */ " ",
    /* 405 */ " ",
    /* 406 */ " ",
    /* 407 */ " ",
    /* 408 */ " ",
    /* 409 */ " ",
    /* 410 */ " ",
    /* 411 */ " ",
    /* 412 */ " ",
    /* 413 */ " ",
    /* 414 */ " ",
    /* 415 */ " "
};

[[nodiscard]] std::uint16_t read_u16(
    std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::out_of_range("string table u16 is outside the input");
    }
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1]) << 8;
}

[[nodiscard]] std::uint32_t read_u32(
    std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::out_of_range("string table u32 is outside the input");
    }
    return static_cast<std::uint32_t>(bytes[offset])
        | static_cast<std::uint32_t>(bytes[offset + 1]) << 8
        | static_cast<std::uint32_t>(bytes[offset + 2]) << 16
        | static_cast<std::uint32_t>(bytes[offset + 3]) << 24;
}

[[nodiscard]] std::string read_value(
    std::span<const std::uint8_t> bytes, std::uint32_t offset,
    std::uint16_t length) {
    if (offset >= bytes.size()) {
        return {};
    }
    const std::size_t start = static_cast<std::size_t>(offset);
    const std::size_t available = bytes.size() - start;
    if (length > available) {
        throw std::out_of_range("string table value is outside the input");
    }
    const std::size_t bounded = length;
    std::size_t actual = 0;
    while (actual < bounded && bytes[start + actual] != 0) {
        ++actual;
    }
    return std::string(reinterpret_cast<const char*>(bytes.data() + start),
                       actual);
}

void split_value(std::string& first, std::string& second) {
    const std::size_t slash = first.find('\\');
    if (slash == std::string::npos || first.find('\\', slash + 1)
            != std::string::npos) {
        return;
    }
    second = first.substr(slash + 1);
    first.resize(slash);
}

[[nodiscard]] std::uint32_t id_number(std::string_view id) noexcept {
    if (id.size() != 4) {
        return 0;
    }
    std::uint32_t value = 0;
    const auto result = std::from_chars(id.data() + 1, id.data() + id.size(),
                                        value);
    return result.ec == std::errc{} && result.ptr == id.data() + id.size()
        ? value : 0;
}

void append_utf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint <= 0x7f) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

} // namespace

std::vector<TableEntry> read_table(std::span<const std::uint8_t> bytes,
                                   bool scan_log, bool game_messages) {
    if (bytes.size() < 4) {
        throw std::out_of_range("string table header is outside the input");
    }
    const std::size_t count = read_u32(bytes, 0);
    const std::size_t records_offset = scan_log ? 8 : 4;
    // RawStringTableEntry's Marshal.SizeOf is 12: Id[4], Offset[4],
    // Length[2], Speed[1], and the one-byte marshalled Category at byte 11.
    constexpr std::size_t record_size = 12;
    if (count > (std::numeric_limits<std::size_t>::max() - records_offset)
                    / record_size
        || records_offset + count * record_size > bytes.size()) {
        throw std::out_of_range("string table records are outside the input");
    }

    std::vector<TableEntry> result;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const std::size_t offset = records_offset + index * record_size;
        std::string id;
        id.reserve(4);
        // RawStringTableEntry.Id is displayed in reverse byte order by the
        // managed code (the on-cartridge bytes are e.g. "412L").
        for (std::size_t byte = 0; byte < 4; ++byte) {
            id.push_back(static_cast<char>(bytes[offset + 3 - byte]));
        }
        const auto string_offset = read_u32(bytes, offset + 4);
        const auto length = read_u16(bytes, offset + 8);
        const char category = static_cast<char>(bytes[offset + 11]);
        // Strings.cs ignores A76E records whose offset points past the file.
        if (string_offset >= bytes.size()) {
            continue;
        }
        std::string value1 = read_value(bytes, string_offset, length);
        std::string value2;
        // Strings.cs removes '$' from every text table value before splitting
        // game messages into the optional two display lines.
        value1.erase(std::remove(value1.begin(), value1.end(), '$'),
                     value1.end());
        char prefix = '\0';
        if (game_messages && !value1.empty()) {
            prefix = value1.front();
            value1.erase(value1.begin());
        }
        split_value(value1, value2);
        std::string string1 = replace_non_ascii(value1);
        std::string string2 = replace_non_ascii(value2);
        result.push_back(TableEntry{std::move(id), prefix, std::move(value1),
                                    std::move(value2), bytes[offset + 10],
                                    category, std::move(string1),
                                    std::move(string2)});
    }
    return result;
}

const TableEntry* find_entry(const std::vector<TableEntry>& entries,
                             char prefix, std::uint32_t id) noexcept {
    for (const auto& entry : entries) {
        if (entry.id.size() != 4 || entry.id.front() != prefix) {
            continue;
        }
        if (id_number(entry.id) == id) {
            return &entry;
        }
    }
    return nullptr;
}

std::string get_message(const std::vector<TableEntry>& entries, char prefix,
                        std::uint32_t id) {
    const auto* entry = find_entry(entries, prefix, id);
    return entry == nullptr ? " " : entry->value1;
}

std::string get_hud_message(
    std::uint32_t id, const std::vector<TableEntry>& common,
    const std::vector<TableEntry>& single_player,
    const std::vector<TableEntry>& multiplayer) {
    if (id >= 1 && id <= 11) {
        return get_message(common, 'H', id);
    }
    if (id >= 101 && id <= 122) {
        return get_message(single_player, 'H', id);
    }
    if (id >= 201 && id <= 257) {
        return get_message(multiplayer, 'H', id);
    }
    if (id >= 301 && id <= 305) {
        return get_message(multiplayer, 'W', id - 300);
    }
    return " ";
}

int scan_category(char category) noexcept {
    switch (category) {
    case 'L':
    case 'l': return 0;
    case 'B':
    case 'b': return 1;
    case 'O':
    case 'o': return 2;
    case 'E':
    case 'e': return 3;
    case 'X':
    case 'x': return 4;
    default: return 5;
    }
}

float scan_time_seconds(std::uint8_t speed) noexcept {
    return 10.0F * static_cast<float>(speed & 7u) / 30.0F;
}

int scan_entry_category(const std::vector<TableEntry>& entries,
                        std::int32_t scan_id) noexcept {
    if (scan_id < 0) return 0;
    const auto* entry = find_entry(entries, 'L',
                                   static_cast<std::uint32_t>(scan_id));
    return entry == nullptr ? 0 : scan_category(entry->category);
}

float scan_entry_time(const std::vector<TableEntry>& entries,
                      std::int32_t scan_id) noexcept {
    if (scan_id < 0) return 2.0F;
    const auto* entry = find_entry(entries, 'L',
                                   static_cast<std::uint32_t>(scan_id));
    return entry == nullptr ? 2.0F : scan_time_seconds(entry->speed);
}

std::vector<std::string> read_text_file(
    std::span<const std::uint8_t> bytes) {
    std::size_t offset = 0;
    std::vector<std::uint32_t> locations;
    while (true) {
        if (offset > bytes.size() || bytes.size() - offset < 4) {
            throw std::out_of_range("text file offset list has no terminator");
        }
        const auto location = read_u32(bytes, offset);
        offset += 4;
        if (location == 0) break;
        locations.push_back(location);
    }

    std::vector<std::string> result;
    result.reserve(locations.size());
    for (const auto location : locations) {
        if (location > bytes.size() || bytes.size() - location < 12) {
            throw std::out_of_range("text file entry is outside the input");
        }
        const auto offset1 = read_u32(bytes, location);
        const auto offset2 = read_u32(bytes, location + 4);
        const auto length1 = read_u16(bytes, location + 8);
        const auto length2 = read_u16(bytes, location + 10);
        (void)offset2;
        (void)length2;
        std::string value = read_value(bytes, offset1, length1);
        value.resize(length1, '\0');
        result.push_back(std::move(value));
    }
    return result;
}

TableEntry empty_scan_entry() {
    TableEntry result{"000", '\0', "INVALID LOG ENTRY",
                      "This object has no entry in the log book.", 0, 'S'};
    result.string1 = replace_non_ascii(result.value1);
    result.string2 = replace_non_ascii(result.value2);
    return result;
}

Font& Font::normal() noexcept {
    static Font value;
    return value;
}

Font& Font::kanji() noexcept {
    static Font value;
    return value;
}

void Font::set_data(std::span<const std::uint8_t> widths,
                    std::span<const std::uint8_t> offsets,
                    std::span<const std::uint8_t> chars,
                    std::int32_t min_char, bool packed) {
    widths_.clear();
    widths_.reserve(widths.size());
    for (const auto value : widths) {
        widths_.push_back(value);
    }
    offsets_.clear();
    offsets_.reserve(offsets.size());
    for (const auto value : offsets) {
        offsets_.push_back(static_cast<std::int8_t>(value));
    }
    if (packed) {
        character_data_.resize(chars.size() * 2);
        for (std::size_t index = 0; index < chars.size(); ++index) {
            character_data_[index * 2] = chars[index] & 0x0f;
            character_data_[index * 2 + 1] = chars[index] >> 4;
        }
    } else {
        character_data_.assign(chars.begin(), chars.end());
    }
    min_character_ = min_char;
}

std::string folder_for_language(Language language) {
    switch (language) {
    case Language::French: return "stringTables_fr";
    case Language::German: return "stringTables_gr";
    case Language::Italian: return "stringTables_it";
    case Language::Japanese: return "stringTables_jp";
    case Language::Spanish: return "stringTables_sp";
    case Language::English: return "stringTables";
    }
    return "stringTables";
}

std::string text_file_suffix(Language language, bool mph_europe) {
    switch (language) {
    case Language::French: return "fr";
    case Language::German: return "de";
    case Language::Italian: return "it";
    case Language::Japanese: return "jp";
    case Language::Spanish: return "es";
    case Language::English: return mph_europe ? "en-gb" : "en";
    }
    return mph_europe ? "en-gb" : "en";
}

void Catalog::set(Language language, std::string key, std::string value) {
    values_[{language, std::move(key)}] = std::move(value);
}

std::string Catalog::lookup(Language language, std::string_view key,
                            std::string_view fallback) const {
    const auto exact = values_.find({language, std::string(key)});
    if (exact != values_.end()) {
        return exact->second;
    }
    const auto english = values_.find({Language::English, std::string(key)});
    return english != values_.end() ? english->second : std::string(fallback);
}

bool Catalog::contains(Language language, std::string_view key) const {
    return values_.find({language, std::string(key)}) != values_.end();
}

Catalog Catalog::builtins() {
    Catalog result;
    result.set(Language::English, "play", "Play");
    result.set(Language::French, "play", "Jouer");
    result.set(Language::German, "play", "Spielen");
    result.set(Language::Italian, "play", "Gioca");
    result.set(Language::Japanese, "play", "プレイ");
    result.set(Language::Spanish, "play", "Jugar");
    result.set(Language::English, "settings", "Settings");
    result.set(Language::French, "settings", "Parametres");
    result.set(Language::German, "settings", "Einstellungen");
    result.set(Language::Italian, "settings", "Impostazioni");
    result.set(Language::Japanese, "settings", "設定");
    result.set(Language::Spanish, "settings", "Ajustes");
    result.set(Language::English, "game_files", "Game files");
    result.set(Language::French, "game_files", "Fichiers du jeu");
    result.set(Language::German, "game_files", "Spieldateien");
    result.set(Language::Italian, "game_files", "File di gioco");
    result.set(Language::Japanese, "game_files", "ゲームファイル");
    result.set(Language::Spanish, "game_files", "Archivos del juego");
    return result;
}

std::string replace_non_ascii(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        const auto byte = static_cast<std::uint8_t>(value[i]);
        if ((byte & 0xa0u) == 0xa0u) {
            return "<kanji>";
        }
        if ((byte & 0x80u) == 0) {
            result.push_back(static_cast<char>(byte));
            continue;
        }
        if (i + 1 >= value.size()) {
            throw std::out_of_range("incomplete encoded string character");
        }
        const auto next = static_cast<std::uint8_t>(value[++i]);
        const auto index = static_cast<std::uint32_t>(next & 0x3fu)
            | (static_cast<std::uint32_t>(byte & 0x1fu) << 6);
        if (index >= 128 && index - 128 < NonAscii.size()) {
            result += NonAscii[index - 128];
        } else {
            append_utf8(result, index);
        }
    }
    return result;
}

} // namespace fruityprime::strings
