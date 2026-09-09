#include "Mods/Chat/chat_font.hpp"

#include <algorithm>
#include <bit>
#include <initializer_list>
#include <string_view>

namespace fruityprime::chat::font {
namespace {

constexpr std::size_t Count =
    static_cast<std::size_t>(Last - First + 1);

struct FontData {
    PixelArray pixels{};
    WidthArray widths{};

    void define(char16_t ch, std::int32_t advance) noexcept {
        widths[static_cast<std::size_t>(ch - First)] = advance;
    }

    void define(char16_t ch, std::int32_t top,
                std::initializer_list<std::string_view> rows) noexcept {
        const std::size_t glyph =
            static_cast<std::size_t>(ch - First) * Cell * Cell;
        std::int32_t width = 0;
        std::int32_t row_index = 0;
        for (const std::string_view row : rows) {
            for (std::size_t column = 0;
                 column < row.size() && column < Cell; ++column) {
                if (row[column] == '#') {
                    const std::int32_t pixel_row = top + row_index;
                    pixels[glyph + static_cast<std::size_t>(pixel_row) *
                                       Cell + column] = 1;
                    width = std::max(
                        width, static_cast<std::int32_t>(column + 1));
                }
            }
            ++row_index;
        }
        widths[static_cast<std::size_t>(ch - First)] = width + 1;
    }
};

FontData make_font() {
    FontData font{};
    font.define(' ', 3);
    font.define('!', 0, {"#", "#", "#", "#", ".", "#"});
    font.define('"', 0, {"#.#", "#.#"});
    font.define('#', 1, {".#.#.", "#####", ".#.#.", "#####", ".#.#."});
    font.define('$', 0, {"..#..", ".####", "##...", ".###.", "...##", "####.", "..#.."});
    font.define('%', 1, {"##..#", "##.#.", "..#..", ".#.##", "#..##"});
    font.define('&', 1, {".##..", "#..#.", ".##..", "#..##", ".##.#"});
    font.define('\'', 0, {"#", "#"});
    font.define('(', 0, {".#", "#.", "#.", "#.", "#.", ".#"});
    font.define(')', 0, {"#.", ".#", ".#", ".#", ".#", "#."});
    font.define('*', 1, {"#.#", ".#.", "#.#"});
    font.define('+', 2, {".#.", "###", ".#."});
    font.define(',', 5, {".#", "#."});
    font.define('-', 3, {"###"});
    font.define('.', 5, {"#"});
    font.define('/', 1, {"...#", "..#.", "..#.", ".#..", "#..."});
    font.define('0', 0, {".##.", "#..#", "#.##", "##.#", "#..#", ".##."});
    font.define('1', 0, {".#.", "##.", ".#.", ".#.", ".#.", "###"});
    font.define('2', 0, {".##.", "#..#", "...#", "..#.", ".#..", "####"});
    font.define('3', 0, {"###.", "...#", ".##.", "...#", "#..#", ".##."});
    font.define('4', 0, {"..#.", ".##.", "#.#.", "####", "..#.", "..#."});
    font.define('5', 0, {"####", "#...", "###.", "...#", "#..#", ".##."});
    font.define('6', 0, {".##.", "#...", "###.", "#..#", "#..#", ".##."});
    font.define('7', 0, {"####", "...#", "..#.", "..#.", ".#..", ".#.."});
    font.define('8', 0, {".##.", "#..#", ".##.", "#..#", "#..#", ".##."});
    font.define('9', 0, {".##.", "#..#", "#..#", ".###", "...#", ".##."});
    font.define(':', 2, {"#", ".", ".", "#"});
    font.define(';', 2, {".#", "..", "..", ".#", "#."});
    font.define('<', 1, {"..#", ".#.", "#..", ".#.", "..#"});
    font.define('=', 2, {"####", "....", "####"});
    font.define('>', 1, {"#..", ".#.", "..#", ".#.", "#.."});
    font.define('?', 0, {".##.", "#..#", "...#", "..#.", "....", "..#."});
    font.define('@', 0, {".###.", "#...#", "#.###", "#.#.#", "#....", ".###."});
    font.define('A', 0, {".###.", "#...#", "#...#", "#####", "#...#", "#...#"});
    font.define('B', 0, {"####.", "#...#", "####.", "#...#", "#...#", "####."});
    font.define('C', 0, {".###.", "#...#", "#....", "#....", "#...#", ".###."});
    font.define('D', 0, {"####.", "#...#", "#...#", "#...#", "#...#", "####."});
    font.define('E', 0, {"#####", "#....", "####.", "#....", "#....", "#####"});
    font.define('F', 0, {"#####", "#....", "####.", "#....", "#....", "#...."});
    font.define('G', 0, {".###.", "#...#", "#....", "#..##", "#...#", ".###."});
    font.define('H', 0, {"#...#", "#...#", "#####", "#...#", "#...#", "#...#"});
    font.define('I', 0, {"###", ".#.", ".#.", ".#.", ".#.", "###"});
    font.define('J', 0, {"...##", "....#", "....#", "....#", "#...#", ".###."});
    font.define('K', 0, {"#...#", "#..#.", "##...", "##...", "#..#.", "#...#"});
    font.define('L', 0, {"#....", "#....", "#....", "#....", "#....", "#####"});
    font.define('M', 0, {"#...#", "##.##", "#.#.#", "#...#", "#...#", "#...#"});
    font.define('N', 0, {"#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#"});
    font.define('O', 0, {".###.", "#...#", "#...#", "#...#", "#...#", ".###."});
    font.define('P', 0, {"####.", "#...#", "#...#", "####.", "#....", "#...."});
    font.define('Q', 0, {".###.", "#...#", "#...#", "#...#", "#..#.", ".##.#"});
    font.define('R', 0, {"####.", "#...#", "#...#", "####.", "#..#.", "#...#"});
    font.define('S', 0, {".####", "#....", ".###.", "....#", "....#", "####."});
    font.define('T', 0, {"#####", "..#..", "..#..", "..#..", "..#..", "..#.."});
    font.define('U', 0, {"#...#", "#...#", "#...#", "#...#", "#...#", ".###."});
    font.define('V', 0, {"#...#", "#...#", "#...#", ".#.#.", ".#.#.", "..#.."});
    font.define('W', 0, {"#...#", "#...#", "#...#", "#.#.#", "#.#.#", ".#.#."});
    font.define('X', 0, {"#...#", "#...#", ".#.#.", ".#.#.", "#...#", "#...#"});
    font.define('Y', 0, {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#.."});
    font.define('Z', 0, {"#####", "....#", "...#.", "..#..", ".#...", "#####"});
    font.define('[', 0, {"##", "#.", "#.", "#.", "#.", "##"});
    font.define('\\', 1, {"#...", ".#..", ".#..", "..#.", "...#"});
    font.define(']', 0, {"##", ".#", ".#", ".#", ".#", "##"});
    font.define('^', 0, {".#.", "#.#"});
    font.define('_', 6, {"####"});
    font.define('`', 0, {"#.", ".#"});
    font.define('a', 1, {".##.", "...#", ".###", "#..#", ".###"});
    font.define('b', 0, {"#...", "#...", "###.", "#..#", "#..#", "###."});
    font.define('c', 1, {".##.", "#..#", "#...", "#..#", ".##."});
    font.define('d', 0, {"...#", "...#", ".###", "#..#", "#..#", ".###"});
    font.define('e', 1, {".##.", "#..#", "####", "#...", ".###"});
    font.define('f', 0, {"..##", ".#..", "###.", ".#..", ".#..", ".#.."});
    font.define('g', 1, {".###", "#..#", "#..#", ".###", "...#", "###."});
    font.define('h', 0, {"#...", "#...", "###.", "#..#", "#..#", "#..#"});
    font.define('i', 0, {"#", ".", "#", "#", "#", "#"});
    font.define('j', 0, {".#", "..", ".#", ".#", ".#", ".#", "#."});
    font.define('k', 0, {"#...", "#..#", "#.#.", "##..", "#.#.", "#..#"});
    font.define('l', 0, {"##", ".#", ".#", ".#", ".#", ".#"});
    font.define('m', 1, {"##.#.", "#.#.#", "#.#.#", "#.#.#", "#.#.#"});
    font.define('n', 1, {"###.", "#..#", "#..#", "#..#", "#..#"});
    font.define('o', 1, {".##.", "#..#", "#..#", "#..#", ".##."});
    font.define('p', 1, {"###.", "#..#", "#..#", "###.", "#...", "#..."});
    font.define('q', 1, {".###", "#..#", "#..#", ".###", "...#", "...#"});
    font.define('r', 1, {"#.##", "##..", "#...", "#...", "#..."});
    font.define('s', 1, {".###", "#...", ".##.", "...#", "###."});
    font.define('t', 0, {".#..", ".#..", "###.", ".#..", ".#..", "..##"});
    font.define('u', 1, {"#..#", "#..#", "#..#", "#..#", ".###"});
    font.define('v', 1, {"#...#", "#...#", ".#.#.", ".#.#.", "..#.."});
    font.define('w', 1, {"#...#", "#...#", "#.#.#", "#.#.#", ".#.#."});
    font.define('x', 1, {"#..#", "#..#", ".##.", "#..#", "#..#"});
    font.define('y', 1, {"#..#", "#..#", "#..#", ".###", "...#", "###."});
    font.define('z', 1, {"####", "...#", ".##.", "#...", "####"});
    font.define('{', 0, {"..#", ".#.", "##.", ".#.", ".#.", "..#"});
    font.define('|', 0, {"#", "#", "#", "#", "#", "#"});
    font.define('}', 0, {"#..", ".#.", ".##", ".#.", ".#.", "#.."});
    font.define('~', 3, {".#..#", "#..#."});
    return font;
}

FontData& data() noexcept {
    static FontData font = make_font();
    return font;
}

template <typename Char>
std::int32_t measure_text(const std::basic_string_view<Char> text) noexcept {
    std::uint32_t width = 0;
    for (const Char ch : text) {
        const std::int32_t glyph = index(ch);
        if (glyph >= 0) {
            width += static_cast<std::uint32_t>(
                data().widths[static_cast<std::size_t>(glyph)]);
        }
    }
    return std::bit_cast<std::int32_t>(width);
}

} // namespace

PixelArray& pixels() noexcept {
    return data().pixels;
}

WidthArray& widths() noexcept {
    return data().widths;
}

std::int32_t index(char16_t ch) noexcept {
    (void)data();
    return ch < First || ch > Last
        ? -1
        : static_cast<std::int32_t>(ch - First);
}

std::int32_t index(char ch) noexcept {
    return index(static_cast<char16_t>(static_cast<unsigned char>(ch)));
}

std::int32_t measure(const std::u16string_view text) noexcept {
    return measure_text(text);
}

std::int32_t measure(const std::string_view text) noexcept {
    return measure_text(text);
}

} // namespace fruityprime::chat::font
